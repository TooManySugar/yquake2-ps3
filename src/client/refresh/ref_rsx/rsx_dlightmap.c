/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2023 TooManySugar
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
 * Per surface dynamic lightmap handling
 *
 * =======================================================================
 */

#include "header/local.h"

#define DLIGHT_CUTOFF 64

typedef struct {

	void *buffers_ptrs[MAX_LIGHTMAPS];
	gcmTexture textures[MAX_LIGHTMAPS];

} r_rsx_dynlightmap_state_t;

r_rsx_dynlightmap_state_t rsx_dlms;

// same indexing as static lightmaps
void
R_RSX_DLM_LoadDynLightmapAtlas(uint32_t dlm_atlas_index)
{
	gcmTexture *dlm_atlas;

	dlm_atlas = &(rsx_dlms.textures[dlm_atlas_index]);

	rsxTextureFilter(
		gcmContext,
		5, // index
		0, // bias
		GCM_TEXTURE_LINEAR, // min filter
		GCM_TEXTURE_LINEAR, // mag filter
		GCM_TEXTURE_CONVOLUTION_QUINCUNX);

	rsxLoadTexture(gcmContext, 5, dlm_atlas);

	rsxTextureControl(
		gcmContext,
		5, // index
		GCM_TRUE,  // enable
		0<<8,
		12<<8,
		GCM_TEXTURE_MAX_ANISO_1);

	rsxTextureWrapMode(
		gcmContext,
		5, // index
		GCM_TEXTURE_CLAMP_TO_EDGE,
		GCM_TEXTURE_CLAMP_TO_EDGE,
		GCM_TEXTURE_CLAMP_TO_EDGE,
		0,
		GCM_TEXTURE_ZFUNC_LESS,
		0);
}

