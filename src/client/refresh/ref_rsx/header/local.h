/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2016-2017 Daniel Gibson
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
 * Local header for the OpenGL3 refresher.
 *
 * =======================================================================
 */


#ifndef SRC_CLIENT_REFRESH_RSX_HEADER_LOCAL_H_
#define SRC_CLIENT_REFRESH_RSX_HEADER_LOCAL_H_

#include "rsx/gcm_sys.h"
#include "rsx/rsx.h"

#include "../../ref_shared.h"

#include "HandmadeMath.h"

#if 0 // only use this for development ..
#define STUB_ONCE(msg) do { \
		static int show=1; \
		if(show) { \
			show = 0; \
			R_Printf(PRINT_ALL, "STUB: %s() %s\n", __FUNCTION__, msg); \
		} \
	} while(0);
#else // .. so make this a no-op in released code
#define STUB_ONCE(msg)
#endif

// Credit to @rexim for to this handy macros
#define NOT_IMPLEMENTED() Com_Printf("ref_rsx::%s NOT IMPLEMENTED\n", __func__)
#define NOT_IMPLEMENTED_TEXT(str) Com_Printf("ref_rsx::%s NOT IMPLEMENTED: %s\n", __func__, str);

// RSX NOTE: I use this macros to truck if I miss to free rsxMemaligned memory
// In future they can be easily replaced with their non _with_log counterparts

extern void *last_rsxMemalign_ptr;
extern void *rsxMemalignUtil(uint32_t alignment, uint32_t size);

// Never put this macro in parentheses for type casting
#define rsxMemalign_with_log(alignment, size)                                  \
    rsxMemalignUtil(alignment, size);                                          \
    Com_DPrintf("function '%s' called for rsxMemalgin(%d, %d):\n",             \
        __func__, alignment, (unsigned int)(size));                            \
    if (last_rsxMemalign_ptr != NULL)                                          \
    {                                                                          \
        Com_DPrintf("    Successful!\n");                                      \
        Com_DPrintf("    Address: %p\n", last_rsxMemalign_ptr);                \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        Com_DPrintf("    Failed!\n");                                          \
    }                                                                          \

#define rsxFree_with_log(ptr) do {                                             \
    Com_DPrintf("function '%s' called for rsxFree(%p):\n",                     \
        __func__, ptr);                                                        \
    rsxFree(ptr);                                                              \
	Com_DPrintf("    Successful!\n");                                          \
} while (0)


// It's mostly temporary stab to remove OpenGL related things
#define GLuint uint32_t

// attribute locations for vertex shaders
enum {
	GL3_ATTRIB_POSITION   = 0,
	GL3_ATTRIB_TEXCOORD   = 1, // for normal texture
	GL3_ATTRIB_LMTEXCOORD = 2, // for lightmap
	GL3_ATTRIB_COLOR      = 3, // per-vertex color
	GL3_ATTRIB_NORMAL     = 4, // vertex normal
	GL3_ATTRIB_LIGHTFLAGS = 5  // uint, each set bit means "dyn light i affects this surface"
};

extern unsigned gl3_rawpalette[256];
extern unsigned d_8to24table[256];

typedef struct
{
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *glsl_version_string;

	int major_version;
	int minor_version;

	// ----

	qboolean anisotropic; // is GL_EXT_texture_filter_anisotropic supported?
	qboolean debug_output; // is GL_ARB_debug_output supported?
	qboolean stencil; // Do we have a stencil buffer?

	qboolean useBigVBO; // workaround for AMDs windows driver for fewer calls to glBufferData()

	// ----

	float max_anisotropy;
} gl3config_t;

typedef struct
{
	GLuint shaderProgram;
	int32_t uniVblend;
	int32_t uniLmScalesOrTime; // for 3D it's lmScales, for 2D underwater PP it's time
	hmm_vec4 lmScales[4];
} gl3ShaderInfo_t;

// ------------------------------------------------------------------------------
// Following RSX_Shader_* structs used do describe any shader
// ------------------------------------------------------------------------------

// NOTE: State structs not all (mostly not) in use,
// some matrix comparsions not necessary though 

// per shader analog of gl3UniCommon_t
typedef struct {
	float gamma;
	float intensity;
	float intensity2D; // for HUD, menus etc
	hmm_vec4 color;
} R_RSX_Shaders_commonState_t;

