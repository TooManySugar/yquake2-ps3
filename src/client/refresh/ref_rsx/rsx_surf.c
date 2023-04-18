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
 * Surface generation and drawing
 *
 * =======================================================================
 */

#include <assert.h>
#include <stddef.h> // ofsetof()
#include <sys/time.h>

#include "header/local.h"

int c_visible_lightmaps;
int c_visible_textures;
static vec3_t modelorg; /* relative to viewpoint */
static msurface_t *gl3_alpha_surfaces;

gl3lightmapstate_t gl3_lms;

#define BACKFACE_EPSILON 0.01

extern gl3image_t rsx_textures[MAX_RSX_TEXTURES];
extern int num_rsx_textures;

void
R_RSX_Surf_Init(void)
{
    // NOTE: I don't know that needed to be enabled on RSX
    //       Binds also useless because we took data on shader loading

	// init the VAO and VBO for the standard vertexdata: 10 floats and 1 uint
	// (X, Y, Z), (S, T), (LMS, LMT), (normX, normY, normZ) ; lightFlags - last two groups for lightmap/dynlights

	// glGenVertexArrays(1, &gl3state.vao3D);
	// GL3_BindVAO(gl3state.vao3D);

	// glGenBuffers(1, &gl3state.vbo3D);
	// GL3_BindVBO(gl3state.vbo3D);

	// if(gl3config.useBigVBO)
	// {
	// 	gl3state.vbo3Dsize = 5*1024*1024; // a 5MB buffer seems to work well?
	// 	gl3state.vbo3DcurOffset = 0;
	// 	glBufferData(GL_ARRAY_BUFFER, gl3state.vbo3Dsize, NULL, GL_STREAM_DRAW); // allocate/reserve that data
	// }

	/**
	 * Just like useBigVBO but it is always big
	 * Usage of this reduses frametime from 200ms to 50ms in emulator on my potato laptop
	 */
	/* Use 5 MiB because yamagi uses so
	 * Not reset offset to 0 at frame begin because for some unknown
	 * reason this data still in use */
	gl3state.rsx_vertex_buffer_size = 5*1024*1024;
	gl3state.rsx_vertex_buffer = (byte *)rsxMemalign_with_log(128, gl3state.rsx_vertex_buffer_size);
	gl3state.rsx_vertex_buffer_offset = 0;

	// glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	// qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), 0);

	// glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	// qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, texCoord));

	// glEnableVertexAttribArray(GL3_ATTRIB_LMTEXCOORD);
	// qglVertexAttribPointer(GL3_ATTRIB_LMTEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, lmTexCoord));

	// glEnableVertexAttribArray(GL3_ATTRIB_NORMAL);
	// qglVertexAttribPointer(GL3_ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, normal));

	// glEnableVertexAttribArray(GL3_ATTRIB_LIGHTFLAGS);
	// qglVertexAttribIPointer(GL3_ATTRIB_LIGHTFLAGS, 1, GL_UNSIGNED_INT, sizeof(gl3_3D_vtx_t), offsetof(gl3_3D_vtx_t, lightFlags));



	// // init VAO and VBO for model vertexdata: 9 floats
	// // (X,Y,Z), (S,T), (R,G,B,A)

	// glGenVertexArrays(1, &gl3state.vaoAlias);
	// GL3_BindVAO(gl3state.vaoAlias);

	// glGenBuffers(1, &gl3state.vboAlias);
	// GL3_BindVBO(gl3state.vboAlias);

	// glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	// qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 0);

	// glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	// qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 3*sizeof(GLfloat));

	// glEnableVertexAttribArray(GL3_ATTRIB_COLOR);
	// qglVertexAttribPointer(GL3_ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 5*sizeof(GLfloat));

	// glGenBuffers(1, &gl3state.eboAlias);

	// // init VAO and VBO for particle vertexdata: 9 floats
	// // (X,Y,Z), (point_size,distace_to_camera), (R,G,B,A)

	// glGenVertexArrays(1, &gl3state.vaoParticle);
	// GL3_BindVAO(gl3state.vaoParticle);

	// glGenBuffers(1, &gl3state.vboParticle);
	// GL3_BindVBO(gl3state.vboParticle);

	// glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	// qglVertexAttribPointer(GL3_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 0);

	// // TODO: maybe move point size and camera origin to UBO and calculate distance in vertex shader
	// glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD); // it's abused for (point_size, distance) here..
	// qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 3*sizeof(GLfloat));

	// glEnableVertexAttribArray(GL3_ATTRIB_COLOR);
	// qglVertexAttribPointer(GL3_ATTRIB_COLOR, 4, GL_FLOAT, GL_FALSE, 9*sizeof(GLfloat), 5*sizeof(GLfloat));
}

