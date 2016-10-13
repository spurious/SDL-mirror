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

#if SDL_VIDEO_DRIVER_VITA

/* SDL internals */
#include "../SDL_sysvideo.h"
#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_keyboard_c.h"



/* VITA declarations */
#include "SDL_vitavideo.h"

/* unused
static SDL_bool VITA_initialized = SDL_FALSE;
*/
static int
VITA_Available(void)
{
    return 1;
}

static void
VITA_Destroy(SDL_VideoDevice * device)
{
/*    SDL_VideoData *phdata = (SDL_VideoData *) device->driverdata; */

    if (device->driverdata != NULL) {
        device->driverdata = NULL;
    }
}

static SDL_VideoDevice *
VITA_Create()
{
    SDL_VideoDevice *device;
    SDL_VideoData *phdata;
    int status;

    /* Check if VITA could be initialized */
    status = VITA_Available();
    if (status == 0) {
        /* VITA could not be used */
        return NULL;
    }

    /* Initialize SDL_VideoDevice structure */
    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Initialize internal VITA specific data */
    phdata = (SDL_VideoData *) SDL_calloc(1, sizeof(SDL_VideoData));
    if (phdata == NULL) {
        SDL_OutOfMemory();
        SDL_free(device);
        return NULL;
    }


    device->driverdata = phdata;

    /* Setup amount of available displays and current display */
    device->num_displays = 0;

    /* Set device free function */
    device->free = VITA_Destroy;

    /* Setup all functions which we can handle */
    device->VideoInit = VITA_VideoInit;
    device->VideoQuit = VITA_VideoQuit;
    device->GetDisplayModes = VITA_GetDisplayModes;
    device->SetDisplayMode = VITA_SetDisplayMode;
    device->CreateWindow = VITA_CreateWindow;
    device->CreateWindowFrom = VITA_CreateWindowFrom;
    device->SetWindowTitle = VITA_SetWindowTitle;
    device->SetWindowIcon = VITA_SetWindowIcon;
    device->SetWindowPosition = VITA_SetWindowPosition;
    device->SetWindowSize = VITA_SetWindowSize;
    device->ShowWindow = VITA_ShowWindow;
    device->HideWindow = VITA_HideWindow;
    device->RaiseWindow = VITA_RaiseWindow;
    device->MaximizeWindow = VITA_MaximizeWindow;
    device->MinimizeWindow = VITA_MinimizeWindow;
    device->RestoreWindow = VITA_RestoreWindow;
    device->SetWindowGrab = VITA_SetWindowGrab;
    device->DestroyWindow = VITA_DestroyWindow;
    device->GetWindowWMInfo = VITA_GetWindowWMInfo;
    device->HasScreenKeyboardSupport = VITA_HasScreenKeyboardSupport;
    device->ShowScreenKeyboard = VITA_ShowScreenKeyboard;
    device->HideScreenKeyboard = VITA_HideScreenKeyboard;
    device->IsScreenKeyboardShown = VITA_IsScreenKeyboardShown;

	device->PumpEvents = VITA_PumpEvents;

    return device;
}

VideoBootStrap VITA_bootstrap = {
    "VITA",
    "VITA Video Driver",
    VITA_Available,
    VITA_Create
};

/*****************************************************************************/
/* SDL Video and Display initialization/handling functions                   */
/*****************************************************************************/
int
VITA_VideoInit(_THIS)
{
    SDL_VideoDisplay display;
    SDL_DisplayMode current_mode;

    SDL_zero(current_mode);

    current_mode.w = 960;
    current_mode.h = 544;

    current_mode.refresh_rate = 60;
    /* 32 bpp for default */
    current_mode.format = SDL_PIXELFORMAT_ABGR8888;

    current_mode.driverdata = NULL;

    SDL_zero(display);
    display.desktop_mode = current_mode;
    display.current_mode = current_mode;
    display.driverdata = NULL;

    SDL_AddVideoDisplay(&display);

    return 1;
}

void
VITA_VideoQuit(_THIS)
{

}

void
VITA_GetDisplayModes(_THIS, SDL_VideoDisplay * display)
{

}

int
VITA_SetDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode)
{
    return 0;
}
#define EGLCHK(stmt)                            \
    do {                                        \
        EGLint err;                             \
                                                \
        stmt;                                   \
        err = eglGetError();                    \
        if (err != EGL_SUCCESS) {               \
            SDL_SetError("EGL error %d", err);  \
            return 0;                           \
        }                                       \
    } while (0)

int
VITA_CreateWindow(_THIS, SDL_Window * window)
{
    SDL_WindowData *wdata;

    /* Allocate window internal data */
    wdata = (SDL_WindowData *) SDL_calloc(1, sizeof(SDL_WindowData));
    if (wdata == NULL) {
        return SDL_OutOfMemory();
    }

    /* Setup driver data for this window */
    window->driverdata = wdata;

    // fix input, we need to find a better way
    SDL_SetKeyboardFocus(window);

    /* Window has been successfully created */
    return 0;
}

int
VITA_CreateWindowFrom(_THIS, SDL_Window * window, const void *data)
{
    return -1;
}

void
VITA_SetWindowTitle(_THIS, SDL_Window * window)
{
}
void
VITA_SetWindowIcon(_THIS, SDL_Window * window, SDL_Surface * icon)
{
}
void
VITA_SetWindowPosition(_THIS, SDL_Window * window)
{
}
void
VITA_SetWindowSize(_THIS, SDL_Window * window)
{
}
void
VITA_ShowWindow(_THIS, SDL_Window * window)
{
}
void
VITA_HideWindow(_THIS, SDL_Window * window)
{
}
void
VITA_RaiseWindow(_THIS, SDL_Window * window)
{
}
void
VITA_MaximizeWindow(_THIS, SDL_Window * window)
{
}
void
VITA_MinimizeWindow(_THIS, SDL_Window * window)
{
}
void
VITA_RestoreWindow(_THIS, SDL_Window * window)
{
}
void
VITA_SetWindowGrab(_THIS, SDL_Window * window, SDL_bool grabbed)
{

}
void
VITA_DestroyWindow(_THIS, SDL_Window * window)
{
}

/*****************************************************************************/
/* SDL Window Manager function                                               */
/*****************************************************************************/
SDL_bool
VITA_GetWindowWMInfo(_THIS, SDL_Window * window, struct SDL_SysWMinfo *info)
{
    if (info->version.major <= SDL_MAJOR_VERSION) {
        return SDL_TRUE;
    } else {
        SDL_SetError("application not compiled with SDL %d.%d\n",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }

    /* Failed to get window manager information */
    return SDL_FALSE;
}


/* TO Write Me */
SDL_bool VITA_HasScreenKeyboardSupport(_THIS)
{
    return SDL_FALSE;
}
void VITA_ShowScreenKeyboard(_THIS, SDL_Window *window)
{
}
void VITA_HideScreenKeyboard(_THIS, SDL_Window *window)
{
}
SDL_bool VITA_IsScreenKeyboardShown(_THIS, SDL_Window *window)
{
    return SDL_FALSE;
}

void VITA_PumpEvents(_THIS)
{
}

#endif /* SDL_VIDEO_DRIVER_VITA */

/* vi: set ts=4 sw=4 expandtab: */