// per shader analog of gl3Uni2D_t
typedef struct {
	hmm_mat4 transMat;
} R_RSX_Shaders_uni2DState_t;

// As you can see there is no analog of gl3Uni3D_t because comparing it painfull 
// and not working good

// lm scales state store inside this handy struct not in root of shader
typedef struct {
	hmm_vec4 lmScales[4];
} R_RSX_Shaders_miscState_t;

// this is huge universal structure to store all possible shader states
typedef struct {

	R_RSX_Shaders_commonState_t common;
	R_RSX_Shaders_uni2DState_t uni2D;
	R_RSX_Shaders_miscState_t uniMisc;

} R_RSX_Shaders_universalState_t;

// All *Binds structures stores posible pointers to shaders uniforms 

// uniCommon consts pointers
typedef struct {

	rsxProgramConst *gamma;
	rsxProgramConst *intensity;
	rsxProgramConst *intensity2D;
	rsxProgramConst *color;

}  R_RSX_Shaders_commonBinds_t;

// uni2D consts pointers
typedef struct {

	rsxProgramConst *transMat;

} R_RSX_Shaders_uni2DBinds_t;

// uni3D consts pointers
typedef struct {

	rsxProgramConst *transProjView; // gl3state.projMat3D * gl3state.viewMat3D - so we don't have to do this in the shader
	rsxProgramConst *transModel;

	rsxProgramConst *scroll; // for SURF_FLOWING
	rsxProgramConst *time; // for warping surfaces like water & possibly other things
	rsxProgramConst *alpha; // for translucent surfaces (water, glass, ..)
	rsxProgramConst *overbrightbits; // gl3_overbrightbits, applied to lightmaps (and elsewhere to models)
	rsxProgramConst *particleFadeFactor; // gl3_particle_fade_factor, higher => less fading out towards edges

} R_RSX_Shaders_uni3DBinds_t;

// unique consts pointers - used by small amount of shaders 
typedef struct {

	rsxProgramConst *screenRes;
	rsxProgramConst *lmScales[4]; // MAX_LIGHTMAPS_PER_SURFACE == 4

} R_RSX_Shaders_miscBinds_t;

// this huge universal structure to store all possible shader binds
typedef struct {

	R_RSX_Shaders_uni2DBinds_t uni2D;
	R_RSX_Shaders_uni3DBinds_t uni3D;
	R_RSX_Shaders_commonBinds_t uniCommon;
	R_RSX_Shaders_miscBinds_t uniMisc;

} R_RSX_Shaders_universalBinds_t;

// as both kinds of shaders wrapped in one structure type
// specific to kind data can be stored together in singe union

typedef struct {
	void *vp_ucode;
	rsxVertexProgram *vpo;
	uint32_t* _padding0;
	uint32_t _padding1;
} R_RSX_Shaders_vertexDescription_t;

typedef struct {
	void *fp_ucode;
	rsxFragmentProgram *fpo;
	uint32_t* fp_buffer;
	uint32_t fp_offset;
} R_RSX_Shaders_fragmentDescription_t;

#define R_RSX_SHADER_TYPE_VERTEX 1
#define R_RSX_SHADER_TYPE_FRAGMENT 2

typedef struct {

	// flag to store type of shader
	// Posible values:
	// 	R_RSX_SHADER_TYPE_VERTEX
	// 	R_RSX_SHADER_TYPE_FRAGMENT
	uint32_t type;

	// union in which store type specific data
	union {
		R_RSX_Shaders_vertexDescription_t vertex;
		R_RSX_Shaders_fragmentDescription_t fragment;
	} description;

	// shader binds to uniforms
	R_RSX_Shaders_universalBinds_t binds;

	// shader uniforms's state 
	R_RSX_Shaders_universalState_t state;

} R_RSX_Shaders_universalWrapper_t;

// ------------------------------------------------------------------------------

typedef struct
{
	float gamma;
	float intensity;
	float intensity2D; // for HUD, menus etc

		// entries of std140 UBOs are aligned to multiples of their own size
		// so we'll need to pad accordingly for following vec4
		float _padding;

	hmm_vec4 color;
} gl3UniCommon_t;

