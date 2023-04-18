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
 * Lightmap handling
 *
 * =======================================================================
 */

#include "header/local.h"

#define TEXNUM_LIGHTMAPS 1024

extern gl3lightmapstate_t gl3_lms;

void
R_RSX_LM_initBlock(void)
{
	memset(gl3_lms.allocated, 0, sizeof(gl3_lms.allocated));
}

/**
 * @brief Sets lightmaps 0..3 from set to fragment shader 
 *        texture indexes 1..4 accordingly 
 * 
 * @param lm_set_index 
 */
void
R_RSX_LM_SetLightMapSet(uint32_t lm_set_index)
{
	uint32_t last_lm_set_index = MAX_LIGHTMAPS;
	if (last_lm_set_index == lm_set_index) return;
	last_lm_set_index = lm_set_index;

	gcmTexture *lightmap;
	uint32_t lm_index;
	for(uint32_t lm_i = 0; lm_i < MAX_LIGHTMAPS_PER_SURFACE; ++lm_i)
	{
		// GL3_SelectTMU(GL_TEXTURE1+map); // this relies on GL_TEXTURE2 being GL_TEXTURE1+1 etc
		lm_index = lm_i + 1;
		lightmap = &(gl3_lms.lightmap_textures[lm_set_index][lm_i]); 

		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		rsxTextureFilter(gcmContext,
		                 lm_index, // index
		                 0, // bias
		                 GCM_TEXTURE_LINEAR, // min filter
		                 GCM_TEXTURE_LINEAR, // mag filter
		                 GCM_TEXTURE_CONVOLUTION_QUINCUNX);

		rsxLoadTexture(gcmContext, lm_index, lightmap);

		// Done it here not sure is needed
		rsxTextureControl(gcmContext, lm_index, GCM_TRUE, 0<<8, 12<<8, GCM_TEXTURE_MAX_ANISO_1);
		rsxTextureWrapMode(gcmContext, lm_index, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
	}
}

static inline void
R_RSX_LM_moveToNextSet(void)
{
	// This function barely plays the role of GL3_LM_UploadBlock.
	// Because we work with texture data directly there is no
	// such thing as "upload" something
	// So the only thing this function does is boundary check
	// for lightmap atlas sets

	if (++gl3_lms.current_lightmap_set == MAX_LIGHTMAPS)
	{
		ri.Sys_Error(ERR_DROP, "%s() - MAX_LIGHTMAPS exceeded\n", __func__);
	}
}

/*
 * returns a texture number and the position inside it
 */
qboolean
R_RSX_LM_allocBlock(int w, int h, int *x, int *y)
{
	int i, j;
	int best, best2;

	best = BLOCK_HEIGHT;

	for (i = 0; i < BLOCK_WIDTH - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (gl3_lms.allocated[i + j] >= best)
			{
				break;
			}

			if (gl3_lms.allocated[i + j] > best2)
			{
				best2 = gl3_lms.allocated[i + j];
			}
		}

		if (j == w)
		{
			/* this is a valid spot */
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > BLOCK_HEIGHT)
	{
		return false;
	}

	for (i = 0; i < w; i++)
	{
		gl3_lms.allocated[*x + i] = best + h;
	}

	return true;
}

void
R_RSX_LM_BuildPolygonFromSurface(gl3model_t *currentmodel, msurface_t *fa)
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec;
	float s, t;
	glpoly_t *poly;
	vec3_t total;
	vec3_t normal;

	/* reconstruct the polygon */
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	VectorClear(total);

	/* draw texture */
	poly = Hunk_Alloc(sizeof(glpoly_t) +
		   (lnumverts - 4) * sizeof(gl3_3D_vtx_t));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	VectorCopy(fa->plane->normal, normal);

	if(fa->flags & SURF_PLANEBACK)
	{
		// if for some reason the normal sticks to the back of the plane, invert it
		// so it's usable for the shader
		for (i=0; i<3; ++i)  normal[i] = -normal[i];
	}

	for (i = 0; i < lnumverts; i++)
	{
		gl3_3D_vtx_t* vert = &poly->vertices[i];

		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = currentmodel->vertexes[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = currentmodel->vertexes[r_pedge->v[1]].position;
		}

		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->image->width;

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->image->height;

		VectorAdd(total, vec, total);
		VectorCopy(vec, vert->pos);
		vert->texCoord[0] = s;
		vert->texCoord[1] = t;

		/* lightmap texture coordinates */
		s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH * 16; /* fa->texinfo->texture->width; */

		t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; /* fa->texinfo->texture->height; */

		vert->lmTexCoord[0] = s;
		vert->lmTexCoord[1] = t;

		VectorCopy(normal, vert->normal);
		vert->lightFlags = 0;
	}
}

