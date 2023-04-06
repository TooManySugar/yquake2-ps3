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

#define HANDMADE_MATH_IMPLEMENTATION
#include "header/HandmadeMath.h"

#define REF_VERSION "Yamagi Quake II RSX Refresher"

// Values are
//   -2 - entire screen
//  < 0 - custom (unsafe don't use it)
// >= 0 - index of vid_modes in vid_ps3.c
//        (video menu values)
#define SAFE_R_MODE -2

// 44
refimport_t ri;

gl3config_t gl3config;
gl3state_t gl3state;

unsigned gl3_rawpalette[256];

/* screen size info */
refdef_t gl3_newrefdef;

viddef_t vid;
gl3model_t *gl3_worldmodel;

float gl3depthmin = 0.0f, gl3depthmax = 1.0f;

cplane_t frustum[4];

/* view origin */
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t gl3_origin;

int gl3_visframecount; /* bumped when going to a new PVS */
int gl3_framecount;	   /* used for dlight push checking */

int c_brush_polys, c_alias_polys;

static float v_blend[4]; /* final blending color */

int gl3_viewcluster, gl3_viewcluster2, gl3_oldviewcluster, gl3_oldviewcluster2;

const hmm_mat4 gl3_identityMat4 = {{
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{0, 0, 0, 1},
}};

cvar_t *gl_msaa_samples; // 83
cvar_t *r_vsync;
cvar_t *r_retexturing;
cvar_t *r_scale8bittextures;
// cvar_t *vid_fullscreen;
cvar_t *r_mode;
cvar_t *r_customwidth;
cvar_t *r_customheight;
// cvar_t *vid_gamma;
cvar_t *gl_anisotropic;
cvar_t *gl_texturemode;
cvar_t *gl_drawbuffer;
cvar_t *r_clear;
cvar_t *gl3_particle_size;
cvar_t *gl3_particle_fade_factor;
cvar_t *gl3_particle_square;
cvar_t *gl3_colorlight;

cvar_t *gl_lefthand;
cvar_t *r_gunfov;
cvar_t *r_farsee;

cvar_t *gl3_intensity;
cvar_t *gl3_intensity_2D;
cvar_t *r_lightlevel;
cvar_t *gl3_overbrightbits;

cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawworld;
cvar_t *gl_nolerp_list;
cvar_t *r_lerp_list;
cvar_t *r_2D_unfiltered;
cvar_t *r_videos_unfiltered;
cvar_t *gl_nobind;
cvar_t *r_lockpvs;
cvar_t *r_novis;
cvar_t *r_speeds;
cvar_t *gl_finish;

cvar_t *gl_cull;
cvar_t *gl_zfix;
cvar_t *r_fullbright;
cvar_t *r_modulate;
cvar_t *gl_lightmap;
cvar_t *gl_shadows;
cvar_t *gl3_debugcontext;
cvar_t *gl3_usebigvbo;
cvar_t *r_fixsurfsky;
cvar_t *gl3_usefbo;

//  Important and usefull
// ======================
#include <sysutil/video.h>
#include <sys/systime.h>
#include "sys/event_queue.h"

#define FRAME_BUFFERS_COUNT 4

#define GCM_PREPARED_BUFFER_INDEX 65
#define GCM_BUFFER_STATUS_INDEX 66
#define GCM_WAIT_LABEL_INDEX 255

#define MAX_BUFFER_QUEUE_SIZE 1

#define BUFFER_IDLE 0
#define BUFFER_BUSY 1

gcmContextData *gcmContext = NULL;
static int rb_width;
static int rb_height;
static int rb_pitch;

// pointers to color buffers in rsx memory must be free'd on shutdown
static void *color_buffer_ptr[FRAME_BUFFERS_COUNT] = {NULL};
// Offset in RSX memory
static uint32_t color_buffer_offset[FRAME_BUFFERS_COUNT] = {0};
// color buffer which in use right now
static uint32_t active_cb = 0;

static uint32_t fbFlipped = 0;
static qboolean fbOnFlip = false;
// color buffer on which drawing happens
static uint32_t work_fb = 0;

#define DEPTH_BUFFER_ZS 4
// pointer to depth buffer in rsx memory must be free'd on shutdown
// don't know how it's used but must be set
static void *depth_buffer_ptr = NULL;
static uint32_t depth_buffer_offset = 0;
static uint32_t depth_buffer_pitch = 0;

sys_event_queue_t flipEventQueue;
sys_event_port_t flipEventPort;

gcmSurface surface;

// ======================

static void flipHandler(const u32 head)
{
	(void)head;
	u32 v = fbFlipped;

	for (u32 i = active_cb; i != v; i = (i + 1) % FRAME_BUFFERS_COUNT)
	{
		*((vu32 *)gcmGetLabelAddress(GCM_BUFFER_STATUS_INDEX + i)) = BUFFER_IDLE;
	}
	active_cb = v;
	fbOnFlip = false;

	sysEventPortSend(flipEventPort, 0, 0, 0);
}

static void vblankHandler(const u32 head)
{
	(void)head;
	u32 data;
	u32 bufferToFlip;
	u32 indexToFlip;

	data = *((vu32 *)gcmGetLabelAddress(GCM_PREPARED_BUFFER_INDEX));
	bufferToFlip = (data >> 8);
	indexToFlip = (data & 0x07);

	if (!fbOnFlip)
	{
		if (bufferToFlip != active_cb)
		{
			s32 ret = gcmSetFlipImmediate(indexToFlip);
			if (ret != 0)
			{
				Com_Printf("flip immediate failed\n");
				return;
			}
			fbFlipped = bufferToFlip;
			fbOnFlip = true;
		}
	}
}

void initFlipEvent()
{
	sys_event_queue_attr_t queueAttr = {SYS_EVENT_QUEUE_PRIO, SYS_EVENT_QUEUE_PPU, "\0"};

	sysEventQueueCreate(&flipEventQueue, &queueAttr, SYS_EVENT_QUEUE_KEY_LOCAL, 32);
	sysEventPortCreate(&flipEventPort, SYS_EVENT_PORT_LOCAL, SYS_EVENT_PORT_NO_NAME);
	sysEventPortConnectLocal(flipEventPort, flipEventQueue);

	gcmSetFlipHandler(flipHandler);
	gcmSetVBlankHandler(vblankHandler);
}

void initRenderTarget()
{
	memset(&surface, 0, sizeof(gcmSurface));

	surface.colorFormat = GCM_SURFACE_X8R8G8B8;
	surface.colorTarget = GCM_SURFACE_TARGET_0;
	surface.colorLocation[0] = GCM_LOCATION_RSX;
	surface.colorOffset[0] = color_buffer_offset[work_fb];
	surface.colorPitch[0] = rb_pitch;

	for (u32 i = 1; i < GCM_MAX_MRT_COUNT; i++)
	{
		surface.colorLocation[i] = GCM_LOCATION_RSX;
		surface.colorOffset[i] = color_buffer_offset[work_fb];
		surface.colorPitch[i] = 64;
	}

	surface.depthFormat = GCM_SURFACE_ZETA_Z16;
	surface.depthLocation = GCM_LOCATION_RSX;
	surface.depthOffset = depth_buffer_offset;
	surface.depthPitch = depth_buffer_pitch;

	surface.type = GCM_SURFACE_TYPE_LINEAR;
	surface.antiAlias = GCM_SURFACE_CENTER_1;

	surface.width = rb_width;
	surface.height = rb_height;
	surface.x = 0;
	surface.y = 0;
}

void setRenderTarget(u32 index)
{
	surface.colorOffset[0] = color_buffer_offset[index];
	rsxSetSurface(gcmContext, &surface);
}

static void syncPPUGPU()
{
	vu32 *label = (vu32 *)gcmGetLabelAddress(GCM_PREPARED_BUFFER_INDEX);
	while (((work_fb + FRAME_BUFFERS_COUNT - ((*label) >> 8)) % FRAME_BUFFERS_COUNT) > MAX_BUFFER_QUEUE_SIZE)
	{
		sys_event_t event;

		sysEventQueueReceive(flipEventQueue, &event, 0);
		sysEventQueueDrain(flipEventQueue);
	}
}

// Yaw-Pitch-Roll
// equivalent to R_z * R_y * R_x where R_x is the trans matrix for rotating around X axis for aroundXdeg
static hmm_mat4 R_RSX_rotAroundAxisZYX(float aroundZdeg, float aroundYdeg, float aroundXdeg)
{
	// Naming of variables is consistent with http://planning.cs.uiuc.edu/node102.html
	// and https://de.wikipedia.org/wiki/Roll-Nick-Gier-Winkel#.E2.80.9EZY.E2.80.B2X.E2.80.B3-Konvention.E2.80.9C
	float alpha = HMM_ToRadians(aroundZdeg);
	float beta = HMM_ToRadians(aroundYdeg);
	float gamma = HMM_ToRadians(aroundXdeg);

	float sinA = HMM_SinF(alpha);
	float cosA = HMM_CosF(alpha);
	// TODO: or sincosf(alpha, &sinA, &cosA); ?? (not a standard function)
	float sinB = HMM_SinF(beta);
	float cosB = HMM_CosF(beta);
	float sinG = HMM_SinF(gamma);
	float cosG = HMM_CosF(gamma);

	hmm_mat4 ret = {{
		{ cosA*cosB,                  sinA*cosB,                   -sinB,    0 }, // first *column*
		{ cosA*sinB*sinG - sinA*cosG, sinA*sinB*sinG + cosA*cosG, cosB*sinG, 0 },
		{ cosA*sinB*cosG + sinA*sinG, sinA*sinB*cosG - cosA*sinG, cosB*cosG, 0 },
		{  0,                          0,                          0,        1 }
	}};

	return ret;
}

// 162
void
R_RSX_RotateForEntity(entity_t *e)
{
	// angles: pitch (around y), yaw (around z), roll (around x)
	// rot matrices to be multiplied in order Z, Y, X (yaw, pitch, roll)
	
	hmm_mat4 transMat = R_RSX_rotAroundAxisZYX(e->angles[1], -e->angles[0], -e->angles[2]);

	// set translation
	transMat.Elements[3][0] = e->origin[0];
	transMat.Elements[3][1] = e->origin[1];
	transMat.Elements[3][2] = e->origin[2];

	// RSX NOTE: on PS3 this matrix need to be transposed
	gl3state.uni3DData.transModelMat4 = HMM_Transpose(HMM_MultiplyMat4(gl3state.uni3DData.transModelMat4, transMat));

	// GL3_UpdateUBO3D();
	R_RSX_Shaders_UpdateUni3DData();
}