typedef struct
{
	hmm_mat4 transMat4;
} gl3Uni2D_t;

typedef struct
{
	hmm_mat4 transProjViewMat4; // gl3state.projMat3D * gl3state.viewMat3D - so we don't have to do this in the shader
	hmm_mat4 transModelMat4;

	float scroll; // for SURF_FLOWING
	float time; // for warping surfaces like water & possibly other things
	float alpha; // for translucent surfaces (water, glass, ..)
	float overbrightbits; // gl3_overbrightbits, applied to lightmaps (and elsewhere to models)
	float particleFadeFactor; // gl3_particle_fade_factor, higher => less fading out towards edges

	float _padding[3]; // again, some padding to ensure this has right size
} gl3Uni3D_t;

extern const hmm_mat4 gl3_identityMat4;

enum {
	// width and height used to be 128, so now we should be able to get the same lightmap data
	// that used 32 lightmaps before into one, so 4 lightmaps should be enough
	BLOCK_WIDTH = 1024,
	BLOCK_HEIGHT = 512,
	LIGHTMAP_BYTES = 4,
	MAX_LIGHTMAPS = 4,
	MAX_LIGHTMAPS_PER_SURFACE = MAXLIGHTMAPS // 4
};

/* NOTE: struct image_s* is what re.RegisterSkin() etc return so no gl3image_s!
 *       (I think the client only passes the pointer around and doesn't know the
 *        definition of this struct, so this being different from struct image_s
 *        in ref_gl should be ok)
 */
typedef struct image_s
{
	char name[MAX_QPATH];               /* game path, including extension */
	imagetype_t type;
	int width, height;                  /* source image */
	//int upload_width, upload_height;    /* after power of two and picmip */
	int registration_sequence;          /* 0 = free */
	struct msurface_s *texturechain;    /* for sort-by-texture world drawing */
	GLuint texnum;                      /* gl texture binding */
	float sl, tl, sh, th;               /* 0,0 - 1,1 unless part of the scrap */
	// qboolean scrap; // currently unused
	qboolean has_alpha;

	qboolean is_mipmap;
	qboolean nolerp;

	gcmTexture gcm_texture;
	byte* data_ptr;
} gl3image_t;

