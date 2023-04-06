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
 * Texture handling for TODO desc
 *
 * =======================================================================
 */

#include "header/local.h"
#include "rsx/mm.h"
#include "rsx/rsx.h"

// 45
int gl_filter_min = GCM_TEXTURE_LINEAR_MIPMAP_NEAREST; // GL_LINEAR_MIPMAP_NEAREST;
int gl_filter_max = GCM_TEXTURE_LINEAR; // GL_LINEAR;

// 48
gl3image_t rsx_textures[MAX_RSX_TEXTURES];
int num_rsx_textures = 0;
static int rsx_textures_max = 0;

void
R_RSX_Image_Init(void)
{
}

void
R_RSX_Image_Shutdown(void)
{
	int i;
	gl3image_t *image;

	for (i = 0, image = rsx_textures; i < num_rsx_textures; i++, image++)
	{
		if (!image->registration_sequence)
		{
			continue; /* free image_t slot */
		}

		/* free it */
		rsxFree_with_log(image->data_ptr);
		memset(image, 0, sizeof(*image));
	}
}

#define GCM_TEXTURE_NO_REMAP ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) | \
(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) | \
(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) | \
(GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) | \
(GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) | \
(GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) | \
(GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) | \
(GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT))

// Uploads RGBA texture to RSX's memory region with format of ARGB
qboolean
R_RSX_Image_allocateTexture(gl3image_t *image, const byte *data, uint32_t width, uint32_t height)
{
	uint32_t i;
	byte *buffer;
	uint32_t offset;

	image->data_ptr = (byte *)rsxMemalign_with_log(128, (width * height * 4));

	if (!image->data_ptr) { return false; }

	rsxAddressToOffset(image->data_ptr, &offset);

	image->gcm_texture.format    = (GCM_TEXTURE_FORMAT_A8R8G8B8 | GCM_TEXTURE_FORMAT_LIN);
	// It kinda strange but mipmap used for different thing here and there
	image->gcm_texture.mipmap    = GCM_TRUE;
	image->gcm_texture.dimension = GCM_TEXTURE_DIMS_2D;
	image->gcm_texture.cubemap   = GCM_FALSE;
	image->gcm_texture.remap     = GCM_TEXTURE_NO_REMAP;
	image->gcm_texture.width     = width;
	image->gcm_texture.height    = height;
	image->gcm_texture.depth     = 1;
	image->gcm_texture.location  = GCM_LOCATION_RSX;
	image->gcm_texture.pitch     = width * 4; // width * <pixel size in bytes>
	// FIXME no need to dedicated offset value
	image->gcm_texture.offset    = offset;

	buffer = image->data_ptr;
	for (i = 0; i < width * height * 4; i += 4)
	{
		/* R */ buffer[i + 1] = *data++;
		/* G */ buffer[i + 2] = *data++;
		/* B */ buffer[i + 3] = *data++;
		/* A */ buffer[i + 0] = *data++;
	}

	return true;
}

void // 141
R_RSX_Image_BindUni2DTex0(gl3image_t *texture)
{
	// TODO: In GL3 refresher texture always binds to TEXTURE0 can do the same
	//       here and drop texture setting in fragment shader loading 
 
	extern gl3image_t *draw_chars;

	if (gl_nobind->value && draw_chars) /* performance evaluation option */
	{
		texture = draw_chars;
	}

	if (gl3state.currenttexture == texture)
	{
		return;
	}

	gl3state.currenttexture = texture;
	// GL3_SelectTMU(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, texnum);
	R_RSX_Shaders_SetTexture0(gl3state.currenttexture);
}


/*
 * Returns has_alpha
 */
qboolean
R_RSX_Image_upload32(gl3image_t *image, unsigned *data, int width, int height)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s()\n", __func__);
	// 	is_first_call = false;
	// }

	// glTexImage2D(GL_TEXTURE_2D, 0, comp, width, height,
	//              0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	R_RSX_Image_allocateTexture(image, (const byte *)(data), width, height);

	// The thing is at current stage of GL3 renderer every texture threats as alpha
	return true;
}

