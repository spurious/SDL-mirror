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

/* PSP2 SDL video driver implementation; this is just enough to make an
 *  SDL-based application THINK it's got a working video driver, for
 *  applications that call SDL_Init(SDL_INIT_VIDEO) when they don't need it,
 *  and also for use as a collection of stubs when porting SDL to a new
 *  platform for which you haven't yet written a valid video driver.
 *
 * This is also a great way to determine bottlenecks: if you think that SDL
 *  is a performance problem for a given platform, enable this driver, and
 *  then see if your application runs faster without video overhead.
 *
 * Initial work by Ryan C. Gordon (icculus@icculus.org). A good portion
 *  of this was cut-and-pasted from Stephane Peter's work in the AAlib
 *  SDL video driver.  Renamed to "DUMMY" by Sam Lantinga.
 */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#include "SDL_psp2video.h"
#include "SDL_psp2events_c.h"
#include "SDL_psp2mouse_c.h"

#define PSP2VID_DRIVER_NAME "psp2"

#define SCREEN_W 960
#define SCREEN_H 544

typedef struct private_hwdata {
	vita2d_texture *texture;
} private_hwdata;

/* Initialization/Query functions */
static int PSP2_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **PSP2_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *PSP2_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static int PSP2_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors);
static void PSP2_VideoQuit(_THIS);

/* Hardware surface functions */
static int PSP2_FlipHWSurface(_THIS, SDL_Surface *surface);
static int PSP2_AllocHWSurface(_THIS, SDL_Surface *surface);
static int PSP2_LockHWSurface(_THIS, SDL_Surface *surface);
static void PSP2_UnlockHWSurface(_THIS, SDL_Surface *surface);
static void PSP2_FreeHWSurface(_THIS, SDL_Surface *surface);

/* etc. */
static void PSP2_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* PSP2 driver bootstrap functions */
static int PSP2_Available(void)
{
	return 1;
}

static void PSP2_DeleteDevice(SDL_VideoDevice *device)
{
	SDL_free(device->hidden);
	SDL_free(device);
}

static SDL_VideoDevice *PSP2_CreateDevice(int devindex)
{
	SDL_VideoDevice *device;

	/* Initialize all variables that we clean on shutdown */
	device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
	if ( device ) {
		SDL_memset(device, 0, (sizeof *device));
		device->hidden = (struct SDL_PrivateVideoData *)
				SDL_malloc((sizeof *device->hidden));
	}
	if ( (device == NULL) || (device->hidden == NULL) ) {
		SDL_OutOfMemory();
		if ( device ) {
			SDL_free(device);
		}
		return(0);
	}
	SDL_memset(device->hidden, 0, (sizeof *device->hidden));

	/* Set the function pointers */
	device->VideoInit = PSP2_VideoInit;
	device->ListModes = PSP2_ListModes;
	device->SetVideoMode = PSP2_SetVideoMode;
	device->CreateYUVOverlay = NULL;
	device->SetColors = PSP2_SetColors;
	device->UpdateRects = PSP2_UpdateRects;
	device->VideoQuit = PSP2_VideoQuit;
	device->AllocHWSurface = PSP2_AllocHWSurface;
	device->CheckHWBlit = NULL;
	device->FillHWRect = NULL;
	device->SetHWColorKey = NULL;
	device->SetHWAlpha = NULL;
	device->LockHWSurface = PSP2_LockHWSurface;
	device->UnlockHWSurface = PSP2_UnlockHWSurface;
	device->FlipHWSurface = PSP2_FlipHWSurface;
	device->FreeHWSurface = PSP2_FreeHWSurface;
	device->SetCaption = NULL;
	device->SetIcon = NULL;
	device->IconifyWindow = NULL;
	device->GrabInput = NULL;
	device->GetWMInfo = NULL;
	device->InitOSKeymap = PSP2_InitOSKeymap;
	device->PumpEvents = PSP2_PumpEvents;

	device->free = PSP2_DeleteDevice;

	return device;
}

int PSP2_VideoInit(_THIS, SDL_PixelFormat *vformat)
{
	vita2d_init();
	vita2d_set_vblank_wait(1);

	vformat->BitsPerPixel = 16;
	vformat->BytesPerPixel = 2;
	vformat->Rmask = 0xF800;
	vformat->Gmask = 0x07E0;
	vformat->Bmask = 0x001F;
	vformat->Amask = 0x0000;

	return(0);
}

