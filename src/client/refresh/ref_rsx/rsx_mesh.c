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
 * Mesh handling
 *
 * =======================================================================
 */

#include "header/local.h"

#define DG_DYNARR_IMPLEMENTATION
#include "header/DG_dynarr.h"

#define NUMVERTEXNORMALS 162
#define SHADEDOT_QUANT 16

static float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../constants/anorms.h"
};

/* precalculated dot products for quantized angles */
static float r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
#include "../constants/anormtab.h"
};

typedef float vec4_t[4];
static vec4_t s_lerped[MAX_VERTS];
extern vec3_t lightspot;

typedef struct rsx_shadowinfo_s {
	vec3_t    lightspot;
	vec3_t    shadevector;
	dmdl_t*   paliashdr;
	entity_t* entity;
} rsx_shadowinfo_t;

DA_TYPEDEF(rsx_shadowinfo_t, ShadowInfoArray_t);
// collect all models casting shadows (each frame)
// to draw shadows last
static ShadowInfoArray_t shadowModels = {0};

DA_TYPEDEF(gl3_alias_vtx_t, AliasVtxArray_t);
DA_TYPEDEF(uint16_t, UShortArray_t);
// dynamic arrays to batch all the data of a model, so we can render a model in one draw call
static AliasVtxArray_t vtxBuf = {0};
static UShortArray_t idxBuf = {0};

// Ok. Here I use the same trick used in state.rsx_vertex_buffer
// Here buffer is monolith - used for vertexes and indices
// The thing is rsx itself not buffers any data before drawing it
// we need to insure that data valid until it's drawn. I came up
// with highly naive but still safe aproach - single time aligning
// buffer and looping overit "allocating" space in it's bounds
// this buffer overflows depending on amount of vertexes needs to be
// drawn. On basic levels with sane amount of enemies, loot, etc.
// this should be enough for data be valid until frame next to next
// (you can not just overwrite this buffer on every frame) if it's
// size not enough geometry glitches will be seen.
//
// In terms how it's working - not efficient. Yamagi's approach is
// to use dynamic arrays to convert all meshes to indexedarrays and
// drawcall on it, for every such operation used same dynamic arrays
// just cleared(not confuse with freed) everytime. As I can not
// insure moment then drawing data no longer needed I copy resulting
// data in segment of buffer mentioned above and providing it to RSX.

static void *rsx_mesh_mem_buffer = NULL;
static size_t rsx_mesh_mem_buffer_utilization = 0;
#define RSX_MESH_MEM_BUFFER_SIZE 2097152

// size - amount of bytes to take in buffer, pointer valid until next frame draw
//        don't need to free this memory in any way.
void *R_RSX_Mesh_memBufferAlloc(size_t size)
{
	size_t aligned_size;
	void *res;

	if (rsx_mesh_mem_buffer == NULL)
	{
		rsx_mesh_mem_buffer = rsxMemalign_with_log(128, RSX_MESH_MEM_BUFFER_SIZE);
		rsx_mesh_mem_buffer_utilization = 0;
	}

	aligned_size = size & ~127;
	if ((size & 127) != 0)
	{
		aligned_size += 128;
	}

	if (rsx_mesh_mem_buffer_utilization + aligned_size > RSX_MESH_MEM_BUFFER_SIZE)
	{
		rsx_mesh_mem_buffer_utilization = 0;

		if (aligned_size > RSX_MESH_MEM_BUFFER_SIZE)
		{
			Com_Printf("%s: can't allocate %ld bytes in mesh mem buffer!\n", __func__, aligned_size);
			return NULL;
		}
	}

	res = rsx_mesh_mem_buffer + rsx_mesh_mem_buffer_utilization;
	rsx_mesh_mem_buffer_utilization += aligned_size;

	return res;
}

// free rsx_mesh_mem_buffer
void R_RSX_Mesh_freeMemBuffer(void)
{
	if (rsx_mesh_mem_buffer != NULL)
	{
		rsxFree_with_log(rsx_mesh_mem_buffer);
		rsx_mesh_mem_buffer = NULL;
	}
}

void R_RSX_Mesh_Shutown(void)
{
	da_free(vtxBuf);
	da_free(idxBuf);

	da_free(shadowModels);

	R_RSX_Mesh_freeMemBuffer();
}

