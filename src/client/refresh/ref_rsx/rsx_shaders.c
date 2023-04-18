/*
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
 * TODO: Desc
 *
 * =======================================================================
 */

#include "ctype.h"

#include "rsx/rsx_program.h"
#include "rsx/gcm_sys.h"

#include "header/local.h"

#include "shaders/autogen_headers/vertexSrc2Dtexture_vpo.h"
#include "shaders/autogen_headers/fragmentSrc2Dtexture_fpo.h"
#include "shaders/autogen_headers/vertexSrc2Dcolor_vpo.h"
#include "shaders/autogen_headers/fragmentSrc2Dcolor_fpo.h"
#include "shaders/autogen_headers/vertexSrc3D_vpo.h"
#include "shaders/autogen_headers/vertex3Dflow_vpo.h"
#include "shaders/autogen_headers/vertexSrc3Dlm_vpo.h"
#include "shaders/autogen_headers/vertex3DLmFlow_vpo.h"
#include "shaders/autogen_headers/fragmentSrc3Dsky_fpo.h"
#include "shaders/autogen_headers/fragmentSrc3Dlm_fpo.h"

#include "shaders/autogen_headers/vertexSrc3DAlias_vpo.h"
#include "shaders/autogen_headers/fragmentSrc3DAlias_fpo.h"
#include "shaders/autogen_headers/fragmentSrc3DaliasColor_fpo.h"

#include "shaders/autogen_headers/fragmentSrc3D_fpo.h"
#include "shaders/autogen_headers/fragmentSrc3Dcolor_fpo.h"

#include "shaders/autogen_headers/vertexSrcParticles_vpo.h"
#include "shaders/autogen_headers/fragmentSrcParticlesSquare_fpo.h"
#include "shaders/autogen_headers/fragmentSrcParticles_fpo.h"

#include "shaders/autogen_headers/fragmentSrc3Dwater_fpo.h"

// Important: rsx* functions fills some kind of queue the main goal is to
// minimize it's fillment.
// Full queue leads to await for space on every call to rsx* functions

void
R_RSX_Shaders_loadVPO(const rsxVertexProgram* program, const void* ucode)
{
	static const rsxVertexProgram* loaded_vpo = NULL;
	if (loaded_vpo == program) return;

	rsxLoadVertexProgram(gcmContext, program, ucode);
	loaded_vpo = program;
}

void
R_RSX_Shaders_loadFPO(const rsxFragmentProgram *program, uint32_t offset)
{
	static const rsxFragmentProgram* loaded_fpo = NULL;
	if (loaded_fpo == program) return;

	rsxLoadFragmentProgramLocation(gcmContext, program, offset, GCM_LOCATION_RSX);
	loaded_fpo = program;
}

void
R_RSX_Shaders_Load(const R_RSX_Shaders_universalWrapper_t* shader)
{
	if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
	{
		R_RSX_Shaders_loadVPO(shader->description.vertex.vpo, shader->description.vertex.vp_ucode);
		return;
	}
	else if (shader->type == R_RSX_SHADER_TYPE_FRAGMENT)
	{
		R_RSX_Shaders_loadFPO((shader->description.fragment.fpo), shader->description.fragment.fp_offset);
		return;
	}

	Com_Printf("ref_rsx::%s: ERROR invalid shader type - %d\n", __func__, shader->type);
}

void
R_RSX_Shaders_UpdateUniCommonDataOnShader(R_RSX_Shaders_universalWrapper_t* shader)
{
	if (shader->type != R_RSX_SHADER_TYPE_FRAGMENT && shader->type != R_RSX_SHADER_TYPE_VERTEX)
	{
		Com_Printf("ref_rsx::%s: ERROR invalid shader type - %d\n", __func__, shader->type);
		return;
	}

	if (shader->binds.uniCommon.gamma != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_VERTEX for gamma");
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uniCommon.gamma,
				(float*)&gl3state.uniCommonData.gamma,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}

	if (shader->binds.uniCommon.intensity != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_VERTEX for intensity");
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
			    shader->description.fragment.fpo,
			    shader->binds.uniCommon.intensity,
			    (float*)&gl3state.uniCommonData.intensity,
			    shader->description.fragment.fp_offset,
			    GCM_LOCATION_RSX);
		}
	}

	if (shader->binds.uniCommon.intensity2D != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_VERTEX for intensity2D");
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uniCommon.intensity2D,
				(float*)&gl3state.uniCommonData.intensity2D,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}

	if (shader->binds.uniCommon.color != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_VERTEX for color");
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uniCommon.color,
				(float*)&gl3state.uniCommonData.color,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}
}