void
R_RSX_Surf_Shutdown(void)
{
	rsxFree_with_log(gl3state.rsx_vertex_buffer);
	gl3state.rsx_vertex_buffer = NULL;
	gl3state.rsx_vertex_buffer_size = 0;
	gl3state.rsx_vertex_buffer_offset = 0;
}

/* // 131 (unchaged)
 * Returns true if the box is completely outside the frustom
 */
/*
	RSX NOTE:
	Quake 2's frustum lacks farface
	instead levels split into areas
	areas behind closed doors are not drawn
*/
static qboolean
R_RSX_Surf_cullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	if (!gl_cull->value)
	{
		return false;
	}

	for (i = 0; i < 4; i++)
	{
		if (BOX_ON_PLANE_SIDE(mins, maxs, &frustum[i]) == 2)
		{
			return true;
		}
	}

	return false;
}

/* 162 (unchanged)
 * Returns the proper texture for a given time and base texture
 */
static gl3image_t *
R_RSX_Surf_textureAnimation(entity_t *currententity, mtexinfo_t *tex)
{
	int c;

	if (!tex->next)
	{
		return tex->image;
	}

	c = currententity->frame % tex->numframes;

	while (c)
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}

static void // 181 (unchanged)
R_RSX_Surf_setLightFlags(msurface_t *surf)
{
	unsigned int lightFlags = 0;
	if (surf->dlightframe == gl3_framecount)
	{
		lightFlags = surf->dlightbits;
	}

	gl3_3D_vtx_t* verts = surf->polys->vertices;

	int numVerts = surf->polys->numverts;
	for(int i=0; i<numVerts; ++i)
	{
		verts[i].lightFlags = lightFlags;
	}
}

static void // 202 (unchanged)
R_RSX_Surf_setAllLightFlags(msurface_t *surf)
{
	unsigned int lightFlags = 0xffffffff;

	gl3_3D_vtx_t* verts = surf->polys->vertices;

	int numVerts = surf->polys->numverts;
	for(int i=0; i<numVerts; ++i)
	{
		verts[i].lightFlags = lightFlags;
	}
}

void // 216
R_RSX_Surf_drawPoly(msurface_t *fa)
{
	glpoly_t *p = fa->polys;

	// GL3_BindVAO(gl3state.vao3D);
	// GL3_BindVBO(gl3state.vbo3D);

	// GL3_BufferAndDraw3D(p->vertices, p->numverts, GL_TRIANGLE_FAN);
	R_RSX_BufferAndDraw3D(p->vertices, p->numverts, GCM_TYPE_TRIANGLE_FAN);
}

// 227
void
R_RSX_Surf_drawFlowingPoly(msurface_t *fa)
{
	glpoly_t *p;
	float scroll;

	p = fa->polys;

	scroll = -64.0f * ((gl3_newrefdef.time / 40.0f) - (int)(gl3_newrefdef.time / 40.0f));

	if (scroll == 0.0f)
	{
		scroll = -64.0f;
	}


	// NOTE: this quite heavy thing to do just to set scroll
	if(gl3state.uni3DData.scroll != scroll)
	{
		gl3state.uni3DData.scroll = scroll;
		R_RSX_Shaders_UpdateUni3DData();
	}

	// GL3_BindVAO(gl3state.vao3D);
	// GL3_BindVBO(gl3state.vbo3D);

	// GL3_BufferAndDraw3D(p->vertices, p->numverts, GL_TRIANGLE_FAN);
	R_RSX_BufferAndDraw3D(p->vertices, p->numverts, GCM_TYPE_TRIANGLE_FAN);
}

