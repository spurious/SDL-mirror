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

#if SDL_VIDEO_RENDER_VITA

#include "SDL_hints.h"
#include "../SDL_sysrender.h"

#include <psp2/types.h>
#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/moduleinfo.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>

#include <vita2d.h>

#define sceKernelDcacheWritebackAll() (void)0

/* VITA renderer implementation, based on the vita2d lib  */


extern int SDL_RecreateWindow(SDL_Window *window, Uint32 flags);


static SDL_Renderer *VITA_CreateRenderer(SDL_Window *window, Uint32 flags);
static void VITA_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event);
static int VITA_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static int VITA_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
	const SDL_Rect *rect, const void *pixels, int pitch);
static int VITA_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
	const SDL_Rect *rect, void **pixels, int *pitch);
static void VITA_UnlockTexture(SDL_Renderer *renderer,
	 SDL_Texture *texture);
static int VITA_SetRenderTarget(SDL_Renderer *renderer,
		 SDL_Texture *texture);
static int VITA_UpdateViewport(SDL_Renderer *renderer);
static int VITA_RenderClear(SDL_Renderer *renderer);
static int VITA_RenderDrawPoints(SDL_Renderer *renderer,
		const SDL_FPoint *points, int count);
static int VITA_RenderDrawLines(SDL_Renderer *renderer,
		const SDL_FPoint *points, int count);
static int VITA_RenderFillRects(SDL_Renderer *renderer,
		const SDL_FRect *rects, int count);
static int VITA_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
	const SDL_Rect *srcrect, const SDL_FRect *dstrect);
static int VITA_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
	Uint32 pixel_format, void *pixels, int pitch);
static int VITA_RenderCopyEx(SDL_Renderer *renderer, SDL_Texture *texture,
	const SDL_Rect *srcrect, const SDL_FRect *dstrect,
	const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip);
static void VITA_RenderPresent(SDL_Renderer *renderer);
static void VITA_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture);
static void VITA_DestroyRenderer(SDL_Renderer *renderer);


SDL_RenderDriver VITA_RenderDriver = {
	.CreateRenderer = VITA_CreateRenderer,
	.info = {
		.name = "VITA",
		.flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC,
		.num_texture_formats = 1,
		.texture_formats = {
		[0] = SDL_PIXELFORMAT_ABGR8888,
		},
		.max_texture_width = 1024,
		.max_texture_height = 1024,
	 }
};

#define VITA_SCREEN_WIDTH	960
#define VITA_SCREEN_HEIGHT	544

#define VITA_FRAME_BUFFER_WIDTH	1024
#define VITA_FRAME_BUFFER_SIZE	(VITA_FRAME_BUFFER_WIDTH*VITA_SCREEN_HEIGHT)

#define COL5650(r,g,b,a)	((r>>3) | ((g>>2)<<5) | ((b>>3)<<11))
#define COL5551(r,g,b,a)	((r>>3) | ((g>>3)<<5) | ((b>>3)<<10) | (a>0?0x7000:0))
#define COL4444(r,g,b,a)	((r>>4) | ((g>>4)<<4) | ((b>>4)<<8) | ((a>>4)<<12))
#define COL8888(r,g,b,a)	((r) | ((g)<<8) | ((b)<<16) | ((a)<<24))

typedef struct
{
	void		*frontbuffer;
	void		*backbuffer;
	SDL_bool	initialized;
	SDL_bool	displayListAvail;
	unsigned int	psm;
	unsigned int	bpp;
	SDL_bool	vsync;
	unsigned int	currentColor;
	int		 currentBlendMode;

} VITA_RenderData;


typedef struct
{
	vita2d_texture	*tex;
	unsigned int	pitch;
	void		*data;
	unsigned int	w;
	unsigned int	h;
} VITA_TextureData;

static int
GetScaleQuality(void)
{
	const char *hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);

	if (!hint || *hint == '0' || SDL_strcasecmp(hint, "nearest") == 0) {
		return SCE_GXM_TEXTURE_FILTER_POINT; /* GU_NEAREST good for tile-map */
	} else {
		return SCE_GXM_TEXTURE_FILTER_LINEAR; /* GU_LINEAR good for scaling */
	}
}