// 199
static void
R_RSX_registerCvars(void)
{
	gl_lefthand = ri.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	r_gunfov = ri.Cvar_Get("r_gunfov", "80", CVAR_ARCHIVE);
	r_farsee = ri.Cvar_Get("r_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);

	gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0);
	r_vsync = ri.Cvar_Get("r_vsync", "1", CVAR_ARCHIVE);
	gl_msaa_samples = ri.Cvar_Get("r_msaa_samples", "0", CVAR_ARCHIVE);
	r_retexturing = ri.Cvar_Get("r_retexturing", "1", CVAR_ARCHIVE);
	r_scale8bittextures = ri.Cvar_Get("r_scale8bittextures", "0", CVAR_ARCHIVE);
	gl3_debugcontext = ri.Cvar_Get("gl3_debugcontext", "0", 0);
	// Use here -2 to force use fullscreen
	r_mode = ri.Cvar_Get("r_mode", "-2", CVAR_ARCHIVE);

	// Those two values are used with specific r_mode.
	// In pair with r_mode == -2 or > 0 are ignored.
	// Other than that are used.
	// They are not used to actual display output resolution
	// changed, but for rendering only
	r_customwidth = ri.Cvar_Get("r_customwidth", "1280", CVAR_ARCHIVE);
	r_customheight = ri.Cvar_Get("r_customheight", "720", CVAR_ARCHIVE);

	gl3_particle_size = ri.Cvar_Get("gl3_particle_size", "40", CVAR_ARCHIVE);
	gl3_particle_fade_factor = ri.Cvar_Get("gl3_particle_fade_factor", "1.2", CVAR_ARCHIVE);
	gl3_particle_square = ri.Cvar_Get("gl3_particle_square", "0", CVAR_ARCHIVE);
	// if set to 0, lights (from lightmaps, dynamic lights and on models) are white instead of colored
	gl3_colorlight = ri.Cvar_Get("gl3_colorlight", "1", CVAR_ARCHIVE);

	//  0: use lots of calls to glBufferData()
	//  1: reduce calls to glBufferData() with one big VBO (see GL3_BufferAndDraw3D())
	// -1: auto (let yq2 choose to enable/disable this based on detected driver)
	gl3_usebigvbo = ri.Cvar_Get("gl3_usebigvbo", "-1", CVAR_ARCHIVE);

	r_norefresh = ri.Cvar_Get("r_norefresh", "0", 0);
	r_drawentities = ri.Cvar_Get("r_drawentities", "1", 0);
	r_drawworld = ri.Cvar_Get("r_drawworld", "1", 0);
	r_fullbright = ri.Cvar_Get("r_fullbright", "0", 0);
	r_fixsurfsky = ri.Cvar_Get("r_fixsurfsky", "0", CVAR_ARCHIVE);

	/* don't bilerp characters and crosshairs */
	gl_nolerp_list = ri.Cvar_Get("r_nolerp_list", "pics/conchars.pcx pics/ch1.pcx pics/ch2.pcx pics/ch3.pcx", CVAR_ARCHIVE);
	/* textures that should always be filtered, even if r_2D_unfiltered or an unfiltered gl mode is used */
	r_lerp_list = ri.Cvar_Get("r_lerp_list", "", CVAR_ARCHIVE);
	/* don't bilerp any 2D elements */
	r_2D_unfiltered = ri.Cvar_Get("r_2D_unfiltered", "0", CVAR_ARCHIVE);
	/* don't bilerp videos */
	r_videos_unfiltered = ri.Cvar_Get("r_videos_unfiltered", "0", CVAR_ARCHIVE);
	gl_nobind = ri.Cvar_Get("gl_nobind", "0", 0);

	gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl_anisotropic = ri.Cvar_Get("r_anisotropic", "0", CVAR_ARCHIVE);

	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = ri.Cvar_Get("vid_gamma", "1.2", CVAR_ARCHIVE);
	gl3_intensity = ri.Cvar_Get("gl3_intensity", "1.5", CVAR_ARCHIVE);
	gl3_intensity_2D = ri.Cvar_Get("gl3_intensity_2D", "1.5", CVAR_ARCHIVE);

	r_lightlevel = ri.Cvar_Get("r_lightlevel", "0", 0);
	gl3_overbrightbits = ri.Cvar_Get("gl3_overbrightbits", "1.3", CVAR_ARCHIVE);

	gl_lightmap = ri.Cvar_Get("r_lightmap", "0", 0);
	gl_shadows = ri.Cvar_Get("r_shadows", "0", CVAR_ARCHIVE);

	r_modulate = ri.Cvar_Get("r_modulate", "1", CVAR_ARCHIVE);
	gl_zfix = ri.Cvar_Get("gl_zfix", "0", 0);
	r_clear = ri.Cvar_Get("r_clear", "0", 0);
	gl_cull = ri.Cvar_Get("gl_cull", "1", 0);
	r_lockpvs = ri.Cvar_Get("r_lockpvs", "0", 0);
	r_novis = ri.Cvar_Get("r_novis", "0", 0);
	r_speeds = ri.Cvar_Get("r_speeds", "0", 0);
	gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE);

	gl3_usefbo = ri.Cvar_Get("gl3_usefbo", "1", CVAR_ARCHIVE); // use framebuffer object for postprocess effects (water)

#if 0  // TODO! // RSX NOTE: this is TODO from gl3 refresher
	//gl_lefthand = ri.Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	//gl_farsee = ri.Cvar_Get("gl_farsee", "0", CVAR_LATCH | CVAR_ARCHIVE);
	//r_norefresh = ri.Cvar_Get("r_norefresh", "0", 0);
	//r_fullbright = ri.Cvar_Get("r_fullbright", "0", 0);
	//r_drawentities = ri.Cvar_Get("r_drawentities", "1", 0);
	//r_drawworld = ri.Cvar_Get("r_drawworld", "1", 0);
	//r_novis = ri.Cvar_Get("r_novis", "0", 0);
	//r_lerpmodels = ri.Cvar_Get("r_lerpmodels", "1", 0); NOTE: screw this, it looks horrible without
	//r_speeds = ri.Cvar_Get("r_speeds", "0", 0);

	//r_lightlevel = ri.Cvar_Get("r_lightlevel", "0", 0);
	//gl_overbrightbits = ri.Cvar_Get("gl_overbrightbits", "0", CVAR_ARCHIVE);

	gl1_particle_min_size = ri.Cvar_Get("gl1_particle_min_size", "2", CVAR_ARCHIVE);
	gl1_particle_max_size = ri.Cvar_Get("gl1_particle_max_size", "40", CVAR_ARCHIVE);
	//gl1_particle_size = ri.Cvar_Get("gl1_particle_size", "40", CVAR_ARCHIVE);
	gl1_particle_att_a = ri.Cvar_Get("gl1_particle_att_a", "0.01", CVAR_ARCHIVE);
	gl1_particle_att_b = ri.Cvar_Get("gl1_particle_att_b", "0.0", CVAR_ARCHIVE);
	gl1_particle_att_c = ri.Cvar_Get("gl1_particle_att_c", "0.01", CVAR_ARCHIVE);

	//gl_modulate = ri.Cvar_Get("gl_modulate", "1", CVAR_ARCHIVE);
	//r_mode = ri.Cvar_Get("r_mode", "4", CVAR_ARCHIVE);
	//gl_lightmap = ri.Cvar_Get("r_lightmap", "0", 0);
	//gl_shadows = ri.Cvar_Get("r_shadows", "0", CVAR_ARCHIVE);
	//gl_nobind = ri.Cvar_Get("gl_nobind", "0", 0);
	gl_showtris = ri.Cvar_Get("gl_showtris", "0", 0);
	gl_showbbox = Cvar_Get("gl_showbbox", "0", 0);
	//gl1_ztrick = ri.Cvar_Get("gl1_ztrick", "0", 0); NOTE: dump this.
	//gl_zfix = ri.Cvar_Get("gl_zfix", "0", 0);
	//gl_finish = ri.Cvar_Get("gl_finish", "0", CVAR_ARCHIVE);
	r_clear = ri.Cvar_Get("r_clear", "0", 0);
//	gl_cull = ri.Cvar_Get("gl_cull", "1", 0);
	//gl1_flashblend = ri.Cvar_Get("gl1_flashblend", "0", 0);

	//gl_texturemode = ri.Cvar_Get("gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl1_texturealphamode = ri.Cvar_Get("gl1_texturealphamode", "default", CVAR_ARCHIVE);
	gl1_texturesolidmode = ri.Cvar_Get("gl1_texturesolidmode", "default", CVAR_ARCHIVE);
	//gl_anisotropic = ri.Cvar_Get("r_anisotropic", "0", CVAR_ARCHIVE);
	//r_lockpvs = ri.Cvar_Get("r_lockpvs", "0", 0);

	//gl1_palettedtexture = ri.Cvar_Get("gl1_palettedtexture", "0", CVAR_ARCHIVE); NOPE.
	gl1_pointparameters = ri.Cvar_Get("gl1_pointparameters", "1", CVAR_ARCHIVE);

	//gl_drawbuffer = ri.Cvar_Get("gl_drawbuffer", "GL_BACK", 0);
	//r_vsync = ri.Cvar_Get("r_vsync", "1", CVAR_ARCHIVE);


	//vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE);
	//vid_gamma = ri.Cvar_Get("vid_gamma", "1.0", CVAR_ARCHIVE);

	//r_customwidth = ri.Cvar_Get("r_customwidth", "1024", CVAR_ARCHIVE);
	//r_customheight = ri.Cvar_Get("r_customheight", "768", CVAR_ARCHIVE);
	//gl_msaa_samples = ri.Cvar_Get ( "r_msaa_samples", "0", CVAR_ARCHIVE );

	//r_retexturing = ri.Cvar_Get("r_retexturing", "1", CVAR_ARCHIVE);


	gl1_stereo = ri.Cvar_Get( "gl1_stereo", "0", CVAR_ARCHIVE );
	gl1_stereo_separation = ri.Cvar_Get( "gl1_stereo_separation", "-0.4", CVAR_ARCHIVE );
	gl1_stereo_anaglyph_colors = ri.Cvar_Get( "gl1_stereo_anaglyph_colors", "rc", CVAR_ARCHIVE );
	gl1_stereo_convergence = ri.Cvar_Get( "gl1_stereo_convergence", "1", CVAR_ARCHIVE );
#endif // 0

	ri.Cmd_AddCommand("imagelist", R_RSX_Image_ImageList_f);
	ri.Cmd_AddCommand("screenshot", R_RSX_ScreenShot);
	ri.Cmd_AddCommand("modellist", R_RSX_Mod_Modellist_f);
	// ri.Cmd_AddCommand("gl_strings", GL3_Strings);
}