static void // 340 This is actual drawing
R_RSX_Surf_renderBrushPoly(entity_t *currententity, msurface_t *fa)
{
	int map;
	gl3image_t *image;

	c_brush_polys++;

	image = R_RSX_Surf_textureAnimation(currententity, fa->texinfo);

	if (fa->flags & SURF_DRAWTURB)
	{
		// GL3_Bind(image->texnum);
		R_RSX_Image_BindUni2DTex0(image);

		// GL3_EmitWaterPolys(fa);
		R_RSX_Warp_EmitWaterPolys(fa);

		return;
	}
	else
	{
		// GL3_Bind(image->texnum);
		R_RSX_Image_BindUni2DTex0(image);
	}

	hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE] = {0};
	memset(&lmScales[0], 0, sizeof(lmScales));
	lmScales[0] = HMM_Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	// GL3_BindLightmap(fa->lightmaptexturenum);
	R_RSX_LM_SetLightMapSet(fa->lightmaptexturenum);

	// apply lightmap animations
	for (map = 0; map < MAX_LIGHTMAPS_PER_SURFACE && fa->styles[map] != 255; map++)
	{
		lmScales[map].R = gl3_newrefdef.lightstyles[fa->styles[map]].rgb[0];
		lmScales[map].G = gl3_newrefdef.lightstyles[fa->styles[map]].rgb[1];
		lmScales[map].B = gl3_newrefdef.lightstyles[fa->styles[map]].rgb[2];
		lmScales[map].A = 1.0f;
	}

	if (fa->texinfo->flags & SURF_FLOWING)
	{
		// GL3_UseProgram(gl3state.si3DlmFlow.shaderProgram);
		R_RSX_Shaders_Load(&(gl3state.vs3DlmFlow));
		if (gl3_colorlight->value == 0.0f)
		{
			// TODO fragment NoColor
			R_RSX_Shaders_Load(&(gl3state.fs3Dlm));
			// R_RSX_Shaders_Load(&(gl3state.fs3D));
		}
		else
		{
			R_RSX_Shaders_Load(&(gl3state.fs3Dlm));
			// R_RSX_Shaders_Load(&(gl3state.fs3D));
		}

		// UpdateLMscales(lmScales, &gl3state.si3DlmFlow);
		R_RSX_Shaders_UpdateLmScales(lmScales);

		// GL3_DrawGLFlowingPoly(fa);
		R_RSX_Surf_drawFlowingPoly(fa);
	}
	else
	{
		// GL3_UseProgram(gl3state.si3Dlm.shaderProgram);
		R_RSX_Shaders_Load(&(gl3state.vs3Dlm));
		R_RSX_Shaders_Load(&(gl3state.fs3Dlm));
		// R_RSX_Shaders_Load(&(gl3state.fs3D));

		// UpdateLMscales(lmScales, &gl3state.si3Dlm);
		R_RSX_Shaders_UpdateLmScales(lmScales);

		R_RSX_Surf_drawPoly(fa);
	}

	// Note: lightmap chains are gone, lightmaps are rendered together with normal texture in one pass
}

/* 393
 * Draw water surfaces and windows.
 * The BSP tree is waled front to back, so unwinding the chain
 * of alpha_surfaces will draw back to front, giving proper ordering.
 */