static int
PixelFormatToVITAFMT(Uint32 format)
{
	switch (format) {
	case SDL_PIXELFORMAT_BGR565:
		return SCE_GXM_COLOR_FORMAT_R5G6B5;
	case SDL_PIXELFORMAT_ABGR1555:
		return SCE_GXM_COLOR_FORMAT_A1R5G5B5;
	case SDL_PIXELFORMAT_ABGR4444:
		return SCE_GXM_COLOR_FORMAT_A4R4G4B4;
	case SDL_PIXELFORMAT_ABGR8888:
	default:
		return SCE_GXM_COLOR_FORMAT_A8B8G8R8;
	}
}

void
StartDrawing(SDL_Renderer *renderer)
{
	VITA_RenderData *data = (VITA_RenderData *) renderer->driverdata;
	if(data->displayListAvail)
		return;

	vita2d_start_drawing();

	data->displayListAvail = SDL_TRUE;
}

SDL_Renderer *
VITA_CreateRenderer(SDL_Window *window, Uint32 flags)
{

	SDL_Renderer *renderer;
	VITA_RenderData *data;

	renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
	if (!renderer) {
		SDL_OutOfMemory();
		return NULL;
	}

	data = (VITA_RenderData *) SDL_calloc(1, sizeof(*data));
	if (!data) {
		VITA_DestroyRenderer(renderer);
		SDL_OutOfMemory();
		return NULL;
	}


	renderer->WindowEvent = VITA_WindowEvent;
	renderer->CreateTexture = VITA_CreateTexture;
	renderer->UpdateTexture = VITA_UpdateTexture;
	renderer->LockTexture = VITA_LockTexture;
	renderer->UnlockTexture = VITA_UnlockTexture;
	renderer->SetRenderTarget = VITA_SetRenderTarget;
	renderer->UpdateViewport = VITA_UpdateViewport;
	renderer->RenderClear = VITA_RenderClear;
	renderer->RenderDrawPoints = VITA_RenderDrawPoints;
	renderer->RenderDrawLines = VITA_RenderDrawLines;
	renderer->RenderFillRects = VITA_RenderFillRects;
	renderer->RenderCopy = VITA_RenderCopy;
	renderer->RenderReadPixels = VITA_RenderReadPixels;
	renderer->RenderCopyEx = VITA_RenderCopyEx;
	renderer->RenderPresent = VITA_RenderPresent;
	renderer->DestroyTexture = VITA_DestroyTexture;
	renderer->DestroyRenderer = VITA_DestroyRenderer;
	renderer->info = VITA_RenderDriver.info;
	renderer->info.flags = (SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
	renderer->driverdata = data;
	renderer->window = window;

	if (data->initialized != SDL_FALSE)
		return 0;
	data->initialized = SDL_TRUE;

	if (flags & SDL_RENDERER_PRESENTVSYNC) {
		data->vsync = SDL_TRUE;
	} else {
		data->vsync = SDL_FALSE;
	}

	vita2d_init();
	vita2d_set_vblank_wait(data->vsync);

	return renderer;
}

static void
VITA_WindowEvent(SDL_Renderer *renderer, const SDL_WindowEvent *event)
{

}


static int
VITA_CreateTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
/*	  VITA_RenderData *renderdata = (VITA_RenderData *) renderer->driverdata; */
	VITA_TextureData* vita_texture = (VITA_TextureData*) SDL_calloc(1, sizeof(*vita_texture));

	if(!vita_texture)
		return -1;

	vita_texture->tex = vita2d_create_empty_texture(texture->w, texture->h);

	if(!vita_texture->tex)
	{
		SDL_free(vita_texture);
		return SDL_OutOfMemory();
	}

	vita_texture->w = vita2d_texture_get_width(vita_texture->tex);
	vita_texture->h = vita2d_texture_get_height(vita_texture->tex);
	vita_texture->data = vita2d_texture_get_datap(vita_texture->tex);
	vita_texture->pitch = vita2d_texture_get_width(vita_texture->tex) *SDL_BYTESPERPIXEL(texture->format);

	texture->driverdata = vita_texture;

	return 0;
}


static int
VITA_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
				   const SDL_Rect *rect, const void *pixels, int pitch)
{
/*  VITA_TextureData *vita_texture = (VITA_TextureData *) texture->driverdata; */
	const Uint8 *src;
	Uint8 *dst;
	int row, length,dpitch;
	src = pixels;

	VITA_LockTexture(renderer, texture,rect,(void **)&dst, &dpitch);
	length = rect->w *SDL_BYTESPERPIXEL(texture->format);
	if (length == pitch && length == dpitch) {
		SDL_memcpy(dst, src, length*rect->h);
	} else {
		for (row = 0; row < rect->h; ++row) {
			SDL_memcpy(dst, src, length);
			src += pitch;
			dst += dpitch;
		}
	}

	sceKernelDcacheWritebackAll();
	return 0;
}

