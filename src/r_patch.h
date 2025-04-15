// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2018-2019 by Jaime "Lactozilla" Passos.
// Copyright (C) 2019      by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_patch.h
/// \brief Patch generation.

#ifndef __R_PATCH__
#define __R_PATCH__

#include "r_defs.h"
#include "r_fps.h"
#include "doomdef.h"

// Structs
typedef enum
{
	ROTAXIS_X, // Roll (the default)
	ROTAXIS_Y, // Pitch
	ROTAXIS_Z  // Yaw
} rotaxis_t;

typedef struct
{
	INT32 x, y;
	rotaxis_t rotaxis;
} spriteframepivot_t;

typedef struct
{
	spriteframepivot_t pivot[64];
	boolean available;
} spriteinfo_t;

// Patch functions
patch_t *Patch_Create(softwarepatch_t *source, size_t srcsize, void *dest);
void Patch_Free(patch_t *patch);

#define Patch_FreeTag(tagnum) Patch_FreeTags(tagnum, tagnum)
void Patch_FreeTags(INT32 lowtag, INT32 hightag);

#ifdef HWRENDER
void *Patch_AllocateHardwarePatch(patch_t *patch);
void *Patch_CreateGL(patch_t *patch);
#endif

// Conversions between patches / flats / textures...
boolean R_CheckIfPatch(lumpnum_t lump);

// SpriteInfo
extern spriteinfo_t spriteinfo[NUMSPRITES];
void R_LoadSpriteInfoLumps(UINT16 wadnum, UINT16 numlumps);
void R_ParseSPRTINFOLump(UINT16 wadNum, UINT16 lumpNum);

// Sprite rotation
#ifdef ROTSPRITE
INT32 R_GetRollAngle(angle_t rollangle);
angle_t R_RotationAngle(angle_t ang, angle_t camang, interpmobjstate_t *interp);

patch_t *Patch_GetRotatedSprite(spriteframe_t *sprite, size_t frame, size_t spriteangle, boolean flip, void *info, INT32 rotationangle);
rotsprite_t *RotatedPatch_Create(INT32 numangles);
void RotatedPatch_DoRotation(rotsprite_t *rotsprite, patch_t *patch, INT32 angle, INT32 xpivot, INT32 ypivot, boolean flip);

extern fixed_t rollcosang[ROTANGLES];
extern fixed_t rollsinang[ROTANGLES];
#endif

#endif // __R_PATCH__
