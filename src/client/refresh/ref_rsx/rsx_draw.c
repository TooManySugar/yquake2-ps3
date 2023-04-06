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
 * Drawing of all images that are not textures
 *
 * =======================================================================
 */

#include "header/local.h"

unsigned d_8to24table[256];

gl3image_t *draw_chars;

#define AMOUNT_OF_2D_BUFFERS 4096
static void *vb2D[AMOUNT_OF_2D_BUFFERS] = { NULL };
uint32_t used_vb2Ds = 0;

static void

R_RSX_Draw_shutdown2DVertexBuffers()
{
	for (uint32_t i = 0; i < AMOUNT_OF_2D_BUFFERS; ++i)
	{
		if (vb2D[i] == NULL) { continue; }

		rsxFree_with_log(vb2D[i]);
		vb2D[i] = NULL;
	}
	used_vb2Ds = 0;
}

static qboolean
R_RSX_Draw_init2DVertexBuffers()
{
	void* ptr;
	for (uint32_t i = 0; i < AMOUNT_OF_2D_BUFFERS; ++i)
	{
		ptr = rsxMemalign_with_log(128, 128);

		if (ptr == NULL)
		{
			R_RSX_Draw_shutdown2DVertexBuffers();
			return false;
		}
		vb2D[i] = ptr;
	}
	used_vb2Ds = 0;
	return true;
}

void *
R_RSX_Draw_next2DVertexBuffer()
{
	void *res = vb2D[used_vb2Ds];
	/* Not sure hot to make it more smart:
	   reseting it at frame end/begin causes RSX to not
	   finish drawing at a time causing flickering on
	   rendered images.
	   Current solution working with a hope what it
	   finished drawing previously used vb2D at the time
	   when lap occurs. */
	used_vb2Ds = (used_vb2Ds + 1) % AMOUNT_OF_2D_BUFFERS;
	return res;
}

void
R_RSX_Draw_InitLocal(void)
{
	/* load console characters */
	draw_chars = R_RSX_Image_FindImage("pics/conchars.pcx", it_pic);
	if (!draw_chars)
	{
		ri.Sys_Error(ERR_FATAL, "Couldn't load pics/conchars.pcx");
	}
	R_Printf(PRINT_ALL, "Console characters loaded\n");

	if (!R_RSX_Draw_init2DVertexBuffers())
	{
		ri.Sys_Error(ERR_FATAL, "Couldn't allocate memory for 2D vertex buffers");
	}
	R_Printf(PRINT_ALL, "Allocated %d 2D Vertex buffers\n", AMOUNT_OF_2D_BUFFERS);
	

	// set up attribute layout for 2D textured rendering
	// glGenVertexArrays(1, &vao2D);
	// glBindVertexArray(vao2D);

	// glGenBuffers(1, &vbo2D);
	// GL3_BindVBO(vbo2D);

	// GL3_UseProgram(gl3state.si2D.shaderProgram);

	// glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	// // Note: the glVertexAttribPointer() configuration is stored in the VAO, not the shader or sth
	// //       (that's why I use one VAO per 2D shader)
	// qglVertexAttribPointer(GL3_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);

	// glEnableVertexAttribArray(GL3_ATTRIB_TEXCOORD);
	// qglVertexAttribPointer(GL3_ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 2*sizeof(float));

	// // set up attribute layout for 2D flat color rendering

	// glGenVertexArrays(1, &vao2Dcolor);
	// glBindVertexArray(vao2Dcolor);

	// GL3_BindVBO(vbo2D); // yes, both VAOs share the same VBO

	// GL3_UseProgram(gl3state.si2Dcolor.shaderProgram);

	// glEnableVertexAttribArray(GL3_ATTRIB_POSITION);
	// qglVertexAttribPointer(GL3_ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), 0);

	// GL3_BindVAO(0);
}