void
R_RSX_LM_CreateSurfaceLightmap(msurface_t *surf)
{
	int smax, tmax;

	if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
	{
		return;
	}

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (!R_RSX_LM_allocBlock(smax, tmax, &surf->light_s, &surf->light_t))
	{
		R_RSX_LM_moveToNextSet();
		R_RSX_LM_initBlock();

		if (!R_RSX_LM_allocBlock(smax, tmax, &surf->light_s, &surf->light_t))
		{
			ri.Sys_Error(ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n",
					smax, tmax);
		}
	}

	surf->lightmaptexturenum = gl3_lms.current_lightmap_set;

	R_RSX_Light_BuildLightMap(surf, (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES, BLOCK_WIDTH * LIGHTMAP_BYTES);
}

// 233
void
R_RSX_LM_BeginBuildingLightmaps(gl3model_t *m)
{
	// lightstyles updates by client on every frame!
	// every msurface_t might have up to 4 lightstyles
	// ?and as less as 0 lightstyles?
	// lightstyle[i] is an animation describes
	// how lightmap affects certain surface at a time
	// values inside rgb may vary from 0.0 to 2.0.
	// lightmap colors will be multiplicated by them
	// white is a sum of rgb values, meanwhile all
	// values in rgb is allways equal to each other
	// in GL1 refresher white values of surface used to
	// check if surface lightmaps need to be reblanded
	// (caching as they call it)
	// (rebland must occur for dynamic lighted surface)
	static lightstyle_t lightstyles[MAX_LIGHTSTYLES];
	int i;

	memset(gl3_lms.allocated, 0, sizeof(gl3_lms.allocated));

	gl3_framecount = 1; /* no dlightcache */

	/* setup the base lightstyles so the lightmaps
	   won't have to be regenerated the first time
	   they're seen */
	// RSX NOTE: comment above is present in OpenGL 3 refresher
	// but it's contents is missleading as it left from OGL1.
	// In OGL3 lightmap textures blended (regenerated) directly
	// in fragment program. So do I in RSX refresher.
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		lightstyles[i].rgb[0] = 1;
		lightstyles[i].rgb[1] = 1;
		lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}

	gl3_newrefdef.lightstyles = lightstyles;

	gl3_lms.current_lightmap_set = 0;
	gl3_lms.internal_format = LIGHTMAP_FORMAT;

	// Note: the dynamic lightmap used to be initialized here, we don't use that anymore.	
	// RSX NOTE: I do use them. But their code is addition to lightmaps,
	// not a part of them. Initialized and handled separetly from static lightmaps
}

void
R_RSX_LM_EndBuildingLightmaps(void)
{
	// Since the lightmap textures are directly modified
	// there is no need to upload them as they already where
	// they should be.
}

void
R_RSX_LM_Init(void)
{
	gcmTexture *lightmap_texture;
	for (uint32_t lm_set = 0; lm_set < MAX_LIGHTMAPS; ++lm_set)
	{
		for (uint32_t lm_i = 0; lm_i < MAX_LIGHTMAPS_PER_SURFACE; ++lm_i)
		{
			lightmap_texture = &(gl3_lms.lightmap_textures[lm_set][lm_i]);

			// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			// Done at texture to TMU bind stage

			gl3_lms.internal_format = (GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN);

			// glTexImage2D(GL_TEXTURE_2D, 0, gl3_lms.internal_format,
			//              BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT,
			//              GL_UNSIGNED_BYTE, gl3_lms.lightmap_buffers[map]);
			lightmap_texture->format    = gl3_lms.internal_format;
			lightmap_texture->mipmap    = 1;
			lightmap_texture->dimension = GCM_TEXTURE_DIMS_2D;
			lightmap_texture->cubemap   = GCM_FALSE;

			/* remap done here because lightmap generation uses RGBA,
			* but RSX store with format ARGB,
			* but in shaders it is still reads RGBA
			*
			* A - R
			* R - G
			* G - B
			* B - A
			*/
			lightmap_texture->remap     = ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
			                               (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
			                               (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
			                               (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
			                               (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
			                               (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
			                               (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
			                               (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
			lightmap_texture->width     = BLOCK_WIDTH;
			lightmap_texture->height    = BLOCK_HEIGHT;
			lightmap_texture->depth     = 1;
			lightmap_texture->location  = GCM_LOCATION_RSX;
			lightmap_texture->pitch     = BLOCK_WIDTH * LIGHTMAP_BYTES;
			gl3_lms.lightmap_buffers_ptr[lm_set][lm_i] = rsxMemalign_with_log(128, (lightmap_texture->pitch * lightmap_texture->height));
			rsxAddressToOffset(gl3_lms.lightmap_buffers_ptr[lm_set][lm_i], &(lightmap_texture->offset));
		}
	}
}

void
R_RSX_LM_Shutdown(void)
{
	for (uint32_t lm_set = 0; lm_set < MAX_LIGHTMAPS; ++lm_set)
	{
		for (uint32_t lm_i = 0; lm_i < MAX_LIGHTMAPS_PER_SURFACE; ++lm_i)
		{
			rsxFree_with_log(gl3_lms.lightmap_buffers_ptr[lm_set][lm_i]);
		}
	}
}