static int
VITA_LockTexture(SDL_Renderer *renderer, SDL_Texture *texture,
				 const SDL_Rect *rect, void **pixels, int *pitch)
{
	VITA_TextureData *vita_texture = (VITA_TextureData *) texture->driverdata;

	*pixels =
		(void *) ((Uint8 *) vita_texture->data + rect->y *vita_texture->w +
				  rect->x *SDL_BYTESPERPIXEL(texture->format));
	*pitch = vita_texture->pitch;
	return 0;
}

static void
VITA_UnlockTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
	VITA_TextureData *vita_texture = (VITA_TextureData *) texture->driverdata;
	SDL_Rect rect;

	/* We do whole texture updates, at least for now */
	rect.x = 0;
	rect.y = 0;
	rect.w = texture->w;
	rect.h = texture->h;
	VITA_UpdateTexture(renderer, texture, &rect, vita_texture->data, vita_texture->pitch);
}

static int
VITA_SetRenderTarget(SDL_Renderer *renderer, SDL_Texture *texture)
{

	return 0;
}

static int
VITA_UpdateViewport(SDL_Renderer *renderer)
{

	return 0;
}


static void
VITA_SetBlendMode(SDL_Renderer *renderer, int blendMode)
{
	/*VITA_RenderData *data = (VITA_RenderData *) renderer->driverdata;
	if (blendMode != data-> currentBlendMode) {
		switch (blendMode) {
		case SDL_BLENDMODE_NONE:
				sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
				sceGuDisable(GU_BLEND);
			break;
		case SDL_BLENDMODE_BLEND:
				sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
				sceGuEnable(GU_BLEND);
				sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );
			break;
		case SDL_BLENDMODE_ADD:
				sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
				sceGuEnable(GU_BLEND);
				sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00FFFFFF );
			break;
		case SDL_BLENDMODE_MOD:
				sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
				sceGuEnable(GU_BLEND);
				sceGuBlendFunc( GU_ADD, GU_FIX, GU_SRC_COLOR, 0, 0);
			break;
		}
		data->currentBlendMode = blendMode;
	}*/
}



static int
VITA_RenderClear(SDL_Renderer *renderer)
{
	/* start list */
	StartDrawing(renderer);

	int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
	vita2d_set_clear_color(color);

	vita2d_clear_screen();

	return 0;
}

static int
VITA_RenderDrawPoints(SDL_Renderer *renderer, const SDL_FPoint *points,
					  int count)
{
	int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
	int i;
	StartDrawing(renderer);

	for (i = 0; i < count; ++i) {
			vita2d_draw_pixel(points[i].x, points[i].y, color);
	}

	return 0;
}

static int
VITA_RenderDrawLines(SDL_Renderer *renderer, const SDL_FPoint *points,
					 int count)
{
	int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
	int i;
	StartDrawing(renderer);

	for (i = 0; i < count; ++i) {
	if (i < count -1) {
		vita2d_draw_line(points[i].x, points[i].y, points[i+1].x, points[i+1].y, color);
	}
	}

	return 0;
}

static int
VITA_RenderFillRects(SDL_Renderer *renderer, const SDL_FRect *rects,
					 int count)
{
	int color = renderer->a << 24 | renderer->b << 16 | renderer->g << 8 | renderer->r;
	int i;
	StartDrawing(renderer);

	for (i = 0; i < count; ++i) {
		const SDL_FRect *rect = &rects[i];

		vita2d_draw_rectangle(rect->x, rect->y, rect->w, rect->h, color);
	}

	return 0;
}


#define PI   3.14159265358979f

#define radToDeg(x) ((x)*180.f/PI)
#define degToRad(x) ((x)*PI/180.f)

float MathAbs(float x)
{
	return (x < 0) ? -x : x;
}

void MathSincos(float r, float *s, float *c)
{
	*s = sinf(r);
	*c = cosf(r);
}

void Swap(float *a, float *b)
{
	float n=*a;
	*a = *b;
	*b = n;
}

static int
VITA_RenderCopy(SDL_Renderer *renderer, SDL_Texture *texture,
				const SDL_Rect *srcrect, const SDL_FRect *dstrect)
{
	VITA_TextureData *vita_texture = (VITA_TextureData *) texture->driverdata;

	StartDrawing(renderer);

	VITA_SetBlendMode(renderer, renderer->blendMode);

	vita2d_draw_texture_part_scale(vita_texture->tex, dstrect->x, dstrect->y,
		srcrect->x, srcrect->y, srcrect->w, srcrect->h, dstrect->w/srcrect->w, dstrect->h/srcrect->h);

	return 0;
}