void
R_RSX_Shaders_updateUni2DDataOnShader(R_RSX_Shaders_universalWrapper_t* shader)
{
	if (shader->type != R_RSX_SHADER_TYPE_FRAGMENT && shader->type != R_RSX_SHADER_TYPE_VERTEX)
	{
		Com_Printf("ref_rsx::%s: ERROR invalid shader type - %d\n", __func__, shader->type);
		return;
	}

	if (shader->binds.uni2D.transMat != NULL)
	{
		// if (memcmp(&(shader->state.uni2D.transMat), &(gl3state.uni2DData.transMat4), sizeof(hmm_mat4)) == 0)
		// {
		// 	Com_Printf(".uni2D.transMat equals\n");
		// }

		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			rsxSetVertexProgramParameter(
				gcmContext,
				(rsxVertexProgram *)(shader->description.vertex.vpo), 
				(rsxProgramConst *)(shader->binds.uni2D.transMat),
				(float*)&gl3state.uni2DData.transMat4);
		}
		else
		{
			// fragment shaders dont really use trans model matrix
			NOT_IMPLEMENTED();
		}

		// memcpy(&(shader->state.uni2D.transMat), &(gl3state.uni2DData.transMat4), sizeof(hmm_mat4));
	}
}

void
R_RSX_Shaders_updateUni3DDataOnShader(R_RSX_Shaders_universalWrapper_t* shader)
{
	if (shader->type != R_RSX_SHADER_TYPE_FRAGMENT && shader->type != R_RSX_SHADER_TYPE_VERTEX)
	{
		Com_Printf("ref_rsx::%s: ERROR invalid shader type - %d\n", __func__, shader->type);
		return;
	}

	if (shader->binds.uni3D.transProjView != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			rsxSetVertexProgramParameter(
				gcmContext,
				(rsxVertexProgram *)(shader->description.vertex.vpo), 
				(rsxProgramConst *)(shader->binds.uni3D.transProjView),
				(float*)&gl3state.uni3DData.transProjViewMat4);
		}
		else
		{
			// fragment shaders dont really use transProjView
			NOT_IMPLEMENTED();
		}
	}

	if (shader->binds.uni3D.transModel != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			rsxSetVertexProgramParameter(
				gcmContext,
				(rsxVertexProgram *)(shader->description.vertex.vpo), 
				(rsxProgramConst *)(shader->binds.uni3D.transModel),
				(float*)&gl3state.uni3DData.transModelMat4);
		}
		else
		{
			// fragment shaders dont really use transProjView
			NOT_IMPLEMENTED();
		}
	}

	if (shader->binds.uni3D.scroll != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			rsxSetVertexProgramParameter(
				gcmContext,
				(rsxVertexProgram *)(shader->description.vertex.vpo), 
				(rsxProgramConst *)(shader->binds.uni3D.scroll),
				(float*)&gl3state.uni3DData.scroll);
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uni3D.scroll,
				(float*)&gl3state.uni3DData.scroll,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}

	if (shader->binds.uni3D.time != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_VERTEX");
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uni3D.time,
				(float*)&gl3state.uni3DData.time,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}

	if (shader->binds.uni3D.alpha != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_VERTEX");
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uni3D.alpha,
				(float*)&gl3state.uni3DData.alpha,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}

	if (shader->binds.uni3D.overbrightbits != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			rsxSetVertexProgramParameter(
				gcmContext,
				(rsxVertexProgram *)(shader->description.vertex.vpo), 
				(rsxProgramConst *)(shader->binds.uni3D.overbrightbits),
				(float*)&gl3state.uni3DData.overbrightbits);
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uni3D.overbrightbits,
				(float*)&gl3state.uni3DData.overbrightbits,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}

	if (shader->binds.uni3D.particleFadeFactor != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_VERTEX");
		}
		else
		{
			rsxSetFragmentProgramParameter(
				gcmContext,
				shader->description.fragment.fpo,
				shader->binds.uni3D.particleFadeFactor,
				(float*)&gl3state.uni3DData.particleFadeFactor,
				shader->description.fragment.fp_offset,
				GCM_LOCATION_RSX);
		}
	}
}

