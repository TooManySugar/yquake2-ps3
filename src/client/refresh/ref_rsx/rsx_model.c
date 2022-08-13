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
 * Model loading and caching for TODO desc. Includes the .bsp file format
 *
 * =======================================================================
 */

#include "header/local.h"

/*
 * Specifies the model that will be used as the world
 */
// 1079
void
rsx_begin_registration(char *model)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s(%s)\n", __func__, model);
		is_first_call = false;
	}
}

/*
 * 
 */
// 1107
struct model_s *
rsx_register_model(char *name)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s(%s)\n", __func__, name);
		is_first_call = false;
	}
	return NULL;
}

// 1154
void
rsx_end_registration(void)
{
	static qboolean is_first_call = true;
	if (is_first_call)
	{
		printf("%s()\n", __func__);
		is_first_call = false;
	}
}