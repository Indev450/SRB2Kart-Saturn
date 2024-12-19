// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  dehacked.h
/// \brief Dehacked files.

#ifndef __DEHACKED_H__
#define __DEHACKED_H__

#include "m_fixed.h" // for get_number
#include "info.h"

// Free slot names
// The crazy word-reading stuff uses these.
extern char *FREE_STATES[NUMSTATEFREESLOTS];
extern char *FREE_MOBJS[NUMMOBJFREESLOTS];
extern UINT8 used_spr[(NUMSPRITEFREESLOTS / 8) + 1]; // Bitwise flag for sprite freeslot in use! I would use ceil() here if I could, but it only saves 1 byte of memory anyway.

extern const char *const STATE_LIST[];
extern const char *const MOBJTYPE_LIST[];
extern const char *const MOBJFLAG_LIST[];
extern const char *const MOBJFLAG2_LIST[]; // \tMF2_(\S+).*// (.+) --> \t"\1", // \2
extern const char *const MOBJEFLAG_LIST[];
extern const char *const MAPTHINGFLAG_LIST[4];
extern const char *const PLAYERFLAG_LIST[];
extern const char *const ML_LIST[]; // Linedef flags
extern const char *COLOR_ENUMS[];
extern const char *const POWERS_LIST[];
extern const char *KARTSTUFF_LIST[];
extern const char *const HUDITEMS_LIST[];

void DEH_LoadDehackedLump(lumpnum_t lumpnum);
void DEH_LoadDehackedLumpPwad(UINT16 wad, UINT16 lump);

void DEH_UpdateMaxFreeslots(void);

void DEH_Check(void);

fixed_t get_number(const char *word);

boolean LUA_SetLuaAction(void *state, const char *actiontocompare);
const char *LUA_GetActionName(void *action);
void LUA_SetActionByName(void *state, const char *actiontocompare);
enum actionnum LUA_GetActionNumByName(const char *actiontocompare);

extern boolean deh_loaded;

#define MAXRECURSION 30
extern const char *superactions[MAXRECURSION];
extern UINT8 superstack;

// If the dehacked patch does not match this version, we throw a warning
#define PATCHVERSION 1

#define MAXLINELEN 1024

// the code was first write for a file
// converted to use memory with this functions
typedef struct
{
	char *data;
	char *curpos;
	size_t size;
	UINT16 wad;
} MYFILE;
#define myfeof(a) (a->data + a->size <= a->curpos)
char *myfgets(char *buf, size_t bufsize, MYFILE *f);
#endif