SDL_Surface *PSP2_SetVideoMode(_THIS, SDL_Surface *current,
				int width, int height, int bpp, Uint32 flags)
{
	// support only 16/24 bits for now
	switch(bpp)
	{
		case 16:
		break;

		case 24:
			if (!SDL_ReallocFormat(current, 24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000))
			{
				SDL_SetError("Couldn't allocate new pixel format for requested mode");
				return(NULL);
			}
		break;

		/* // TODO: crash
		case 32:
			if (!SDL_ReallocFormat(current, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000))
			{
				SDL_SetError("Couldn't allocate new pixel format for requested mode");
				return(NULL);
			}
		break;
		*/

		default:
			if (!SDL_ReallocFormat(current, 16, 0xF800, 0x07E0, 0x001F, 0x0000))
			{
				SDL_SetError("Couldn't allocate new pixel format for requested mode");
				return(NULL);
			}
		break;
	}

	current->flags = flags | SDL_FULLSCREEN | SDL_DOUBLEBUF;
	current->w = width;
	current->h = height;
	if(current->hwdata == NULL)
	{
		PSP2_AllocHWSurface(this, current);
	}

	return(current);
}

SDL_Rect **PSP2_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags)
{
	static SDL_Rect PSP2_Rects[] = {
		{0, 0, 960, 544},
	};
	static SDL_Rect *PSP2_modes[] = {
		&PSP2_Rects[0],
		NULL
	};
	SDL_Rect **modes = PSP2_modes;

	// support only 16/24 bits for now
	switch(format->BitsPerPixel)
	{
		case 16:
		case 24:
		//case 32: // TODO
		return modes;

		default:
		return (SDL_Rect **) -1;
	}
}

static int PSP2_AllocHWSurface(_THIS, SDL_Surface *surface)
{
	surface->hwdata = (private_hwdata*) SDL_malloc (sizeof (private_hwdata));
	if (surface->hwdata == NULL)
	{
		SDL_OutOfMemory();
		return -1;
	}
	SDL_memset (surface->hwdata, 0, sizeof(private_hwdata));

	switch(surface->format->BitsPerPixel)
	{
		case 16:
			surface->hwdata->texture =
				vita2d_create_empty_texture_format(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_R5G6B5);
		break;

		case 24:
			surface->hwdata->texture =
				vita2d_create_empty_texture_format(surface->w, surface->h, SCE_GXM_TEXTURE_FORMAT_U8U8U8_RGB);
		break;

		/* // TODO: crash
		case 32:
			surface->hwdata->texture =
				vita2d_create_empty_texture_format(surface->w, surface->h, SCE_GXM_COLOR_FORMAT_A8B8G8R8);
		break;
		*/

		default:
			SDL_SetError("unsupported BitsPerPixel: %i\n", surface->format->BitsPerPixel);
		return -1;
	}

	surface->pixels = vita2d_texture_get_datap(surface->hwdata->texture);
	surface->pitch = vita2d_texture_get_stride(surface->hwdata->texture);
	surface->flags |= SDL_HWSURFACE;

	return(0);
}

static void PSP2_FreeHWSurface(_THIS, SDL_Surface *surface)
{
	vita2d_wait_rendering_done();
	vita2d_free_texture(surface->hwdata->texture);
	SDL_free(surface->hwdata);
	surface->pixels = NULL;
}

static int PSP2_FlipHWSurface(_THIS, SDL_Surface *surface)
{
	vita2d_start_drawing();
	vita2d_draw_texture(surface->hwdata->texture, 0, 0);
	vita2d_end_drawing();
	vita2d_wait_rendering_done();
	vita2d_swap_buffers();
}

static int PSP2_LockHWSurface(_THIS, SDL_Surface *surface)
{
	return(0);
}

static void PSP2_UnlockHWSurface(_THIS, SDL_Surface *surface)
{
	return;
}

static void PSP2_UpdateRects(_THIS, int numrects, SDL_Rect *rects)
{

}

int PSP2_SetColors(_THIS, int firstcolor, int ncolors, SDL_Color *colors)
{
	return(1);
}

void PSP2_VideoQuit(_THIS)
{
	if (this->screen->hwdata != NULL && this->screen->pixels != NULL)
	{
		PSP2_FreeHWSurface(this, this->screen);
	}
	vita2d_fini();
}

VideoBootStrap PSP2_bootstrap = {
	PSP2VID_DRIVER_NAME, "SDL psp2 video driver",
	PSP2_Available, PSP2_CreateDevice
};