void
R_RSX_Surf_DrawAlphaSurfaces(void)
{
	msurface_t *s;

	/* go back to the world matrix */
	gl3state.uni3DData.transModelMat4 = gl3_identityMat4;
	// GL3_UpdateUBO3D();
	R_RSX_Shaders_UpdateUni3DData();

	// glEnable(GL_BLEND);
	rsxSetBlendEnable(gcmContext, GCM_TRUE);

	for (s = gl3_alpha_surfaces; s != NULL; s = s->texturechain)
	{
		// GL3_Bind(s->texinfo->image->texnum);
		R_RSX_Image_BindUni2DTex0(s->texinfo->image);

		c_brush_polys++;
		float alpha = 1.0f;
		if (s->texinfo->flags & SURF_TRANS33)
		{
			alpha = 0.333f;
		}
		else if (s->texinfo->flags & SURF_TRANS66)
		{
			alpha = 0.666f;
		}

		if(alpha != gl3state.uni3DData.alpha)
		{
			gl3state.uni3DData.alpha = alpha;
			// GL3_UpdateUBO3D();
			R_RSX_Shaders_UpdateUni3DData();
		}

		if (s->flags & SURF_DRAWTURB)
		{
			R_RSX_Warp_EmitWaterPolys(s);
		}
		else if (s->texinfo->flags & SURF_FLOWING)
		{
			// // GL3_UseProgram(gl3state.si3DtransFlow.shaderProgram);
			R_RSX_Shaders_Load(&(gl3state.vs3DFlow));
			R_RSX_Shaders_Load(&(gl3state.fs3D));
			
			// GL3_DrawGLFlowingPoly(s);
			R_RSX_Surf_drawFlowingPoly(s);
		}
		else
		{
			// window drawing here

			// GL3_UseProgram(gl3state.si3Dtrans.shaderProgram);
			R_RSX_Shaders_Load(&(gl3state.vs3D));
			R_RSX_Shaders_Load(&(gl3state.fs3D));
			
			// GL3_DrawGLPoly(s);
			R_RSX_Surf_drawPoly(s);
		}
	}

	gl3state.uni3DData.alpha = 1.0f;
	// GL3_UpdateUBO3D();
	R_RSX_Shaders_UpdateUni3DData();

	// glDisable(GL_BLEND);
	rsxSetBlendEnable(gcmContext, GCM_FALSE);

	gl3_alpha_surfaces = NULL;
}

// 451 (unchanged)
static void
R_RSX_Surf_drawTextureChains(entity_t *currententity)
{
	int i;
	msurface_t *s;
	gl3image_t *image;
#if defined(WORLD_DRAW_SPEEDS)
	int polys_renderered = 0;
#endif

	c_visible_textures = 0;


	for (i = 0, image = rsx_textures; i < num_rsx_textures; i++, image++)
	{
		if (!image->registration_sequence)
		{
			continue;
		}

		s = image->texturechain;

		if (!s)
		{
			continue;
		}

		c_visible_textures++;

		for ( ; s; s = s->texturechain)
		{
			R_RSX_Surf_setLightFlags(s);
			R_RSX_Surf_renderBrushPoly(currententity, s);

#if defined(WORLD_DRAW_SPEEDS)
			++polys_renderered;
#endif
		}

		image->texturechain = NULL;
	}

#if defined(WORLD_DRAW_SPEEDS)
	Com_Printf("ref_rsx::%s looped %d times polys_renderered %d\n", __func__, i, polys_renderered);
#endif
	// TODO: maybe one loop for normal faces and one for SURF_DRAWTURB ???
}

static void // 489
R_RSX_Surf_renderLightmappedPoly(entity_t *currententity, msurface_t *surf)
{
	int map;
	gl3image_t *image = R_RSX_Surf_textureAnimation(currententity, surf->texinfo);

	hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE] = {0};
	lmScales[0] = HMM_Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	assert((surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)) == 0
			&& "RenderLightMappedPoly mustn't be called with transparent, sky or warping surfaces!");

	// apply lightmap animations
	for (map = 0; map < MAX_LIGHTMAPS_PER_SURFACE && surf->styles[map] != 255; map++)
	{
		lmScales[map].R = gl3_newrefdef.lightstyles[surf->styles[map]].rgb[0];
		lmScales[map].G = gl3_newrefdef.lightstyles[surf->styles[map]].rgb[1];
		lmScales[map].B = gl3_newrefdef.lightstyles[surf->styles[map]].rgb[2];
		lmScales[map].A = 1.0f;
	}

	c_brush_polys++;

	// GL3_Bind(image->texnum);
	R_RSX_Image_BindUni2DTex0(image);

	// GL3_BindLightmap(surf->lightmaptexturenum);
	R_RSX_LM_SetLightMapSet(surf->lightmaptexturenum);

	if (surf->texinfo->flags & SURF_FLOWING)
	{
		// GL3_UseProgram(gl3state.si3DlmFlow.shaderProgram);
		R_RSX_Shaders_Load(&(gl3state.vs3DlmFlow));
		R_RSX_Shaders_Load(&(gl3state.fs3Dlm));

		// UpdateLMscales(lmScales, &gl3state.si3DlmFlow);
		R_RSX_Shaders_UpdateLmScales(lmScales);
		
		// GL3_DrawGLFlowingPoly(surf);
		R_RSX_Surf_drawFlowingPoly(surf);
	}
	else
	{
		// GL3_UseProgram(gl3state.si3Dlm.shaderProgram);
		R_RSX_Shaders_Load(&(gl3state.vs3Dlm));
		R_RSX_Shaders_Load(&(gl3state.fs3Dlm));

		// UpdateLMscales(lmScales, &gl3state.si3Dlm);
		R_RSX_Shaders_UpdateLmScales(lmScales);
		
		// GL3_DrawGLPoly(surf);
		R_RSX_Surf_drawPoly(surf);
	}
}