void
R_RSX_Draw_ShutdownLocal(void)
{
	R_RSX_Draw_shutdown2DVertexBuffers();
	// glDeleteBuffers(1, &vbo2D);
	// vbo2D = 0;
	// glDeleteVertexArrays(1, &vao2D);
	// vao2D = 0;
	// glDeleteVertexArrays(1, &vao2Dcolor);
	// vao2Dcolor = 0;
}

// bind the texture before calling this
static void
R_RSX_Draw_texturedRectangle(float x, float y, float w, float h,
                             float sl, float tl, float sh, float th)
{
	u32 offset;
	float fx,fy,fw,fh;

	/*
	 *  x,y+h      x+w,y+h
	 * sl,th--------sh,th
	 *  |             |
	 *  |             |
	 *  |             |
	 * sl,tl--------sh,tl
	 *  x,y        x+w,y
	 */

	// conversion from Quake2 cords to RSX's render cords
	fx = x / (vid.width / 2) - 1;
	fy = -(y / (vid.height / 2) - 1);
	fw = ((x+w) / (vid.width / 2)) - 1;
	fh = -(((y+h) / (vid.height / 2)) - 1);

	// Take this Buf from dedicated to 2d drawing rsx's memory array
	// which is allocated only once and had size 128 * 4096 bytes
	// 4096 2d images limit should be enough as well as 128 bytes
	// for 4 element vertexes arrays (with uv actual size is 64)
	float* vBuf = (float*)R_RSX_Draw_next2DVertexBuffer();

	float vBuf_c[16] = {
	//  X,   Y,   S,  T
		fx,  fh,  sl, th,
		fx,  fy,  sl, tl,
		fw,  fh,  sh, th,
		fw,  fy,  sh, tl
	};

	memcpy(vBuf, &vBuf_c, sizeof(float) * 16);

	// GL3_BindVAO(vao2D);

	// // Note: while vao2D "remembers" its vbo for drawing, binding the vao does *not*
	// //       implicitly bind the vbo, so I need to explicitly bind it before glBufferData()
	// GL3_BindVBO(vbo2D);
	// glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);

	// glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	//glMultiDrawArrays(mode, first, count, drawcount) ??

	rsxAddressToOffset(&vBuf[0], &offset);

	rsxBindVertexArrayAttrib(gcmContext,
	                         GCM_VERTEX_ATTRIB_POS,
	                         0,
	                         offset,
	                         sizeof(float) * 4,
	                         2,
	                         GCM_VERTEX_DATA_TYPE_F32,
	                         GCM_LOCATION_RSX);

	rsxAddressToOffset(&vBuf[2], &offset);
	rsxBindVertexArrayAttrib(gcmContext,
	                         GCM_VERTEX_ATTRIB_TEX0,
	                         0,
	                         offset,
	                         sizeof(float) * 4,
	                         2,
	                         GCM_VERTEX_DATA_TYPE_F32,
	                         GCM_LOCATION_RSX);

	rsxDrawVertexArray(gcmContext, GCM_TYPE_TRIANGLE_STRIP, 0, 4);
}

/*
 * Draws one 8*8 graphics character with 0 being transparent.
 * It can be clipped to the top of the screen to allow the console to be
 * smoothly scrolled off.
 */
void
R_RSX_Draw_CharScaled(int x, int y, int num, float scale)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%c)\n", __func__, num);
	// 	is_first_call = false;
	// }

	int row, col;
	float frow, fcol, size, scaledSize;
	num &= 255;

	if ((num & 127) == 32)
	{
		return; /* space */
	}

	if (y <= -8)
	{
		return; /* totally off screen */
	}

	row = num >> 4;
	col = num & 15;

	frow = row * 0.0625;
	fcol = col * 0.0625;
	size = 0.0625;

	scaledSize = 8*scale;

	// TODO: batchen?

	// GL3_UseProgram(gl3state.si2D.shaderProgram);
	// GL3_Bind(draw_chars->texnum);
	R_RSX_Image_BindUni2DTex0(draw_chars);
	R_RSX_Shaders_Load(&(gl3state.vs2DTexture));
	R_RSX_Shaders_Load(&(gl3state.fs2DTexture));

	R_RSX_Draw_texturedRectangle(x, y, scaledSize, scaledSize, fcol, frow, fcol+size, frow+size);
}