/*
 * Returns has_alpha
 */
qboolean
R_RSX_Image_upload8(gl3image_t *image, byte *data, int width, int height, qboolean mipmap, qboolean is_sky)
{
	int s = width * height;
	unsigned *trans = malloc(s * sizeof(unsigned));

	for (int i = 0; i < s; i++)
	{
		int p = data[i];
		trans[i] = d_8to24table[p];

		/* transparent, so scan around for
		   another color to avoid alpha fringes */
		if (p == 255)
		{
			if ((i > width) && (data[i - width] != 255))
			{
				p = data[i - width];
			}
			else if ((i < s - width) && (data[i + width] != 255))
			{
				p = data[i + width];
			}
			else if ((i > 0) && (data[i - 1] != 255))
			{
				p = data[i - 1];
			}
			else if ((i < s - 1) && (data[i + 1] != 255))
			{
				p = data[i + 1];
			}
			else
			{
				p = 0;
			}

			/* copy rgb components */
			((byte *)&trans[i])[0] = ((byte *)&d_8to24table[p])[0];
			((byte *)&trans[i])[1] = ((byte *)&d_8to24table[p])[1];
			((byte *)&trans[i])[2] = ((byte *)&d_8to24table[p])[2];
		}
	}

	qboolean ret = R_RSX_Image_upload32(image, trans, width, height);
	free(trans);
	return ret;
}

// 288
typedef struct
{
	short x, y;
} floodfill_t;

/* must be a power of 2 */
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy)	\
	{ \
		if (pos[off] == fillcolor) \
		{ \
			pos[off] = 255;	\
			fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
			inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
		} \
		else if (pos[off] != 255) \
		{ \
			fdc = pos[off];	\
		} \
	}

/* 311
 * Fill background pixels so mipmapping doesn't have haloes
 */
static void
R_RSX_Image_floodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; /* assume this is the pixel to fill */
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int filledcolor = 0;
	int i;

	/* attempt to find opaque black */
	for (i = 0; i < 256; ++i)
	{
		if (LittleLong(d_8to24table[i]) == (255 << 0)) /* alpha 1.0 */
		{
			filledcolor = i;
			break;
		}
	}

	/* can't fill to filled color or to transparent color (used as visited marker) */
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int x = fifo[outpt].x, y = fifo[outpt].y;
		int fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
		{
			FLOODFILL_STEP(-1, -1, 0);
		}

		if (x < skinwidth - 1)
		{
			FLOODFILL_STEP(1, 1, 0);
		}

		if (y > 0)
		{
			FLOODFILL_STEP(-skinwidth, 0, -1);
		}

		if (y < skinheight - 1)
		{
			FLOODFILL_STEP(skinwidth, 0, 1);
		}

		skin[x + skinwidth * y] = fdc;
	}
}

/* 374
 * This is also used as an entry point for the generated r_notexture
 */