// 341
//  the following is only used in the next to functions,
//  no need to put it in a header
typedef enum
{
	rserr_ok,
	rserr_invalid_mode,
	rserr_unknown
} rserr_t;

/*
 *
 */
// 352
static rserr_t
R_RSX_setModImpl(int *pwidth, int *pheight, int mode, int fullscreen)
{
	R_Printf(PRINT_ALL, "R_RSX_setModImpl: fullscreen: %d, width: %d height: %d mode: %d\n", fullscreen, *pwidth, *pheight, mode);

	R_Printf(PRINT_ALL, "Setting mode %d:", mode);

	if ((mode >= 0) && !ri.Vid_GetModeInfo(pwidth, pheight, mode))
	{
		R_Printf(PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	/* We trying to get resolution from desktop */
	if (mode == -2)
	{
		if (!ri.GLimp_GetDesktopMode(pwidth, pheight))
		{
			R_Printf(PRINT_ALL, " can't detect mode\n");
			return rserr_invalid_mode;
		}
	}

	R_Printf(PRINT_ALL, " %dx%d (vid_fullscreen %i)\n", *pwidth, *pheight, fullscreen);

	if (!ri.GLimp_InitGraphics(fullscreen, pwidth, pheight))
	{
		// failed to set a valid mode in windowed mode
		return rserr_invalid_mode;
	}

	return rserr_ok;
}

// 386
static qboolean R_RSX_setMode(void)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		Com_Printf("%s()\n", __func__);
		// is_first_call = false;
	}

	int err;
	int fullscreen;

	fullscreen = (int)vid_fullscreen->value;

	/* a bit hackish approach to enable custom resolutions:
	   Glimp_SetMode needs these values set for mode -1 */
	vid.width = r_customwidth->value;
	vid.height = r_customheight->value;

	if ((err = R_RSX_setModImpl(&vid.width, &vid.height, r_mode->value, fullscreen)) == rserr_ok)
	{
		R_Printf(PRINT_ALL, "R_RSX_setModImpl returned rserr_ok\n");
		if (r_mode->value == -1)
		{

			gl3state.prev_mode = SAFE_R_MODE;
		}
		else
		{
			gl3state.prev_mode = r_mode->value;
		}
	}
	else
	{
		if (err == rserr_invalid_mode)
		{
			R_Printf(PRINT_ALL, "ref_rsx::%s() - invalid mode\n", __func__);

			if (gl_msaa_samples->value != 0.0f)
			{
				R_Printf(PRINT_ALL, "gl_msaa_samples was %d - will try again with gl_msaa_samples = 0\n", (int)gl_msaa_samples->value);
				ri.Cvar_SetValue("r_msaa_samples", 0.0f);
				gl_msaa_samples->modified = false;

				if ((err = R_RSX_setModImpl(&vid.width, &vid.height, r_mode->value, 0)) == rserr_ok)
				{
					return true;
				}
			}
			if (r_mode->value == gl3state.prev_mode)
			{
				// trying again would result in a crash anyway, give up already
				// (this would happen if your initing fails at all and your resolution already was 640x480)
				return false;
			}

			// Seting safe value and using it in next if statement
			ri.Cvar_SetValue("r_mode", gl3state.prev_mode);
			r_mode->modified = false;
		}
		else
		{
			R_Printf(PRINT_ALL, "ref_rsx::%s() - unknown mode\n", __func__);
		}

		/* try setting it back to something safe */
		if ((err = R_RSX_setModImpl(&vid.width, &vid.height, gl3state.prev_mode, 0)) != rserr_ok)
		{
			R_Printf(PRINT_ALL, "ref_rsx::%s() - could not revert to safe mode\n", __func__);
			return false;
		}
	}

	return true;
}

/**
 * @brief
 *
 * @return
 * true  - success
 * false - fail
 */
// 452
static qboolean
R_RSX_Init(void)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		Com_Printf("%s()\n", __func__);
		// is_first_call = false;
	}

	// Swap_Init(); // FIXME: for fucks sake, this doesn't have to be done at runtime!

	R_Printf(PRINT_ALL, "Refresh: " REF_VERSION "\n");
	R_Printf(PRINT_ALL, "Client: " YQ2VERSION "\n\n");

	// initialize 8 to 24 color conversion table
	R_RSX_Draw_GetPalette();

	// load banch of render related cvars
	R_RSX_registerCvars();

	// Check where it's used
	gl3state.prev_mode = SAFE_R_MODE;

	// create the window and set up the context
	if (!R_RSX_setMode())
	{
		R_Printf(PRINT_ALL, "ref_rsx::%s() - could not R_RSX_setMode()\n", __func__);
		return false;
	}

	registration_sequence = 1;

	// init
	R_RSX_Image_Init();
	R_RSX_LM_Init();

	R_RSX_Mod_Init();

	R_RSX_InitParticleTexture();
	R_RSX_InitNoTexture();

	R_RSX_Draw_InitLocal();

	R_RSX_Surf_Init();
	R_RSX_Light_Init();

	return true;
}

#if defined(WORLD_DRAW_SPEEDS)
unsigned long verts_drawn = 0;
#endif

// 657
/**
 * @brief Replacement for GL3_BufferAndDraw.
 * 
 * @details Copies /numVerts/ from /verts/ to vertex buffers and sends
 * command to RSX to draw them.
 * 
 * @param verts Pointer to array of vertexes 
 * @param numVerts Size of that array
 * @param drawMode GCM_TYPE_TRIANGLE_STRIP, GCM_TYPE_TRIANGLE_FAN, etc.
 */
void
R_RSX_BufferAndDraw3D(const gl3_3D_vtx_t *verts, int numVerts, uint32_t drawMode)
{
	gl3_3D_vtx_t *vertex_buffer_ptr;
	uint32_t offset;
	uint32_t current_offset = gl3state.rsx_vertex_buffer_offset;
	uint32_t data_size = numVerts * sizeof(gl3_3D_vtx_t);
	/* used memory would be aligned as like with rsxMemalign */
	uint32_t chunk_size = data_size;
	if ((chunk_size & 127) != 0)
	{
		chunk_size = (chunk_size & 0xFFFFFF80) + 128;
	}

	/* make sure it fit in buffer */
	if (current_offset + chunk_size > gl3state.rsx_vertex_buffer_size)
	{
		/* if not - set offset to 0
		 * I think at the time then we reach end of buffer while
		 * still drawing frame command buffer will done with drawing
		 * vertexes at buffer begining - usually it means this vertexes
		 * belong to other frame
		 * Moreover it's safe as at draw heavy frames we just drop data
		 * (i.e. draw garbage) instead of resident more */

		current_offset = 0;
	}

	vertex_buffer_ptr = (void *)(gl3state.rsx_vertex_buffer + current_offset);

	// glBufferData( GL_ARRAY_BUFFER, sizeof(gl3_3D_vtx_t)*numVerts, verts, GL_STREAM_DRAW );
	memcpy(vertex_buffer_ptr, verts, data_size);

	rsxAddressToOffset(&vertex_buffer_ptr[0].pos, &offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_POS, 0, offset, sizeof(gl3_3D_vtx_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vertex_buffer_ptr[0].normal, &offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_NORMAL, 0, offset, sizeof(gl3_3D_vtx_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vertex_buffer_ptr[0].texCoord, &offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX0, 0, offset, sizeof(gl3_3D_vtx_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vertex_buffer_ptr[0].lmTexCoord, &offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX1, 0, offset, sizeof(gl3_3D_vtx_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vertex_buffer_ptr[0].lightFlags, &offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX2, 0, offset, sizeof(gl3_3D_vtx_t), 4, GCM_VERTEX_DATA_TYPE_U8, GCM_LOCATION_RSX);

	// rsxSetUserClipPlaneControl(gcmContext,
	// 							GCM_USER_CLIP_PLANE_DISABLE,
	// 							GCM_USER_CLIP_PLANE_DISABLE,
	// 							GCM_USER_CLIP_PLANE_DISABLE,
	// 							GCM_USER_CLIP_PLANE_DISABLE,
	// 							GCM_USER_CLIP_PLANE_DISABLE,
	// 							GCM_USER_CLIP_PLANE_DISABLE);

	// glDrawArrays( drawMode, 0, numVerts );
	rsxDrawVertexArray(gcmContext, drawMode, 0, numVerts);

#if defined(WORLD_DRAW_SPEEDS)
	verts_drawn += numVerts;
#endif

	gl3state.rsx_vertex_buffer_offset = current_offset + chunk_size;
}

static void // 728
R_RSX_DrawBeam(entity_t *e)
{
	int i;
	float r, g, b;

	enum { NUM_BEAM_SEGS = 6 };

	vec3_t perpvec;
	vec3_t direction, normalized_direction;
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS];
	vec3_t oldorigin, origin;

	gl3_3D_vtx_t verts[NUM_BEAM_SEGS*4];
	unsigned int pointb;

	oldorigin[0] = e->oldorigin[0];
	oldorigin[1] = e->oldorigin[1];
	oldorigin[2] = e->oldorigin[2];

	origin[0] = e->origin[0];
	origin[1] = e->origin[1];
	origin[2] = e->origin[2];

	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0];
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1];
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2];

	if (VectorNormalize(normalized_direction) == 0)
	{
		return;
	}

	PerpendicularVector(perpvec, normalized_direction);
	VectorScale(perpvec, e->frame / 2, perpvec);

	for (i = 0; i < 6; i++)
	{
		RotatePointAroundVector(start_points[i], normalized_direction, perpvec,
		                        (360.0 / NUM_BEAM_SEGS) * i);
		VectorAdd(start_points[i], origin, start_points[i]);
		VectorAdd(start_points[i], direction, end_points[i]);
	}

	//glDisable(GL_TEXTURE_2D);
	// glEnable(GL_BLEND);
	rsxSetBlendEnable(gcmContext, GCM_TRUE);

	// glDepthMask(GL_FALSE);
	rsxSetDepthWriteEnable(gcmContext, GCM_FALSE);

	// GL3_UseProgram(gl3state.si3DcolorOnly.shaderProgram);
	// RSX NOTE: load shader after all vars is set

	r = (LittleLong(d_8to24table[e->skinnum & 0xFF])) & 0xFF;
	g = (LittleLong(d_8to24table[e->skinnum & 0xFF]) >> 8) & 0xFF;
	b = (LittleLong(d_8to24table[e->skinnum & 0xFF]) >> 16) & 0xFF;

	r *= 1 / 255.0F;
	g *= 1 / 255.0F;
	b *= 1 / 255.0F;

	gl3state.uniCommonData.color = HMM_Vec4(r, g, b, e->alpha);
	// GL3_UpdateUBOCommon();
	R_RSX_Shaders_UpdateUniCommonData();

	for ( i = 0; i < NUM_BEAM_SEGS; i++ )
	{
		VectorCopy(start_points[i], verts[4*i+0].pos);
		VectorCopy(end_points[i], verts[4*i+1].pos);

		pointb = ( i + 1 ) % NUM_BEAM_SEGS;

		VectorCopy(start_points[pointb], verts[4*i+2].pos);
		VectorCopy(end_points[pointb], verts[4*i+3].pos);
	}

	// GL3_BindVAO(gl3state.vao3D);
	// GL3_BindVBO(gl3state.vbo3D);

	R_RSX_Shaders_Load(&(gl3state.vs3D));
	R_RSX_Shaders_Load(&(gl3state.fs3Dcolor));

	// GL3_BufferAndDraw3D(verts, NUM_BEAM_SEGS*4, GL_TRIANGLE_STRIP);
	R_RSX_BufferAndDraw3D(verts, NUM_BEAM_SEGS*4, GCM_TYPE_TRIANGLE_STRIP);

	// glDisable(GL_BLEND);
	rsxSetBlendEnable(gcmContext, GCM_FALSE);

	// glDepthMask(GL_TRUE);
	rsxSetDepthWriteEnable(gcmContext, GCM_TRUE);
}

