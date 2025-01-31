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
 * Lightmaps and dynamic lighting
 *
 * =======================================================================
 */

#include "header/local.h"

extern gl3lightmapstate_t gl3_lms;

// byte *dyn_lights_pos_data;
// byte *dyn_lights_color_data;

// gcmTexture dyn_lights_pos_vec;
// gcmTexture dyn_lights_color_vec;

#define DLIGHT_CUTOFF 64

static int r_dlightframecount;
static vec3_t pointcolor;
static cplane_t *lightplane; /* used as shadow plane */
vec3_t lightspot;

/*
 * index: 5 - position, 6 - color
 */
// void 
// R_RSX_Light_setDataVec(const uint32_t index, gcmTexture *vec, const byte* data)
// {
// 	rsxInvalidateTextureCache(gcmContext, GCM_INVALIDATE_TEXTURE);

// 	vec->format		= (GCM_TEXTURE_FORMAT_W32_Z32_Y32_X32_FLOAT | GCM_TEXTURE_FORMAT_LIN);
// 	// PS3 doesn't like float textures :(

// 	// vec->format		= (GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN);
// 	vec->mipmap		= GCM_TRUE;
// 	vec->dimension	= GCM_TEXTURE_DIMS_1D;
// 	vec->cubemap	= GCM_FALSE;
// 	vec->remap		= ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
// 						   (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
// 						   (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
// 						   (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
// 						   (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
// 						   (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
// 						   (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
// 						   (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
// 	vec->width		= MAX_DLIGHTS;
// 	vec->height		= 1;
// 	vec->depth		= 1;
// 	vec->location	= GCM_LOCATION_RSX;
// 	vec->pitch		= MAX_DLIGHTS*4*4;
// 	gcmAddressToOffset(data, &(vec->offset));

// 	rsxTextureFilter(gcmContext, index, 0, GCM_TEXTURE_LINEAR, GCM_TEXTURE_NEAREST, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
// 	rsxTextureWrapMode(gcmContext, index, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
// }

void
R_RSX_Light_Init(void)
{
	// Previously I used textures to pass light data to shader but PS3 doesn't like float textures

	// uint32_t size = MAX_DLIGHTS * sizeof(hmm_vec4);

	// dyn_lights_pos_data = (byte *)rsxMemalign_with_log(128, size);

	// // RSX NOTE: Instead of single 2D texture with dimensions size x 2 I use two linear textures

	// R_RSX_Light_setDataVec(5, &dyn_lights_pos_vec, dyn_lights_pos_data);
	// Com_Printf("[Color] Allocated and set texture for dynamic lights pos with size %d bytes\n", size);

	// dyn_lights_color_data = (byte *)rsxMemalign_with_log(128, size);

	// R_RSX_Light_setDataVec(6, &dyn_lights_color_vec, dyn_lights_color_data);
	// Com_Printf("[Color] Allocated and set texture for dynamic lights color with size %d bytes\n", size);
}

void
R_RSX_Light_Shutdown(void)
{
	// rsxFree_with_log(dyn_lights_pos_data);
	// Com_Printf("[Color] Free'd data for dynimic lights pos\n");

	// rsxFree_with_log(dyn_lights_color_data);
	// Com_Printf("[Color] Free'd data for dynimic lights color\n");
}

void // bit: 1 << i for light number i, will be or'ed into msurface_t::dlightbits if surface is affected by this light
R_RSX_Light_MarkLights(dlight_t *light, int bit, mnode_t *node)
{
	cplane_t *splitplane;
	float dist;
	msurface_t *surf;
	int i;
	int sidebit;

	if (node->contents != -1)
	{
		return;
	}

	splitplane = node->plane;
	dist = DotProduct(light->origin, splitplane->normal) - splitplane->dist;

	if (dist > light->intensity - DLIGHT_CUTOFF)
	{
		R_RSX_Light_MarkLights(light, bit, node->children[0]);
		return;
	}

	if (dist < -light->intensity + DLIGHT_CUTOFF)
	{
		R_RSX_Light_MarkLights(light, bit, node->children[1]);
		return;
	}

	/* mark the polygons */
	surf = gl3_worldmodel->surfaces + node->firstsurface;

	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}

		dist = DotProduct(light->origin, surf->plane->normal) - surf->plane->dist;

		if (dist >= 0)
		{
			sidebit = 0;
		}
		else
		{
			sidebit = SURF_PLANEBACK;
		}

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
		{
			continue;
		}

		surf->dlightbits |= bit;
	}

	R_RSX_Light_MarkLights(light, bit, node->children[0]);
	R_RSX_Light_MarkLights(light, bit, node->children[1]);
}