gl3image_t *
R_RSX_Image_LoadPic(char *name, byte *pic, int width, int realwidth,
                    int height, int realheight, imagetype_t type, int bits)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%s)\n", __func__, name);
	// 	is_first_call = false;
	// }

	gl3image_t *image = NULL;
	int i;

	qboolean nolerp = false;
	if (r_2D_unfiltered->value && type == it_pic)
	{
		// if r_2D_unfiltered is true(ish), nolerp should usually be true,
		// *unless* the texture is on the r_lerp_list
		nolerp = (r_lerp_list->string == NULL) || (strstr(r_lerp_list->string, name) == NULL);
	}
	else if (gl_nolerp_list != NULL && gl_nolerp_list->string != NULL)
	{
		nolerp = strstr(gl_nolerp_list->string, name) != NULL;
	}
	/* find a free gl3image_t */
	for (i = 0, image = rsx_textures; i < num_rsx_textures; i++, image++)
	{
		if (image->texnum == 0)
		{
			break;
		}
	}

	if (i == num_rsx_textures)
	{
		if (num_rsx_textures == MAX_RSX_TEXTURES)
		{
			ri.Sys_Error(ERR_DROP, "MAX_RSX_TEXTURES");
		}

		num_rsx_textures++;
	}

	image = &rsx_textures[i];

	if (strlen(name) >= sizeof(image->name))
	{
		ri.Sys_Error(ERR_DROP, "%s: \"%s\" is too long", __func__, name);
	}

	strcpy(image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	if ((type == it_skin) && (bits == 8))
	{
		R_RSX_Image_floodFillSkin(pic, width, height);
	}

	// image->scrap = false; // TODO: reintroduce scrap? would allow optimizations in 2D rendering..

	// start_gen_texture(&texNum);
	// glGenTextures(1, &texNum);

	image->texnum = 1;

	// GL3_SelectTMU(GL_TEXTURE0);
	// GL3_Bind(texNum);

	qboolean is_mipmap = (image->type != it_pic && image->type != it_sky);

	if (bits == 8)
	{
		// resize 8bit images only when we forced such logic
		if (r_scale8bittextures->value)
		{
			byte *image_converted;
			int scale = 2;

			// scale 3 times if lerp image
			if (!nolerp && (vid.height >= 240 * 3))
				scale = 3;

			image_converted = malloc(width * height * scale * scale);
			if (!image_converted)
				return NULL;

			if (scale == 3) {
				scale3x(pic, image_converted, width, height);
			} else {
				scale2x(pic, image_converted, width, height);
			}

			image->has_alpha = R_RSX_Image_upload8(image, image_converted, width * scale, height * scale,
						is_mipmap,
						image->type == it_sky);
			free(image_converted);
		}
		else
		{
			image->has_alpha = R_RSX_Image_upload8(image, pic, width, height,
						is_mipmap,
						image->type == it_sky);
		}
	}
	else
	{
		image->has_alpha = R_RSX_Image_upload32(image, (unsigned *)pic, width, height);
	}

	image->is_mipmap = is_mipmap;
	image->nolerp = nolerp;

	if (realwidth && realheight)
	{
		if ((realwidth <= image->width) && (realheight <= image->height))
		{
			image->width = realwidth;
			image->height = realheight;
		}
		else
		{
			R_Printf(PRINT_DEVELOPER,
					"Warning, image '%s' has hi-res replacement smaller than the original! (%d x %d) < (%d x %d)\n",
					name, image->width, image->height, realwidth, realheight);
		}
	}

	image->sl = 0;
	image->sh = 1;
	image->tl = 0;
	image->th = 1;

	// printf("got texnum: %d\n", image->texnum);

	return image;
}

static gl3image_t * // 599
R_RSX_Image_loadWal(char *origname, imagetype_t type)
{
	miptex_t *mt;
	int width, height, ofs, size;
	gl3image_t *image;
	char name[256];

	Q_strlcpy(name, origname, sizeof(name));

	/* Add the extension */
	if (strcmp(COM_FileExtension(name), "wal"))
	{
		Q_strlcat(name, ".wal", sizeof(name));
	}

	size = ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		R_Printf(PRINT_ALL, "R_RSX_Image_loadWal: can't load %s\n", name);
		return rsx_no_texture;
	}

	if (size < sizeof(miptex_t))
	{
		R_Printf(PRINT_ALL, "R_RSX_Image_loadWal: can't load %s, small header\n", name);
		ri.FS_FreeFile((void *)mt);
		return rsx_no_texture;
	}

	width = LittleLong(mt->width);
	height = LittleLong(mt->height);
	ofs = LittleLong(mt->offsets[0]);

	if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
	    (((size - ofs) / height) < width))
	{
		R_Printf(PRINT_ALL, "R_RSX_Image_loadWal: can't load %s, small body\n", name);
		ri.FS_FreeFile((void *)mt);
		return rsx_no_texture;
	}

	image = R_RSX_Image_LoadPic(name, (byte *)mt + ofs, width, 0, height, 0, type, 8);

	ri.FS_FreeFile((void *)mt);

	return image;
}

