/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#if SDL_THREAD_PSP2

/* An implementation of semaphores using mutexes and condition variables */

#include "SDL_timer.h"
#include "SDL_thread.h"
#include "SDL_systhread_c.h"

#include <psp2/types.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr.h>

struct SDL_semaphore
{
	SceUID  semid;
};

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
	SDL_sem *sem;

	sem = (SDL_sem *)SDL_malloc(sizeof(*sem));
	if ( ! sem )
	{
		SDL_OutOfMemory();
		return NULL;
	}

	sem->semid = sceKernelCreateSema("SDL sema", 0, initial_value, 255, 0);
	if (sem->semid < 0)
	{
		SDL_SetError("Couldn't create semaphore");
		free(sem);
		sem = NULL;
	}

	return sem;
}

/* WARNING:
   You cannot call this function when another thread is using the semaphore.
*/
void SDL_DestroySemaphore(SDL_sem *sem)
{
	if (sem != NULL)
	{
		if (sem->semid > 0)
		{
            sceKernelDeleteSema(sem->semid);
            sem->semid = 0;
        }
        free(sem);
	}
}

int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout)
{
	Uint32 *pTimeout;
	unsigned int res;

    if (sem == NULL)
    {
        SDL_SetError("Passed a NULL sem");
        return 0;
    }

    if (timeout == 0)
    {
        res = sceKernelPollSema(sem->semid, 1);
        if (res < 0)
        {
            return SDL_MUTEX_TIMEDOUT;
        }
        return 0;
    }

    if (timeout == SDL_MUTEX_MAXWAIT)
    {
        pTimeout = NULL;
    }
    else
    {
        timeout *= 1000;  /* Convert to microseconds. */
        pTimeout = &timeout;
    }

    res = sceKernelWaitSema(sem->semid, 1, pTimeout);
	switch (res)
	{
		case SCE_KERNEL_OK:
			return 0;
		case SCE_KERNEL_ERROR_WAIT_TIMEOUT:
			return SDL_MUTEX_TIMEDOUT;
		default:
			SDL_SetError("WaitForSingleObject() failed");
			return -1;
	}
}

int SDL_SemTryWait(SDL_sem *sem)
{
	return SDL_SemWaitTimeout(sem, 0);
}

int SDL_SemWait(SDL_sem *sem)
{
	return SDL_SemWaitTimeout(sem, SDL_MUTEX_MAXWAIT);
}

Uint32 SDL_SemValue(SDL_sem *sem)
{
    SceKernelSemaInfo info;
	info.size = sizeof(info);

    if (sem == NULL)
    {
        SDL_SetError("Passed a NULL sem");
        return 0;
    }

    if (sceKernelGetSemaInfo(sem->semid, &info) >= 0)
    {
        return info.currentCount;
    }

    return 0;
}

int SDL_SemPost(SDL_sem *sem)
{
    int res;

    if (sem == NULL)
    {
		SDL_SetError("Passed a NULL sem");
        return -1;
    }

    res = sceKernelSignalSema(sem->semid, 1);
    if (res < 0)
    {
		SDL_SetError("sceKernelSignalSema() failed");
        return -1;
    }

    return 0;
}

#endif // SDL_THREAD_PSP2
