// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2019 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief globals (shared data & code) for hw_ modules

#ifndef _HWR_GLOB_H_
#define _HWR_GLOB_H_

#include "hw_defs.h"
#include "../m_misc.h"
#include "../r_defs.h"

// Uncomment this to enable the OpenGL loading screen
//#define HWR_LOADING_SCREEN

// needed for sprite rendering
// equivalent of the software renderer's vissprites
typedef struct gl_vissprite_s
{
	float x1, x2;
	float z1, z2;
	float gz, gzt;
	float tz;
	float scale;
	float spritexscale, spriteyscale;
	float spritexoffset, spriteyoffset;
	GLPatch_t *gpatch;
	boolean flip;
	UINT8 translucency;       //alpha level 0-255
	mobj_t *mobj;
	boolean precip; // Tails 08-25-2002
	boolean vflip;
   //Hurdler: 25/04/2000: now support colormap in hardware mode
	UINT8 *colormap;
	INT32 dispoffset; // copy of info->dispoffset, affects ordering but not drawing
} gl_vissprite_t;

// --------
// hw_bsp.c
// --------
extern extrasubsector_t *extrasubsectors;
extern size_t addsubsector;

// --------
// hw_cache.c
// --------
void HWR_InitTextureCache(void);
void HWR_FreeTextureCache(void);
void HWR_FreeMipmapCache(void);
void HWR_FreeExtraSubsectors(void);

void HWR_GetFlat(lumpnum_t flatlumpnum, boolean noencoremap);
// ^ some flats must NOT be remapped to encore, since we remap them as we cache them for ease, adding a toggle here seems wise.

GLMapTexture_t *HWR_GetTexture(INT32 tex, boolean noencore);
void HWR_GetPatch(GLPatch_t *gpatch);
void HWR_GetMappedPatch(GLPatch_t *gpatch, const UINT8 *colormap);
void HWR_MakePatch(patch_t *patch, GLPatch_t *glPatch, GLMipmap_t *glMipmap, boolean makebitmap);
void HWR_UnlockCachedPatch(GLPatch_t *gpatch);
void HWR_SetPalette(RGBA_t *palette);

void HWR_SetMapPalette(void);
UINT32 HWR_CreateLightTable(UINT8 *lighttable);
UINT32 HWR_GetLightTableID(extracolormap_t *colormap);
void HWR_ClearLightTables(void);
GLPatch_t *HWR_GetCachedGLPatchPwad(UINT16 wad, UINT16 lump);
GLPatch_t *HWR_GetCachedGLPatch(lumpnum_t lumpnum);
void HWR_GetFadeMask(lumpnum_t fademasklumpnum);

// --------
// hw_draw.c
// --------
extern INT32 patchformat;
extern INT32 textureformat;

// --------
// hw_shaders.c
// --------
boolean HWR_InitShaders(void);
void HWR_CompileShaders(void);

int HWR_GetShaderFromTarget(int shader_target);

void HWR_LoadAllCustomShaders(void);
void HWR_LoadCustomShadersFromFile(UINT16 wadnum, boolean PK3);
const char *HWR_GetShaderName(INT32 shader);

extern customshaderxlat_t shaderxlat[];

#endif //_HW_GLOB_
