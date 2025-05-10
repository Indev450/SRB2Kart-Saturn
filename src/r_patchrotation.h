// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020-2023 by Lactozilla.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_patchrotation.h
/// \brief Patch rotation.

#ifndef __R_PATCHROTATION__
#define __R_PATCHROTATION__

#include "r_fps.h"

// Sprite rotation
#ifdef ROTSPRITE
INT32 R_GetRollAngle(angle_t rollangle);
angle_t R_RotationAngle(angle_t ang, angle_t camang, interpmobjstate_t *interp);

rotsprite_t *RotatedPatch_Create(INT32 numangles);
void RotatedPatch_DoRotation(rotsprite_t *rotsprite, patch_t *patch, INT32 angle, INT32 xpivot, INT32 ypivot, boolean flip);
patch_t *Patch_GetRotatedSprite(spriteframe_t *sprite, size_t frame, size_t spriteangle, boolean flip, void *info, INT32 rotationangle);

extern fixed_t rollcosang[ROTANGLES];
extern fixed_t rollsinang[ROTANGLES];
#endif

#endif // __R_PATCHROTATION__
