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

#if defined(SDL_TIMER_PSP2)

#include "SDL_timer.h"
#include "../SDL_timer_c.h"
#include <psp2/kernel/processmgr.h>

static uint64_t start;
static SDL_bool ticks_started = SDL_FALSE;

void SDL_StartTicks(void)
{
	if (ticks_started)
	{
		return;
	}
    ticks_started = SDL_TRUE;
    start = sceKernelGetProcessTimeWide();
}

Uint32 SDL_GetTicks (void)
{
    if (!ticks_started) {
        SDL_StartTicks();
    }

    uint64_t now;
    Uint32 ticks;

    now = sceKernelGetProcessTimeWide();
    ticks = (now - start)/1000;
    return (ticks);
}

void SDL_Delay (Uint32 ms)
{
	const Uint32 max_delay = 0xffffffffUL / 1000;
    if(ms > max_delay)
        ms = max_delay;
    sceKernelDelayThreadCB(ms * 1000);
}

#include "SDL_thread.h"

/* Data to handle a single periodic alarm */
static int timer_alive = 0;
static SDL_Thread *timer = NULL;

static int RunTimer(void *unused)
{
	while ( timer_alive ) {
		if ( SDL_timer_running ) {
			SDL_ThreadedTimerCheck();
		}
		SDL_Delay(1);
	}
	return(0);
}

/* This is only called if the event thread is not running */
int SDL_SYS_TimerInit(void)
{
	timer_alive = 1;
	timer = SDL_CreateThread(RunTimer, NULL);
	if ( timer == NULL )
		return(-1);
	return(SDL_SetTimerThreaded(1));
}

void SDL_SYS_TimerQuit(void)
{
	timer_alive = 0;
	if ( timer ) {
		SDL_WaitThread(timer, NULL);
		timer = NULL;
	}
}

int SDL_SYS_StartTimer(void)
{
	SDL_SetError("Internal logic error: threaded timer in use");
	return(-1);
}

void SDL_SYS_StopTimer(void)
{
	return;
}

#endif /* SDL_TIMER_PSP2 */
