/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_JOYSTICK_VITA

/* This is the PSVita implementation of the SDL joystick API */
#include <psp2/types.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>

#include <stdio.h>      /* For the definition of NULL */
#include <stdlib.h>

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#include "SDL_events.h"
#include "SDL_error.h"
#include "SDL_thread.h"
#include "SDL_mutex.h"
#include "SDL_timer.h"

/* Current pad state */
static SceCtrlData pad = { .lx = 0, .ly = 0, .rx = 0, .ry = 0, .buttons = 0 };
static const unsigned int button_map[] = {
    SCE_CTRL_TRIANGLE, SCE_CTRL_CIRCLE, SCE_CTRL_CROSS, SCE_CTRL_SQUARE,
    SCE_CTRL_LTRIGGER, SCE_CTRL_RTRIGGER,
    SCE_CTRL_DOWN, SCE_CTRL_LEFT, SCE_CTRL_UP, SCE_CTRL_RIGHT,
    SCE_CTRL_SELECT, SCE_CTRL_START};
static int analog_map[256];  /* Map analog inputs to -32768 -> 32767 */

typedef struct
{
  int x;
  int y;
} point;

/* 4 points define the bezier-curve. */
static point a = { 0, 0 };
static point b = { 50, 0  };
static point c = { 78, 32767 };
static point d = { 128, 32767 };

/* simple linear interpolation between two points */
static SDL_INLINE void lerp (point *dest, point *a, point *b, float t)
{
    dest->x = a->x + (b->x - a->x)*t;
    dest->y = a->y + (b->y - a->y)*t;
}

/* evaluate a point on a bezier-curve. t goes from 0 to 1.0 */
static int calc_bezier_y(float t)
{
    point ab, bc, cd, abbc, bccd, dest;
    lerp (&ab, &a, &b, t);           /* point between a and b */
    lerp (&bc, &b, &c, t);           /* point between b and c */
    lerp (&cd, &c, &d, t);           /* point between c and d */
    lerp (&abbc, &ab, &bc, t);       /* point between ab and bc */
    lerp (&bccd, &bc, &cd, t);       /* point between bc and cd */
    lerp (&dest, &abbc, &bccd, t);   /* point on the bezier-curve */
    return dest.y;
}

/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * It should return number of joysticks, or -1 on an unrecoverable fatal error.
 */
int SDL_SYS_JoystickInit(void)
{
    int i;

    /* Setup input */
    //sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    /* Create an accurate map from analog inputs (0 to 255)
       to SDL joystick positions (-32768 to 32767) */
    for (i = 0; i < 128; i++)
    {
        float t = (float)i/127.0f;
        analog_map[i+128] = calc_bezier_y(t);
        analog_map[127-i] = -1 * analog_map[i+128];
    }

    return 1;
}

int SDL_SYS_NumJoysticks()
{
    return 1;
}

void SDL_SYS_JoystickDetect()
{
}

/* Function to get the device-dependent name of a joystick */
const char * SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    return "PSVita builtin joypad";
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
    return device_index;
}

/* Function to get the device-dependent name of a joystick */
const char *SDL_SYS_JoystickName(int index)
{
    if (index == 0)
        return "PSVita controller";

    SDL_SetError("No joystick available with that index");
    return(NULL);
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int SDL_SYS_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    joystick->nbuttons = sizeof(button_map)/sizeof(*button_map);
    joystick->naxes = 4;
    joystick->nhats = 0;

    return 0;
}

/* Function to determine if this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
    return SDL_TRUE;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void SDL_SYS_JoystickUpdate(SDL_Joystick *joystick)
{
    int i;
    unsigned int buttons;
    unsigned int changed;
    unsigned char lx, ly, rx, ry;
    static unsigned int old_buttons = 0;
    static unsigned char old_lx = 0, old_ly = 0;
    static unsigned char old_rx = 0, old_ry = 0;

    sceCtrlPeekBufferPositive(0, &pad, 1);

    buttons = pad.buttons;
    lx = pad.lx;
    ly = pad.ly;
    rx = pad.rx;
    ry = pad.ry;

    /* Axes */
    if(old_lx != lx) {
        SDL_PrivateJoystickAxis(joystick, 0, analog_map[lx]);
        old_lx = lx;
    }
    if(old_ly != ly) {
        SDL_PrivateJoystickAxis(joystick, 1, analog_map[ly]);
        old_ly = ly;
    }
    if(old_rx != rx) {
        SDL_PrivateJoystickAxis(joystick, 2, analog_map[rx]);
        old_rx = rx;
    }
    if(old_ry != ry) {
        SDL_PrivateJoystickAxis(joystick, 3, analog_map[ry]);
        old_ry = ry;
    }

    /* Buttons */
    changed = old_buttons ^ buttons;
    old_buttons = buttons;
    if(changed) {
        for(i=0; i<sizeof(button_map)/sizeof(button_map[0]); i++) {
            if(changed & button_map[i]) {
                SDL_PrivateJoystickButton(
                    joystick, i,
                    (buttons & button_map[i]) ?
                    SDL_PRESSED : SDL_RELEASED);
            }
        }
    }

}

/* Function to close a joystick after use */
void SDL_SYS_JoystickClose(SDL_Joystick *joystick)
{
}

/* Function to perform any system-specific joystick related cleanup */
void SDL_SYS_JoystickQuit(void)
{
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID( int device_index )
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = SDL_SYS_JoystickNameForDeviceIndex( device_index );
    SDL_zero( guid );
    SDL_memcpy( &guid, name, SDL_min( sizeof(guid), SDL_strlen( name ) ) );
    return guid;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = joystick->name;
    SDL_zero( guid );
    SDL_memcpy( &guid, name, SDL_min( sizeof(guid), SDL_strlen( name ) ) );
    return guid;
}

#endif /* SDL_JOYSTICK_VITA */

/* vim: ts=4 sw=4
 */