static void // 888
R_RSX_drawNullModel(entity_t *currententity)
{
	vec3_t shadelight;

	if (currententity->flags & RF_FULLBRIGHT)
	{
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F;
	}
	else
	{
		R_RSX_Light_LightPoint(currententity, currententity->origin, shadelight);
	}

	hmm_mat4 origModelMat = gl3state.uni3DData.transModelMat4;
	R_RSX_RotateForEntity(currententity);

	gl3state.uniCommonData.color = HMM_Vec4( shadelight[0], shadelight[1], shadelight[2], 1 );
	// GL3_UpdateUBOCommon();
	R_RSX_Shaders_UpdateUniCommonData();

	// GL3_UseProgram(gl3state.si3DcolorOnly.shaderProgram);
	R_RSX_Shaders_Load(&(gl3state.vs3D));
	R_RSX_Shaders_Load(&(gl3state.fs3Dcolor));

	// GL3_BindVAO(gl3state.vao3D);
	// GL3_BindVBO(gl3state.vbo3D);

	gl3_3D_vtx_t vtxA[6] = {
		{{0, 0, -16}, {0,0}, {0,0}},
		{{16 * cos( 0 * M_PI / 2 ), 16 * sin( 0 * M_PI / 2 ), 0}, {0,0}, {0,0}},
		{{16 * cos( 1 * M_PI / 2 ), 16 * sin( 1 * M_PI / 2 ), 0}, {0,0}, {0,0}},
		{{16 * cos( 2 * M_PI / 2 ), 16 * sin( 2 * M_PI / 2 ), 0}, {0,0}, {0,0}},
		{{16 * cos( 3 * M_PI / 2 ), 16 * sin( 3 * M_PI / 2 ), 0}, {0,0}, {0,0}},
		{{16 * cos( 4 * M_PI / 2 ), 16 * sin( 4 * M_PI / 2 ), 0}, {0,0}, {0,0}}
	};

	// GL3_BufferAndDraw3D(vtxA, 6, GL_TRIANGLE_FAN);
	R_RSX_BufferAndDraw3D(vtxA, 6, GCM_TYPE_TRIANGLE_FAN);

	gl3_3D_vtx_t vtxB[6] = {
		{{0, 0, 16}, {0,0}, {0,0}},
		vtxA[5], vtxA[4], vtxA[3], vtxA[2], vtxA[1]
	};

	// GL3_BufferAndDraw3D(vtxB, 6, GL_TRIANGLE_FAN);
	R_RSX_BufferAndDraw3D(vtxB, 6, GCM_TYPE_TRIANGLE_FAN);

	gl3state.uni3DData.transModelMat4 = origModelMat;
	// GL3_UpdateUBO3D();
	R_RSX_Shaders_UpdateUni3DData();
}

// sdl part 113
/*
 * Swaps the buffers and shows the next frame.
 */
void R_RSX_EndFrame(void)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	Com_Printf("%s()\n", __func__);
	// 	// is_first_call = false;
	// }

	s32 qid = gcmSetPrepareFlip(gcmContext, work_fb);
	while (qid < 0)
	{
		// usleep(100);
		sysUsleep(100);
		qid = gcmSetPrepareFlip(gcmContext, work_fb);
	}

	rsxSetWriteBackendLabel(gcmContext, GCM_PREPARED_BUFFER_INDEX, ((work_fb << 8) | qid));
	rsxFlushBuffer(gcmContext);

	syncPPUGPU();

	work_fb = (work_fb + 1) % FRAME_BUFFERS_COUNT;

	rsxSetWaitLabel(gcmContext, GCM_BUFFER_STATUS_INDEX + work_fb, BUFFER_IDLE);
	rsxSetWriteCommandLabel(gcmContext, GCM_BUFFER_STATUS_INDEX + work_fb, BUFFER_BUSY);

	setRenderTarget(work_fb);
}

// sdl part 129
qboolean
R_RSX_IsVSyncActive(void)
{
	return true;
}

// sdl part 171
int R_RSX_PrepareForWindow(void)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		Com_Printf("%s()\n", __func__);
		// is_first_call = false;
	}
	return 0;
}

/*
 * Returns true at success
 * and false at failure.
 */
// sdl part 294
int R_RSX_InitContext(void *gcmCon)
{
	// At this point display output already activated and
	// memory provided to RSX

	static qboolean is_first_call = true;
	if (is_first_call)
	{
		Com_Printf("%s()\n", __func__);
		// is_first_call = false;
	}
	if (gcmCon == NULL)
	{
		ri.Sys_Error(ERR_FATAL,
					 "%s() must not be called with NULL argument!", __func__);
		// FIXME unmapped memory on that
		return false;
	}

	gcmContext = (gcmContextData *)gcmCon;

	if (r_vsync->value)
	{
		gcmSetFlipMode(GCM_FLIP_VSYNC);
	}

	// Acquire real screeen resolution to
	// create buffers with apropriate size
	// for RSX to flip between
	videoConfiguration vConfig;
	if (videoGetConfiguration(0, &vConfig, NULL) != 0)
	{
		Com_Printf("Can't get video configuration\n");
		// fail for safety
		return false;
	}

	videoResolution res;
	// vConfig must containt valid resolution id
	videoGetResolution(vConfig.resolution, &res);

	rb_width = res.width;
	rb_height = res.height;
	rb_pitch = vConfig.pitch;

	depth_buffer_pitch = rb_width * DEPTH_BUFFER_ZS;

	// prevent non native res rendering for now
	assert(vid.width == res.width);
	assert(vid.height == res.height);

	Com_Printf("rb_width: %d\n", rb_width);
	Com_Printf("rb_height: %d\n", rb_height);
	Com_Printf("rb_pitch: %d\n", rb_pitch);

	// Here we create buffers for rsx to flip and draw on
	for (int i = 0; i < FRAME_BUFFERS_COUNT; ++i)
	{
		color_buffer_ptr[i] = rsxMemalign_with_log(64, (rb_height * rb_pitch));
		rsxAddressToOffset(color_buffer_ptr[i], &color_buffer_offset[i]);
		Com_Printf("fb[%d]: %p (offset: %08x) [%dx%d] %d\n", i, color_buffer_ptr[i], color_buffer_offset[i], rb_width, rb_height, rb_pitch);
		// Register buffers as ones which would be displayed
		gcmSetDisplayBuffer(i, color_buffer_offset[i], rb_pitch, rb_width, rb_height);
	}

	depth_buffer_ptr = rsxMemalign_with_log(64, ((rb_height * depth_buffer_pitch) * 2));
	rsxAddressToOffset(depth_buffer_ptr, &depth_buffer_offset);
	Com_Printf("db: %p (offset: %08x) \n", depth_buffer_ptr, depth_buffer_offset);

	// Setting created color buffers to be available except on wich would be used next
	for (int i = 0; i < FRAME_BUFFERS_COUNT; ++i)
	{
		*((vu32 *)gcmGetLabelAddress(GCM_BUFFER_STATUS_INDEX + i)) = BUFFER_IDLE;
	}
	*((vu32 *)gcmGetLabelAddress(GCM_PREPARED_BUFFER_INDEX)) = (active_cb << 8);
	*((vu32 *)gcmGetLabelAddress(GCM_BUFFER_STATUS_INDEX + active_cb)) = BUFFER_BUSY;

	// getting index of color buffer
	work_fb = (active_cb + 1) % FRAME_BUFFERS_COUNT;

	initFlipEvent();
	initRenderTarget();

	if (R_RSX_Shaders_Init())
	{
		R_Printf(PRINT_ALL, "Loading shaders succeeded.\n");
	}
	else
	{
		R_Printf(PRINT_ALL, "Loading shaders failed.\n");
		return false;
	}

	rsxSetWriteCommandLabel(gcmContext, GCM_BUFFER_STATUS_INDEX + work_fb, BUFFER_BUSY);

	return true;
}

/*
 * Shuts the context down.
 */
void R_RSX_ShutdownContext()
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		Com_Printf("%s()\n", __func__);
		// is_first_call = false;
	}

	uint32_t i;
	// In reverse way how things were done in initialization

	// 0 next buffer and flush it to show black screen
	// gcmSetWaitFlip(gcmContext);
	// memset(render_buffer.ptr, 0,
	// 	rend_buffer_width * rend_buffer_height * sizeof(uint32_t));
	// RE_flip(gcmContext, render_buffer.id);
	// RE_waitFlip();
	
	R_RSX_Shaders_Shutdown();

	// Free color buffers
	for (i = 0; i < FRAME_BUFFERS_COUNT; ++i)
	{
		Com_Printf("fb[%d]: ", i);
		if (color_buffer_ptr[i] == NULL)
		{
			Com_Printf("uninited\n");
			continue;
		}
		Com_Printf("%p (offset: %08x) ", color_buffer_ptr[i], color_buffer_offset[i]);
		rsxFree_with_log(color_buffer_ptr[i]);
		color_buffer_ptr[i] = NULL;
		Com_Printf("free'd\n");
	}

	Com_Printf("db: ");
	if (depth_buffer_ptr != NULL)
	{
		Com_Printf("%p (offset: %08x) ", depth_buffer_ptr, depth_buffer_offset);
		rsxFree_with_log(depth_buffer_ptr);
		depth_buffer_ptr = NULL;
		Com_Printf("free'd\n");
	}
	else
	{
		Com_Printf("uninited\n");
	}
}