static gl3image_t *
R_RSX_Image_loadM8(char *origname, imagetype_t type)
{
	m8tex_t *mt;
	int width, height, ofs, size;
	gl3image_t *image;
	char name[256];
	unsigned char *image_buffer = NULL;

	Q_strlcpy(name, origname, sizeof(name));

	/* Add the extension */
	if (strcmp(COM_FileExtension(name), "m8"))
	{
		Q_strlcat(name, ".m8", sizeof(name));
	}

	size = ri.FS_LoadFile(name, (void **)&mt);

	if (!mt)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, name);
		return rsx_no_texture;
	}

	if (size < sizeof(m8tex_t))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small header\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return rsx_no_texture;
	}

	if (LittleLong (mt->version) != M8_VERSION)
	{
		R_Printf(PRINT_ALL, "R_RSX_Image_loadWal: can't load %s, wrong magic value.\n", name);
		ri.FS_FreeFile ((void *)mt);
		return rsx_no_texture;
	}

	width = LittleLong(mt->width[0]);
	height = LittleLong(mt->height[0]);
	ofs = LittleLong(mt->offsets[0]);

	if ((ofs <= 0) || (width <= 0) || (height <= 0) ||
	    (((size - ofs) / height) < width))
	{
		R_Printf(PRINT_ALL, "%s: can't load %s, small body\n", __func__, name);
		ri.FS_FreeFile((void *)mt);
		return rsx_no_texture;
	}

	image_buffer = malloc (width * height * 4);
	for(int i=0; i<width * height; i++)
	{
		unsigned char value = *((byte *)mt + ofs + i);
		image_buffer[i * 4 + 0] = mt->palette[value].r;
		image_buffer[i * 4 + 1] = mt->palette[value].g;
		image_buffer[i * 4 + 2] = mt->palette[value].b;
		image_buffer[i * 4 + 3] = value == 255 ? 0 : 255;
	}

	image = R_RSX_Image_LoadPic(name, image_buffer, width, 0, height, 0, type, 32);
	free(image_buffer);

	ri.FS_FreeFile((void *)mt);

	return image;
}

/*
 * Finds or loads the given image
 */
