// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_saveg.h
/// \brief Savegame I/O, archiving, persistence

#ifndef __P_SAVEG__
#define __P_SAVEG__

#ifdef __GNUG__
#pragma interface
#endif

typedef struct
{
	UINT8 skincolor;
	UINT8 skin;
	UINT8 botskin;
	UINT8 botcolor;
	INT32 score;
	INT32 lives;
	INT32 continues;
	UINT16 emeralds;
} savedata_t;

typedef struct
{
	UINT8 buffer[16 * 1024 * 1024]; // 16 megs per frame should do
} savestate_t;

// Persistent storage/archiving.
// These are the load / save game routines.

void P_SaveGame(void);
void P_SaveNetGame(void);
void P_SaveGameState(savestate_t* savestate);
boolean P_LoadGame(INT16 mapoverride);
boolean P_LoadNetGame(boolean preserveLevel);
boolean P_LoadGameState(const savestate_t* savestate);
void P_GameStateFreeMemory(savestate_t* savestate);

mobj_t *P_FindNewPosition(UINT32 oldposition);

extern savedata_t savedata;
extern UINT8 *save_p;

#endif
