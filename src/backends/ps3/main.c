/*
 * Copyright (C) 1997-2001 Id Software, Inc.
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
 * This file is the starting point of the program. It does some platform
 * specific initialization stuff and calls the common initialization code.
 *
 * =======================================================================
 */

#include "sys/process.h"

#include "../../common/header/common.h"

// Setting process priority and stack size
// Fixes crushes on huge stack size usage while running
// as EBOOT.BIN launched directly from XMB.
// Values taken from some random multiMan source code,
// as all testing durring development done using it.
SYS_PROCESS_PARAM(1200, 0x100000)

int
main(int argc, char **argv)
{
	Sys_SetWorkDir("/dev_hdd0/game/QUAKE2_00");

	// Call the initialization code.
	// Never returns.
	Qcommon_Init(argc, argv);

	return 0;
}