void
R_RSX_Shaders_updateLmScalesOnShader(const hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE], R_RSX_Shaders_universalWrapper_t* shader)
{
	if (shader->type != R_RSX_SHADER_TYPE_FRAGMENT && shader->type != R_RSX_SHADER_TYPE_VERTEX)
	{
		Com_Printf("ref_rsx::%s: ERROR invalid shader type - %d\n", __func__, shader->type);
		return;
	}

	// only fragment shaders use lightmap scales
	if (shader->type != R_RSX_SHADER_TYPE_FRAGMENT)
	{
		return;
	}

	int i;
	qboolean hasChanged = false;

	for(i = 0; i < MAX_LIGHTMAPS_PER_SURFACE; ++i)
	{
		if(hasChanged)
		{
			shader->state.uniMisc.lmScales[i] = lmScales[i];
		}
		else if(   shader->state.uniMisc.lmScales[i].R != lmScales[i].R
		        || shader->state.uniMisc.lmScales[i].G != lmScales[i].G
		        || shader->state.uniMisc.lmScales[i].B != lmScales[i].B
		        || shader->state.uniMisc.lmScales[i].A != lmScales[i].A )
		{
			shader->state.uniMisc.lmScales[i] = lmScales[i];
			hasChanged = true;
		}
	}

	if (!hasChanged)
	{
		return;
	}

	for (i = 0; i < MAX_LIGHTMAPS_PER_SURFACE; ++i)
	{
		rsxSetFragmentProgramParameter(
			gcmContext,
			shader->description.fragment.fpo,
			shader->binds.uniMisc.lmScales[i],
			(float*)&lmScales[i],
			shader->description.fragment.fp_offset,
			GCM_LOCATION_RSX);
	}
}

void
R_RSX_Shaders_updateScreenResolutionOnShader(R_RSX_Shaders_universalWrapper_t* shader)
{
	if (shader->type != R_RSX_SHADER_TYPE_FRAGMENT && shader->type != R_RSX_SHADER_TYPE_VERTEX)
	{
		Com_Printf("ref_rsx::%s: ERROR invalid shader type - %d\n", __func__, shader->type);
		return;
	}

	if (shader->binds.uniMisc.screenRes != NULL)
	{
		if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
		{
			float sr[2] = {gl3_newrefdef.width, gl3_newrefdef.height}; 

			rsxSetVertexProgramParameter(
				gcmContext,
				(rsxVertexProgram *)(shader->description.vertex.vpo), 
				(rsxProgramConst *)(shader->binds.uniMisc.screenRes),
				(float*)&sr);
		}
		else
		{
			NOT_IMPLEMENTED_TEXT("shader->type == R_RSX_SHADER_TYPE_FRAGMENT");
		}
	}
}

uint32_t
R_RSX_Shaders_basicInitVertex(R_RSX_Shaders_universalWrapper_t* shader, rsxVertexProgram* vpo)
{

	memset(shader, 0, sizeof(*shader));

	shader->type = R_RSX_SHADER_TYPE_VERTEX;

	// size of shader in bytes
	uint32_t vpsize;
	shader->description.vertex.vpo = vpo;
	rsxVertexProgramGetUCode(shader->description.vertex.vpo, &(shader->description.vertex.vp_ucode), &vpsize);

	return vpsize;
}

uint32_t
R_RSX_Shaders_basicInitFragment(R_RSX_Shaders_universalWrapper_t* shader, rsxFragmentProgram* fpo)
{
	memset(shader, 0, sizeof(*shader));

	shader->type = R_RSX_SHADER_TYPE_FRAGMENT;
	
	shader->description.fragment.fpo = fpo;

	uint32_t fpsize;
	rsxFragmentProgramGetUCode(shader->description.fragment.fpo, &(shader->description.fragment.fp_ucode), &fpsize);

	shader->description.fragment.fp_buffer = (uint32_t*)rsxMemalign_with_log(64, fpsize);
	memcpy(shader->description.fragment.fp_buffer, (const void*)(shader->description.fragment.fp_ucode), fpsize);
	rsxAddressToOffset(shader->description.fragment.fp_buffer, &(shader->description.fragment.fp_offset));

	return fpsize;
}

