/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
 * Copyright (C) 2022 TooManySugar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Refresher setup TODO: desc
 *
 * =======================================================================
 */

#include "../ref_shared.h"
#include "header/local.h"

// 44
refimport_t ri;

/**
 * @brief 
 * 
 * @return
 * true  - success
 * false - fail
 */
// 452
static qboolean
rsx_init(void)
{
    static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
    return true;
}

// 619
void
rsx_shutdown(void)
{
    static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
}

// sdl part 113
/*
 * Swaps the buffers and shows the next frame.
 */
void
rsx_end_frame(void)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
}

// sdl part 129
qboolean
rsx_is_vsync_active(void)
{
	return true;
}

// sdl part 171
int
rsx_prepare_for_window(void)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
	return 0;
}
/*
 * Returns true at success
 * and false at failure.
 */
// sdl part 294
int 
rsx_init_context(void* win)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}

    return true;
}

/*
 * Shuts the context down.
 */
void rsx_shutdown_context()
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
}

// 1737
static void
rsx_render_frame(refdef_t *fd)
{
    static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
}

// 1812
void
rsx_begin_frame(float camera_separation)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
}

// 1924
static void
rsx_set_palette(const unsigned char *palette)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
}

// 1961 
static qboolean
rsx_end_world_render_pass( void )
{
	// Just like in GL3
	return true;
}

// 1967
refexport_t
GetRefAPI(refimport_t imp)
{
	// struct for save refexport callbacks, copy of re struct from main file
	// used different variable name for prevent confusion and cppcheck warnings
	refexport_t	refexport;

	memset(&refexport, 0, sizeof(refexport_t));
	ri = imp;

	refexport.api_version = API_VERSION;

	refexport.BeginRegistration  = rsx_begin_registration;
	refexport.RegisterModel      = rsx_register_model;
	refexport.RegisterSkin       = rsx_register_skin;
	refexport.DrawFindPic        = rsx_draw_find_pic;
	refexport.SetSky             = rsx_set_sky;
	refexport.EndRegistration    = rsx_end_registration;

	refexport.RenderFrame        = rsx_render_frame;

	refexport.DrawGetPicSize     = res_draw_get_pic_size;

	refexport.DrawPicScaled      = rsx_draw_pic_scaled;
	refexport.DrawStretchPic     = rsx_draw_stretch_pic;
	refexport.DrawCharScaled     = rsx_draw_char_scaled;
	refexport.DrawTileClear      = rsx_draw_tile_clear;
	refexport.DrawFill           = rsx_draw_fill;
	refexport.DrawFadeScreen     = rsx_draw_fade_screen;

	refexport.DrawStretchRaw     = rsx_draw_stretch_raw;

	refexport.Init               = rsx_init;
	refexport.IsVSyncActive      = rsx_is_vsync_active;
	refexport.Shutdown           = rsx_shutdown;
	refexport.InitContext        = rsx_init_context;
	refexport.ShutdownContext    = rsx_shutdown_context;
	refexport.PrepareForWindow   = rsx_prepare_for_window;

	refexport.SetPalette         = rsx_set_palette;
	refexport.BeginFrame         = rsx_begin_frame;
	refexport.EndWorldRenderpass = rsx_end_world_render_pass;
	refexport.EndFrame           = rsx_end_frame;

	// Tell the client that we're unsing the
	// new renderer restart API.
	ri.Vid_RequestRestart(RESTART_NO);

	Swap_Init();

	return refexport;
}

// 2017
void R_Printf(int level, const char* msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	ri.Com_VPrintf(level, msg, argptr);
	va_end(argptr);
}