// 619
void R_RSX_Shutdown(void)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		Com_Printf("%s()\n", __func__);
		// is_first_call = false;
	}

	R_RSX_Light_Shutdown();
	R_RSX_Surf_Shutdown();
	R_RSX_Draw_ShutdownLocal();

	R_RSX_Mesh_Shutown();

	R_RSX_Mod_FreeAll();

	R_RSX_LM_Shutdown();
	R_RSX_Image_Shutdown();


	R_RSX_ShutdownContext();
}

static void
R_RSX_setupFrame(void)
{
	int i;
	mleaf_t *leaf;

	gl3_framecount++;

	/* build the transformation matrix for the given view angles */
	VectorCopy(gl3_newrefdef.vieworg, gl3_origin);

	AngleVectors(gl3_newrefdef.viewangles, vpn, vright, vup);

	/* current viewcluster */
	if (!(gl3_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		gl3_oldviewcluster = gl3_viewcluster;
		gl3_oldviewcluster2 = gl3_viewcluster2;
		leaf = R_RSX_Mod_PointInLeaf(gl3_origin, gl3_worldmodel);
		gl3_viewcluster = gl3_viewcluster2 = leaf->cluster;

		/* check above and below so crossing solid water doesn't draw wrong */
		if (!leaf->contents)
		{
			/* look down a bit */
			vec3_t temp;

			VectorCopy(gl3_origin, temp);
			temp[2] -= 16;
			leaf = R_RSX_Mod_PointInLeaf(temp, gl3_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != gl3_viewcluster2))
			{
				gl3_viewcluster2 = leaf->cluster;
			}
		}
		else
		{
			/* look up a bit */
			vec3_t temp;

			VectorCopy(gl3_origin, temp);
			temp[2] += 16;
			leaf = R_RSX_Mod_PointInLeaf(temp, gl3_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) &&
				(leaf->cluster != gl3_viewcluster2))
			{
				gl3_viewcluster2 = leaf->cluster;
			}
		}
	}

	for (i = 0; i < 4; i++)
	{
		v_blend[i] = gl3_newrefdef.blend[i];
	}

	c_brush_polys = 0;
	c_alias_polys = 0;

	/* clear out the portion of the screen that the NOWORLDMODEL defines */
	if (gl3_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		// glEnable(GL_SCISSOR_TEST);
		// I'm not sure but looks like scissor test enabled all the time

		// glClearColor(0.3, 0.3, 0.3, 1);
		//                              A R G B
		rsxSetClearColor(gcmContext, 0xFF4C4C4C);
		// glScissor(gl3_newrefdef.x,
		// 		vid.height - gl3_newrefdef.height - gl3_newrefdef.y,
		// 		gl3_newrefdef.width, gl3_newrefdef.height);
		rsxSetScissor(gcmContext,
					  /*  x */ gl3_newrefdef.x,
					  /*  y */ vid.height - gl3_newrefdef.height - gl3_newrefdef.y,
					  /*  w */ gl3_newrefdef.width,
					  /*  h */ gl3_newrefdef.height);

		// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		rsxClearSurface(gcmContext,
						GCM_CLEAR_R |
							GCM_CLEAR_G |
							GCM_CLEAR_B |
							GCM_CLEAR_A |
							GCM_CLEAR_Z);

		// glClearColor(1, 0, 0.5, 0.5);
		//                              A R G B
		rsxSetClearColor(gcmContext, 0x7FFF007F);
		// glDisable(GL_SCISSOR_TEST);
		rsxSetScissor(gcmContext, 0, 0, vid.width, vid.height);
	}
}

static void // 935
R_RSX_drawParticles(void)
{
	// TODO: stereo
	//qboolean stereo_split_tb = ((gl_state.stereo_mode == STEREO_SPLIT_VERTICAL) && gl_state.camera_separation);
	//qboolean stereo_split_lr = ((gl_state.stereo_mode == STEREO_SPLIT_HORIZONTAL) && gl_state.camera_separation);

	//if (!(stereo_split_tb || stereo_split_lr))
	{
		int i;
		int numParticles = gl3_newrefdef.num_particles;
		if (numParticles == 0)
		{
			// RSX NOTE: this is important!
			return;
		}

		YQ2_ALIGNAS_TYPE(unsigned) byte color[4];
		const particle_t *p;
		// assume the size looks good with window height 480px and scale according to real resolution
		float pointSize = gl3_particle_size->value * (float)gl3_newrefdef.height/480.0f;

		typedef struct part_vtx_s {
			vec3_t pos;
			float size;
			float dist;
			float color[4];
		} part_vtx;

		YQ2_VLA(part_vtx, buf, numParticles);

		// TODO: viewOrg could be in UBO
		vec3_t viewOrg;
		VectorCopy(gl3_newrefdef.vieworg, viewOrg);

		// glDepthMask(GL_FALSE);
		rsxSetDepthWriteEnable(gcmContext, GCM_FALSE);

		// glEnable(GL_BLEND);
		rsxSetBlendEnable(gcmContext, GCM_TRUE);

// #ifdef YQ2_GL3_GLES
// 		// the RPi4 GLES3 implementation doesn't draw particles if culling is
// 		// enabled (at least with GL_FRONT which seems to be default in q2?)
// 		glDisable(GL_CULL_FACE);
// #else
// 		// GLES doesn't have this, maybe it's always enabled? (https://gamedev.stackexchange.com/a/15528 says it works)
// 		// luckily we don't use glPointSize() but set gl_PointSize in shader anyway
// 		glEnable(GL_PROGRAM_POINT_SIZE);
// #endif

		// RSX NOTE:
		// Update screen resolution in vertex shader to pass point pos to fragment
		// (there is some kind of conflict not allowing to do it earlier,
		// not affects perfomace because only vertex particle shader uses it)
		R_RSX_Shaders_UpdateScreenResolution();

		// GL3_UseProgram(gl3state.siParticle.shaderProgram);
		R_RSX_Shaders_Load(&(gl3state.vsParticle));
		if(gl3_particle_square->value != 0.0f)
		{
			R_RSX_Shaders_Load(&(gl3state.fsParticleSquare));
		}
		else
		{
			R_RSX_Shaders_Load(&(gl3state.fsParticle));
			// R_RSX_Shaders_Load(&(gl3state.fsParticleSquare));
		}

		for (i = 0, p = gl3_newrefdef.particles; i < numParticles; i++, p++ )
		{
			*(int *) color = d_8to24table [ p->color & 0xFF ];
			part_vtx* cur = &buf[i];
			vec3_t offset; // between viewOrg and particle position
			VectorSubtract(viewOrg, p->origin, offset);

			VectorCopy(p->origin, cur->pos);
			cur->size = pointSize;
			cur->dist = VectorLength(offset);

			for(int j=0; j<3; ++j)  cur->color[j] = color[j]*(1.0f/255.0f);

			cur->color[3] = p->alpha;
		}

		// GL3_BindVAO(gl3state.vaoParticle);
		// GL3_BindVBO(gl3state.vboParticle);

		// glBufferData(GL_ARRAY_BUFFER, sizeof(part_vtx)*numParticles, buf, GL_STREAM_DRAW);
		// copypasted from R_RSX_BufferAndDraw3D
		uint32_t rsx_mem_offset;
		uint32_t data_size = numParticles * sizeof(part_vtx);
		/* used memory would be aligned as like with rsxMemalign */
		uint32_t chunk_size = data_size;
		if ((chunk_size & 127) != 0)
		{
			chunk_size = (chunk_size & 0xFFFFFF80) + 128;
		}

		uint32_t current_offset = gl3state.rsx_vertex_buffer_offset;
		if (current_offset + chunk_size > gl3state.rsx_vertex_buffer_size)
		{
			current_offset = 0;
		}

		part_vtx *vertex_buffer_ptr = (part_vtx *)(gl3state.rsx_vertex_buffer + current_offset);
		gl3state.rsx_vertex_buffer_offset = current_offset + chunk_size;

		memcpy(vertex_buffer_ptr, buf, sizeof(part_vtx)*numParticles);

		// bind offsets
		// rsxAddressToOffset(&vertex_buffer_ptr[0].pos, &rsx_mem_offset);
		// rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_POS, 0, rsx_mem_offset, sizeof(part_vtx), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

		// // RSX NOTE: size and dist passed together in TEXCOORD0 as float2
		// rsxAddressToOffset(&vertex_buffer_ptr[0].size, &rsx_mem_offset);
		// rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX0, 0, rsx_mem_offset, sizeof(part_vtx), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

		// rsxAddressToOffset(&vertex_buffer_ptr[0].color, &rsx_mem_offset);
		// rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX1, 0, rsx_mem_offset, sizeof(part_vtx), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

		// rsxAddressToOffset(&vertex_buffer_ptr[0].size_ex, &rsx_mem_offset);
		// rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX2, 0, rsx_mem_offset, sizeof(part_vtx), 1, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);


		// glDrawArrays(GL_POINTS, 0, numParticles);
		// rsxDrawVertexArray(gcmContext, GCM_TYPE_POINTS, 0, numParticles);

		// RSX NOTE: PSIZ vertex atribute looks like not working on real PS3.
		// So draw each particle separatly. Yes: 1 draw call per 1 point.
		// with max at 4096 drawcalls. It's not that much but still really dumm.
		for (i = 0; i < numParticles; ++i)
		{
			rsxAddressToOffset(&vertex_buffer_ptr[i].pos, &rsx_mem_offset);
			rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_POS, 0, rsx_mem_offset, sizeof(part_vtx), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

			// RSX NOTE: size and dist passed together in TEXCOORD0 as float2
			rsxAddressToOffset(&vertex_buffer_ptr[i].size, &rsx_mem_offset);
			rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX0, 0, rsx_mem_offset, sizeof(part_vtx), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

			rsxAddressToOffset(&vertex_buffer_ptr[i].color, &rsx_mem_offset);
			rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX1, 0, rsx_mem_offset, sizeof(part_vtx), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

			float point_size = vertex_buffer_ptr[i].size / (vertex_buffer_ptr[i].dist * 0.1);
			rsxSetPointSize(gcmContext, point_size);

			// glDrawArrays(GL_POINTS, 0, numParticles);
			rsxDrawVertexArray(gcmContext, GCM_TYPE_POINTS, 0, 1);
		}

		// glDisable(GL_BLEND);
		rsxSetBlendEnable(gcmContext, GCM_FALSE);

		// glDepthMask(GL_TRUE);
		rsxSetDepthWriteEnable(gcmContext, GCM_TRUE);

// #ifdef YQ2_GL3_GLES
// 		if(gl_cull->value != 0.0f)
// 			glEnable(GL_CULL_FACE);
// #else
// 		glDisable(GL_PROGRAM_POINT_SIZE);
// #endif

		YQ2_VLAFREE(buf);
	}
}