void
R_RSX_Light_PushDynLights(void)
{
	int i;
	dlight_t *l;

	/* because the count hasn't advanced yet for this frame */
	r_dlightframecount = gl3_framecount + 1;

	l = gl3_newrefdef.dlights;

	for (i = 0; i < gl3_newrefdef.num_dlights; i++, l++)
	{
		R_RSX_Light_MarkLights(l, 1 << i, gl3_worldmodel->nodes);
	}

	assert(MAX_DLIGHTS == 32 && "If MAX_DLIGHTS changes... Well if it's leq to 32 just change value here, if it's more... change whole dlightbits mechanism");
	// RSX NOTE: I really wish to use G-buffers instead
}

static int
R_RSX_Light_recursiveLightPoint(mnode_t *node, vec3_t start, vec3_t end)
{
	float front, back, frac;
	int side;
	cplane_t *plane;
	vec3_t mid;
	msurface_t *surf;
	int s, t, ds, dt;
	int i;
	mtexinfo_t *tex;
	byte *lightmap;
	int maps;
	int r;

	if (node->contents != -1)
	{
		return -1;     /* didn't hit anything */
	}

	/* calculate mid point */
	plane = node->plane;
	front = DotProduct(start, plane->normal) - plane->dist;
	back = DotProduct(end, plane->normal) - plane->dist;
	side = front < 0;

	if ((back < 0) == side)
	{
		return R_RSX_Light_recursiveLightPoint(node->children[side], start, end);
	}

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	/* go down front side */
	r = R_RSX_Light_recursiveLightPoint(node->children[side], start, mid);

	if (r >= 0)
	{
		return r;     /* hit something */
	}

	if ((back < 0) == side)
	{
		return -1;     /* didn't hit anuthing */
	}

	/* check for impact on this node */
	VectorCopy(mid, lightspot);
	lightplane = plane;

	surf = gl3_worldmodel->surfaces + node->firstsurface;

	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
		{
			continue; /* no lightmaps */
		}

		tex = surf->texinfo;

		s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

		if ((s < surf->texturemins[0]) ||
			(t < surf->texturemins[1]))
		{
			continue;
		}

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if ((ds > surf->extents[0]) || (dt > surf->extents[1]))
		{
			continue;
		}

		if (!surf->samples)
		{
			return 0;
		}

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		VectorCopy(vec3_origin, pointcolor);

		lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

		for (maps = 0; maps < MAX_LIGHTMAPS_PER_SURFACE && surf->styles[maps] != 255;
			 maps++)
		{
			const float *rgb;
			int j;

			rgb = gl3_newrefdef.lightstyles[surf->styles[maps]].rgb;

			/* Apply light level to models */
			for (j = 0; j < 3; j++)
			{
				float	scale;

				scale = rgb[j] * r_modulate->value;
				pointcolor[j] += lightmap[j] * scale * (1.0 / 255);
			}

			lightmap += 3 * ((surf->extents[0] >> 4) + 1) *
						((surf->extents[1] >> 4) + 1);
		}

		return 1;
	}

	/* go down back side */
	return R_RSX_Light_recursiveLightPoint(node->children[!side], mid, end);
}

void
R_RSX_Light_LightPoint(entity_t *currententity, vec3_t p, vec3_t color)
{
	vec3_t end;
	float r;
	int lnum;
	dlight_t *dl;
	vec3_t dist;
	float add;

	if (!gl3_worldmodel->lightdata || !currententity)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	// TODO: don't just aggregate the color, but also save position of brightest+nearest light
	//       for shadow position and maybe lighting on model?

	r = R_RSX_Light_recursiveLightPoint(gl3_worldmodel->nodes, p, end);

	if (r == -1)
	{
		VectorCopy(vec3_origin, color);
	}
	else
	{
		VectorCopy(pointcolor, color);
	}

	/* add dynamic lights */
	dl = gl3_newrefdef.dlights;

	for (lnum = 0; lnum < gl3_newrefdef.num_dlights; lnum++, dl++)
	{
		VectorSubtract(currententity->origin,
				dl->origin, dist);
		add = dl->intensity - VectorLength(dist);
		add *= (1.0f / 256.0f);

		if (add > 0)
		{
			VectorMA(color, add, dl->color, color);
		}
	}

	VectorScale(color, r_modulate->value, color);
}


