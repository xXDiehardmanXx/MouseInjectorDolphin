//==========================================================================
// Mouse Injector for Dolphin
//==========================================================================
// Copyright (C) 2019-2020 Carnivorous
// All rights reserved.
//
// Mouse Injector is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, visit http://www.gnu.org/licenses/gpl-2.0.html
//==========================================================================
#include <stdint.h>
#include "../main.h"
#include "../memory.h"
#include "../mouse.h"
#include "game.h"

#define PI 3.14159265f // 0x40490FDB
#define TAU 6.2831853f // 0x40C90FDB
#define CAMYLIMITPLUS 1.308996916f // 0x3FA78D36
#define CAMYLIMITMINUS -1.308996916f // 0xBFA78D36
// SERIOUS ADDRESSES - OFFSET ADDRESSES BELOW (REQUIRES PLAYERBASE TO USE)
#define SERIOUS_camx 0x8133C930 - 0x8133C6A0
#define SERIOUS_camy 0x8133C934 - 0x8133C6A0
#define SERIOUS_fov 0x8133C99C - 0x8133C6A0
// STATIC ADDRESSES BELOW
#define SERIOUS_playerbase 0x802D8948 // playable character pointer

static uint8_t SERIOUS_Status(void);
static void SERIOUS_Inject(void);

static const GAMEDRIVER GAMEDRIVER_INTERFACE =
{
	"Serious Sam: Next Encounter",
	SERIOUS_Status,
	SERIOUS_Inject,
	13, // if tickrate is any lower, mouse input will get sluggish
	0 // crosshair sway not supported for driver
};

const GAMEDRIVER *GAME_SERIOUS = &GAMEDRIVER_INTERFACE;

//==========================================================================
// Purpose: return 1 if game is detected
//==========================================================================
static uint8_t SERIOUS_Status(void)
{
	return (MEM_ReadUInt(0x80000000) == 0x47334245U && MEM_ReadUInt(0x80000004) == 0x39470001U); // check game header to see if it matches SERIOUS
}
//==========================================================================
// Purpose: calculate mouse look and inject into current game
//==========================================================================
static void SERIOUS_Inject(void)
{
	if(xmouse == 0 && ymouse == 0) // if mouse is idle
		return;
	const uint32_t playerbase = MEM_ReadUInt(SERIOUS_playerbase);
	if(!playerbase) // if playerbase is invalid
		return;
	const float looksensitivity = (float)sensitivity / 40.f;
	const float fov = MEM_ReadFloat(playerbase + SERIOUS_fov);
	float camx = MEM_ReadFloat(playerbase + SERIOUS_camx);
	float camy = MEM_ReadFloat(playerbase + SERIOUS_camy);
	if(camx > -TAU && camx < TAU && fov > 9.f)
	{
		camx += (float)xmouse / 10.f * looksensitivity / (360.f / TAU) * (fov / 90); // normal calculation method for X
		camy += (float)(invertpitch ? -ymouse : ymouse) / 10.f * looksensitivity / (90.f / PI) * (fov / 90); // normal calculation method for Y
		while(camx >= TAU)
			camx -= TAU;
		while(camx <= TAU)
			camx += TAU;
		camy = ClampFloat(camy, CAMYLIMITMINUS, CAMYLIMITPLUS);
		MEM_WriteFloat(playerbase + SERIOUS_camx, camx);
		MEM_WriteFloat(playerbase + SERIOUS_camy, camy);
	}
}