void
R_RSX_Shaders_shutdownShader(R_RSX_Shaders_universalWrapper_t* shader)
{
	if (shader->type != R_RSX_SHADER_TYPE_FRAGMENT && shader->type != R_RSX_SHADER_TYPE_VERTEX)
	{
		Com_Printf("ref_rsx::%s: ERROR invalid shader type - %d\n", __func__, shader->type);
		return;
	}

	if (shader->type == R_RSX_SHADER_TYPE_VERTEX)
	{
		// vertex shaders dont do any memory allocations
	}
	else
	{
		if (shader->description.fragment.fp_buffer != NULL)
		{
			rsxFree_with_log(shader->description.fragment.fp_buffer);
			shader->description.fragment.fp_buffer = NULL;
		}
	}
}

// 2D Vertex Color
void
R_RSX_Shaders_initVs2Dcolor(void)
{
	Com_Printf("[Shader] Vertex 2D Color\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vs2DColor),
		(rsxVertexProgram*)vertexSrc2Dcolor_vpo);

	gl3state.vs2DColor.binds.uni2D.transMat = rsxVertexProgramGetConst(
		gl3state.vs2DColor.description.vertex.vpo,
		"transModel");
}

// 2D Fragment Color
void
R_RSX_Shaders_initFs2Dcolor(void)
{
	Com_Printf("[Shader] Fragment 2D Color\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs2DColor),
		(rsxFragmentProgram*)fragmentSrc2Dcolor_fpo);

	gl3state.fs2DColor.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs2DColor.description.fragment.fpo,
		"gamma");

	gl3state.fs2DColor.binds.uniCommon.intensity2D = rsxFragmentProgramGetConst(
		gl3state.fs2DColor.description.fragment.fpo,
		"intensity2D");

	gl3state.fs2DColor.binds.uniCommon.color = rsxFragmentProgramGetConst(
		gl3state.fs2DColor.description.fragment.fpo,
		"color");
}

// 2D Vertex Texture
void 
R_RSX_Shaders_initVs2Dtexture(void)
{
	Com_Printf("[Shader] Vertex 2D Texture\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vs2DTexture),
		(rsxVertexProgram*)vertexSrc2Dtexture_vpo);

	gl3state.vs2DTexture.binds.uni2D.transMat = rsxVertexProgramGetConst(
		gl3state.vs2DTexture.description.vertex.vpo,
		"transModel");
}

// 2D Fragment Texture
void
R_RSX_Shaders_initFs2Dtexture(void)
{
	Com_Printf("[Shader] Fragment 2D Texture\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs2DTexture),
		(rsxFragmentProgram*)fragmentSrc2Dtexture_fpo);

	gl3state.fs2DTexture.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs2DTexture.description.fragment.fpo,
		"gamma");

	gl3state.fs2DTexture.binds.uniCommon.intensity2D = rsxFragmentProgramGetConst(
		gl3state.fs2DTexture.description.fragment.fpo,
		"intensity2D");

	// Insure texture at index 0
	rsxProgramAttrib* texture = rsxFragmentProgramGetAttrib(
		gl3state.fs2DTexture.description.fragment.fpo,
		"texture");

	assert(texture->index == 0);
}

// 3D Vertex
void
R_RSX_Shaders_initVs3D(void)
{
	Com_Printf("[Shader] Vertex 3D size\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vs3D),
		(rsxVertexProgram*)vertexSrc3D_vpo);

	gl3state.vs3D.binds.uni3D.transProjView = rsxVertexProgramGetConst(
		gl3state.vs3D.description.vertex.vpo,
		"transProjView");

	gl3state.vs3D.binds.uni3D.transModel = rsxVertexProgramGetConst(
		gl3state.vs3D.description.vertex.vpo,
		"transModel");
}

// 3D Vertex Flow
void
R_RSX_Shaders_initVs3Dflow(void)
{
	Com_Printf("[Shader] Vertex 3D Flow\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vs3DFlow),
		(rsxVertexProgram*)vertex3Dflow_vpo);

	// gl3state.vs3DFlow.binds.uni3D.scroll = rsxVertexProgramGetConst(
	// 	gl3state.vs3DFlow.description.vertex.vpo,
	// 	"scroll");
	
	gl3state.vs3DFlow.binds.uni3D.transModel = rsxVertexProgramGetConst(
		gl3state.vs3DFlow.description.vertex.vpo,
		"transModel");

	gl3state.vs3DFlow.binds.uni3D.transProjView = rsxVertexProgramGetConst(
		gl3state.vs3DFlow.description.vertex.vpo,
		"transProjView");
}