static void // 75
R_RSX_Mesh_lerpVerts(qboolean powerUpEffect, int nverts, dtrivertx_t *v, dtrivertx_t *ov,
		dtrivertx_t *verts, float *lerp, float move[3],
		float frontv[3], float backv[3])
{
	int i;

	if (powerUpEffect)
	{
		for (i = 0; i < nverts; i++, v++, ov++, lerp += 4)
		{
			float *normal = r_avertexnormals[verts[i].lightnormalindex];

			lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0] +
					  normal[0] * POWERSUIT_SCALE;
			lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1] +
					  normal[1] * POWERSUIT_SCALE;
			lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2] +
					  normal[2] * POWERSUIT_SCALE;
		}
	}
	else
	{
		for (i = 0; i < nverts; i++, v++, ov++, lerp += 4)
		{
			lerp[0] = move[0] + ov->v[0] * backv[0] + v->v[0] * frontv[0];
			lerp[1] = move[1] + ov->v[1] * backv[1] + v->v[1] * frontv[1];
			lerp[2] = move[2] + ov->v[2] * backv[2] + v->v[2] * frontv[2];
		}
	}
}

/* 107
 * Interpolates between two frames and origins
 */
static void
R_RSX_Mesh_drawAliasFrameLerp(dmdl_t *paliashdr, entity_t* entity, vec3_t shadelight)
{
	uint32_t type;
	float l;
	daliasframe_t *frame, *oldframe;
	dtrivertx_t *v, *ov, *verts;
	int *order;
	int count;
	float alpha;
	vec3_t move, delta, vectors[3];
	vec3_t frontv, backv;
	int i;
	int index_xyz;
	float backlerp = entity->backlerp;
	float frontlerp = 1.0 - backlerp;
	float *lerp;
	// draw without texture? used for quad damage effect etc, I think
	qboolean colorOnly = 0 != (entity->flags &
			(RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE |
			 RF_SHELL_HALF_DAM));

	// TODO: maybe we could somehow store the non-rotated normal and do the dot in shader?
	float* shadedots = r_avertexnormal_dots[((int)(entity->angles[1] *
				(SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
							  + entity->frame * paliashdr->framesize);
	verts = v = frame->verts;

	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
				+ entity->oldframe * paliashdr->framesize);
	ov = oldframe->verts;

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);

	if (entity->flags & RF_TRANSLUCENT)
	{
		alpha = entity->alpha * 0.666f;
	}
	else
	{
		alpha = 1.0;
	}

	R_RSX_Shaders_Load(&(gl3state.vs3Dalias));
	if (colorOnly)
	{
		// GL3_UseProgram(gl3state.si3DaliasColor.shaderProgram);
		R_RSX_Shaders_Load(&(gl3state.fs3DaliasColor));
	}
	else
	{
		// GL3_UseProgram(gl3state.si3Dalias.shaderProgram);
		R_RSX_Shaders_Load(&(gl3state.fs3Dalias));
	}


	if(gl3_colorlight->value == 0.0f)
	{
		float avg = 0.333333f * (shadelight[0]+shadelight[1]+shadelight[2]);
		shadelight[0] = shadelight[1] = shadelight[2] = avg;
	}

	/* move should be the delta back to the previous frame * backlerp */
	VectorSubtract(entity->oldorigin, entity->origin, delta);
	AngleVectors(entity->angles, vectors[0], vectors[1], vectors[2]);

	move[0] = DotProduct(delta, vectors[0]); /* forward */
	move[1] = -DotProduct(delta, vectors[1]); /* left */
	move[2] = DotProduct(delta, vectors[2]); /* up */

	VectorAdd(move, oldframe->translate, move);

	for (i = 0; i < 3; i++)
	{
		move[i] = backlerp * move[i] + frontlerp * frame->translate[i];

		frontv[i] = frontlerp * frame->scale[i];
		backv[i] = backlerp * oldframe->scale[i];
	}

	lerp = s_lerped[0];

	R_RSX_Mesh_lerpVerts(colorOnly, paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv);

	assert(sizeof(gl3_alias_vtx_t) == 9*sizeof(float));

	// all the triangle fans and triangle strips of this model will be converted to
	// just triangles: the vertices stay the same and are batched in vtxBuf,
	// but idxBuf will contain indices to draw them all as GL_TRIANGLE
	// this way there's only one draw call (and two glBufferData() calls)
	// instead of (at least) dozens. *greatly* improves performance.

	// so first clear out the data from last call to this function
	// (the buffers are static global so we don't have malloc()/free() for each rendered model)
	da_clear(vtxBuf);
	da_clear(idxBuf);

	while (1)
	{
		uint16_t nextVtxIdx = da_count(vtxBuf);

		/* get the vertex count and primitive type */
		count = *order++;

		if (!count)
		{
			break; /* done */
		}

		if (count < 0)
		{
			count = -count;

			// type = GL_TRIANGLE_FAN;
			type = GCM_TYPE_TRIANGLE_FAN;
		}
		else
		{
			// type = GL_TRIANGLE_STRIP;
			type = GCM_TYPE_TRIANGLE_STRIP;
		}

		gl3_alias_vtx_t* buf = da_addn_uninit(vtxBuf, count);

		if (colorOnly)
		{
			int i;
			for(i=0; i<count; ++i)
			{
				int j=0;
				gl3_alias_vtx_t* cur = &buf[i];
				index_xyz = order[2];
				order += 3;

				for(j=0; j<3; ++j)
				{
					cur->pos[j] = s_lerped[index_xyz][j];
					cur->color[j] = shadelight[j];
				}
				cur->color[3] = alpha;
			}
		}
		else
		{
			int i;
			for(i=0; i<count; ++i)
			{
				int j=0;
				gl3_alias_vtx_t* cur = &buf[i];
				/* texture coordinates come from the draw list */
				cur->texCoord[0] = ((float *) order)[0];
				cur->texCoord[1] = ((float *) order)[1];

				index_xyz = order[2];

				order += 3;

				/* normals and vertexes come from the frame list */
				// shadedots is set above according to rotation (around Z axis I think)
				// to one of 16 (SHADEDOT_QUANT) presets in r_avertexnormal_dots
				l = shadedots[verts[index_xyz].lightnormalindex];

				for(j=0; j<3; ++j)
				{
					cur->pos[j] = s_lerped[index_xyz][j];
					cur->color[j] = l * shadelight[j];
				}
				cur->color[3] = alpha;
			}
		}

		// translate triangle fan/strip to just triangle indices
		// if(type == GL_TRIANGLE_FAN)
		if (type == GCM_TYPE_TRIANGLE_FAN)
		{
			uint16_t i;
			for(i = 1; i < count-1; ++i)
			{
				uint16_t* add = da_addn_uninit(idxBuf, 3);

				add[0] = nextVtxIdx;
				add[1] = nextVtxIdx+i;
				add[2] = nextVtxIdx+i+1;
			}
		}
		else // triangle strip
		{
			uint16_t i;
			for(i = 1; i < count-2; i+=2)
			{
				// add two triangles at once, because the vertex order is different
				// for odd vs even triangles
				uint16_t* add = da_addn_uninit(idxBuf, 6);

				add[0] = nextVtxIdx + i-1;
				add[1] = nextVtxIdx + i;
				add[2] = nextVtxIdx + i+1;

				add[3] = nextVtxIdx + i;
				add[4] = nextVtxIdx + i+2;
				add[5] = nextVtxIdx + i+1;
			}
			// add remaining triangle, if any
			if (i < count-1)
			{
				uint16_t* add = da_addn_uninit(idxBuf, 3);

				add[0] = nextVtxIdx + i-1;
				add[1] = nextVtxIdx + i;
				add[2] = nextVtxIdx + i+1;
			}
		}
	}

	// GL3_BindVAO(gl3state.vaoAlias);
	// GL3_BindVBO(gl3state.vboAlias);

	uint32_t rsx_mem_offset;

	// glBufferData(GL_ARRAY_BUFFER, da_count(vtxBuf)*sizeof(gl3_alias_vtx_t), vtxBuf.p, GL_STREAM_DRAW);
	// GL3_BindEBO(gl3state.eboAlias);

	gl3_alias_vtx_t *vtxs = (gl3_alias_vtx_t*)R_RSX_Mesh_memBufferAlloc(sizeof(gl3_alias_vtx_t) * da_count(vtxBuf));
	memcpy(vtxs, vtxBuf.p, sizeof(gl3_alias_vtx_t) * da_count(vtxBuf));

	rsxAddressToOffset(&vtxs[0].pos, &rsx_mem_offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_POS, 0, rsx_mem_offset, sizeof(gl3_alias_vtx_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vtxs[0].texCoord, &rsx_mem_offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX0, 0, rsx_mem_offset, sizeof(gl3_alias_vtx_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vtxs[0].color, &rsx_mem_offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX1, 0, rsx_mem_offset, sizeof(gl3_alias_vtx_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	// glBufferData(GL_ELEMENT_ARRAY_BUFFER, da_count(idxBuf)*sizeof(GLushort), idxBuf.p, GL_STREAM_DRAW);
	// glDrawElements(GL_TRIANGLES, da_count(idxBuf), GL_UNSIGNED_SHORT, NULL);	

	// Set idxBuf
	uint16_t *idxs = (uint16_t *)R_RSX_Mesh_memBufferAlloc(sizeof(uint16_t) * da_count(idxBuf));
	memcpy(idxs, idxBuf.p, sizeof(uint16_t) * da_count(idxBuf));
	rsxAddressToOffset(idxs, &rsx_mem_offset);

	rsxDrawIndexArray(gcmContext,
	                  GCM_TYPE_TRIANGLES,
					  rsx_mem_offset,
					  da_count(idxBuf),
					  GCM_INDEX_TYPE_16B,
					  GCM_LOCATION_RSX);
}

static void // 328
R_RSX_Mesh_drawAliasShadow(rsx_shadowinfo_t* shadowInfo)
{
	uint32_t type;
	int *order;
	vec3_t point;
	float height = 0, lheight;
	int count;

	dmdl_t* paliashdr = shadowInfo->paliashdr;
	entity_t* entity = shadowInfo->entity;

	vec3_t shadevector;
	VectorCopy(shadowInfo->shadevector, shadevector);

	// all in this scope is to set s_lerped
	{
		daliasframe_t *frame, *oldframe;
		dtrivertx_t *v, *ov, *verts;
		float backlerp = entity->backlerp;
		float frontlerp = 1.0f - backlerp;
		vec3_t move, delta, vectors[3];
		vec3_t frontv, backv;
		int i;

		frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
								  + entity->frame * paliashdr->framesize);
		verts = v = frame->verts;

		oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames
					+ entity->oldframe * paliashdr->framesize);
		ov = oldframe->verts;

		/* move should be the delta back to the previous frame * backlerp */
		VectorSubtract(entity->oldorigin, entity->origin, delta);
		AngleVectors(entity->angles, vectors[0], vectors[1], vectors[2]);

		move[0] = DotProduct(delta, vectors[0]); /* forward */
		move[1] = -DotProduct(delta, vectors[1]); /* left */
		move[2] = DotProduct(delta, vectors[2]); /* up */

		VectorAdd(move, oldframe->translate, move);

		for (i = 0; i < 3; i++)
		{
			move[i] = backlerp * move[i] + frontlerp * frame->translate[i];

			frontv[i] = frontlerp * frame->scale[i];
			backv[i] = backlerp * oldframe->scale[i];
		}

		// false: don't extrude vertices for powerup - this means the powerup shell
		//  is not seen in the shadow, only the underlying model..
		R_RSX_Mesh_lerpVerts(false, paliashdr->num_xyz, v, ov, verts, s_lerped[0], move, frontv, backv);
	}

	lheight = entity->origin[2] - shadowInfo->lightspot[2];
	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds);
	height = -lheight + 0.1f;

	// GL1 uses alpha 0.5, but in GL3 0.3 looks better
	float color[4] = {0.0, 0, 0, 0.3};

	// draw the shadow in a single draw call, just like the model

	da_clear(vtxBuf);
	da_clear(idxBuf);

	while (1)
	{
		int i, j;
		uint16_t nextVtxIdx = da_count(vtxBuf);

		/* get the vertex count and primitive type */
		count = *order++;

		if (!count)
		{
			break; /* done */
		}

		if (count < 0)
		{
			count = -count;

			type = GCM_TYPE_TRIANGLE_FAN;
		}
		else
		{
			type = GCM_TYPE_TRIANGLE_STRIP;
		}

		gl3_alias_vtx_t* buf = da_addn_uninit(vtxBuf, count);

		for(i=0; i<count; ++i)
		{
			/* normals and vertexes come from the frame list */
			VectorCopy(s_lerped[order[2]], point);

			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;

			VectorCopy(point, buf[i].pos);

			for(j=0; j<4; ++j)  buf[i].color[j] = color[j];

			order += 3;
		}

		// translate triangle fan/strip to just triangle indices
		if(type == GCM_TYPE_TRIANGLE_FAN)
		{
			uint16_t i;
			for(i=1; i < count-1; ++i)
			{
				uint16_t* add = da_addn_uninit(idxBuf, 3);

				add[0] = nextVtxIdx;
				add[1] = nextVtxIdx+i;
				add[2] = nextVtxIdx+i+1;
			}
		}
		else // triangle strip
		{
			uint16_t i;
			for(i=1; i < count-2; i+=2)
			{
				// add two triangles at once, because the vertex order is different
				// for odd vs even triangles
				uint16_t* add = da_addn_uninit(idxBuf, 6);

				add[0] = nextVtxIdx + i-1;
				add[1] = nextVtxIdx + i;
				add[2] = nextVtxIdx + i+1;

				add[3] = nextVtxIdx + i;
				add[4] = nextVtxIdx + i+2;
				add[5] = nextVtxIdx + i+1;
			}
			// add remaining triangle, if any
			if(i < count-1)
			{
				uint16_t* add = da_addn_uninit(idxBuf, 3);

				add[0] = nextVtxIdx + i-1;
				add[1] = nextVtxIdx + i;
				add[2] = nextVtxIdx + i+1;
			}
		}
	}

	// GL3_BindVAO(gl3state.vaoAlias);
	// GL3_BindVBO(gl3state.vboAlias);

	uint32_t rsx_mem_offset;

	// glBufferData(GL_ARRAY_BUFFER, da_count(vtxBuf)*sizeof(gl3_alias_vtx_t), vtxBuf.p, GL_STREAM_DRAW);
	// GL3_BindEBO(gl3state.eboAlias);

	gl3_alias_vtx_t *vtxs = (gl3_alias_vtx_t*)R_RSX_Mesh_memBufferAlloc(sizeof(gl3_alias_vtx_t) * da_count(vtxBuf));
	memcpy(vtxs, vtxBuf.p, sizeof(gl3_alias_vtx_t) * da_count(vtxBuf));

	rsxAddressToOffset(&vtxs[0].pos, &rsx_mem_offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_POS, 0, rsx_mem_offset, sizeof(gl3_alias_vtx_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vtxs[0].texCoord, &rsx_mem_offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX0, 0, rsx_mem_offset, sizeof(gl3_alias_vtx_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	rsxAddressToOffset(&vtxs[0].color, &rsx_mem_offset);
	rsxBindVertexArrayAttrib(gcmContext, GCM_VERTEX_ATTRIB_TEX1, 0, rsx_mem_offset, sizeof(gl3_alias_vtx_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

	// glBufferData(GL_ELEMENT_ARRAY_BUFFER, da_count(idxBuf)*sizeof(GLushort), idxBuf.p, GL_STREAM_DRAW);
	// glDrawElements(GL_TRIANGLES, da_count(idxBuf), GL_UNSIGNED_SHORT, NULL);

	// Set idxBuf
	uint16_t *idxs = (uint16_t *)R_RSX_Mesh_memBufferAlloc(sizeof(uint16_t) * da_count(idxBuf));
	memcpy(idxs, idxBuf.p, sizeof(uint16_t) * da_count(idxBuf));
	rsxAddressToOffset(idxs, &rsx_mem_offset);

	rsxDrawIndexArray(gcmContext,
	                  GCM_TYPE_TRIANGLES,
					  rsx_mem_offset,
					  da_count(idxBuf),
					  GCM_INDEX_TYPE_16B,
					  GCM_LOCATION_RSX);
}

static qboolean // 489
R_RSX_Mesh_cullAliasModel(vec3_t bbox[8], entity_t *e)
{
	int i;
	vec3_t mins, maxs;
	dmdl_t *paliashdr;
	vec3_t vectors[3];
	vec3_t thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t angles;

	gl3model_t* model = e->model;

	paliashdr = (dmdl_t *)model->extradata;

	if ((e->frame >= paliashdr->num_frames) || (e->frame < 0))
	{
		R_Printf(PRINT_DEVELOPER, "%s %s: no such frame %d\n", __func__,
				model->name, e->frame);
		e->frame = 0;
	}

	if ((e->oldframe >= paliashdr->num_frames) || (e->oldframe < 0))
	{
		R_Printf(PRINT_DEVELOPER, "%s %s: no such oldframe %d\n", __func__,
				model->name, e->oldframe);
		e->oldframe = 0;
	}

	pframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
			e->frame * paliashdr->framesize);

	poldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames +
			e->oldframe * paliashdr->framesize);

	/* compute axially aligned mins and maxs */
	if (pframe == poldframe)
	{
		for (i = 0; i < 3; i++)
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i] * 255;

			oldmins[i] = poldframe->translate[i];
			oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255;

			if (thismins[i] < oldmins[i])
			{
				mins[i] = thismins[i];
			}
			else
			{
				mins[i] = oldmins[i];
			}

			if (thismaxs[i] > oldmaxs[i])
			{
				maxs[i] = thismaxs[i];
			}
			else
			{
				maxs[i] = oldmaxs[i];
			}
		}
	}

	/* compute a full bounding box */
	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		if (i & 1)
		{
			tmp[0] = mins[0];
		}
		else
		{
			tmp[0] = maxs[0];
		}

		if (i & 2)
		{
			tmp[1] = mins[1];
		}
		else
		{
			tmp[1] = maxs[1];
		}

		if (i & 4)
		{
			tmp[2] = mins[2];
		}
		else
		{
			tmp[2] = maxs[2];
		}

		VectorCopy(tmp, bbox[i]);
	}

	/* rotate the bounding box */
	VectorCopy(e->angles, angles);
	angles[YAW] = -angles[YAW];
	AngleVectors(angles, vectors[0], vectors[1], vectors[2]);

	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		VectorCopy(bbox[i], tmp);

		bbox[i][0] = DotProduct(vectors[0], tmp);
		bbox[i][1] = -DotProduct(vectors[1], tmp);
		bbox[i][2] = DotProduct(vectors[2], tmp);

		VectorAdd(e->origin, bbox[i], bbox[i]);
	}

	int p, f, aggregatemask = ~0;

	for (p = 0; p < 8; p++)
	{
		int mask = 0;

		for (f = 0; f < 4; f++)
		{
			float dp = DotProduct(frustum[f].normal, bbox[p]);

			if ((dp - frustum[f].dist) < 0)
			{
				mask |= (1 << f);
			}
		}

		aggregatemask &= mask;
	}

	if (aggregatemask)
	{
		return true;
	}

	return false;
}

void // 643
R_RSX_Mesh_DrawAliasModel(entity_t *entity)
{
	int i;
	dmdl_t *paliashdr;
	float an;
	vec3_t bbox[8];
	vec3_t shadelight;
	vec3_t shadevector;
	gl3image_t *skin;
	hmm_mat4 origProjViewMat = {0}; // use for left-handed rendering
	// used to restore ModelView matrix after changing it for this entities position/rotation
	hmm_mat4 origModelMat = {0};

	if (!(entity->flags & RF_WEAPONMODEL))
	{
		if (R_RSX_Mesh_cullAliasModel(bbox, entity))
		{
			return;
		}
	}

	if (entity->flags & RF_WEAPONMODEL)
	{
		if (gl_lefthand->value == 2)
		{
			return;
		}
	}

	gl3model_t* model = entity->model;
	paliashdr = (dmdl_t *)model->extradata;

	/* get lighting information */
	if (entity->flags &
		(RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED |
		 RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		VectorClear(shadelight);

		if (entity->flags & RF_SHELL_HALF_DAM)
		{
			shadelight[0] = 0.56;
			shadelight[1] = 0.59;
			shadelight[2] = 0.45;
		}

		if (entity->flags & RF_SHELL_DOUBLE)
		{
			shadelight[0] = 0.9;
			shadelight[1] = 0.7;
		}

		if (entity->flags & RF_SHELL_RED)
		{
			shadelight[0] = 1.0;
		}

		if (entity->flags & RF_SHELL_GREEN)
		{
			shadelight[1] = 1.0;
		}

		if (entity->flags & RF_SHELL_BLUE)
		{
			shadelight[2] = 1.0;
		}
	}
	else if (entity->flags & RF_FULLBRIGHT)
	{
		for (i = 0; i < 3; i++)
		{
			shadelight[i] = 1.0;
		}
	}
	else
	{
		R_RSX_Light_LightPoint(entity, entity->origin, shadelight);

		/* player lighting hack for communication back to server */
		if (entity->flags & RF_WEAPONMODEL)
		{
			/* pick the greatest component, which should be
			   the same as the mono value returned by software */
			if (shadelight[0] > shadelight[1])
			{
				if (shadelight[0] > shadelight[2])
				{
					r_lightlevel->value = 150 * shadelight[0];
				}
				else
				{
					r_lightlevel->value = 150 * shadelight[2];
				}
			}
			else
			{
				if (shadelight[1] > shadelight[2])
				{
					r_lightlevel->value = 150 * shadelight[1];
				}
				else
				{
					r_lightlevel->value = 150 * shadelight[2];
				}
			}
		}
	}

	if (entity->flags & RF_MINLIGHT)
	{
		for (i = 0; i < 3; i++)
		{
			if (shadelight[i] > 0.1)
			{
				break;
			}
		}

		if (i == 3)
		{
			shadelight[0] = 0.1;
			shadelight[1] = 0.1;
			shadelight[2] = 0.1;
		}
	}

	if (entity->flags & RF_GLOW)
	{
		/* bonus items will pulse with time */
		float scale;
		float min;

		scale = 0.1 * sin(gl3_newrefdef.time * 7);

		for (i = 0; i < 3; i++)
		{
			min = shadelight[i] * 0.8;
			shadelight[i] += scale;

			if (shadelight[i] < min)
			{
				shadelight[i] = min;
			}
		}
	}

	// Note: gl_overbrightbits are now applied in shader.

	/* ir goggles color override */
	if ((gl3_newrefdef.rdflags & RDF_IRGOGGLES) && (entity->flags & RF_IR_VISIBLE))
	{
		shadelight[0] = 1.0;
		shadelight[1] = 0.0;
		shadelight[2] = 0.0;
	}

	an = entity->angles[1] / 180 * M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize(shadevector);

	/* locate the proper data */
	c_alias_polys += paliashdr->num_tris;

	/* draw all the triangles */
	if (entity->flags & RF_DEPTHHACK)
	{
		/* hack the depth range to prevent view model from poking into walls */
		// glDepthRange(gl3depthmin, gl3depthmin + 0.3 * (gl3depthmax - gl3depthmin));
		rsxSetDepthBounds(gcmContext, gl3depthmin, gl3depthmin + 0.3 * (gl3depthmax - gl3depthmin));
	}

	if (entity->flags & RF_WEAPONMODEL)
	{
		extern hmm_mat4 R_RSX_MYgluPerspective(float fovy, float aspect, float zNear, float zFar);

		origProjViewMat = gl3state.uni3DData.transProjViewMat4;

		// render weapon with a different FOV (r_gunfov) so it's not distorted at high view FOV
		float screenaspect = (float)gl3_newrefdef.width / gl3_newrefdef.height;
		float dist = (r_farsee->value == 0) ? 4096.0f : 8192.0f;

		hmm_mat4 projMat;
		if (r_gunfov->value < 0)
		{
			projMat = R_RSX_MYgluPerspective(gl3_newrefdef.fov_y, screenaspect, 4, dist);
		}
		else
		{
			projMat = R_RSX_MYgluPerspective(r_gunfov->value, screenaspect, 4, dist);
		}

		if(gl_lefthand->value == 1.0F)
		{
			// to mirror gun so it's rendered left-handed, just invert X-axis column
			// of projection matrix
			for(int i=0; i<4; ++i)
			{
				projMat.Elements[0][i] = - projMat.Elements[0][i];
			}
			// GL3_UpdateUBO3D(); Note: GL3_RotateForEntity() will call this,no need to do it twice before drawing

			// glCullFace(GL_BACK);
			rsxSetCullFace(gcmContext, GCM_CULL_BACK);
		}
		// RSX NOTE: again here we need transpose matrix
		gl3state.uni3DData.transProjViewMat4 = HMM_Transpose(HMM_MultiplyMat4(projMat, gl3state.viewMat3D));
	}


	//glPushMatrix();
	origModelMat = gl3state.uni3DData.transModelMat4;

	entity->angles[PITCH] = -entity->angles[PITCH];
	R_RSX_RotateForEntity(entity);
	entity->angles[PITCH] = -entity->angles[PITCH];


	/* select skin */
	if (entity->skin)
	{
		skin = entity->skin; /* custom player skin */
	}
	else
	{
		if (entity->skinnum >= MAX_MD2SKINS)
		{
			skin = model->skins[0];
		}
		else
		{
			skin = model->skins[entity->skinnum];

			if (!skin)
			{
				skin = model->skins[0];
			}
		}
	}

	if (!skin)
	{
		skin = rsx_no_texture; /* fallback... */
	}

	// GL3_Bind(skin->texnum);
	R_RSX_Image_BindUni2DTex0(skin);

	if (entity->flags & RF_TRANSLUCENT)
	{
		// glEnable(GL_BLEND);
		rsxSetBlendEnable(gcmContext, GCM_TRUE);
	}


	if ((entity->frame >= paliashdr->num_frames) ||
		(entity->frame < 0))
	{
		R_Printf(PRINT_DEVELOPER, "%s %s: no such frame %d\n", __func__,
				model->name, entity->frame);
		entity->frame = 0;
		entity->oldframe = 0;
	}

	if ((entity->oldframe >= paliashdr->num_frames) ||
		(entity->oldframe < 0))
	{
		R_Printf(PRINT_DEVELOPER, "%s %s: no such oldframe %d\n", __func__,
				model->name, entity->oldframe);
		entity->frame = 0;
		entity->oldframe = 0;
	}

	R_RSX_Mesh_drawAliasFrameLerp(paliashdr, entity, shadelight);

	//glPopMatrix();
	gl3state.uni3DData.transModelMat4 = origModelMat;
	R_RSX_Shaders_UpdateUni3DData();
	// GL3_UpdateUBO3D();

	if (entity->flags & RF_WEAPONMODEL)
	{
		gl3state.uni3DData.transProjViewMat4 = origProjViewMat;
		R_RSX_Shaders_UpdateUni3DData();
		// GL3_UpdateUBO3D();
		if (gl_lefthand->value == 1.0F)
		{
			// glCullFace(GL_FRONT);
			rsxSetCullFace(gcmContext, GCM_CULL_FRONT);
		}
	}

	if (entity->flags & RF_TRANSLUCENT)
	{
		// glDisable(GL_BLEND);
		rsxSetBlendEnable(gcmContext, GCM_FALSE);
	}

	if (entity->flags & RF_DEPTHHACK)
	{
		// glDepthRange(gl3depthmin, gl3depthmax);
		rsxSetDepthBounds(gcmContext, gl3depthmin, gl3depthmax);
	}

	if (gl_shadows->value && gl3config.stencil && !(entity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL | RF_NOSHADOW)))
	{
		rsx_shadowinfo_t si = {0};
		VectorCopy(lightspot, si.lightspot);
		VectorCopy(shadevector, si.shadevector);
		si.paliashdr = paliashdr;
		si.entity = entity;

		da_push(shadowModels, si);
	}
}


void R_RSX_Mesh_ResetShadowAliasModels(void) // 949
{
    da_clear(shadowModels);
}

void R_RSX_Mesh_DrawAliasShadows(void) // 954
{
	size_t numShadowModels = da_count(shadowModels);
	if(numShadowModels == 0)
	{
		return;
	}

	//glPushMatrix();
	hmm_mat4 oldMat = gl3state.uni3DData.transModelMat4;

	// glEnable(GL_BLEND);
	rsxSetBlendEnable(gcmContext, GCM_TRUE);

	// GL3_UseProgram(gl3state.si3DaliasColor.shaderProgram);
	R_RSX_Shaders_Load(&(gl3state.vs3Dalias));
	R_RSX_Shaders_Load(&(gl3state.fs3DaliasColor));

	if (gl3config.stencil)
	{
		// RSX NOTE: I'm surprised how those functions are simillar

		// glEnable(GL_STENCIL_TEST);
		rsxSetStencilTestEnable(gcmContext, GCM_TRUE);

		// glStencilFunc(GL_EQUAL, 1, 2);
		rsxSetStencilFunc(gcmContext, GCM_SCULL_SFUNC_EQUAL, 1, 2);

		// glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
		rsxSetStencilOp(gcmContext, GCM_KEEP, GCM_KEEP, GCM_INCR);
	}

	for(size_t i=0; i<numShadowModels; ++i)
	{
		rsx_shadowinfo_t* si = &shadowModels.p[i]; // XXX da_getptr(shadowModels, i);
		entity_t* e = si->entity;

		/* don't rotate shadows on ungodly axes */
		//glTranslatef(e->origin[0], e->origin[1], e->origin[2]);
		//glRotatef(e->angles[1], 0, 0, 1);
		hmm_mat4 rotTransMat = HMM_Rotate(e->angles[1], HMM_Vec3(0, 0, 1));
		VectorCopy(e->origin, rotTransMat.Elements[3]);

		gl3state.uni3DData.transModelMat4 = HMM_Transpose(HMM_MultiplyMat4(oldMat, rotTransMat));
		R_RSX_Shaders_UpdateUni3DData();
		// GL3_UpdateUBO3D();

		R_RSX_Mesh_drawAliasShadow(si);
	}

	if (gl3config.stencil)
	{
		// glDisable(GL_STENCIL_TEST);
		rsxSetStencilTestEnable(gcmContext, GCM_FALSE);
	}

	// glDisable(GL_BLEND);
	rsxSetBlendEnable(gcmContext, GCM_FALSE);

	//glPopMatrix();
	gl3state.uni3DData.transModelMat4 = oldMat;
	// GL3_UpdateUBO3D();
	R_RSX_Shaders_UpdateUni3DData();
}