gl3image_t *
R_RSX_Draw_FindPic(char *name)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%s)\n", __func__, name);
	// 	is_first_call = false;
	// }

	gl3image_t *pic;
	char fullname[MAX_QPATH];

	if ((name[0] != '/') && (name[0] != '\\'))
	{
		Com_sprintf(fullname, sizeof(fullname), "pics/%s.pcx", name);
		pic = R_RSX_Image_FindImage(fullname, it_pic);
	}
	else
	{
		pic = R_RSX_Image_FindImage(name + 1, it_pic);
	}

	return pic;
}

void
R_RSX_Draw_GetPicSize(int *w, int *h, char *pic)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%s)\n", __func__, pic);
	// 	is_first_call = false;
	// }

	gl3image_t *gl;

	gl = R_RSX_Draw_FindPic(pic);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

void
R_RSX_Draw_StretchPic(int x, int y, int w, int h, char *pic)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%s)\n", __func__, pic);
	// 	is_first_call = false;
	// }

	gl3image_t *gl = R_RSX_Draw_FindPic(pic);

	if (!gl)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	// GL3_UseProgram(gl3state.si2D.shaderProgram);
	// GL3_Bind(gl->texnum);
	R_RSX_Image_BindUni2DTex0(gl);
	R_RSX_Shaders_Load(&(gl3state.vs2DTexture));
	R_RSX_Shaders_Load(&(gl3state.fs2DTexture));

	R_RSX_Draw_texturedRectangle(x, y, w, h, gl->sl, gl->tl, gl->sh, gl->th);
}

void
R_RSX_Draw_PicScaled(int x, int y, char *pic, float factor)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%s)\n", __func__, pic);
	// 	is_first_call = false;
	// }

	gl3image_t *gl = R_RSX_Draw_FindPic(pic);
	if (!gl)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	// GL3_UseProgram(gl3state.si2D.shaderProgram);
	// GL3_Bind(gl->texnum);
	R_RSX_Image_BindUni2DTex0(gl);
	R_RSX_Shaders_Load(&(gl3state.vs2DTexture));
	R_RSX_Shaders_Load(&(gl3state.fs2DTexture));

	R_RSX_Draw_texturedRectangle(x, y, gl->width*factor, gl->height*factor, gl->sl, gl->tl, gl->sh, gl->th);
}

/*
 * This repeats a 64*64 tile graphic to fill
 * the screen around a sized down
 * refresh window.
 */
void
R_RSX_Draw_TileClear(int x, int y, int w, int h, char *pic)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%s)\n", __func__, pic);
	// 	is_first_call = false;
	// }

	gl3image_t *image = R_RSX_Draw_FindPic(pic);
	if (!image)
	{
		R_Printf(PRINT_ALL, "Can't find pic: %s\n", pic);
		return;
	}

	// GL3_UseProgram(gl3state.si2D.shaderProgram);
	// GL3_Bind(image->texnum);
	R_RSX_Image_BindUni2DTex0(image);
	R_RSX_Shaders_Load(&(gl3state.vs2DTexture));
	R_RSX_Shaders_Load(&(gl3state.fs2DTexture));

	R_RSX_Draw_texturedRectangle(x, y, w, h, x/64.0f, y/64.0f, (x+w)/64.0f, (y+h)/64.0f);
}

void // 252
R_RSX_Draw_FrameBufferObject(int x, int y, int w, int h, GLuint fboTexture, const float v_blend[4])
{
	NOT_IMPLEMENTED();
	// qboolean underwater = (gl3_newrefdef.rdflags & RDF_UNDERWATER) != 0;
	// gl3ShaderInfo_t* shader = underwater ? &gl3state.si2DpostProcessWater
	//                                      : &gl3state.si2DpostProcess;
	// GL3_UseProgram(shader->shaderProgram);
	// GL3_Bind(fboTexture);

	// if(underwater && shader->uniLmScalesOrTime != -1)
	// {
	// 	glUniform1f(shader->uniLmScalesOrTime, gl3_newrefdef.time);
	// }
	// if(shader->uniVblend != -1)
	// {
	// 	glUniform4fv(shader->uniVblend, 1, v_blend);
	// }

	// R_RSX_Draw_texturedRectangle(x, y, w, h, 0, 1, 1, 0);
}