// 529
static void
R_RSX_Surf_drawInlineBModel(entity_t *currententity, gl3model_t *currentmodel)
{
	int i, k;
	cplane_t *pplane;
	float dot;
	msurface_t *psurf;
	dlight_t *lt;

	/* calculate dynamic lighting for bmodel */
	lt = gl3_newrefdef.dlights;

	for (k = 0; k < gl3_newrefdef.num_dlights; k++, lt++)
	{
		R_RSX_Light_MarkLights(lt, 1 << k, currentmodel->nodes + currentmodel->firstnode);
	}

	psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface];

	if (currententity->flags & RF_TRANSLUCENT)
	{
		rsxSetBlendEnable(gcmContext, GCM_TRUE);
		/* TODO: should I care about the 0.25 part? we'll just set alpha to 0.33 or 0.66 depending on surface flag..
		glColor4f(1, 1, 1, 0.25);
		R_TexEnv(GL_MODULATE);
		*/
	}

	/* draw texture */
	for (i = 0; i < currentmodel->nummodelsurfaces; i++, psurf++)
	{
		/* find which side of the node we are on */
		pplane = psurf->plane;

		dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

		/* draw the polygon */
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (psurf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
			{
				/* add to the translucent chain */
				psurf->texturechain = gl3_alpha_surfaces;
				gl3_alpha_surfaces = psurf;
			}
			else if(!(psurf->flags & SURF_DRAWTURB))
			{
				R_RSX_Surf_setAllLightFlags(psurf);
				R_RSX_Surf_renderLightmappedPoly(currententity, psurf);
			}
			else
			{
				R_RSX_Surf_renderBrushPoly(currententity, psurf);
			}
		}
	}

	if (currententity->flags & RF_TRANSLUCENT)
	{
		rsxSetBlendEnable(gcmContext, GCM_FALSE);
	}
}