// 3D Vertex Lightmap
void
R_RSX_Shaders_initVs3Dlm()
{
	Com_Printf("[Shader] Vertex 3D LightMap\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vs3Dlm),
		(rsxVertexProgram*)vertexSrc3Dlm_vpo);

	gl3state.vs3Dlm.binds.uni3D.transProjView = rsxVertexProgramGetConst(
		gl3state.vs3Dlm.description.vertex.vpo,
		"transProjView");

	gl3state.vs3Dlm.binds.uni3D.transModel = rsxVertexProgramGetConst(
		gl3state.vs3Dlm.description.vertex.vpo,
		"transModel");
}

// 3D Vertex Lightmap Flow
void
R_RSX_Shaders_initVs3DlmFlow(void)
{
	Com_Printf("[Shader] Vertex 3D LightMap Flow\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vs3DlmFlow),
		(rsxVertexProgram*)vertex3DLmFlow_vpo);

	// gl3state.vs3DlmFlow.binds.uni3D.scroll = rsxVertexProgramGetConst(
	// 	gl3state.vs3DlmFlow.description.vertex.vpo,
	// 	"scroll");

	gl3state.vs3DlmFlow.binds.uni3D.transProjView = rsxVertexProgramGetConst(
		gl3state.vs3DlmFlow.description.vertex.vpo,
		"transProjView");

	gl3state.vs3DlmFlow.binds.uni3D.transModel = rsxVertexProgramGetConst(
		gl3state.vs3DlmFlow.description.vertex.vpo,
		"transModel");
}

// 3D Fragment Sky
void
R_RSX_Shaders_initFs3sky(void)
{
	Com_Printf("[Shader] Fragment 3D Sky\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs3Dsky),
		(rsxFragmentProgram*)fragmentSrc3Dsky_fpo);

	gl3state.fs3Dsky.binds.uni3D.alpha = rsxFragmentProgramGetConst(
		gl3state.fs3Dsky.description.fragment.fpo,
		"alpha");

	gl3state.fs3Dsky.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs3Dsky.description.fragment.fpo,
		"gamma");

	// Insure texture at index 0
	rsxProgramAttrib *texture = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dsky.description.fragment.fpo,
		"texture");

	assert(texture->index == 0);
}

// 3D Fragment Lightmap
void
R_RSX_Shaders_initFs3Dlm(void)
{
	Com_Printf("[Shader] Fragment 3D LightMap\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs3Dlm),
		(rsxFragmentProgram*)fragmentSrc3Dlm_fpo);

	gl3state.fs3Dlm.binds.uni3D.overbrightbits = rsxFragmentProgramGetConst(
		gl3state.fs3Dlm.description.fragment.fpo,
		"overbrightbits");
	
	gl3state.fs3Dlm.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs3Dlm.description.fragment.fpo,
		"gamma");

	gl3state.fs3Dlm.binds.uniCommon.intensity = rsxFragmentProgramGetConst(
		gl3state.fs3Dlm.description.fragment.fpo,
		"intensity");

	gl3state.fs3Dlm.binds.uniMisc.lmScales[0] = rsxFragmentProgramGetConst(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lm0_scale");

	gl3state.fs3Dlm.binds.uniMisc.lmScales[1] = rsxFragmentProgramGetConst(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lm1_scale");

	gl3state.fs3Dlm.binds.uniMisc.lmScales[2] = rsxFragmentProgramGetConst(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lm2_scale");

	gl3state.fs3Dlm.binds.uniMisc.lmScales[3] = rsxFragmentProgramGetConst(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lm3_scale");

	
	// Insure all samplers have correct indexes
	rsxProgramAttrib* texture = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dlm.description.fragment.fpo,
		"texture");

	rsxProgramAttrib* lightmap0  = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lightmap0");

	rsxProgramAttrib* lightmap1 = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lightmap1");

	rsxProgramAttrib* lightmap2 = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lightmap2");

	rsxProgramAttrib* lightmap3 = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dlm.description.fragment.fpo,
		"lightmap3");

	rsxProgramAttrib* dlightmap = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dlm.description.fragment.fpo,
		"dlightmap");

	Com_DPrintf("fs3Dlm texture: %d\n", texture->index);
	Com_DPrintf("fs3Dlm lightmap0: %d\n", lightmap0->index);
	Com_DPrintf("fs3Dlm lightmap1: %d\n", lightmap1->index);
	Com_DPrintf("fs3Dlm lightmap2: %d\n", lightmap2->index);
	Com_DPrintf("fs3Dlm lightmap3: %d\n", lightmap3->index);
	Com_DPrintf("fs3Dlm dlightmap: %d\n", dlightmap->index);

	/* 
		Atrib indexes is like OpenGL's TMUs so they can be
		interpreted as OGL's GL_TEXTURE0, GL_TEXTURE1 and so one.
		Need to be sure that atributes had right index within
		the shader (basically definition order) don't need to
		activate and bind atributes manually becuse we use
		precompiled shaders on RSX.
	*/
	assert(texture->index == 0);
	assert(lightmap0->index == 1);
	assert(lightmap1->index == 2);
	assert(lightmap2->index == 3);
	assert(lightmap3->index == 4);
	assert(dlightmap->index == 5);
}

