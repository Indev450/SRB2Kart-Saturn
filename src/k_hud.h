// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2018-2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_hud.h
/// \brief HUD drawing functions exclusive to Kart

#ifndef __K_HUD__
#define __K_HUD__

#ifdef __cplusplus
extern "C" {
#endif

#include "doomdef.h"
#include "d_player.h" // Need for player_t
#include "command.h"
#include "r_defs.h"

UINT8 K_GetHudColor(void);
boolean K_UseColorHud(void);
boolean K_UseHighResPortraits(void);

void K_RegisterKartHudStuff(void);

extern consvar_t cv_colorizedhud,
				 cv_colorizeditembox,
				 cv_colorizedhudcolor;

extern consvar_t cv_darkitembox;
extern consvar_t cv_biglaps;
extern consvar_t cv_highresportrait;
extern consvar_t cv_showstats;
extern consvar_t cv_showstats_skinname;
extern consvar_t cv_fancyroulette;
extern consvar_t cv_showlaptimes;
extern consvar_t cv_battlespeedo;
extern consvar_t cv_multiitemicon;
extern consvar_t cv_huditemamount;
extern consvar_t cv_roulettecolor;

// for use in timeattack menu
extern patch_t *kp_facenum[MAXPLAYERS+1];

#define NUMSPEEDOSTUFF 8
extern CV_PossibleValue_t speedo_cons_t[NUMSPEEDOSTUFF];
#define NUMDGAUGESTUFF 6
extern CV_PossibleValue_t driftgaugestyle_cons_t[NUMDGAUGESTUFF];
#define NUMINPUTDISPLAYSTUFF 5
extern CV_PossibleValue_t inputdisplay_cons_t[NUMINPUTDISPLAYSTUFF];
#define NUMMINIMAPDOTSTUFF 5
extern CV_PossibleValue_t minimapdot_cons_t[NUMMINIMAPDOTSTUFF];

void K_KartPlayerHUDUpdate(player_t *player);

const char *K_GetItemPatch(UINT8 item, boolean tiny);
INT32 K_calcSplitFlags(INT32 snapflags);
void K_LoadKartHUDGraphics(void);
void K_drawKartHUD(void);
void K_drawKartFreePlay(UINT32 flashtime);
void K_drawKartTimestamp(tic_t drawtime, INT32 TX, INT32 TY, INT16 emblemmap, UINT8 mode);

typedef struct
{
	INT32 x;
	INT32 y;
	INT32 flags;
} drawinfo_t;

patch_t *K_getItemBoxPatch(boolean small, boolean dark);
patch_t *K_getItemMulPatch(boolean small);
void K_getItemBoxDrawinfo(drawinfo_t *out);
INT32 K_getMinimapTrans(void);
void K_getLapsDrawinfo(drawinfo_t *out);
void K_getMinimapDrawinfo(drawinfo_t *out);

#ifdef __cplusplus
} // extern "C"
#endif

// =========================================================================
#endif  // __K_HUD__
