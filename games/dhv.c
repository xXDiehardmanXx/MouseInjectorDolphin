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

#define TAU 6.2831853f // 0x40C90FDB
#define CAMYPLUS 1.336848021f // 0x3FAB1DD6
#define CAMYMINUS -0.9973310232f // 0xBF7F5116
#define CROSSHAIRX 0.5399999022f // 0x3F0A3D6F
#define CROSSHAIRY 0.3779999614f // 0x3EC18936
// DHV ADDRESSES - OFFSET ADDRESSES BELOW (REQUIRES PLAYERBASE TO USE)
#define DHV_camx 0x811DCBA8 - 0x811D96A0
#define DHV_camy 0x811DCBA0 - 0x811D96A0
#define DHV_height 0x811DB634 - 0x811D96A0
#define DHV_crosshairx 0x811DCBB8 - 0x811D96A0
#define DHV_crosshairy 0x811DCBBC - 0x811D96A0
#define DHV_health 0x811DA758 - 0x811D96A0
#define DHV_fov 0x803459C8 - 0x8032D140
// STATIC ADDRESSES BELOW
#define DHV_playerbase 0x8032C02C // random stack address, commonly holds player pointer - requires sanity checks before using!
#define DHV_fovbase 0x80317F30

static uint8_t DHV_Status(void);
static uint8_t DHV_DetectPlayer(void);
static void DHV_Inject(void);

static const GAMEDRIVER GAMEDRIVER_INTERFACE =
{
	"Die Hard: Vendetta",
	DHV_Status,
	DHV_Inject,
	1, // 1000 Hz tickrate
	1 // crosshair sway supported for driver
};

const GAMEDRIVER *GAME_DHV = &GAMEDRIVER_INTERFACE;

static uint32_t playerbase = 0;
static uint32_t fovbase = 0;

//==========================================================================
// Purpose: return 1 if game is detected
//==========================================================================
static uint8_t DHV_Status(void)
{
	return (MEM_ReadUInt(0x80000000) == 0x47444945U && MEM_ReadUInt(0x80000004) == 0x37440000U); // check game header to see if it matches DHV
}
//==========================================================================
// Purpose: detects player pointer from stack address
// Changed Globals: fovbase, playerbase
//==========================================================================
static uint8_t DHV_DetectPlayer(void)
{
	const uint32_t tempfovbase = MEM_ReadUInt(DHV_fovbase);
	const uint32_t tempfov = MEM_ReadUInt(tempfovbase + DHV_fov);
	if(WITHINMEMRANGE(tempfovbase) && tempfov >= 0x3D8F5C29U && tempfov <= 0x3F99999AU) // if fov base is valid, use fov pointer for level
		fovbase = tempfovbase;
	const uint32_t tempplayerbase = MEM_ReadUInt(DHV_playerbase);
	if(WITHINMEMRANGE(tempplayerbase) && tempplayerbase != playerbase) // if pointer is valid, sanity check pointer
	{
		const uint32_t temphealth = MEM_ReadUInt(tempplayerbase + DHV_health);
		const uint32_t tempheight = MEM_ReadUInt(tempplayerbase + DHV_height);
		if(temphealth > 0 && temphealth <= 0x3F800000U && tempheight == 0x40400000U) // if player base is valid, use player pointer for level
		{
			playerbase = tempplayerbase;
			return 1;
		}
	}
	return WITHINMEMRANGE(playerbase);
}
//==========================================================================
// Purpose: calculate mouse look and inject into current game
//==========================================================================
static void DHV_Inject(void)
{
	if(!DHV_DetectPlayer()) // if player pointer was not found
		return;
	if(xmouse == 0 && ymouse == 0) // if mouse is idle
		return;
	float camx = MEM_ReadFloat(playerbase + DHV_camx);
	float camy = MEM_ReadFloat(playerbase + DHV_camy);
	const float fov = MEM_ReadFloat(fovbase + DHV_fov);
	const float health = MEM_ReadFloat(playerbase + DHV_health);
	const float looksensitivity = (float)sensitivity / 40.f;
	const float crosshairsensitivity = ((float)crosshair / 100.f) * looksensitivity;
	if(camx >= 0 && camx < TAU && health > 0 && fov > 0 && fov <= 1.2f)
	{
		camx -= (float)xmouse / 10.f * looksensitivity / (360.f / TAU) / (1.2f / fov); // normal calculation method for X
		camy += (float)(invertpitch ? ymouse : -ymouse) / 10.f * looksensitivity / (360.f / TAU) / (1.2f / fov); // normal calculation method for Y
		while(camx >= TAU)
			camx -= TAU;
		camy = ClampFloat(camy, CAMYMINUS, CAMYPLUS);
		MEM_WriteFloat(playerbase + DHV_camx, camx);
		MEM_WriteFloat(playerbase + DHV_camy, camy);
		if(crosshair) // if crosshair sway is enabled
		{
			float crosshairx = MEM_ReadFloat(playerbase + DHV_crosshairx); // after camera x and y have been calculated and injected, calculate the crosshair/gun sway
			float crosshairy = MEM_ReadFloat(playerbase + DHV_crosshairy);
			crosshairx -= (float)xmouse / 80.f * crosshairsensitivity / (1.2f / fov);
			crosshairy -= (float)(!invertpitch ? ymouse : -ymouse) / 80.f * crosshairsensitivity / (1.2f / fov);
			MEM_WriteFloat(playerbase + DHV_crosshairx, ClampFloat(crosshairx, -CROSSHAIRX, CROSSHAIRX));
			MEM_WriteFloat(playerbase + DHV_crosshairy, ClampFloat(crosshairy, -CROSSHAIRY, CROSSHAIRY));
		}
	}
}