typedef struct
{
	// TODO: what of this do we need?
	qboolean fullscreen;

	int prev_mode;

	// each lightmap consists of 4 sub-lightmaps allowing changing shadows on the same surface
	// used for switching on/off light and stuff like that.
	// most surfaces only have one really and the remaining for are filled with dummy data
	GLuint lightmap_textureIDs[MAX_LIGHTMAPS][MAX_LIGHTMAPS_PER_SURFACE]; // instead of lightmap_textures+i use lightmap_textureIDs[i]

	gl3image_t* currenttexture; // bound to GL_TEXTURE0
	int currentlightmap; // lightmap_textureIDs[currentlightmap] bound to GL_TEXTURE1
	GLuint currenttmu; // GL_TEXTURE0 or GL_TEXTURE1

	// FBO for postprocess effects (like under-water-warping)
	GLuint ppFBO;
	GLuint ppFBtex; // ppFBO's texture for color buffer
	int ppFBtexWidth, ppFBtexHeight;
	GLuint ppFBrbo; // ppFBO's renderbuffer object for depth and stencil buffer
	qboolean ppFBObound; // is it currently bound (rendered to)?

	//float camera_separation;
	//enum stereo_modes stereo_mode;

	GLuint currentVAO;
	GLuint currentVBO;
	GLuint currentEBO;
	GLuint currentShaderProgram;
	GLuint currentUBO;

	// Vertex shaders

	// Vertex Shader 2D Color
	R_RSX_Shaders_universalWrapper_t vs2DColor;
	// Vertex Shader 2D Texture
	R_RSX_Shaders_universalWrapper_t vs2DTexture;
	// Vertex Shader 3D
	R_RSX_Shaders_universalWrapper_t vs3D;
	// Vertex Shader 3D Flow
	R_RSX_Shaders_universalWrapper_t vs3DFlow;
	// Vertex Shader 3D Lightmap
	R_RSX_Shaders_universalWrapper_t vs3Dlm;
	// Vertex Shader 3D Lightmap Flow
	R_RSX_Shaders_universalWrapper_t vs3DlmFlow;
	// Vertex Shader 3D Alias
	R_RSX_Shaders_universalWrapper_t vs3Dalias;
	// Vertex Shader Particle
	R_RSX_Shaders_universalWrapper_t vsParticle;

	// Fragment shaders

	// Fragment Shader 2D Color (Flashes, menu background tint)
	R_RSX_Shaders_universalWrapper_t fs2DColor;
	// Fragment Shader 2D Texture (UI, ?videos?)
	R_RSX_Shaders_universalWrapper_t fs2DTexture;
	// Fragment Shader 3D (alpha surfaces)
	R_RSX_Shaders_universalWrapper_t fs3D;
	// Fragment Shader 3D Color (null model, beams/lasers)
	R_RSX_Shaders_universalWrapper_t fs3Dcolor;
	// Fragment Shader 3D Sky
	R_RSX_Shaders_universalWrapper_t fs3Dsky;
	// Fragment Shader 3D Ligtmap
	R_RSX_Shaders_universalWrapper_t fs3Dlm;
	// Fragment Shader 3D Alias
	R_RSX_Shaders_universalWrapper_t fs3Dalias;
	// Fragment Shader 3D Alias Color
	R_RSX_Shaders_universalWrapper_t fs3DaliasColor;
	// Fragment Shader 3D Water
	R_RSX_Shaders_universalWrapper_t fs3Dwater;
	// Fragment Shader Particle Square (simple squared particles)
	R_RSX_Shaders_universalWrapper_t fsParticleSquare;
	// Fragment Shader Particle (nice paricles)
	R_RSX_Shaders_universalWrapper_t fsParticle;


	// NOTE: make sure si2D is always the first shaderInfo (or adapt GL3_ShutdownShaders())
	// gl3ShaderInfo_t si2D;      // shader for rendering 2D with textures
	// gl3ShaderInfo_t si2Dcolor; // shader for rendering 2D with flat colors
	// gl3ShaderInfo_t si2DpostProcess; // shader to render postprocess FBO, when *not* underwater
	// gl3ShaderInfo_t si2DpostProcessWater; // shader to apply water-warp postprocess effect

	// gl3ShaderInfo_t si3Dlm;        // a regular opaque face (e.g. from brush) with lightmap
	// // TODO: lm-only variants for gl_lightmap 1
	// gl3ShaderInfo_t si3Dtrans;     // transparent is always w/o lightmap
	// gl3ShaderInfo_t si3DcolorOnly; // used for beams - no lightmaps
	// gl3ShaderInfo_t si3Dturb;      // for water etc - always without lightmap
	// gl3ShaderInfo_t si3DlmFlow;    // for flowing/scrolling things with lightmap (conveyor, ..?)
	// gl3ShaderInfo_t si3DtransFlow; // for transparent flowing/scrolling things (=> no lightmap)
	// gl3ShaderInfo_t si3Dsky;       // guess what..
	// gl3ShaderInfo_t si3Dsprite;    // for sprites
	// gl3ShaderInfo_t si3DspriteAlpha; // for sprites with alpha-testing

	// gl3ShaderInfo_t si3Dalias;      // for models
	// gl3ShaderInfo_t si3DaliasColor; // for models w/ flat colors

	// // NOTE: make sure siParticle is always the last shaderInfo (or adapt GL3_ShutdownShaders())
	// gl3ShaderInfo_t siParticle; // for particles. surprising, right?

	GLuint vao3D, vbo3D; // for brushes etc, using 10 floats and one uint as vertex input (x,y,z, s,t, lms,lmt, normX,normY,normZ ; lightFlags)

	// the next two are for gl3config.useBigVBO == true
	int vbo3Dsize;
	int vbo3DcurOffset;

	byte* rsx_vertex_buffer;
	uint32_t rsx_vertex_buffer_size;
	uint32_t rsx_vertex_buffer_offset;

	GLuint vaoAlias, vboAlias, eboAlias; // for models, using 9 floats as (x,y,z, s,t, r,g,b,a)
	GLuint vaoParticle, vboParticle; // for particles, using 9 floats (x,y,z, size,distance, r,g,b,a)

	// UBOs and their data
	gl3UniCommon_t uniCommonData;
	gl3Uni2D_t uni2DData;
	gl3Uni3D_t uni3DData;
	GLuint uniCommonUBO;
	GLuint uni2DUBO;
	GLuint uni3DUBO;

	hmm_mat4 projMat3D;
	hmm_mat4 viewMat3D;

	uint32_t anisotropic;
} gl3state_t;

