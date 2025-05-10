// SONIC ROBO BLAST 2 Kart
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
// Copyright (C) 2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file hw_gl.h
/// \brief OpenGL low-level interface API

#ifndef __HWR_GL_H__
#define __HWR_GL_H__

#include "../screen.h"

#include "hw_data.h"
#include "hw_defs.h"
#include "hw_md2.h"

#define SCREENVERTS 10

boolean GL_Init(void);
void SetupGLInfo(void);

void GL_SetSpecialState(hwdspecialstate_t IdState, INT32 Value);
void GL_SetTransform(FTransform *stransform);
void GL_SetBlend(FBITFIELD PolyFlags);
void GL_SetPalette(RGBA_t *palette);
void GL_ClearBuffer(FBOOLEAN ColorMask, FBOOLEAN DepthMask, FBOOLEAN StencilMask, FRGBAFloat * ClearColor);

void GL_DrawPolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags);
void GL_DrawIndexedTriangles(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, unsigned int *IndexArray);
void GL_Draw2DLine(F2DCoord *v1, F2DCoord *v2, RGBA_t Color);

void GL_DrawModelEx(model_t *model, INT32 frameIndex, float duration, float tics, INT32 nextFrameIndex, FTransform *pos, float hscale, float vscale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface);
#define GL_DrawModel(model, frameIndex, duration, tics, nextFrameIndex, pos, hscale, vscale, flipped, hflipped, Surface) GL_DrawModelEx(model, frameIndex, duration, tics, nextFrameIndex, pos, hscale, vscale, flipped, hflipped, Surface)

void GL_RenderSkyDome(gl_sky_t *sky);

void GL_UpdateTexture(GLMipmap_t *pTexInfo);
void GL_SetTexture(GLMipmap_t *pTexInfo);
void GL_DeleteTexture(GLMipmap_t *pTexInfo);

void GL_Flush(void);
#define GL_ClearMipMapCache GL_Flush

INT32 GL_GetTextureUsed(void);

void GL_CreateModelVBOs(model_t *model);

void GL_ReadScreenTexture(int tex, UINT16 *dst_data);
void GL_GClipRect(INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip, float farclip);

void GL_MakeScreenTexture(int tex);
void GL_FlushScreenTextures(void);

void GL_DrawScreenTexture(int tex, FSurfaceInfo *surf, FBITFIELD polyflags);
void GL_DoScreenWipe(int wipeStart, int wipeEnd);
void GL_RenderVhsEffect(fixed_t upbary, fixed_t downbary, UINT8 updistort, UINT8 downdistort, UINT8 barsize);
void GL_DrawScreenFinalTexture(int tex, INT32 width, INT32 height, boolean useshader);

void GL_PostImgRedraw(float points[SCREENVERTS][SCREENVERTS][2]);

boolean GL_InitShaders(void);
void GL_LoadShader(int slot, char *code, hwdshaderstage_t stage);
boolean GL_CompileShader(int slot);
void GL_SetShader(int slot);
void GL_UnSetShader(void);

void GL_SetShaderInfo(hwdshaderinfo_t info, INT32 value);

void GL_SetPaletteLookup(UINT8 *lut);
UINT32 GL_CreateLightTable(RGBA_t *hw_lighttable);
void GL_ClearLightTables(void);
void GL_SetScreenPalette(RGBA_t *palette);

#endif // __HWR_GL_H__