// 3D Vertex Alias
void
R_RSX_Shaders_initVs3Dalias(void)
{
	Com_Printf("[Shader] Vertex 3D Alias\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vs3Dalias),
		(rsxVertexProgram*)vertexSrc3DAlias_vpo);

	gl3state.vs3Dalias.binds.uni3D.transProjView = rsxVertexProgramGetConst(
		gl3state.vs3Dalias.description.vertex.vpo,
		"transProjView");
	
	gl3state.vs3Dalias.binds.uni3D.transModel = rsxVertexProgramGetConst(
		gl3state.vs3Dalias.description.vertex.vpo,
		"transModel");
	
	// conflicts with vs3d lm shader then set 
	// gl3state.vs3Dalias.binds.uni3D.overbrightbits = rsxVertexProgramGetConst(
	// 	gl3state.vs3Dalias.description.vertex.vpo,
	// 	"overbrightbits");
}

// 3D Fragment Alias
void
R_RSX_Shaders_initFs3Dalias(void)
{
	Com_Printf("[Shader] Fragment 3D Alias\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs3Dalias),
		(rsxFragmentProgram*)fragmentSrc3DAlias_fpo
	);
	
	gl3state.fs3Dalias.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs3Dalias.description.fragment.fpo,
		"gamma");
	
	gl3state.fs3Dalias.binds.uniCommon.intensity = rsxFragmentProgramGetConst(
		gl3state.fs3Dalias.description.fragment.fpo,
		"intensity");

	// Insure texture at index 0
	rsxProgramAttrib* texture = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dalias.description.fragment.fpo,
		"texture");

	assert(texture->index == 0);
}

// 3D Fragment Alias Color
void
R_RSX_Shaders_initFs3DaliasColor(void)
{
	Com_Printf("[Shader] Fragment 3D Alias Color\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs3DaliasColor),
		(rsxFragmentProgram*)fragmentSrc3DaliasColor_fpo);

	gl3state.fs3DaliasColor.binds.uni3D.alpha = rsxFragmentProgramGetConst(
		gl3state.fs3DaliasColor.description.fragment.fpo,
		"alpha");
	
	gl3state.fs3DaliasColor.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs3DaliasColor.description.fragment.fpo,
		"gamma");
}

// 3D Fragment
void
R_RSX_Shaders_initFs3D(void)
{
	Com_Printf("[Shader] Fragment 3D\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs3D),
		(rsxFragmentProgram*)fragmentSrc3D_fpo);

	gl3state.fs3D.binds.uni3D.alpha = rsxFragmentProgramGetConst(
		gl3state.fs3D.description.fragment.fpo,
		"alpha");

	gl3state.fs3D.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs3D.description.fragment.fpo,
		"gamma");

	gl3state.fs3D.binds.uniCommon.intensity = rsxFragmentProgramGetConst(
		gl3state.fs3D.description.fragment.fpo,
		"intensity");

	// Insure texture at index 0
	rsxProgramAttrib* texture =  rsxFragmentProgramGetAttrib(
		gl3state.fs3D.description.fragment.fpo,
		"texture");

	assert(texture->index == 0);
}

// 3D Fragment Color
void
R_RSX_Shaders_initFs3color(void)
{
	Com_Printf("[Shader] Fragment 3D Color\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs3Dcolor),
		(rsxFragmentProgram*)fragmentSrc3Dcolor_fpo);

	gl3state.fs3Dcolor.binds.uni3D.alpha = rsxFragmentProgramGetConst(
		gl3state.fs3Dcolor.description.fragment.fpo,
		"alpha");

	gl3state.fs3Dcolor.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs3Dcolor.description.fragment.fpo,
		"gamma");

	// gl3state.fs3Dcolor.binds.uniCommon.intensity = rsxFragmentProgramGetConst(
	// 	gl3state.fs3Dcolor.description.fragment.fpo,
	// 	"intensity");
	
	gl3state.fs3Dcolor.binds.uniCommon.color = rsxFragmentProgramGetConst(
		gl3state.fs3Dcolor.description.fragment.fpo,
		"color");
}

