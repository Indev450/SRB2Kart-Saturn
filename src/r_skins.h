// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2025 by Vivian "toastergrl" Grannell.
// Copyright (C) 2025 by Kart Krew.
// Copyright (C) 2020 by Sonic Team Junior.
// Copyright (C) 2000 by DooM Legacy Team.
// Copyright (C) 1996 by id Software, Inc.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_skins.h
/// \brief Skins stuff

#ifndef __R_SKINS__
#define __R_SKINS__
#ifdef __cplusplus
extern "C" {
#endif

#include "d_player.h"
#include "sounds.h"
#include "r_patch.h"

// "Left" and "Right" character symbols for additional rotation functionality
#define ROT_L ('L' - '0')
#define ROT_R ('R' - '0')

#define SKINNAMESIZE 16
// should be all lowercase!! S_SKIN processing does a strlwr
#define DEFAULTSKIN "sonic"
#define DEFAULTSKIN2 "tails" // secondary player
#define DEFAULTSKIN3 "knuckles" // third player
#define DEFAULTSKIN4 "eggman" // fourth player

typedef struct
{
	char name[SKINNAMESIZE+1]; // INT16 descriptive name of the skin
	spritedef_t spritedef;
	spriteinfo_t sprinfo;
	UINT16 wadnum;
	char sprite[4]; // Sprite name, if seperated from S_SKIN.
	skinflags_t flags;

	char realname[SKINNAMESIZE+1]; // Display name for level completion.
	char hudname[SKINNAMESIZE+1]; // HUD name to display (officially exactly 5 characters long)
	char facerank[9], facewant[9], facemmap[9]; // Arbitrarily named patch lumps

	// SRB2kart
	UINT8 kartspeed;
	UINT8 kartweight;
	//

	// Definable color translation table
	UINT8 starttranscolor;
	UINT8 prefcolor;
	fixed_t highresscale; // scale of highres, default is 0.5

	// specific sounds per skin
	sfxenum_t soundsid[NUMSKINSOUNDS]; // sound # in S_sfx table

	boolean localskin;
	INT32 localnum;
} skin_t;

extern CV_PossibleValue_t Forceskin_cons_t[];

extern INT32 numskins;
extern INT32 numlocalskins;
extern INT32 numallskins;
extern skin_t skins[MAXSKINS];
extern UINT8 skinstats[9][9][MAXSKINS];
extern UINT8 skinstatscount[9][9];
extern UINT8 skinsorted[MAXSKINS];

extern skin_t localskins[MAXLOCALSKINS];
extern skin_t allskins[MAXSKINS+MAXLOCALSKINS];

//faB: find sprites in wadfile, replace existing, add new ones
//     (only sprites from namelist are added or replaced)
void R_AddSpriteDefs(UINT16 wadnum);

void R_InitSkins(void);
void R_AddSkins(UINT16 wadnum, boolean local);

INT32 R_SkinAvailable(const char *name);
INT32 R_AnySkinAvailable(const char *name);
INT32 R_LocalSkinAvailable(const char *name, boolean local);

// had to move those here Zzz...
INT32 K_GetSkinNum(player_t *player);
INT32 K_GetMobjSkinNum(const skin_t *skin, boolean local);
skin_t *K_GetPlayerSkin(player_t *player);
skin_t *K_GetMobjSkin(const mobj_t *mobj);
patch_t *K_GetFacePrefix(player_t *player, INT32 skinnum);
skin_t *K_GetSkinArray(boolean local);

void sortSkinGrid(void);

boolean SetPlayerSkin(INT32 playernum,const char *skinname);
void SetPlayerSkinByNum(INT32 playernum,INT32 skinnum); // Tails 03-16-2002
void SetLocalPlayerSkin(INT32 playernum,const char *skinname, consvar_t *cvar);

char *GetPlayerFacePic(INT32 skinnum);

// Functions to go from sprite character ID to frame number
// for 2.1 compatibility this still uses the old 'A' + frame code
// The use of symbols tends to be painful for wad editors though
// So the future version of this tries to avoid using symbols
// as much as possible while also defining all 64 slots in a sane manner
// 2.1:    [[ ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~   ]]
// Future: [[ ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz!@ ]]
FUNCMATH FUNCINLINE static ATTRINLINE char R_Frame2Char(UINT8 frame)
{
#if 1 // 2.1 compat
	return 'A' + frame;
#else
	if (frame < 26) return 'A' + frame;
	if (frame < 36) return '0' + (frame - 26);
	if (frame < 62) return 'a' + (frame - 36);
	if (frame == 62) return '!';
	if (frame == 63) return '@';
	return '\xFF';
#endif
}

FUNCMATH FUNCINLINE static ATTRINLINE UINT8 R_Char2Frame(char cn)
{
#if 1 // 2.1 compat
	if (cn == '+') return '\\' - 'A'; // PK3 can't use backslash, so use + instead
	return cn - 'A';
#else
	if (cn >= 'A' && cn <= 'Z') return cn - 'A';
	if (cn >= '0' && cn <= '9') return (cn - '0') + 26;
	if (cn >= 'a' && cn <= 'z') return (cn - 'a') + 36;
	if (cn == '!') return 62;
	if (cn == '@') return 63;
	return 255;
#endif
}

FUNCMATH FUNCINLINE static ATTRINLINE boolean R_ValidSpriteAngle(UINT8 rotation)
{
	return ((rotation <= 8) || (rotation == ROT_L) || (rotation == ROT_R));
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__R_SKINS__