gl3image_t * // 721
R_RSX_Image_FindImage(char *name, imagetype_t type)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	// printf("%s(%s)\n", __func__, name);
	// 	// is_first_call = false;
	// }

	gl3image_t *image;
	int i, len;
	byte *pic;
	int width, height;
	char *ptr;
	char namewe[256];
	int realwidth = 0, realheight = 0;
	const char* ext;

	if (!name)
	{
		return NULL;
	}
	
	ext = COM_FileExtension(name);
	if(!ext[0])
	{
		/* file has no extension */
		return NULL;
	}

	len = strlen(name);

	/* Remove the extension */
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - (strlen(ext) + 1));

	len = strlen(name);

	/* Remove the extension */
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - (strlen(ext) + 1));

	/* fix backslashes */
	while ((ptr = strchr(name, '\\')))
	{
		*ptr = '/';
	}

	/* look for it */
	for (i = 0, image = rsx_textures; i < num_rsx_textures; i++, image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	/* load the pic from disk */
	pic = NULL;

	if (strcmp(ext, "pcx") == 0)
	{
		if (r_retexturing->value)
		{
			GetPCXInfo(name, &realwidth, &realheight);
			if(realwidth == 0)
			{
				/* No texture found */
				return NULL;
			}

			/* try to load a tga, png or jpg (in that order/priority) */
			if (  LoadSTB(namewe, "tga", &pic, &width, &height)
			   || LoadSTB(namewe, "png", &pic, &width, &height)
			   || LoadSTB(namewe, "jpg", &pic, &width, &height) )
			{
				/* upload tga or png or jpg */
				image = R_RSX_Image_LoadPic(name, pic, width, realwidth, height,
						realheight, type, 32);
			}
			else
			{
				/* PCX if no TGA/PNG/JPEG available (exists always) */
				LoadPCX(name, &pic, NULL, &width, &height);

				if (!pic)
				{
					/* No texture found */
					return NULL;
				}

				/* Upload the PCX */
				image = R_RSX_Image_LoadPic(name, pic, width, 0, height, 0, type, 8);
			}
		}
		else /* gl_retexture is not set */
		{
			LoadPCX(name, &pic, NULL, &width, &height);

			if (!pic)
			{
				return NULL;
			}

			image = R_RSX_Image_LoadPic(name, pic, width, 0, height, 0, type, 8);
		}
	} // "pcx"
	else if (strcmp(ext, "wal") == 0 || strcmp(ext, "m8") == 0)
	{
		if (r_retexturing->value)
		{
			/* Get size of the original texture */
			if (strcmp(ext, "m8") == 0)
			{
				GetM8Info(name, &realwidth, &realheight);
			}
			else
			{
				GetWalInfo(name, &realwidth, &realheight);
			}

			if(realwidth == 0)
			{
				/* No texture found */
				return NULL;
			}

			/* try to load a tga, png or jpg (in that order/priority) */
			if (  LoadSTB(namewe, "tga", &pic, &width, &height)
			   || LoadSTB(namewe, "png", &pic, &width, &height)
			   || LoadSTB(namewe, "jpg", &pic, &width, &height) )
			{
				/* upload tga or png or jpg */
				image = R_RSX_Image_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
			}
			else if (strcmp(ext, "m8") == 0)
			{
				image = R_RSX_Image_loadM8(namewe, type);
			}
			else
			{
				/* WAL if no TGA/PNG/JPEG available (exists always) */
				image = R_RSX_Image_loadWal(namewe, type);
			}

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
		else if (strcmp(ext, "m8") == 0)
		{
			image = R_RSX_Image_loadM8(name, type);

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
		else /* gl_retexture is not set */
		{
			image = R_RSX_Image_loadWal(name, type);

			if (!image)
			{
				/* No texture found */
				return NULL;
			}
		}
	} // "wal" || "m8"
	else if (strcmp(ext, "tga") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0)
	{
		char tmp_name[256];

		realwidth = 0;
		realheight = 0;

		strcpy(tmp_name, namewe);
		strcat(tmp_name, ".wal");
		GetWalInfo(tmp_name, &realwidth, &realheight);

		if (realwidth == 0 || realheight == 0) {
			strcpy(tmp_name, namewe);
			strcat(tmp_name, ".m8");
			GetM8Info(tmp_name, &realwidth, &realheight);
		}

		if (realwidth == 0 || realheight == 0) {
			/* It's a sky or model skin. */
			strcpy(tmp_name, namewe);
			strcat(tmp_name, ".pcx");
			GetPCXInfo(tmp_name, &realwidth, &realheight);
		}

		/* TODO: not sure if not having realwidth/heigth is bad - a tga/png/jpg
		 * was requested, after all, so there might be no corresponding wal/pcx?
		 * if (realwidth == 0 || realheight == 0) return NULL;
		 */

		if(LoadSTB(name, ext, &pic, &width, &height))
		{
			image = R_RSX_Image_LoadPic(name, pic, width, realwidth, height, realheight, type, 32);
		} else {
			return NULL;
		}
	} // "tga" || "png" || "jpg"
	else
	{
		return NULL;
	}

	if (pic)
	{
		free(pic);
	}

	return image;
}

gl3image_t *
R_RSX_Image_RegisterSkin(char *name)
{
	// static qboolean is_first_call = true;
	// if (is_first_call)
	// {
	// 	printf("%s(%s)\n", __func__, name);
	//	is_first_call = false;
	// }
	return R_RSX_Image_FindImage(name, it_skin);
}

/* 942
 * Any image that was not touched on
 * this registration sequence
 * will be freed.
 */
void
R_RSX_Image_FreeUnusedImages(void)
{
	int i;
	gl3image_t *image;

	/* never free r_notexture or particle texture */
	rsx_no_texture->registration_sequence = registration_sequence;
	rsx_particle_texture->registration_sequence = registration_sequence;

	for (i = 0, image = rsx_textures; i < num_rsx_textures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
		{
			continue; /* used this sequence */
		}

		if (!image->registration_sequence)
		{
			continue; /* free image_t slot */
		}

		if (image->type == it_pic)
		{
			continue; /* don't free pics */
		}

		/* free it */
		rsxFree_with_log(image->data_ptr);
		memset(image, 0, sizeof(*image));
	}
}

// 980
qboolean
R_RSX_Image_HasFreeSpace(void)
{
	int	i, used;
	gl3image_t *image;

	used = 0;

	for (i = 0, image = rsx_textures; i < num_rsx_textures; i++, image++)
	{
		if (!image->name[0])
		{
			continue;
		}
		if (image->registration_sequence == registration_sequence)
		{
			used++;
		}
	}

	if (rsx_textures_max < used)
	{
		rsx_textures_max = used;
	}

	// should same size of free slots as currently used
	return (num_rsx_textures + used) < MAX_RSX_TEXTURES;
}

static qboolean
R_RSX_Image_isNPOT(int v) // 1062
{
	unsigned int uv = v;
	// just try all the power of two values between 1 and 1 << 15 (32k)
	for(unsigned int i=0; i<16; ++i)
	{
		unsigned int pot = (1u << i);
		if(uv & pot)
		{
			return uv != pot;
		}
	}

	return true;
}

void // 1042
R_RSX_Image_ImageList_f(void)
{
	int i, used, texels;
	qboolean	freeup;
	gl3image_t *image;
	const char *formatstrings[2] = {
		"RGB ",
		"RGBA"
	};

	const char* potstrings[2] = {
		" POT", "NPOT"
	};

	R_Printf(PRINT_ALL, "------------------\n");
	texels = 0;
	used = 0;

	for (i = 0, image = rsx_textures; i < num_rsx_textures; i++, image++)
	{
		int w, h;
		char *in_use = "";
		qboolean isNPOT = false;
		if (image->texnum == 0)
		{
			continue;
		}

		if (image->registration_sequence == registration_sequence)
		{
			in_use = "*";
			used++;
		}

		w = image->width;
		h = image->height;

		isNPOT = R_RSX_Image_isNPOT(w) || R_RSX_Image_isNPOT(h);

		texels += w*h;

		switch (image->type)
		{
			case it_skin:
				R_Printf(PRINT_ALL, "M");
				break;
			case it_sprite:
				R_Printf(PRINT_ALL, "S");
				break;
			case it_wall:
				R_Printf(PRINT_ALL, "W");
				break;
			case it_pic:
				R_Printf(PRINT_ALL, "P");
				break;
			case it_sky:
				R_Printf(PRINT_ALL, "Y");
				break;
			default:
				R_Printf(PRINT_ALL, "?");
				break;
		}

		R_Printf(PRINT_ALL, " %3i %3i %s %s: %s %s\n", w, h,
		         formatstrings[image->has_alpha], potstrings[isNPOT], image->name, in_use);
	}

	R_Printf(PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
	freeup = R_RSX_Image_HasFreeSpace();
	R_Printf(PRINT_ALL, "Used %d of %d images%s.\n", used, rsx_textures_max, freeup ? ", has free space" : "");
}