/*
 * Fills a box of pixels with a single color
 */
void
R_RSX_Draw_Fill(int x, int y, int w, int h, int c)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }

	union {
		unsigned c;
		byte v[4];
	} color;

	int i;
	uint32_t offset;
	float fx, fy, fw, fh;

	if ((unsigned)c > 255)
	{
		ri.Sys_Error(ERR_FATAL, "Draw_Fill: bad color");
	}

	color.c = d_8to24table[c];

	// conversion from Quake2 cords to RSX's render cords
	fx = ((float)x) / (vid.width / 2) - 1.0f;
	fy = -((((float)y) / (vid.height / 2)) - 1.0f);
	fw = (((float)(x+w)) / (vid.width / 2)) - 1.0f;
	fh = -((((float)(y+h)) / (vid.height / 2)) - 1.0f);

	float* vBuf = (float*)R_RSX_Draw_next2DVertexBuffer();

	float vBuf_c[8] = {
	//  X,   Y, 
		fx,  fh,
		fx,  fy,
		fw,  fh,
		fw,  fy
	};

	memcpy(vBuf, &vBuf_c, sizeof(float) * 8);

	for(i = 0; i < 3; ++i)
	{
		gl3state.uniCommonData.color.Elements[i] = color.v[i] * (1.0f/255.0f);
	}
	gl3state.uniCommonData.color.A = 1.0f;

	// GL3_UpdateUBOCommon();
	R_RSX_Shaders_UpdateUniCommonData();

	// GL3_UseProgram(gl3state.si2Dcolor.shaderProgram);
	// GL3_BindVAO(vao2Dcolor);
	R_RSX_Shaders_Load(&(gl3state.vs2DColor));
	R_RSX_Shaders_Load(&(gl3state.fs2DColor));

	// GL3_BindVBO(vbo2D);
	// glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);
	rsxAddressToOffset(&vBuf[0], &offset);

	rsxBindVertexArrayAttrib(gcmContext,
	                         GCM_VERTEX_ATTRIB_POS,
	                         0,
	                         offset,
	                         sizeof(float) * 2,
	                         2,
	                         GCM_VERTEX_DATA_TYPE_F32,
	                         GCM_LOCATION_RSX);

	// glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	rsxDrawVertexArray(gcmContext, GCM_TYPE_TRIANGLE_STRIP, 0, 4);
}

// in GL1 this is called R_Flash() (which just calls R_PolyBlend())
// now implemented in 2D mode and called after SetGL2D() because
// it's pretty similar to GL3_Draw_FadeScreen()
void
R_RSX_Draw_Flash(const float color[4], float x, float y, float w, float h)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }
	int i = 0;
	uint32_t offset;
	float fx, fy, fw, fh;

	// conversion from Quake2 cords to RSX's render cords
	fx = ((float)x) / (vid.width / 2) - 1.0f;
	fy = -((((float)y) / (vid.height / 2)) - 1.0f);
	fw = (((float)(x+w)) / (vid.width / 2)) - 1.0f;
	fh = -((((float)(y+h)) / (vid.height / 2)) - 1.0f);

	float* vBuf = (float*)R_RSX_Draw_next2DVertexBuffer();

	float vBuf_c[8] = {
	//  X,   Y, 
		fx,  fh,
		fx,  fy,
		fw,  fh,
		fw,  fy
	};

	memcpy(vBuf, &vBuf_c, sizeof(float) * 8);

	// glEnable(GL_BLEND);
	rsxSetBlendEnable(gGcmContext, GCM_TRUE);

	for (i = 0; i < 4; ++i)
	{
		gl3state.uniCommonData.color.Elements[i] = color[i];
	}

	// GL3_UpdateUBOCommon();
	R_RSX_Shaders_UpdateUniCommonData();

	// GL3_UseProgram(gl3state.si2Dcolor.shaderProgram);
	// GL3_BindVAO(vao2Dcolor);
	R_RSX_Shaders_Load(&(gl3state.vs2DColor));
	R_RSX_Shaders_Load(&(gl3state.fs2DColor));

	// GL3_BindVBO(vbo2D);
	// glBufferData(GL_ARRAY_BUFFER, sizeof(vBuf), vBuf, GL_STREAM_DRAW);
	rsxAddressToOffset(&vBuf[0], &offset);
	rsxBindVertexArrayAttrib(gcmContext,
	                         GCM_VERTEX_ATTRIB_POS,
	                         0,
	                         offset,
	                         sizeof(float) * 2,
	                         2,
	                         GCM_VERTEX_DATA_TYPE_F32,
	                         GCM_LOCATION_RSX);

	// glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	rsxDrawVertexArray(gcmContext, GCM_TYPE_TRIANGLE_STRIP, 0, 4);

	// glDisable(GL_BLEND);
	rsxSetBlendEnable(gGcmContext, GCM_FALSE);
}

