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
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include "memory.h"

#define DOLPHIN_PTR 0xF84800 // offset within dolphin.exe module that points to gamecube memory

static uint64_t emuoffset = 0;
static HANDLE emuhandle = NULL;

uint8_t MEM_Init(void);
void MEM_Quit(void);
void MEM_UpdateEmuoffset(void);
int32_t MEM_ReadInt(const uint32_t addr);
uint32_t MEM_ReadUInt(const uint32_t addr);
float MEM_ReadFloat(const uint32_t addr);
void MEM_WriteInt(const uint32_t addr, int32_t value);
void MEM_WriteUInt(const uint32_t addr, uint32_t value);
void MEM_WriteFloat(const uint32_t addr, float value);
static void MEM_ByteSwap32(uint32_t *input);

//==========================================================================
// Purpose: initialize dolphin handle and setup for memory injection
// Changed Globals: emuhandle
//==========================================================================
uint8_t MEM_Init(void)
{
	emuhandle = NULL;
	HANDLE processes; // will store a snapshot of all processes
	PROCESSENTRY32 pe32; // stores basic info of a process, using this one to read the ProcessID from
	processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // make process snapshot
	pe32.dwSize = sizeof(PROCESSENTRY32); // correct size
	Process32First(processes, &pe32); // read info about the first process into pe32
	do // loop to find emulator
	{
		if(strcmp(pe32.szExeFile, "Dolphin.exe") == 0) // if dolphin was found
		{
			emuhandle = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION, FALSE, pe32.th32ProcessID);
			break;
		}
	}
	while(Process32Next(processes, &pe32)); // loop continued until Process32Next deliver NULL or its interrupted with the "break" above
	CloseHandle(processes);
	return (emuhandle != NULL);
}
//==========================================================================
// Purpose: close emuhandle safely
// Changed Globals: emuhandle
//==========================================================================
void MEM_Quit(void)
{
	if(emuhandle != NULL)
		CloseHandle(emuhandle);
}
//==========================================================================
// Purpose: update emuoffset pointer to location of gamecube memory
// Changed Globals: emuoffset
//==========================================================================
void MEM_UpdateEmuoffset(void)
{
	emuoffset = 0;
	HMODULE modules[1024]; // stores all modules for process
	DWORD totalbytes; // the number of bytes required to store all module handles in the modules array
	if(EnumProcessModules(emuhandle, modules, sizeof(modules), &totalbytes)) // get a list of all the modules in dolphin
	{
		for(uint32_t index = 0; index < (totalbytes / sizeof(HMODULE)); index++)
		{
			TCHAR szModName[MAX_PATH]; // get the full path to the module's file
			if(GetModuleFileNameEx(emuhandle, modules[index], szModName, sizeof(szModName) / sizeof(TCHAR)))
			{
				if(strstr(szModName, "Dolphin.exe")) // scan for dolphin.exe (usually the first entry in module array)
				{
					uint64_t gamebase = (uint64_t)modules[index];
					ReadProcessMemory(emuhandle, (LPVOID)(gamebase + DOLPHIN_PTR), &emuoffset, sizeof(emuoffset), NULL);
					break;
				}
			}
		}
	}
}
//==========================================================================
// Purpose: read int from memory
// Parameter: address location
//==========================================================================
int32_t MEM_ReadInt(const uint32_t addr)
{
	if(!emuoffset || NOTWITHINMEMRANGE(addr)) // if gamecube memory has not been init by dolphin or reading from outside of memory range
		return 0;
	int32_t output; // temp var used for output of function
	ReadProcessMemory(emuhandle, (LPVOID)(emuoffset + (addr - 0x80000000)), &output, sizeof(output), NULL);
	MEM_ByteSwap32((uint32_t *)&output); // byteswap
	return output;
}
//==========================================================================
// Purpose: read unsigned int from memory
// Parameter: address location
//==========================================================================
uint32_t MEM_ReadUInt(const uint32_t addr)
{
	if(!emuoffset || NOTWITHINMEMRANGE(addr)) // if gamecube memory has not been init by dolphin or reading from outside of memory range
		return 0;
	uint32_t output; // temp var used for output of function
	ReadProcessMemory(emuhandle, (LPVOID)(emuoffset + (addr - 0x80000000)), &output, sizeof(output), NULL);
	MEM_ByteSwap32(&output); // byteswap
	return output;
}
//==========================================================================
// Purpose: read float from memory
// Parameter: address location
//==========================================================================
float MEM_ReadFloat(const uint32_t addr)
{
	if(!emuoffset || NOTWITHINMEMRANGE(addr)) // if gamecube memory has not been init by dolphin or reading from outside of memory range
		return 0;
	float output; // temp var used for output of function
	ReadProcessMemory(emuhandle, (LPVOID)(emuoffset + (addr - 0x80000000)), &output, sizeof(output), NULL);
	MEM_ByteSwap32((uint32_t *)&output); // byteswap
	return output;
}
//==========================================================================
// Purpose: write int to memory
// Parameter: address location and value
//==========================================================================
void MEM_WriteInt(const uint32_t addr, int32_t value)
{
	if(!emuoffset || NOTWITHINMEMRANGE(addr)) // if gamecube memory has not been init by dolphin or writing to outside of memory range
		return;
	MEM_ByteSwap32((uint32_t *)&value); // byteswap
	WriteProcessMemory(emuhandle, (LPVOID)(emuoffset + (addr - 0x80000000)), &value, sizeof(value), NULL);
}
//==========================================================================
// Purpose: write unsigned int to memory
// Parameter: address location and value
//==========================================================================
void MEM_WriteUInt(const uint32_t addr, uint32_t value)
{
	if(!emuoffset || NOTWITHINMEMRANGE(addr)) // if gamecube memory has not been init by dolphin or writing to outside of memory range
		return;
	MEM_ByteSwap32(&value); // byteswap
	WriteProcessMemory(emuhandle, (LPVOID)(emuoffset + (addr - 0x80000000)), &value, sizeof(value), NULL);
}
//==========================================================================
// Purpose: write float to memory
// Parameter: address location and value
//==========================================================================
void MEM_WriteFloat(const uint32_t addr, float value)
{
	if(!emuoffset || NOTWITHINMEMRANGE(addr)) // if gamecube memory has not been init by dolphin or writing to outside of memory range
		//
		return;
	MEM_ByteSwap32((uint32_t *)&value); // byteswap
	WriteProcessMemory(emuhandle, (LPVOID)(emuoffset + (addr - 0x80000000)), &value, sizeof(value), NULL);
}
//==========================================================================
// Purpose: byteswap input value
// Parameter: pointer of value (must be 4 byte long)
//==========================================================================
static void MEM_ByteSwap32(uint32_t *input)
{
	const uint8_t *inputarray = ((uint8_t *)input); // set byte array to input
	*input = (uint32_t)((inputarray[0] << 24) | (inputarray[1] << 16) | (inputarray[2] << 8) | (inputarray[3])); // reassign input to swapped value
}
