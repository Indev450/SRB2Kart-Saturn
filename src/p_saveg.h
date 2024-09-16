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

// 1024 bytes is plenty for a savegame
#define SAVEGAMESIZE (1024)

// For netgames
#define NETSAVEGAMESIZE (768*1024)

// Persistent storage/archiving.
// These are the load / save game routines.

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

extern savedata_t savedata;

typedef struct
{
	UINT8 *buffer;
	UINT8 *p;
	UINT8 *end;
	size_t size;
} savebuffer_t;

boolean P_SaveBufferZAlloc(savebuffer_t *save, size_t alloc_size, INT32 tag, void *user);
#define P_SaveBufferAlloc(a,b) P_SaveBufferZAlloc(a, b, PU_STATIC, NULL)
boolean P_SaveBufferFromExisting(savebuffer_t *save, UINT8 *existing_buffer, size_t existing_size);
boolean P_SaveBufferFromLump(savebuffer_t *save, lumpnum_t lump);
boolean P_SaveBufferFromFile(savebuffer_t *save, char const *name);
void P_SaveBufferFree(savebuffer_t *save);

// Persistent storage/archiving.
// These are the load / save game routines.

void P_SaveGame(savebuffer_t *save);
void P_SaveNetGame(savebuffer_t *save, boolean resending);
boolean P_LoadGame(savebuffer_t *save, INT16 mapoverride);
boolean P_LoadNetGame(savebuffer_t *save, boolean reloading);

mobj_t *P_FindNewPosition(UINT32 oldposition);

#endif
