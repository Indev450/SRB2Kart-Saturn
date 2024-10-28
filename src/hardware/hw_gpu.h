// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_gpu.h
/// \brief GPU low-level interface API

#ifndef __HWR_GPU_H__
#define __HWR_GPU_H__

#include "../screen.h"
#include "hw_data.h"
#include "hw_defs.h"
#include "hw_md2.h"

#define SCREENVERTS 10

boolean Init (void);
void FinishUpdate (INT32 waitvbl);
void SetupGLInfo (void);

void SetSpecialState (hwdspecialstate_t IdState, INT32 Value);
void SetTransform (FTransform *stransform);
void SetBlend (FBITFIELD PolyFlags);
void SetPalette (RGBA_t *palette);
void ClearBuffer (FBOOLEAN ColorMask, FBOOLEAN DepthMask, FBOOLEAN StencilMask, FRGBAFloat * ClearColor);

void DrawPolygon (FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags);
void DrawIndexedTriangles (FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, unsigned int *IndexArray);
void Draw2DLine (F2DCoord *v1, F2DCoord *v2, RGBA_t Color);
void DrawModel (model_t *model, INT32 frameIndex, float duration, float tics, INT32 nextFrameIndex, FTransform *pos, float hscale, float vscale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface);
void RenderSkyDome (INT32 tex, INT32 texture_width, INT32 texture_height, FTransform transform);

void SetTexture (GLMipmap_t *pTexInfo);
void UpdateTexture (GLMipmap_t *pTexInfo);
void DeleteTexture (GLMipmap_t *pTexInfo);

void ClearMipMapCache (void);
INT32 GetTextureUsed (void);

void CreateModelVBOs (model_t *model);

void ReadScreenTexture (int tex, UINT16 *dst_data);
void GClipRect (INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip, float farclip);

void MakeScreenTexture (int tex);
void FlushScreenTextures (void);

void DrawScreenTexture(int tex, FSurfaceInfo *surf, FBITFIELD polyflags);
void DoScreenWipe (int wipeStart, int wipeEnd);
void RenderVhsEffect (fixed_t upbary, fixed_t downbary, UINT8 updistort, UINT8 downdistort, UINT8 barsize);
void DrawScreenFinalTexture (int tex, INT32 width, INT32 height);

void PostImgRedraw (float points[SCREENVERTS][SCREENVERTS][2]);

boolean InitShaders (void);
void LoadShader (int slot, char *code, hwdshaderstage_t stage);
boolean CompileShader (int slot);
void SetShader (int slot);
void UnSetShader (void);

void SetShaderInfo (hwdshaderinfo_t info, INT32 value);

void SetPaletteLookup (UINT8 *lut);
UINT32 CreateLightTable (RGBA_t *hw_lighttable);
void ClearLightTables (void);
void SetScreenPalette (RGBA_t *palette);

#endif // __HWR_GPU_H__