// Vertex Particles
void
R_RSX_Shaders_initVsParticle(void)
{
	Com_Printf("[Shader] Vertex Particle\n");

	R_RSX_Shaders_basicInitVertex(
		&(gl3state.vsParticle),
		(rsxVertexProgram*)vertexSrcParticles_vpo);

	gl3state.vsParticle.binds.uni3D.transProjView = rsxVertexProgramGetConst(
		gl3state.vsParticle.description.vertex.vpo,
		"transProjView");
	
	gl3state.vsParticle.binds.uni3D.transModel = rsxVertexProgramGetConst(
		gl3state.vsParticle.description.vertex.vpo,
		"transModel");

	gl3state.vsParticle.binds.uniMisc.screenRes = rsxVertexProgramGetConst(
		gl3state.vsParticle.description.vertex.vpo,
		"screenResolution");
}

// Frarment Particle Square
void
R_RSX_Shaders_initFsParticleSquare(void)
{
	Com_Printf("[Shader] Fragment Particle Square\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fsParticleSquare),
		(rsxFragmentProgram*)fragmentSrcParticlesSquare_fpo);
	
	gl3state.fsParticleSquare.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fsParticleSquare.description.fragment.fpo,
		"gamma");
}

// Fragment Particle
void
R_RSX_shaders_initFsParticle(void)
{
	Com_Printf("[Shader] Fragment Particle\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fsParticle),
		(rsxFragmentProgram*)fragmentSrcParticles_fpo);

	gl3state.fsParticle.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fsParticle.description.fragment.fpo,
		"gamma");

	gl3state.fsParticle.binds.uni3D.particleFadeFactor = rsxFragmentProgramGetConst(
		gl3state.fsParticle.description.fragment.fpo,
		"particleFadeFactor");
}

// 3D Fragment Water
void
R_RSX_Shaders_initFs3Dwater(void)
{
	Com_Printf("[Shader] Fragment 3D Water size\n");

	R_RSX_Shaders_basicInitFragment(
		&(gl3state.fs3Dwater),
		(rsxFragmentProgram*)fragmentSrc3Dwater_fpo);

	gl3state.fs3Dwater.binds.uni3D.alpha = rsxFragmentProgramGetConst(
		gl3state.fs3Dwater.description.fragment.fpo,
		"alpha");

	gl3state.fs3Dwater.binds.uniCommon.gamma = rsxFragmentProgramGetConst(
		gl3state.fs3Dwater.description.fragment.fpo,
		"gamma");

	gl3state.fs3Dwater.binds.uniCommon.intensity = rsxFragmentProgramGetConst(
		gl3state.fs3Dwater.description.fragment.fpo,
		"intensity");

	gl3state.fs3Dwater.binds.uni3D.time = rsxFragmentProgramGetConst(
		gl3state.fs3Dwater.description.fragment.fpo,
		"time");
	
	gl3state.fs3Dwater.binds.uni3D.scroll = rsxFragmentProgramGetConst(
		gl3state.fs3Dwater.description.fragment.fpo,
		"scroll");
	
	// Insure texture at index 0
	rsxProgramAttrib *texture = rsxFragmentProgramGetAttrib(
		gl3state.fs3Dwater.description.fragment.fpo,
		"texture");

	assert(texture->index == 0);
}

qboolean
R_RSX_Shaders_Init()
{
	R_RSX_Shaders_initVs2Dcolor();
	R_RSX_Shaders_initFs2Dcolor();
	R_RSX_Shaders_initVs2Dtexture();
	R_RSX_Shaders_initFs2Dtexture();
	R_RSX_Shaders_initVs3D();
	R_RSX_Shaders_initVs3Dflow();
	R_RSX_Shaders_initVs3Dlm();
	R_RSX_Shaders_initVs3DlmFlow();
	R_RSX_Shaders_initFs3sky();
	R_RSX_Shaders_initFs3Dlm();
	R_RSX_Shaders_initVs3Dalias();
	R_RSX_Shaders_initFs3Dalias();
	R_RSX_Shaders_initFs3DaliasColor();
	R_RSX_Shaders_initFs3D();
	R_RSX_Shaders_initFs3color();
	R_RSX_Shaders_initVsParticle();
	R_RSX_Shaders_initFsParticleSquare();
	R_RSX_shaders_initFsParticle();
	R_RSX_Shaders_initFs3Dwater();

    return true;
}