// 1014
static void
R_RSX_drawEntitiesOnList(void)
{
	int i;

	if (!r_drawentities->value)
	{
		return;
	}

	R_RSX_Mesh_ResetShadowAliasModels();

	/* draw non-transparent first */
	for (i = 0; i < gl3_newrefdef.num_entities; i++)
	{
		entity_t *currententity = &gl3_newrefdef.entities[i];

		if (currententity->flags & RF_TRANSLUCENT)
		{
			continue; /* translucent */
		}

		if (currententity->flags & RF_BEAM)
		{
			// GL3_DrawBeam(currententity);
			R_RSX_DrawBeam(currententity);
		}
		else
		{
			gl3model_t *currentmodel = currententity->model;

			if (!currentmodel)
			{
				// GL3_DrawNullModel(currententity);
				R_RSX_drawNullModel(currententity);
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					// GL3_DrawAliasModel(currententity);
					R_RSX_Mesh_DrawAliasModel(currententity);
					break;
				case mod_brush:
					// printf("%s(): draw_brush_model solid\n", __func__);
					// GL3_DrawBrushModel(currententity, currentmodel);
					R_RSX_Surf_DrawBrushModel(currententity, currentmodel);
					break;
				case mod_sprite:
					Com_Printf("%s(): draw_sprite_model solid\n", __func__);
					// GL3_DrawSpriteModel(currententity, currentmodel);
					break;
				default:
					ri.Sys_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}
	
	/* draw transparent entities
	   we could sort these if it ever
	   becomes a problem... */
	// glDepthMask(0);
	rsxSetDepthWriteEnable(gcmContext, GCM_FALSE);

	for (i = 0; i < gl3_newrefdef.num_entities; i++)
	{
		entity_t *currententity = &gl3_newrefdef.entities[i];

		if (!(currententity->flags & RF_TRANSLUCENT))
		{
			continue; /* solid */
		}

		if (currententity->flags & RF_BEAM)
		{
			// GL3_DrawBeam(currententity);
			R_RSX_DrawBeam(currententity);
		}
		else
		{
			gl3model_t *currentmodel = currententity->model;

			if (!currentmodel)
			{
				// GL3_DrawNullModel(currententity);
				R_RSX_drawNullModel(currententity);
				continue;
			}

			switch (currentmodel->type)
			{
				case mod_alias:
					// printf("%s(): draw_alias_model transparent\n", __func__);
					// GL3_DrawAliasModel(currententity);
					R_RSX_Mesh_DrawAliasModel(currententity);
					break;
				case mod_brush:
					// printf("%s(): draw_brush_model transparent\n", __func__);
					// GL3_DrawBrushModel(currententity, currentmodel);
					R_RSX_Surf_DrawBrushModel(currententity, currentmodel);
					break;
				case mod_sprite:
					Com_Printf("%s(): draw_sprite_model transparent\n", __func__);
					// GL3_DrawSpriteModel(currententity, currentmodel);
					break;
				default:
					ri.Sys_Error(ERR_DROP, "Bad modeltype");
					break;
			}
		}
	}

	// GL3_DrawAliasShadows();
	R_RSX_Mesh_DrawAliasShadows();

	// glDepthMask(1); /* back to writing */
	rsxSetDepthWriteEnable(gcmContext, GCM_TRUE);
}

// 1120
static int
R_RSX_signbitsForPlane(cplane_t *out)
{
	int bits, j;

	/* for fast box on planeside test */
	bits = 0;

	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
		{
			bits |= 1 << j;
		}
	}

	return bits;
}

// 1139
static void
R_RSX_setFrustum(void)
{
	// NOTE: Pure vector function better move it to spu
	int i;

	/* rotate VPN right by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[0].normal, vup, vpn,
							-(90 - gl3_newrefdef.fov_x / 2));
	/* rotate VPN left by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[1].normal, vup, vpn, 
							90 - gl3_newrefdef.fov_x / 2);
	/* rotate VPN up by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[2].normal, vright, vpn,
							90 - gl3_newrefdef.fov_y / 2);
	/* rotate VPN down by FOV_X/2 degrees */
	RotatePointAroundVector(frustum[3].normal, vright, vpn,
							-(90 - gl3_newrefdef.fov_y / 2));

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct(gl3_origin, frustum[i].normal);
		frustum[i].signbits = R_RSX_signbitsForPlane(&frustum[i]);
	}
}

// 1241
static void
R_RSX_setup2DDrawing(void)
{
	int x = 0;
	int w = vid.width;
	int y = 0;
	int h = vid.height;
	float min, max;
	float scale[4], offset[4];

	min = 0.0f;
	max = 1.0f;
	scale[0] = w * 0.5f;
	scale[1] = h * -0.5f;
	scale[2] = (max - min) * 0.5f;
	scale[3] = 0.0f;
	offset[0] = x + w * 0.5f;
	offset[1] = y + h * 0.5f;
	offset[2] = (max + min) * 0.5f;
	offset[3] = 0.0f;

	rsxSetViewport(gcmContext, x, y, w, h, min, max, scale, offset);

	// RSX uses stupid relative coords
	hmm_mat4 transMatr = HMM_Orthographic(-1.0f, 1.0f, -1.0f, 1.0f, -99999, 99999);

	// btw as it basically a const it can be removed fully
	gl3state.uni2DData.transMat4 = transMatr;

	R_RSX_Shaders_UpdateUni2DData();

	// glDisable(GL_DEPTH_TEST);
	rsxSetDepthTestEnable(gcmContext, GCM_FALSE);
	// glDisable(GL_CULL_FACE);
	rsxSetCullFaceEnable(gcmContext, GCM_FALSE);
	// glDisable(GL_BLEND);
	rsxSetBlendEnable(gcmContext, GCM_FALSE);
}

// 1279
// equivalent to R_x * R_y * R_z where R_x is the trans matrix for rotating around X axis for aroundXdeg
static hmm_mat4
R_RSX_rotAroundAxisXYZ(float aroundXdeg, float aroundYdeg, float aroundZdeg)
{
	// Not pure vector/matix function move to spu

	float alpha = HMM_ToRadians(aroundXdeg);
	float beta = HMM_ToRadians(aroundYdeg);
	float gamma = HMM_ToRadians(aroundZdeg);

	float sinA = HMM_SinF(alpha);
	float cosA = HMM_CosF(alpha);
	float sinB = HMM_SinF(beta);
	float cosB = HMM_CosF(beta);
	float sinG = HMM_SinF(gamma);
	float cosG = HMM_CosF(gamma);

	hmm_mat4 ret = {{{cosB * cosG, sinA * sinB * cosG + cosA * sinG, -cosA * sinB * cosG + sinA * sinG, 0}, // first *column*
					 {-cosB * sinG, -sinA * sinB * sinG + cosA * cosG, cosA * sinB * sinG + sinA * cosG, 0},
					 {sinB, -sinA * cosB, cosA * cosB, 0},
					 {0, 0, 0, 1}}};

	return ret;
}

// equivalent to R_MYgluPerspective() but returning a matrix instead of setting internal OpenGL state
hmm_mat4 // 1304
R_RSX_MYgluPerspective(float fovy, float aspect, float zNear, float zFar)
{
	// calculation of left, right, bottom, top is from R_MYgluPerspective() of old gl backend
	// which seems to be slightly different from the real gluPerspective()
	// and thus also from HMM_Perspective()
	float left, right, bottom, top;
	float A, B, C, D;

	top = zNear * tan(fovy * M_PI / 360.0);
	bottom = -top;

	left = bottom * aspect;
	right = top * aspect;

	// TODO:  stereo stuff
	// left += - gl1_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;
	// right += - gl1_stereo_convergence->value * (2 * gl_state.camera_separation) / zNear;

	// the following emulates glFrustum(left, right, bottom, top, zNear, zFar)
	// see https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/glFrustum.xml
	//  or http://docs.gl/gl2/glFrustum#description (looks better in non-Firefox browsers)
	A = (right+left)/(right-left);
	B = (top+bottom)/(top-bottom);
	C = -(zFar+zNear)/(zFar-zNear);
	D = -(2.0*zFar*zNear)/(zFar-zNear);

	hmm_mat4 ret = {{
		{ (2.0*zNear)/(right-left), 0, 0, 0 }, // first *column*
		{ 0, (2.0*zNear)/(top-bottom), 0, 0 },
		{ A, B, C, -1.0 },
		{ 0, 0, D, 0 }
	}};

	return ret;
}

// faking glViewPort
void R_RSX_fakeViewPort(int x, int y, int width, int height)
{
	/* RSX NOTE:
	   Code taken from rsxSetViewport's description 
	   I dont know what it's doing.
	*/

	float min, max;
	float scale[4], offset[4];

	y = vid.height - y - height;
	min = 0.0f;
	max = 1.0f;
	scale[0] = width * 0.5f;
	scale[1] = height * -0.5f;
	scale[2] = (max - min) * 0.5f;
	offset[0] = x + width * 0.5f;
	offset[1] = y + height * 0.5f;
	offset[2] = (max + min) * 0.5f;

	rsxSetViewport(gcmContext, x, y, width, height, min, max, scale, offset);
}