extern gl3config_t gl3config;
extern gl3state_t gl3state;

extern viddef_t vid;

extern refdef_t gl3_newrefdef;

extern int gl3_visframecount; /* bumped when going to a new PVS */
extern int gl3_framecount; /* used for dlight push checking */

extern int gl3_viewcluster, gl3_viewcluster2, gl3_oldviewcluster, gl3_oldviewcluster2;

extern int c_brush_polys, c_alias_polys;

enum {MAX_RSX_TEXTURES = 1024};

// include this down here so it can use gl3image_t
#include "model.h"

typedef struct
{
	int internal_format;
	int current_lightmap_set; // index into gl3state.lightmap_textureIDs[]

	//msurface_t *lightmap_surfaces[MAX_LIGHTMAPS]; - no more lightmap chains, lightmaps are rendered multitextured

	int allocated[BLOCK_WIDTH];

	/* the lightmap texture data needs to be kept in
	   main memory so texsubimage can update properly */
	// byte lightmap_buffers[MAX_LIGHTMAPS_PER_SURFACE][4 * BLOCK_WIDTH * BLOCK_HEIGHT];

	/* With PS3 we had shared memory.
	 * So lightmaps data during generation already at RSX's memory.
	 * And there is no need in lightmap_buffers in static memory.
	 * Instead of uploading block we just switching to the next.
	 * By the way, 'upload'(R_RSX_LM_moveToNextSet) still can be
	 * used to bind textures to their buffers but it can be done at
	 * any point after lightmap buffers allocated in RSX's memory. */
	void *lightmap_buffers_ptr[MAX_LIGHTMAPS][MAX_LIGHTMAPS_PER_SURFACE];
	gcmTexture lightmap_textures[MAX_LIGHTMAPS][MAX_LIGHTMAPS_PER_SURFACE];
} gl3lightmapstate_t;

extern gl3model_t *gl3_worldmodel;

extern float gl3depthmin, gl3depthmax;

extern cplane_t frustum[4];

extern vec3_t gl3_origin;

extern gl3image_t *rsx_no_texture; /* use for bad textures */
extern gl3image_t *rsx_particle_texture; /* little dot for particles */

extern int gl_filter_min;
extern int gl_filter_max;

// static inline void
// GL3_UseProgram(GLuint shaderProgram)
// {
// 	if(shaderProgram != gl3state.currentShaderProgram)
// 	{
// 		gl3state.currentShaderProgram = shaderProgram;
// 		glUseProgram(shaderProgram);
// 	}
// }

// static inline void
// GL3_BindVAO(GLuint vao)
// {
// 	if(vao != gl3state.currentVAO)
// 	{
// 		gl3state.currentVAO = vao;
// 		glBindVertexArray(vao);
// 	}
// }

// static inline void
// GL3_BindVBO(GLuint vbo)
// {
// 	if(vbo != gl3state.currentVBO)
// 	{
// 		gl3state.currentVBO = vbo;
// 		glBindBuffer(GL_ARRAY_BUFFER, vbo);
// 	}
// }

// static inline void
// GL3_BindEBO(GLuint ebo)
// {
// 	if(ebo != gl3state.currentEBO)
// 	{
// 		gl3state.currentEBO = ebo;
// 		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
// 	}
// }

// rsx_main.c TODO: Separate
extern gcmContextData* gcmContext;

extern void R_RSX_BufferAndDraw3D(const gl3_3D_vtx_t* verts, int numVerts, uint32_t drawMode);
extern void R_RSX_RotateForEntity(entity_t *e);

// rsx_misc.c
extern void R_RSX_InitParticleTexture(void);
extern void R_RSX_InitNoTexture(void);
extern void R_RSX_ScreenShot(void);
// extern void GL3_SetDefaultState(void);