void
R_RSX_Draw_FadeScreen(void)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }
	float color[4] = {0, 0, 0, 0.6f};
	R_RSX_Draw_Flash(color, 0, 0, vid.width, vid.height);
}

/**
 * Used for per frame cinematic rendering
 */
void
R_RSX_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data)
{
	// Requires to build texture from <data>, draw it and then free it
	// static qboolean is_first_call = true;
	NOT_IMPLEMENTED();

	// int i, j;

	// GL3_Bind(0);

	// unsigned image32[320*240]; /* was 256 * 256, but we want a bit more space */

	// unsigned* img = image32;

	// if(cols*rows > 320*240)
	// {
	// 	/* in case there is a bigger video after all,
	// 	 * malloc enough space to hold the frame */
	// 	img = (unsigned*)malloc(cols*rows*4);
	// }

	// for(i=0; i<rows; ++i)
	// {
	// 	int rowOffset = i*cols;
	// 	for(j=0; j<cols; ++j)
	// 	{
	// 		byte palIdx = data[rowOffset+j];
	// 		img[rowOffset+j] = gl3_rawpalette[palIdx];
	// 	}
	// }

	// GL3_UseProgram(gl3state.si2D.shaderProgram);

	// GLuint glTex;
	// glGenTextures(1, &glTex);
	// GL3_SelectTMU(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, glTex);

	// glTexImage2D(GL_TEXTURE_2D, 0, gl3_tex_solid_format,
	//              cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

	// if(img != image32)
	// {
	// 	free(img);
	// }

	// // Note: gl_filter_min could be GL_*_MIPMAP_* so we can't use it for min filter here (=> no mipmaps)
	// //       but gl_filter_max (either GL_LINEAR or GL_NEAREST) should do the trick.
	// GLint filter = (r_videos_unfiltered->value == 0) ? gl_filter_max : GL_NEAREST;
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	// drawTexturedRectangle(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f);

	// glDeleteTextures(1, &glTex);

	// GL3_Bind(0);
}

int
R_RSX_Draw_GetPalette(void)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }

	int i;
	int r, g, b;
	unsigned v;
	byte *pic, *pal;
	int width, height;

	/* get the palette */
	LoadPCX("pics/colormap.pcx", &pic, &pal, &width, &height);

	if (!pal)
	{
		ri.Sys_Error(ERR_FATAL, "Couldn't load pics/colormap.pcx");
	}

	for (i = 0; i < 256; i++)
	{
		r = pal[i * 3 + 0];
		g = pal[i * 3 + 1];
		b = pal[i * 3 + 2];

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		d_8to24table[i] = LittleLong(v);
	}

	d_8to24table[255] &= LittleLong(0xffffff); /* 255 is transparent */

	free(pic);
	free(pal);

	return 0;
}