// 1341
static void
R_RSX_setup3DStuff(void)
{
	int x, x2, y2, y, w, h;

	/* set up viewport */
	x = floor(gl3_newrefdef.x * vid.width / vid.width);
	x2 = ceil((gl3_newrefdef.x + gl3_newrefdef.width) * vid.width / vid.width);
	y = floor(vid.height - gl3_newrefdef.y * vid.height / vid.height);
	y2 = ceil(vid.height - (gl3_newrefdef.y + gl3_newrefdef.height) * vid.height / vid.height);

	w = x2 - x;
	h = y - y2;

	// set up the FBO accordingly, but only if actually rendering the world
	// (=> don't use FBO when rendering the playermodel in the player menu)
	// also, only do this when under water, because this has a noticeable overhead on some systems
	if (gl3_usefbo->value && gl3state.ppFBO != 0 // Just NO =)
		&& (gl3_newrefdef.rdflags & (RDF_NOWORLDMODEL | RDF_UNDERWATER)) == RDF_UNDERWATER && false)
	{
		// glBindFramebuffer(GL_FRAMEBUFFER, gl3state.ppFBO);
		// gl3state.ppFBObound = true;
		// if(gl3state.ppFBtex == 0)
		// {
		// 	gl3state.ppFBtexWidth = -1; // make sure we generate the texture storage below
		// 	glGenTextures(1, &gl3state.ppFBtex);
		// }

		// if(gl3state.ppFBrbo == 0)
		// {
		// 	gl3state.ppFBtexWidth = -1; // make sure we generate the RBO storage below
		// 	glGenRenderbuffers(1, &gl3state.ppFBrbo);
		// }

		// // even if the FBO already has a texture and RBO, the viewport size
		// // might have changed so they need to be regenerated with the correct sizes
		// if(gl3state.ppFBtexWidth != w || gl3state.ppFBtexHeight != h)
		// {
		// 	gl3state.ppFBtexWidth = w;
		// 	gl3state.ppFBtexHeight = h;
		// 	GL3_Bind(gl3state.ppFBtex);
		// 	// create texture for FBO with size of the viewport
		// 	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		// 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// 	GL3_Bind(0);
		// 	// attach it to currently bound FBO
		// 	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl3state.ppFBtex, 0);

		// 	// also create a renderbuffer object so the FBO has a stencil- and depth-buffer
		// 	glBindRenderbuffer(GL_RENDERBUFFER, gl3state.ppFBrbo);
		// 	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
		// 	glBindRenderbuffer(GL_RENDERBUFFER, 0);
		// 	// attach it to the FBO
		// 	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
		// 	                          GL_RENDERBUFFER, gl3state.ppFBrbo);

		// 	GLenum fbState = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		// 	if(fbState != GL_FRAMEBUFFER_COMPLETE)
		// 	{
		// 		R_Printf(PRINT_ALL, "GL3 %s(): WARNING: FBO is not complete, status = 0x%x\n", __func__, fbState);
		// 		gl3state.ppFBtexWidth = -1; // to try again next frame; TODO: maybe give up?
		// 		gl3state.ppFBObound = false;
		// 		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		// 	}
		// }

		// GL3_Clear(); // clear the FBO that's bound now

		// glViewport(0, 0, w, h); // this will be moved to the center later, so no x/y offset
	}
	else // rendering directly (not to FBO for postprocessing)
	{
		// glViewport(x, y2, w, h);
		R_RSX_fakeViewPort(x, y2, w, h);
		// float min, max;
		// float scale[4], offset[4];
		// min = 0.0f;
		// max = 1.0f;
		// scale[0] = w * 0.5f;
		// scale[1] = h * -0.5f; // Store all this crap at state?
		// scale[2] = (max - min) * 0.5f;
		// scale[3] = 0.0f;
		// offset[0] = x + w * 0.5f;
		// offset[1] = y + h * 0.5f;
		// offset[2] = (max + min) * 0.5f;
		// offset[3] = 0.0f;
		
		// rsxSetViewport(gcmContext, x, y2, w, h, min, max, scale, offset);
	
		// uint16_t rsx_x = x;
		// uint16_t rsx_y = w - y2 - h;
		// uint16_t rsx_width = w;
		// uint16_t rsx_height = h;
		// uint16_t rsx_min = 0.0f;
		// uint16_t rsx_max = 1.0f;
		// float scale[4], offset[4];
		// scale[0] = rsx_width * 0.5f;
		// scale[1] = rsx_height * -0.5f;
		// scale[2] = (rsx_max - rsx_min) * 0.5f;
		// offset[0] = x + rsx_width * 0.5f;
		// offset[1] = y + rsx_height * 0.5f;
		// offset[2] = (rsx_max + rsx_min) * 0.5f;
		// offset[3] = 0.0f;

		// rsxSetViewport(gcmContext, rsx_x, rsx_y, rsx_width, rsx_height, rsx_min, rsx_max, scale, offset);
	}


	gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_1;
	if (gl_anisotropic->value)
	{
		float anisotropic = gl_anisotropic->value;

		if (anisotropic > 7.0f)
		{
			if (anisotropic > 11.0f)
			{
				if (anisotropic > 14.0f)
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_16;
				}
				else
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_12;
				}
			}
			else
			{
				if (anisotropic > 9.0f)
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_10;
				}
				else
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_8;
				}
			}
		}
		else
		{
			if (anisotropic > 3.5f)
			{
				if (anisotropic > 5.0f)
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_6;
				}
				else
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_4;
				}
			}
			else
			{
				if (anisotropic > 1.5f)
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_2;
				}
				else
				{
					gl3state.anisotropic = GCM_TEXTURE_MAX_ANISO_1;
				}
			}
		}
	}

	/* set up projection matrix (eye coordinates -> clip coordinates) */
	{
		float screenaspect = (float)gl3_newrefdef.width / gl3_newrefdef.height;
		float dist = (r_farsee->value == 0) ? 4096.0f : 8192.0f;
		gl3state.projMat3D = R_RSX_MYgluPerspective(gl3_newrefdef.fov_y, screenaspect, 1.0f, dist);
	}


	// glCullFace(GL_FRONT);
	rsxSetCullFace(gcmContext, GCM_CULL_FRONT);


	/* set up view matrix (world coordinates -> eye coordinates) */
	{
		// first put Z axis going up
		hmm_mat4 viewMat = {{{ 0,  0, -1,  0}, // first *column* (the matrix is column-major)
		                     {-1,  0,  0,  0},
		                     { 0,  1,  0,  0},
		                     { 0,  0,  0,  1}}};

		// now rotate by view angles
		hmm_mat4 rotMat = R_RSX_rotAroundAxisXYZ(-gl3_newrefdef.viewangles[2], -gl3_newrefdef.viewangles[0], -gl3_newrefdef.viewangles[1]);

		viewMat = HMM_MultiplyMat4(viewMat, rotMat);

		// .. and apply translation for current position
		hmm_vec3 trans = HMM_Vec3(-gl3_newrefdef.vieworg[0], -gl3_newrefdef.vieworg[1], -gl3_newrefdef.vieworg[2]);
		viewMat = HMM_MultiplyMat4( viewMat, HMM_Translate(trans) );

		gl3state.viewMat3D = viewMat;
	}

	// just use one projection-view-matrix (premultiplied here)
	// so we have one less mat4 multiplication in the 3D shaders
	gl3state.uni3DData.transProjViewMat4 = HMM_Transpose(HMM_MultiplyMat4(gl3state.projMat3D, gl3state.viewMat3D));

	gl3state.uni3DData.transModelMat4 = gl3_identityMat4;

	gl3state.uni3DData.time = gl3_newrefdef.time;

	// GL3_UpdateUBO3D();
	R_RSX_Shaders_UpdateUni3DData();

	/* set drawing parms */
	if (gl_cull->value)
	{
		// glEnable(GL_CULL_FACE);
		rsxSetCullFaceEnable(gcmContext, GCM_TRUE);
	}
	else
	{
		// glDisable(GL_CULL_FACE);
		rsxSetCullFaceEnable(gcmContext, GCM_FALSE);
	}

	// glEnable(GL_DEPTH_TEST);
	rsxSetDepthTestEnable(gcmContext, GCM_TRUE);
}

extern int c_visible_lightmaps, c_visible_textures;

// 1494
/*
 * gl3_newrefdef must be set before the first call
 */
static void
R_RSX_renderView(refdef_t *fd)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }

	if (r_norefresh->value)
	{
		return;
	}

	gl3_newrefdef = *fd;

	if (!gl3_worldmodel && !(gl3_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		ri.Sys_Error(ERR_DROP, "R_RenderView: NULL worldmodel");
	}

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	// GL3_PushDlights();
	R_RSX_Light_PushDynLights();
	

	// if (gl_finish->value)
	// {
	// 	// glFinish();
	// 	rsxFlushBuffer(gcmContext);
	// }

	R_RSX_setupFrame();

	R_RSX_setFrustum();

	R_RSX_setup3DStuff();

	// GL3_MarkLeaves(); /* done here so we know if we're in water */
	// not implemented
	// R_RSX_Surf_MarkLeaves();

	R_RSX_Surf_DrawWorld();
	
	// GL3_DrawEntitiesOnList();
	R_RSX_drawEntitiesOnList();

	// kick the silly gl1_flashblend poly lights
	// GL3_RenderDlights();

	// GL3_DrawParticles();
	R_RSX_drawParticles();

	// GL3_DrawAlphaSurfaces();
	R_RSX_Surf_DrawAlphaSurfaces();

	// Note: R_Flash() is now GL3_Draw_Flash() and called from GL3_RenderFrame()

	if (r_speeds->value)
	{
		R_Printf(PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n",
				 c_brush_polys, c_alias_polys, c_visible_textures,
				 c_visible_lightmaps);
	}
}

// 1737
static void
R_RSX_RenderFrame(refdef_t *fd)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	Com_Printf("%s()\n", __func__);
	// 	// is_first_call = false;
	// }

	R_RSX_renderView(fd);

	// GL3_SetLightLevel(NULL);
	// qboolean usedFBO = gl3state.ppFBObound; // if it was/is used this frame
	// if(usedFBO)
	// {
	// 	glBindFramebuffer(GL_FRAMEBUFFER, 0); // now render to default framebuffer
	// 	gl3state.ppFBObound = false;
	// }
	R_RSX_setup2DDrawing();

	int x = (vid.width - gl3_newrefdef.width) / 2;
	int y = (vid.height - gl3_newrefdef.height) / 2;
	// if (usedFBO)
	if (false)
	{
		// if we're actually drawing the world and using an FBO, render the FBO's texture
		R_RSX_Draw_FrameBufferObject(x, y, gl3_newrefdef.width, gl3_newrefdef.height, gl3state.ppFBtex, v_blend);
	}
	else if (v_blend[3] != 0.0f)
	{
		R_RSX_Draw_Flash(v_blend, x, y, gl3_newrefdef.width, gl3_newrefdef.height);
	}
}