// rsx_model.c
extern int registration_sequence;
extern void R_RSX_Mod_Init(void);
extern void R_RSX_Mod_FreeAll(void);
extern void R_RSX_Mod_BeginRegistration(char *model);
extern struct model_s* R_RSX_Mod_RegisterModel(char *name);
extern void R_RSX_Mod_EndRegistration(void);
extern void R_RSX_Mod_Modellist_f(void);
extern const byte* R_RSX_Mod_ClusterPVS(int cluster, const gl3model_t *model);
extern mleaf_t* R_RSX_Mod_PointInLeaf(vec3_t p, gl3model_t *model);

// rsx_draw.c
extern uint32_t used_vb2Ds;

extern void R_RSX_Draw_InitLocal(void);
extern void R_RSX_Draw_ShutdownLocal(void);
extern gl3image_t * R_RSX_Draw_FindPic(char *name);
extern void R_RSX_Draw_GetPicSize(int *w, int *h, char *pic);
extern int R_RSX_Draw_GetPalette(void);

extern void R_RSX_Draw_PicScaled(int x, int y, char *pic, float factor);
extern void R_RSX_Draw_StretchPic(int x, int y, int w, int h, char *pic);
extern void R_RSX_Draw_CharScaled(int x, int y, int num, float scale);
extern void R_RSX_Draw_TileClear(int x, int y, int w, int h, char *pic);
extern void R_RSX_Draw_FrameBufferObject(int x, int y, int w, int h, GLuint fboTexture, const float v_blend[4]);
extern void R_RSX_Draw_Fill(int x, int y, int w, int h, int c);
extern void R_RSX_Draw_FadeScreen(void);
extern void R_RSX_Draw_Flash(const float color[4], float x, float y, float w, float h);
extern void R_RSX_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data);

// rsx_image.c
extern void R_RSX_Image_Init(void);
extern void R_RSX_Image_Shutdown(void);

extern gl3image_t rsx_textures[MAX_RSX_TEXTURES];

// extern void GL3_TextureMode(char *string);
extern void R_RSX_Image_BindUni2DTex0(gl3image_t *texture);

extern gl3image_t *R_RSX_Image_LoadPic(char *name, byte *pic, int width, int realwidth,
                                       int height, int realheight, imagetype_t type, int bits);
extern gl3image_t *R_RSX_Image_FindImage(char *name, imagetype_t type);
extern gl3image_t *R_RSX_Image_RegisterSkin(char *name);
extern void R_RSX_Image_Shutdown(void);
extern void R_RSX_Image_FreeUnusedImages(void);
extern qboolean R_RSX_Image_HasFreeSpace(void);
extern void R_RSX_Image_ImageList_f(void);

// gl3_light.c
extern void R_RSX_Light_Init(void);
extern void R_RSX_Light_Shutdown(void);

extern void R_RSX_Light_MarkLights(dlight_t *light, int bit, mnode_t *node);
extern void R_RSX_Light_PushDynLights(void);
extern void R_RSX_Light_LightPoint(entity_t *currententity, vec3_t p, vec3_t color);
extern void R_RSX_Light_BuildLightMap(msurface_t *surf, int offsetInLMbuf, int stride);

// rsx_lightmap.c
#define LIGHTMAP_FORMAT 0

extern void R_RSX_LM_Init(void);
extern void R_RSX_LM_Shutdown(void);

extern void R_RSX_LM_SetLightMapSet(uint32_t lightmapnum);

extern void R_RSX_LM_BuildPolygonFromSurface(gl3model_t *currentmodel, msurface_t *fa);
extern void R_RSX_LM_CreateSurfaceLightmap(msurface_t *surf);
extern void R_RSX_LM_BeginBuildingLightmaps(gl3model_t *m);
extern void R_RSX_LM_EndBuildingLightmaps(void);

// rsx_dlightmap.c
extern void R_RSX_DLM_Init(void);
extern void R_RSX_DLM_Shutdown(void);
extern void R_RSX_DLM_LoadDynLightmapAtlas(uint32_t dlm_atlas_index);
extern void R_RSX_DLM_RecreateDynamicLightmapForSurface(msurface_t *surf);

// rsx_warp.c
extern void R_RSX_Warp_EmitWaterPolys(msurface_t *fa);
extern void R_RSX_Warp_SubdivideSurface(msurface_t *fa, gl3model_t* loadmodel);

