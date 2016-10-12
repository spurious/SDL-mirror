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

#ifndef _SDL_psp2audio_h
#define _SDL_psp2audio_h

#include "../SDL_sysaudio.h"

/* Hidden "this" pointer for the video functions */
#define _THIS	SDL_AudioDevice *this

#define NUM_BUFFERS 2

struct SDL_PrivateAudioData {
	/* The file descriptor for the audio device */
    /* The hardware output channel. */
    int     channel;
    /* The raw allocated mixing buffer. */
    Uint8   *rawbuf;
    /* Individual mixing buffers. */
    Uint8   *mixbufs[NUM_BUFFERS];
    /* Index of the next available mixing buffer. */
    int     next_buffer;
};

#endif /* _SDL_psp2audio_h */
