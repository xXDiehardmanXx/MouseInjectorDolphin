//==========================================================================
// Mouse Injector for Dolphin
//==========================================================================
// Copyright (C) 2019 Carnivorous
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
#define DHV_camx 0x8090F8F8 - 0x8090C3F0
#define DHV_camy 0x8090F8F0 - 0x8090C3F0
#define DHV_height 0x8090E384 - 0x8090C3F0
#define DHV_crosshairx 0x8090F908 - 0x8090C3F0
#define DHV_crosshairy 0x8090F90C - 0x8090C3F0
#define DHV_health 0x8090D4A8 - 0x8090C3F0
#define DHV_fov 0x808A28F0 - 0x808A28D8
// STATIC ADDRESSES BELOW
#define DHV_playerbase 0x8032C178 // playable character pointer (random stack address because this game does not keep the player's pointer in one spot)
#define DHV_fovbase 0x8032CA68 // fov base pointer

static uint8_t DHV_Status(void);
static uint8_t DHV_DetectPlayer(void);
static void DHV_Inject(void);

static const GAMEDRIVER GAMEDRIVER_INTERFACE =
{
	"Die Hard: Vendetta",
	DHV_Status,
	DHV_Inject
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
	if(WITHINMEMRANGE(tempfovbase) && tempfov >= 0x3D8F5C29U && tempfov <= 0x3F99999AU)
		fovbase = tempfovbase;
	const uint32_t tempplayerbase = MEM_ReadUInt(DHV_playerbase);
	const uint32_t tempcamx = MEM_ReadUInt(tempplayerbase + DHV_camx);
	const uint32_t tempcamy = MEM_ReadUInt(tempplayerbase + DHV_camy);
	const uint32_t tempcrosshairx = MEM_ReadUInt(tempplayerbase + DHV_crosshairx);
	const uint32_t tempcrosshairy = MEM_ReadUInt(tempplayerbase + DHV_crosshairy);
	const uint32_t temphealth = MEM_ReadUInt(tempplayerbase + DHV_health);
	const uint32_t tempheight = MEM_ReadUInt(tempplayerbase + DHV_height);
	if(WITHINMEMRANGE(tempplayerbase) && tempcamx < 0x40C90FDBU && tempcamy == 0 && tempcrosshairx == 0 && tempcrosshairy == 0 && temphealth > 0 && temphealth <= 0x3F800000U && tempheight == 0x40400000U) // test for valid player pointer
	{
		playerbase = tempplayerbase;
		return 1;
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
		if(camx >= TAU)
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