// 1764
static void
R_RSX_Clear(void)
{
	rsxSetClearDepthStencil(gcmContext, 0xffff);
	if (r_clear->value)
	{
		// Both color and depth
		// glClear(GL_COLOR_BUFFER_BIT | stencilFlags | GL_DEPTH_BUFFER_BIT);
		rsxClearSurface(gcmContext,
						GCM_CLEAR_R |
							GCM_CLEAR_G |
							GCM_CLEAR_B |
							GCM_CLEAR_A |
							GCM_CLEAR_Z);
	}
	else
	{
		// Depth only
		// glClear(GL_DEPTH_BUFFER_BIT | stencilFlags);
		rsxClearSurface(gcmContext,
						GCM_CLEAR_Z);
	}

	// gl3depthmin = 0;
	// gl3depthmax = 1;
	// Whose like constants and we settings them at
	// rsxSetViewport

	// glDepthFunc(GL_LEQUAL);
	rsxSetDepthFunc(gcmContext, GCM_LEQUAL);

	// if (gl_zfix->value)
	// {
	// 	if (gl3depthmax > gl3depthmin)
	// 	{
	// 		glPolygonOffset(0.05, 1);
	// 	}
	// 	else
	// 	{
	// 		glPolygonOffset(-0.05, -1);
	// 	}
	// }
	// Again at rsxSetViewport with 0 and 1 as values change to 0.05?

	/* stencilbuffer shadows */
	if (gl_shadows->value && gl3config.stencil)
	{
		// glClearStencil(1);
		// shared between rsxSetClearDepthStencil
		// glClear(GL_STENCIL_BUFFER_BIT);
		rsxClearSurface(gcmContext,
						GCM_CLEAR_S);
	}
}

// 1812
void R_RSX_BeginFrame(float camera_separation)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	Com_Printf("%s()\n", __func__);
	// 	// is_first_call = false;
	// }
	rsxSetColorMask(gcmContext, GCM_COLOR_MASK_B |
									GCM_COLOR_MASK_G |
									GCM_COLOR_MASK_R |
									GCM_COLOR_MASK_A);

	rsxSetColorMaskMrt(gcmContext, 0);

	u16 x, y, w, h;
	f32 min, max;
	f32 scale[4], offset[4];

	x = 0;
	y = 0;
	w = vid.width;
	h = vid.height;
	min = 0.0f;
	max = 1.0f;
	scale[0] = w * 0.5f;
	scale[1] = h * -0.5f;
	scale[2] = (max - min) * 0.5f;
	scale[3] = 0.0f;
	offset[0] = x + w * 0.5f;
	offset[1] = y + h * 0.5f;
	offset[2] = (max + min) * 0.5f;
	offset[3] = 0.0f;

	rsxSetViewport(gcmContext, x, y, w, h, min, max, scale, offset);
	rsxSetScissor(gcmContext, x, y, w, h);

	rsxSetDepthTestEnable(gcmContext, GCM_TRUE);
	rsxSetDepthFunc(gcmContext, GCM_LEQUAL);
	rsxSetShadeModel(gcmContext, GCM_SHADE_MODEL_SMOOTH);
	rsxSetDepthWriteEnable(gcmContext, 1);
	rsxSetFrontFace(gcmContext, GCM_FRONTFACE_CCW);
	rsxSetBlendFunc(gcmContext, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);

	rsxSetClearColor(gcmContext, 0xb56974);
	// rsxSetClearColor(gcmContext, 0x000000);

	rsxSetClearDepthStencil(gcmContext, 0xffff);
	rsxClearSurface(gcmContext, GCM_CLEAR_R |
									GCM_CLEAR_G |
									GCM_CLEAR_B |
									GCM_CLEAR_A |
									GCM_CLEAR_S |
									GCM_CLEAR_Z);
	rsxSetScissor(gcmContext, 0, 0, vid.width, vid.height);

	// rsxSetZMinMaxControl(gcmContext, 1, 1, 0);

	for (uint32_t i = 0; i < 8; i++)
	{
		rsxSetViewportClip(gcmContext, i, vid.width, vid.height);
	}

	// this is hack to track new memory alignments
	{
		static uint32_t prev_offset = 0;
		uint32_t offset;
		void *ptr = rsxMemalign(128, 4);
		rsxAddressToOffset(ptr, &offset);
		if (prev_offset != offset)
		{
			Com_Printf("current offset: %d delta: %+d\n", offset, prev_offset - offset);
			prev_offset = offset;
		}
		rsxFree(ptr);
	}

	// GL3 like things starting from here
	if (vid_gamma->modified || gl3_intensity->modified || gl3_intensity_2D->modified)
	{
		vid_gamma->modified = false;
		gl3_intensity->modified = false;
		gl3_intensity_2D->modified = false;

		gl3state.uniCommonData.gamma = 1.0f / vid_gamma->value;
		gl3state.uniCommonData.intensity = gl3_intensity->value;
		gl3state.uniCommonData.intensity2D = gl3_intensity_2D->value;
		/* we done it at shader loadings */
		// GL3_UpdateUBOCommon();
		R_RSX_Shaders_UpdateUniCommonData();
	}

	// in GL3, overbrightbits can have any positive value
	if (gl3_overbrightbits->modified)
	{
		gl3_overbrightbits->modified = false;

		if (gl3_overbrightbits->value < 0.0f)
		{
			ri.Cvar_Set("gl3_overbrightbits", "0");
		}

		gl3state.uni3DData.overbrightbits = (gl3_overbrightbits->value <= 0.0f) ? 1.0f : gl3_overbrightbits->value;
		// GL3_UpdateUBO3D();
		R_RSX_Shaders_UpdateUni3DData();
	}

	if(gl3_particle_fade_factor->modified)
	{
		gl3_particle_fade_factor->modified = false;
		gl3state.uni3DData.particleFadeFactor = gl3_particle_fade_factor->value;
	// 	GL3_UpdateUBO3D();
		R_RSX_Shaders_UpdateUni3DData();
	}

	if(gl3_particle_square->modified || gl3_colorlight->modified)
	{
		gl3_particle_square->modified = false;
		gl3_colorlight->modified = false;
	// 	GL3_RecreateShaders();
	}

	/* go into 2D mode */
	R_RSX_setup2DDrawing();

	/* draw buffer stuff */
	if (gl_drawbuffer->modified)
	{
		gl_drawbuffer->modified = false;
		R_Printf(PRINT_ALL, "NOTE: gl_drawbuffer not supported by RSX renderer!\n");
	}

	/* texturemode stuff */
	if (gl_texturemode->modified || (gl3config.anisotropic && gl_anisotropic->modified) || gl_nolerp_list->modified || r_lerp_list->modified || r_2D_unfiltered->modified || r_videos_unfiltered->modified)
	{
		// GL3_TextureMode(gl_texturemode->string);
		/* NOTE: This applies to all textures that is used by all shaders
		   rsxTextureControl
		   rsxTextureFilter
		   rsxTextureWrapMode
		   must be used to set them. */
		gl_texturemode->modified = false;
		gl_anisotropic->modified = false;
		gl_nolerp_list->modified = false;
		r_lerp_list->modified = false;
		r_2D_unfiltered->modified = false;
		r_videos_unfiltered->modified = false;
	}

	// RSX NOTE: can it run non vsync'ed?
	// if (r_vsync->modified)
	// {
	// 	GL3_SetVsync();
	// 	r_vsync->modified = false;
	// }

	/* clear screen if desired */
	R_RSX_Clear();
}

// 1924
static void
R_RSX_SetPalette(const unsigned char *palette)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }

	int i;
	byte *rp = (byte *)gl3_rawpalette;

	if (palette)
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = palette[i * 3 + 0];
			rp[i * 4 + 1] = palette[i * 3 + 1];
			rp[i * 4 + 2] = palette[i * 3 + 2];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 0] = LittleLong(d_8to24table[i]) & 0xff;
			rp[i * 4 + 1] = (LittleLong(d_8to24table[i]) >> 8) & 0xff;
			rp[i * 4 + 2] = (LittleLong(d_8to24table[i]) >> 16) & 0xff;
			rp[i * 4 + 3] = 0xff;
		}
	}

	// glClearColor(0, 0, 0, 0);
	rsxSetClearColor(gcmContext, 0x00000000);
	// glClear(GL_COLOR_BUFFER_BIT);
	rsxClearSurface(gcmContext,
					GCM_CLEAR_R |
						GCM_CLEAR_G |
						GCM_CLEAR_B |
						GCM_CLEAR_A);

	// Why?
	// glClearColor(1, 0, 0.5, 0.5);
	//                              A R G B
	rsxSetClearColor(gcmContext, 0x7FFF007F);
}

// 1961
static qboolean
R_RSX_EndWorldRenderPass(void)
{
	// Just like in GL3
	return true;
}

// 1967
refexport_t
GetRefAPI(refimport_t imp)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		Com_Printf("%s()\n", __func__);
		// is_first_call = false;
	}
	// struct for save refexport callbacks, copy of re struct from main file
	// used different variable name for prevent confusion and cppcheck warnings
	refexport_t refexport;

	memset(&refexport, 0, sizeof(refexport_t));
	ri = imp;

	refexport.api_version = API_VERSION;

	refexport.BeginRegistration = R_RSX_Mod_BeginRegistration;
	refexport.RegisterModel = R_RSX_Mod_RegisterModel;
	refexport.RegisterSkin = R_RSX_Image_RegisterSkin;
	refexport.DrawFindPic = R_RSX_Draw_FindPic;
	refexport.SetSky = R_RSX_Warp_SetSky;
	refexport.EndRegistration = R_RSX_Mod_EndRegistration;

	refexport.RenderFrame = R_RSX_RenderFrame;

	refexport.DrawGetPicSize = R_RSX_Draw_GetPicSize;

	refexport.DrawPicScaled = R_RSX_Draw_PicScaled;
	refexport.DrawStretchPic = R_RSX_Draw_StretchPic;
	refexport.DrawCharScaled = R_RSX_Draw_CharScaled;
	refexport.DrawTileClear = R_RSX_Draw_TileClear;
	refexport.DrawFill = R_RSX_Draw_Fill;
	refexport.DrawFadeScreen = R_RSX_Draw_FadeScreen;
	refexport.DrawStretchRaw = R_RSX_Draw_StretchRaw;

	refexport.Init = R_RSX_Init;
	refexport.IsVSyncActive = R_RSX_IsVSyncActive;
	refexport.PrepareForWindow = R_RSX_PrepareForWindow;
	refexport.InitContext = R_RSX_InitContext;
	refexport.ShutdownContext = R_RSX_ShutdownContext;
	refexport.Shutdown = R_RSX_Shutdown;

	refexport.SetPalette = R_RSX_SetPalette;
	refexport.BeginFrame = R_RSX_BeginFrame;
	refexport.EndWorldRenderpass = R_RSX_EndWorldRenderPass;
	refexport.EndFrame = R_RSX_EndFrame;

	// Tell the client that we're unsing the
	// new renderer restart API.
	ri.Vid_RequestRestart(RESTART_NO);

	Swap_Init();

	return refexport;
}

// 2017
void R_Printf(int level, const char *msg, ...)
{
	va_list argptr;
	va_start(argptr, msg);
	ri.Com_VPrintf(level, msg, argptr);
	va_end(argptr);
}