static int
VITA_RenderReadPixels(SDL_Renderer *renderer, const SDL_Rect *rect,
					Uint32 pixel_format, void *pixels, int pitch)

{
		return 0;
}


static int
VITA_RenderCopyEx(SDL_Renderer *renderer, SDL_Texture *texture,
				const SDL_Rect *srcrect, const SDL_FRect *dstrect,
				const double angle, const SDL_FPoint *center, const SDL_RendererFlip flip)
{
	/*float x, y, width, height;
	float u0, v0, u1, v1;
	unsigned char alpha;
	float centerx, centery;

	x = dstrect->x;
	y = dstrect->y;
	width = dstrect->w;
	height = dstrect->h;

	u0 = srcrect->x;
	v0 = srcrect->y;
	u1 = srcrect->x + srcrect->w;
	v1 = srcrect->y + srcrect->h;

	centerx = center->x;
	centery = center->y;

	alpha = texture->a;

	StartDrawing(renderer);
	TextureActivate(texture);
	VITA_SetBlendMode(renderer, renderer->blendMode);

	if(alpha != 255)
	{
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuColor(GU_RGBA(255, 255, 255, alpha));
	}else{
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		sceGuColor(0xFFFFFFFF);
	}

	x += centerx;
	y += centery;

	float c, s;

	MathSincos(degToRad(angle), &s, &c);

	width  -= centerx;
	height -= centery;


	float cw = c*width;
	float sw = s*width;
	float ch = c*height;
	float sh = s*height;

	VertTV* vertices = (VertTV*)sceGuGetMemory(sizeof(VertTV)<<2);

	vertices[0].u = u0;
	vertices[0].v = v0;
	vertices[0].x = x - cw + sh;
	vertices[0].y = y - sw - ch;
	vertices[0].z = 0;

	vertices[1].u = u0;
	vertices[1].v = v1;
	vertices[1].x = x - cw - sh;
	vertices[1].y = y - sw + ch;
	vertices[1].z = 0;

	vertices[2].u = u1;
	vertices[2].v = v1;
	vertices[2].x = x + cw - sh;
	vertices[2].y = y + sw + ch;
	vertices[2].z = 0;

	vertices[3].u = u1;
	vertices[3].v = v0;
	vertices[3].x = x + cw + sh;
	vertices[3].y = y + sw - ch;
	vertices[3].z = 0;

	if (flip & SDL_FLIP_HORIZONTAL) {
				Swap(&vertices[0].v, &vertices[2].v);
				Swap(&vertices[1].v, &vertices[3].v);
	}
	if (flip & SDL_FLIP_VERTICAL) {
				Swap(&vertices[0].u, &vertices[2].u);
				Swap(&vertices[1].u, &vertices[3].u);
	}

	sceGuDrawArray(GU_TRIANGLE_FAN, GU_TEXTURE_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_2D, 4, 0, vertices);

	if(alpha != 255)
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	return 0;*/
	return 1;
}

static void
VITA_RenderPresent(SDL_Renderer *renderer)
{
	VITA_RenderData *data = (VITA_RenderData *) renderer->driverdata;
	if(!data->displayListAvail)
		return;

	vita2d_end_drawing();
	vita2d_swap_buffers();

	data->displayListAvail = SDL_FALSE;
}

static void
VITA_DestroyTexture(SDL_Renderer *renderer, SDL_Texture *texture)
{
	VITA_RenderData *renderdata = (VITA_RenderData *) renderer->driverdata;
	VITA_TextureData *vita_texture = (VITA_TextureData *) texture->driverdata;

	if (renderdata == 0)
		return;

	if(vita_texture == 0)
		return;

	vita2d_free_texture(vita_texture->tex);
	SDL_free(vita_texture);
	texture->driverdata = NULL;
}

static void
VITA_DestroyRenderer(SDL_Renderer *renderer)
{
	VITA_RenderData *data = (VITA_RenderData *) renderer->driverdata;
	if (data) {
		if (!data->initialized)
			return;

		vita2d_fini();

		data->initialized = SDL_FALSE;
		data->displayListAvail = SDL_FALSE;
		SDL_free(data);
	}
	SDL_free(renderer);
}

#endif /* SDL_VIDEO_RENDER_VITA */

/* vi: set ts=4 sw=4 expandtab: */