void
R_RSX_DLM_RecreateDynamicLightmapForSurface(msurface_t *surf)
{
	int lnum;
	int sd, td;
	float fdist, frad, fminlight;
	vec3_t impact, local;
	int s, t;
	int i, j;
	int smax, tmax, size, stride;
	int r, g, b, a, max;
	mtexinfo_t *tex;
	dlight_t *dl;
	float *pfBL;
	float fsacc, ftacc;
	uint8_t *dest;
	static float buf[34 * 34 * 3];

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	stride = BLOCK_WIDTH * LIGHTMAP_BYTES;
	// Cut off stride with width of surface lightmap
	stride -= (smax << 2);

	size = smax * tmax;
	tex = surf->texinfo;

	// begining of dynamic lightmap data of used by surface set plus offset in it
	// (dynamics use same cords as common lightmaps)
	dest = rsx_dlms.buffers_ptrs[surf->lightmaptexturenum]
		+ (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;

	if (surf->dlightframe != gl3_framecount)
	{
		// surface not affected by dynamic lights at least this frame
		// check is it's dyn lightmap clean
		int need_update = surf->dlights_affected;

		if (!need_update)
		{
			return;
		}

		// Fill space in dynamic lightmaps atlas with 0s
		for (i = 0; i < tmax; i++, dest += stride)
		{
			for (j = 0; j < smax; j++)
			{
				dest[0] = 0.0f;
				dest[1] = 0.0f;
				dest[2] = 0.0f;
				dest[3] = 0.0f;

				dest += 4;
			}
		}

		surf->dlights_affected = 0;

		return;
	}

	surf->dlights_affected = 1;

	// clear temp buffer with enough space to fit surface lightmap
	memset(buf, 0, sizeof(buf[0]) * size * 3);

	// Id's R_AddDynamicLights but without global scope variables
	for (lnum = 0; lnum < gl3_newrefdef.num_dlights; ++lnum)
	{
		if (!(surf->dlightbits & (1 << lnum)) && false)
		{
			continue; /* not lit by this light */
		}

		dl = &gl3_newrefdef.dlights[lnum];
		frad = dl->intensity;
		fdist = DotProduct(dl->origin, surf->plane->normal) - surf->plane->dist;
		frad -= fabs(fdist);

		/* rad is now the highest intensity on the plane */
		fminlight = DLIGHT_CUTOFF;

		if (frad < fminlight)
		{
			continue;
		}

		fminlight = frad - fminlight;

		for (i = 0; i < 3; i++)
		{
			impact[i] = dl->origin[i] - surf->plane->normal[i] * fdist;
		}

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		pfBL = buf;
		for (t = 0, ftacc = 0; t < tmax; t++, ftacc += 16)
		{
			td = local[1] - ftacc;

			if (td < 0)
			{
				td = -td;
			}

			for (s = 0, fsacc = 0; s < smax; s++, fsacc += 16, pfBL += 3)
			{
				sd = Q_ftol(local[0] - fsacc);

				if (sd < 0)
				{
					sd = -sd;
				}

				if (sd > td)
				{
					fdist = sd + (td >> 1);
				}
				else
				{
					fdist = td + (sd >> 1);
				}

				if (fdist < fminlight)
				{
					pfBL[0] += (frad - fdist) * dl->color[0];
					pfBL[1] += (frad - fdist) * dl->color[1];
					pfBL[2] += (frad - fdist) * dl->color[2];
				}
			}
		}
	}

	// Write temp buffer into space in dynamic lightmaps atlas
	pfBL = buf;
	for (i = 0; i < tmax; i++, dest += stride)
	{
		for (j = 0; j < smax; j++)
		{
			r = Q_ftol(pfBL[0]);
			g = Q_ftol(pfBL[1]);
			b = Q_ftol(pfBL[2]);

			/* catch negative lights */
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;

			/* determine the brightest of the three color components */
			if (r > g)
				max = r;
			else
				max = g;

			if (b > max)
				max = b;

			/* alpha is ONLY used for the mono lightmap case. For this
			   reason we set it to the brightest of the color components
			   so that things don't get too dim. */
			a = max;

			/* rescale all the color components if the
			   intensity of the greatest channel exceeds
			   1.0 */
			if (max > 255)
			{
				float t = 255.0f / max;

				r = r * t;
				g = g * t;
				b = b * t;
				a = a * t;
			}

			dest[0] = r;
			dest[1] = g;
			dest[2] = b;
			dest[3] = a;

			pfBL += 3;
			dest += 4;
		}
	}
}

void
R_RSX_DLM_Init(void)
{
	gcmTexture *texture;

	memset(&rsx_dlms, 0, sizeof(rsx_dlms));

	for (uint32_t dlm_index = 0; dlm_index < MAX_LIGHTMAPS; ++dlm_index)
	{
		texture = &(rsx_dlms.textures[dlm_index]);

		texture->format    = (GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN);
		texture->mipmap    = 1;
		texture->dimension = GCM_TEXTURE_DIMS_2D;
		texture->cubemap   = GCM_FALSE;

		/* remap done here because lightmap generation uses RGBA,
		 * but RSX store with format ARGB,
		 * but in shaders it is still reads RGBA
		 *
		 * A - R
		 * R - G
		 * G - B
		 * B - A
		 * */
		texture->remap     = ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
		                      (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
		                      (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
		                      (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
		                      (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
		                      (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
		                      (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
		                      (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
		// For now left dynamic lightmaps have same dimensions as statics
		// because they both use same coords in shader
		texture->width     = BLOCK_WIDTH;
		texture->height    = BLOCK_HEIGHT;
		texture->depth     = 1;
		texture->location  = GCM_LOCATION_RSX;
		texture->pitch     = BLOCK_WIDTH * LIGHTMAP_BYTES;

		rsx_dlms.buffers_ptrs[dlm_index] = rsxMemalign_with_log(128, BLOCK_WIDTH * BLOCK_HEIGHT * LIGHTMAP_BYTES);

		rsxAddressToOffset(rsx_dlms.buffers_ptrs[dlm_index], &(texture->offset));
	}
}

void
R_RSX_DLM_Shutdown()
{
	for (uint32_t dlm_index = 0; dlm_index < MAX_LIGHTMAPS; ++dlm_index)
	{
		if (rsx_dlms.buffers_ptrs[dlm_index] != NULL)
		{
			rsxFree_with_log(rsx_dlms.buffers_ptrs[dlm_index]);
		}
	}

	memset(&rsx_dlms, 0, sizeof(rsx_dlms));
}