void // 593
R_RSX_Surf_DrawBrushModel(entity_t *e, gl3model_t *currentmodel)
{
	vec3_t mins, maxs;
	int i;
	qboolean rotated;

	if (currentmodel->nummodelsurfaces == 0)
	{
		return;
	}

	gl3state.currenttexture = NULL;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;

		for (i = 0; i < 3; i++)
		{
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if (R_RSX_Surf_cullBox(mins, maxs))
	{
		return;
	}

	if (gl_zfix->value)
	{
		// glEnable(GL_POLYGON_OFFSET_FILL);
		rsxSetPolygonOffsetFillEnable(gcmContext, GCM_TRUE);
	}

	VectorSubtract(gl3_newrefdef.vieworg, e->origin, modelorg);

	if (rotated)
	{
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	//glPushMatrix();
	hmm_mat4 oldMat = gl3state.uni3DData.transModelMat4;

	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];
	R_RSX_RotateForEntity(e);
	e->angles[0] = -e->angles[0];
	e->angles[2] = -e->angles[2];

	// DrawInlineBModel(e, currentmodel);
	R_RSX_Surf_drawInlineBModel(e, currentmodel);

	// glPopMatrix();
	gl3state.uni3DData.transModelMat4 = oldMat;
	// GL3_UpdateUBO3D();
	R_RSX_Shaders_UpdateUni3DData();

	if (gl_zfix->value)
	{
		// glDisable(GL_POLYGON_OFFSET_FILL);
		rsxSetPolygonOffsetFillEnable(gcmContext, GCM_FALSE);
	}
}

#if defined(WORLD_DRAW_SPEEDS)
static int rwn_calls = 0;
#endif

// 670 (unchanged)
static void
R_RSX_Surf_recursiveWorldNode(entity_t *currententity, mnode_t *node)
{
#if defined(WORLD_DRAW_SPEEDS)
	rwn_calls++;
#endif
    // static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }

	int c, side, sidebit;
	cplane_t *plane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	float dot;
	gl3image_t *image;

	if (node->contents == CONTENTS_SOLID)
	{
		return; /* solid */
	}

	if (node->visframe != gl3_visframecount)
	{
		return;
	}

	if (R_RSX_Surf_cullBox(node->minmaxs, node->minmaxs + 3))
	{
		return;
	}

	/* if a leaf node, draw stuff */
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *)node;

		/* check for door connected areas */
		if (gl3_newrefdef.areabits)
		{
			if (!(gl3_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
			{
				return; /* not visible */
			}
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = gl3_framecount;
				mark++;
			}
			while (--c);
		}

		return;
	}

	/* node is just a decision point, so go down the apropriate
	   sides find which side of the node we are on */
	plane = node->plane;

	switch (plane->type)
	{
		case PLANE_X:
			dot = modelorg[0] - plane->dist;
			break;
		case PLANE_Y:
			dot = modelorg[1] - plane->dist;
			break;
		case PLANE_Z:
			dot = modelorg[2] - plane->dist;
			break;
		default:
			dot = DotProduct(modelorg, plane->normal) - plane->dist;
			break;
	}

	if (dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	/* recurse down the children, front side first */
	R_RSX_Surf_recursiveWorldNode(currententity, node->children[side]);

	/* draw stuff */
	for (c = node->numsurfaces,
		 surf = gl3_worldmodel->surfaces + node->firstsurface;
		 c; c--, surf++)
	{
		if (surf->visframe != gl3_framecount)
		{
			continue;
		}

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
		{
			continue; /* wrong side */
		}

		if (surf->texinfo->flags & SURF_SKY)
		{
			/* just adds to visible sky bounds */
			R_RSX_Warp_AddSkySurface(surf);
		}
		else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
		{
			/* add to the translucent chain */
			surf->texturechain = gl3_alpha_surfaces;
			gl3_alpha_surfaces = surf;
			gl3_alpha_surfaces->texinfo->image = R_RSX_Surf_textureAnimation(currententity, surf->texinfo);
		}
		else
		{
			// calling R_RSX_Surf_renderLightmappedPoly() here probably isn't optimal, rendering everything
			// through texturechains should be faster, because far less glBindTexture() is needed
			// (and it might allow batching the drawcalls of surfaces with the same texture)
#if 0
			if(!(surf->flags & SURF_DRAWTURB))
			{
				R_RSX_Surf_renderLightmappedPoly(currententity, surf);
			}
			else
#endif // 0
			{
				/* the polygon is visible, so add it to the texture sorted chain */
				image = R_RSX_Surf_textureAnimation(currententity, surf->texinfo);
				surf->texturechain = image->texturechain;
				image->texturechain = surf;
			}
		}
	}

	/* recurse down the back side */
	R_RSX_Surf_recursiveWorldNode(currententity, node->children[!side]);
}

uint32_t getUTicks(void)
{
	static qboolean ticks_started = false;
	static struct timeval start;
	if (!ticks_started)
	{
		ticks_started = true;
		gettimeofday(&start, NULL);
	}

	struct timeval now;
	uint32_t ticks;

	gettimeofday(&now, NULL);
	ticks = (now.tv_sec - start.tv_sec) * 1000000 + (now.tv_usec - start.tv_usec);
	return ticks;
}

#if defined(WORLD_DRAW_SPEEDS)
extern unsigned long verts_drawn;
#endif

// 811
void
R_RSX_Surf_DrawWorld(void)
{
    // static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }

	entity_t ent;

	if (!r_drawworld->value)
	{
		return;
	}

	if (gl3_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		return;
	}

	VectorCopy(gl3_newrefdef.vieworg, modelorg);

	/* auto cycle the world frame for texture animation */
	memset(&ent, 0, sizeof(ent));
	ent.frame = (int)(gl3_newrefdef.time * 2);

	gl3state.currenttexture = NULL;

	R_RSX_Warp_ClearSkyBox();

#if defined(WORLD_DRAW_SPEEDS)
	uint32_t time_pre_rwn = getUTicks();

	rwn_calls = 0;
	R_RSX_Surf_recursiveWorldNode(&ent, gl3_worldmodel->nodes);
	uint32_t time_post_rwn = getUTicks();

	verts_drawn = 0;
	R_RSX_Surf_drawTextureChains(&ent);

	uint32_t time_post_dtc = getUTicks();

	float rwn_durration = time_post_rwn;
	rwn_durration -= time_pre_rwn;
	rwn_durration /= 1000;

	float dtc_durration = time_post_dtc;
	dtc_durration -= time_post_rwn;
	dtc_durration /= 1000;

	Com_Printf("ref_rsx::%s rwn (%d) %02.4f ms dtc %02.4f %ld\n", __func__, rwn_calls, rwn_durration, dtc_durration, verts_drawn);
#else // 0
	R_RSX_Surf_recursiveWorldNode(&ent, gl3_worldmodel->nodes);

	R_RSX_Surf_drawTextureChains(&ent);
#endif

	R_RSX_Warp_DrawSkyBox();

	// DrawTriangleOutlines();
	// This thing draws triangle borders in release code all of it's
	// code commented out
}

/* 842
 * Mark the leaves and nodes that are
 * in the PVS for the current cluster
 */
void
R_RSX_Surf_MarkLeaves(void)
{
	NOT_IMPLEMENTED();
	// const byte *vis;
	// YQ2_ALIGNAS_TYPE(int) byte fatvis[MAX_MAP_LEAFS / 8];
	// mnode_t *node;
	// int i, c;
	// mleaf_t *leaf;
	// int cluster;

	// if ((gl3_oldviewcluster == gl3_viewcluster) &&
	// 	(gl3_oldviewcluster2 == gl3_viewcluster2) &&
	// 	!r_novis->value &&
	// 	(gl3_viewcluster != -1))
	// {
	// 	return;
	// }

	// /* development aid to let you run around
	//    and see exactly where the pvs ends */
	// if (r_lockpvs->value)
	// {
	// 	return;
	// }

	// gl3_visframecount++;
	// gl3_oldviewcluster = gl3_viewcluster;
	// gl3_oldviewcluster2 = gl3_viewcluster2;

	// if (r_novis->value || (gl3_viewcluster == -1) || !gl3_worldmodel->vis)
	// {
	// 	/* mark everything */
	// 	for (i = 0; i < gl3_worldmodel->numleafs; i++)
	// 	{
	// 		gl3_worldmodel->leafs[i].visframe = gl3_visframecount;
	// 	}

	// 	for (i = 0; i < gl3_worldmodel->numnodes; i++)
	// 	{
	// 		gl3_worldmodel->nodes[i].visframe = gl3_visframecount;
	// 	}

	// 	return;
	// }

	// vis = GL3_Mod_ClusterPVS(gl3_viewcluster, gl3_worldmodel);

	// /* may have to combine two clusters because of solid water boundaries */
	// if (gl3_viewcluster2 != gl3_viewcluster)
	// {
	// 	memcpy(fatvis, vis, (gl3_worldmodel->numleafs + 7) / 8);
	// 	vis = GL3_Mod_ClusterPVS(gl3_viewcluster2, gl3_worldmodel);
	// 	c = (gl3_worldmodel->numleafs + 31) / 32;

	// 	for (i = 0; i < c; i++)
	// 	{
	// 		((int *)fatvis)[i] |= ((int *)vis)[i];
	// 	}

	// 	vis = fatvis;
	// }

	// for (i = 0, leaf = gl3_worldmodel->leafs;
	// 	 i < gl3_worldmodel->numleafs;
	// 	 i++, leaf++)
	// {
	// 	cluster = leaf->cluster;

	// 	if (cluster == -1)
	// 	{
	// 		continue;
	// 	}

	// 	if (vis[cluster >> 3] & (1 << (cluster & 7)))
	// 	{
	// 		node = (mnode_t *)leaf;

	// 		do
	// 		{
	// 			if (node->visframe == gl3_visframecount)
	// 			{
	// 				break;
	// 			}

	// 			node->visframe = gl3_visframecount;
	// 			node = node->parent;
	// 		}
	// 		while (node);
	// 	}
	// }
}
