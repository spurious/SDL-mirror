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

/* Thread management routines for SDL */

#if SDL_THREAD_PSP2

#include "SDL_thread.h"
#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"

#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>

static int ThreadEntry(SceSize args, void *argp)
{
    SDL_RunThread(*(void **) argp);
    return 0;
}

int SDL_SYS_CreateThread(SDL_Thread *thread, void *args)
{
    SceKernelThreadInfo info;
    int priority = 32;

    /* Set priority of new thread to the same as the current thread */
    info.size = sizeof(SceKernelThreadInfo);
    if (sceKernelGetThreadInfo(sceKernelGetThreadId(), &info) == 0)
    {
        priority = info.currentPriority;
    }

    thread->handle = sceKernelCreateThread("SDL thread", ThreadEntry,
                           priority, 0x10000, 0, 0, NULL);

    if (thread->handle < 0)
    {
		SDL_SetError("sceKernelCreateThread() failed");
        return -1;
    }

    sceKernelStartThread(thread->handle, 4, &args);
    return 0;
}

void SDL_SYS_SetupThread(void)
{
	return;
}

Uint32 SDL_ThreadID(void)
{
	return (Uint32) sceKernelGetThreadId();
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    sceKernelWaitThreadEnd(thread->handle, NULL, NULL);
    sceKernelDeleteThread(thread->handle);
}

void SDL_SYS_KillThread(SDL_Thread *thread)
{
	sceKernelDeleteThread(thread->handle);
}

#endif // SDL_THREAD_PSP2
