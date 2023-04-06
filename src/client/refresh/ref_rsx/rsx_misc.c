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
 * Misc RSX refresher functions
 *
 * =======================================================================
 */

#include "header/local.h"

gl3image_t *rsx_no_texture; /* use for bad textures */
gl3image_t *rsx_particle_texture; /* little dot for particles */

// 76
static byte dottexture[8][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
};

// 87
void
R_RSX_InitParticleTexture(void)
{
	int x, y;
	byte data[8][8][4];

	/* particle texture */
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y] * 255;
		}
	}

	rsx_particle_texture = R_RSX_Image_LoadPic("***particle***", (byte *)data,
	                                           8, 0, 8, 0, it_sprite, 32);
}

void
R_RSX_InitNoTexture(void)
{
    int x, y;
    byte data[8][8][4];
    /* also use this for bad textures, but without alpha */
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = dottexture[x & 3][y & 3] * 255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}

	rsx_no_texture = R_RSX_Image_LoadPic("***r_notexture***", (byte *)data,
	                                     8, 0, 8, 0, it_wall, 32);
}

void
R_RSX_ScreenShot(void)
{
	NOT_IMPLEMENTED();
}

void *last_rsxMemalign_ptr = NULL;
void *rsxMemalignUtil(uint32_t alignment, uint32_t size)
{
	last_rsxMemalign_ptr = rsxMemalign(alignment, size);
	return last_rsxMemalign_ptr;
}