void
R_RSX_Shaders_Shutdown()
{
	R_RSX_Shaders_shutdownShader(&(gl3state.fs2DColor));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs2DTexture));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs3Dsky));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs3Dlm));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs3Dalias));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs3DaliasColor));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs3D));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs3Dcolor));
	R_RSX_Shaders_shutdownShader(&(gl3state.fsParticleSquare));
	R_RSX_Shaders_shutdownShader(&(gl3state.fsParticle));
	R_RSX_Shaders_shutdownShader(&(gl3state.fs3Dwater));
}

void
R_RSX_Shaders_UpdateUniCommonData(void)
{
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs2DColor));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs2DTexture));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs3Dsky));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs3Dlm));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs3Dalias));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs3DaliasColor));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs3D));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs3Dcolor));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fsParticleSquare));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fsParticle));
	R_RSX_Shaders_UpdateUniCommonDataOnShader(&(gl3state.fs3Dwater));
}

void
R_RSX_Shaders_UpdateUni2DData(void)
{
	R_RSX_Shaders_updateUni2DDataOnShader(&(gl3state.vs2DColor));
	R_RSX_Shaders_updateUni2DDataOnShader(&(gl3state.vs2DTexture));
}

void
R_RSX_Shaders_UpdateUni3DData(void)
{
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.vs3D));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.vs3DFlow));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.vs3Dlm));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.vs3DlmFlow));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.fs3Dsky));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.fs3Dlm));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.vs3Dalias));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.fs3DaliasColor));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.fs3D));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.fs3Dcolor));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.vsParticle));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.fsParticle));
	R_RSX_Shaders_updateUni3DDataOnShader(&(gl3state.fs3Dwater));
}

void
R_RSX_Shaders_UpdateScreenResolution(void)
{
	R_RSX_Shaders_updateScreenResolutionOnShader(&(gl3state.vsParticle));
}

// for now only one shader using them in future shader must be specified
void
R_RSX_Shaders_UpdateLmScales(const hmm_vec4 lmScales[MAX_LIGHTMAPS_PER_SURFACE])
{
	R_RSX_Shaders_updateLmScalesOnShader(lmScales, &(gl3state.fs3Dlm));

}

void R_RSX_Shaders_setTexture(gl3image_t *texture, uint8_t index)
{
	if (texture->registration_sequence == 0) { return; }

	rsxInvalidateTextureCache(gcmContext, GCM_INVALIDATE_TEXTURE);

	rsxLoadTexture(gcmContext, index, &(texture->gcm_texture));

	rsxTextureControl(gcmContext, index, GCM_TRUE, 0<<8, 12<<8, gl3state.anisotropic);

	if (texture->nolerp)
	{
		rsxTextureFilter(gcmContext,
                         index, // index
                         0, // bias
                         GCM_TEXTURE_NEAREST, // min filter
                         GCM_TEXTURE_NEAREST, // mag filter
                         GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	}
	else if (texture->is_mipmap)
	{
		// TODO: some hardware may require mipmapping disabled for NPOT textures!
		// glGenerateMipmap(GL_TEXTURE_2D);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		rsxTextureFilter(gcmContext,
                         index, // index
                         0, // bias
                         gl_filter_min, // min filter
                         gl_filter_max, // mag filter
                         GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	}
	else // if the texture has no mipmaps, we can't use gl_filter_min which might be GL_*_MIPMAP_*
	{
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		// rsxTextureFilter(gcmContext,
		//                  index, // index
		//                  0, // bias
		//                  gl_filter_max, // min filter
		//                  gl_filter_max, // mag filter
		//                  GCM_TEXTURE_CONVOLUTION_QUINCUNX);

		rsxTextureFilter(gcmContext,
		                 index, // index
		                 0, // bias
		                 GCM_TEXTURE_LINEAR, // min filter
		                 GCM_TEXTURE_NEAREST, // mag filter
		                 GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	}
	// rsxTextureWrapMode(gcmContext, index, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
}

void
R_RSX_Shaders_SetTexture0(gl3image_t *texture)
{
	R_RSX_Shaders_setTexture(texture, 0);
}