extern void R_RSX_Warp_SetSky(char *name, float rotate, vec3_t axis);
extern void R_RSX_Warp_DrawSkyBox(void);
extern void R_RSX_Warp_ClearSkyBox(void);
extern void R_RSX_Warp_AddSkySurface(msurface_t *fa);


// rsx_surf.c
extern void R_RSX_Surf_Init(void);
extern void R_RSX_Surf_Shutdown(void);
// extern void R_RSX_Surf_drawPoly(msurface_t *fa);
// extern void R_RSX_Surf_drawFlowingPoly(msurface_t *fa);
// extern void R_RSX_Surf_drawTriangleOutlines(void); /* not implemented in Yamagi's GL3 renderer */
extern void R_RSX_Surf_DrawAlphaSurfaces(void);
extern void R_RSX_Surf_DrawBrushModel(entity_t *e, gl3model_t *currentmodel);
extern void R_RSX_Surf_DrawWorld(void);
extern void R_RSX_Surf_MarkLeaves(void);

// rsx_mesh.c
extern void R_RSX_Mesh_DrawAliasModel(entity_t *e);
// extern void GL3_ResetShadowAliasModels(void);
extern void R_RSX_Mesh_ResetShadowAliasModels(void);
// extern void GL3_DrawAliasShadows(void);
extern void R_RSX_Mesh_DrawAliasShadows(void);
// extern void GL3_ShutdownMeshes(void);
extern void R_RSX_Mesh_Shutown(void);

// rsx_shaders.c

// extern qboolean GL3_RecreateShaders(void);
extern qboolean R_RSX_Shaders_Init(void);
extern void R_RSX_Shaders_Shutdown(void);
// extern void GL3_UpdateUBOCommon(void);
extern void R_RSX_Shaders_UpdateUniCommonData(void);
// extern void GL3_UpdateUBO2D(void);
extern void R_RSX_Shaders_UpdateUni2DData(void);
// extern void GL3_UpdateUBO3D(void);
extern void R_RSX_Shaders_UpdateUni3DData(void);
extern void R_RSX_Shaders_UpdateScreenResolution(void);
// extern void GL3_UpdateUBOLights(void);
extern void R_RSX_Shaders_SetTexture0(gl3image_t *texture);
extern void R_RSX_Shaders_UpdateLmScales(const hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE]);

extern void R_RSX_Shaders_Load(const R_RSX_Shaders_universalWrapper_t* shader);

// ############ Cvars ###########

extern cvar_t *gl_msaa_samples;
extern cvar_t *r_vsync;
extern cvar_t *r_retexturing;
extern cvar_t *r_scale8bittextures;
extern cvar_t *vid_fullscreen;
extern cvar_t *r_mode;
extern cvar_t *r_customwidth;
extern cvar_t *r_customheight;

extern cvar_t *r_2D_unfiltered;
extern cvar_t *r_videos_unfiltered;
extern cvar_t *gl_nolerp_list;
extern cvar_t *r_lerp_list;
extern cvar_t *gl_nobind;
extern cvar_t *r_lockpvs;
extern cvar_t *r_novis;

extern cvar_t *gl_cull;
extern cvar_t *gl_zfix;
extern cvar_t *r_fullbright;

extern cvar_t *r_norefresh;
extern cvar_t *gl_lefthand;
extern cvar_t *r_gunfov;
extern cvar_t *r_farsee;
extern cvar_t *r_drawworld;

extern cvar_t *vid_gamma;
extern cvar_t *gl3_intensity;
extern cvar_t *gl3_intensity_2D;
extern cvar_t *gl_anisotropic;
extern cvar_t *gl_texturemode;

extern cvar_t *r_lightlevel;
extern cvar_t *gl3_overbrightbits;
extern cvar_t *gl3_particle_fade_factor;
extern cvar_t *gl3_particle_square;
extern cvar_t *gl3_colorlight;

extern cvar_t *r_modulate;
extern cvar_t *gl_lightmap;
extern cvar_t *gl_shadows;
extern cvar_t *r_fixsurfsky;

extern cvar_t *gl3_debugcontext;

#endif /* SRC_CLIENT_REFRESH_RSX_HEADER_LOCAL_H_ */