/*
 * Combine and scale multiple lightmaps into the floating format in blocklights
 */
void
R_RSX_Light_BuildLightMap(msurface_t *surf, int offsetInLMbuf, int stride)
{
	int smax, tmax;
	int r, g, b, a, max;
	int i, j, size, map, numMaps;
	byte *lightmap;

	if (surf->texinfo->flags &
		(SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP))
	{
		ri.Sys_Error(ERR_DROP, "%s called for non-lit surface", __func__);
	}

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;

	stride -= (smax << 2);

	if (size > 34*34*3)
	{
		ri.Sys_Error(ERR_DROP, "Bad s_blocklights size");
	}

	// count number of lightmaps surf actually has
	for (numMaps = 0; numMaps < MAX_LIGHTMAPS_PER_SURFACE && surf->styles[numMaps] != 255; ++numMaps)
	{}

	if (!surf->samples)
	{
		// no lightmap samples? set at least one lightmap to fullbright, rest to 0 as normal

		if (numMaps == 0)  numMaps = 1; // make sure at least one lightmap is set to fullbright

		for (map = 0; map < MAX_LIGHTMAPS_PER_SURFACE; ++map)
		{
			// we always create 4 (MAX_LIGHTMAPS_PER_SURFACE) lightmaps.
			// if surf has less (numMaps < 4), the remaining ones are zeroed out.
			// this makes sure that all 4 lightmap textures in gl3state.lightmap_textureIDs[i] have the same layout
			// and the shader can use the same texture coordinates for all of them

			int c = (map < numMaps) ? 255 : 0;
			byte* dest = gl3_lms.lightmap_buffers_ptr[gl3_lms.current_lightmap_set][map] + offsetInLMbuf;

			for (i = 0; i < tmax; i++, dest += stride)
			{
				memset(dest, c, 4*smax);
				dest += 4*smax;
			}
		}

		return;
	}

	/* add all the lightmaps */

	// Note: dynamic lights aren't handled here anymore, they're handled in the shader

	// as we don't apply scale here anymore, nor blend the numMaps lightmaps together,
	// the code has gotten a lot easier and we can copy directly from surf->samples to dest
	// without converting to float first etc

	lightmap = surf->samples;

	for(map=0; map<numMaps; ++map)
	{
		byte* dest = gl3_lms.lightmap_buffers_ptr[gl3_lms.current_lightmap_set][map] + offsetInLMbuf;
		int idxInLightmap = 0;
		for (i = 0; i < tmax; i++, dest += stride)
		{
			for (j = 0; j < smax; j++)
			{
				r = lightmap[idxInLightmap * 3 + 0];
				g = lightmap[idxInLightmap * 3 + 1];
				b = lightmap[idxInLightmap * 3 + 2];

				/* determine the brightest of the three color components */
				if (r > g)  max = r;
				else  max = g;

				if (b > max)  max = b;

				/* alpha is ONLY used for the mono lightmap case. For this
				   reason we set it to the brightest of the color components
				   so that things don't get too dim. */
				a = max;

				dest[0] = r;
				dest[1] = g;
				dest[2] = b;
				dest[3] = a;

				dest += 4;
				++idxInLightmap;
			}
		}

		lightmap += size * 3; /* skip to next lightmap */
	}

	for ( ; map < MAX_LIGHTMAPS_PER_SURFACE; ++map)
	{
		// like above, fill up remaining lightmaps with 0

		byte* dest = gl3_lms.lightmap_buffers_ptr[gl3_lms.current_lightmap_set][map] + offsetInLMbuf;

		for (i = 0; i < tmax; i++, dest += stride)
		{
			memset(dest, 0, 4*smax);
			dest += 4*smax;
		}
	}
}
