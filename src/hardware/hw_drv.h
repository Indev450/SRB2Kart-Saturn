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
/// \brief imports/exports for the GPU hardware low-level interface API

#ifndef __HWR_DRV_H__
#define __HWR_DRV_H__

// this must be here 19991024 by Kin
#include "../screen.h"
#include "hw_data.h"
#include "hw_defs.h"
#include "hw_md2.h"

#include "hw_dll.h"
#include "../doomdef.h"

// ==========================================================================
//                                                       STANDARD DLL EXPORTS
// ==========================================================================

EXPORT boolean HWRAPI(Init) (void);
EXPORT void HWRAPI(SetupGLInfo) (void);
#if defined (PURESDL) || defined (macintosh)
EXPORT void HWRAPI(SetPalette) (INT32 *, RGBA_t *gamma);
#else
EXPORT void HWRAPI(SetPalette) (RGBA_t *ppal, RGBA_t *pgamma);
#endif
EXPORT void HWRAPI(FinishUpdate) (INT32 waitvbl);
EXPORT void HWRAPI(Draw2DLine) (F2DCoord *v1, F2DCoord *v2, RGBA_t Color);
EXPORT void HWRAPI(DrawPolygon) (FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, boolean horizonSpecial);
EXPORT void HWRAPI(SetBlend) (FBITFIELD PolyFlags);
EXPORT void HWRAPI(ClearBuffer) (FBOOLEAN ColorMask, FBOOLEAN DepthMask, FBOOLEAN StencilMask, FRGBAFloat *ClearColor);
EXPORT void HWRAPI(SetTexture) (GLMipmap_t *TexInfo);
EXPORT void HWRAPI(UpdateTexture) (GLMipmap_t *TexInfo);
EXPORT void HWRAPI(DeleteTexture) (GLMipmap_t *TexInfo);
EXPORT void HWRAPI(ReadRect) (INT32 x, INT32 y, INT32 width, INT32 height, INT32 dst_stride, UINT16 *dst_data);
EXPORT void HWRAPI(GClipRect) (INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip, float farclip);
EXPORT void HWRAPI(ClearMipMapCache) (void);

//Hurdler: added for backward compatibility
EXPORT void HWRAPI(SetSpecialState) (hwdspecialstate_t IdState, INT32 Value);

//Hurdler: added for new development

EXPORT void HWRAPI(DrawModel) (model_t *model, INT32 frameIndex, float duration, float tics, INT32 nextFrameIndex, FTransform *pos, float hscale, float vscale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface);

EXPORT void HWRAPI(CreateModelVBOs) (model_t *model);
EXPORT void HWRAPI(SetTransform) (FTransform *stransform);
EXPORT INT32 HWRAPI(GetTextureUsed) (void);

EXPORT void HWRAPI(RenderSkyDome) (INT32 tex, INT32 texture_width, INT32 texture_height, FTransform transform);

EXPORT void HWRAPI(FlushScreenTextures) (void);
EXPORT void HWRAPI(StartScreenWipe) (void);
EXPORT void HWRAPI(EndScreenWipe) (void);
EXPORT void HWRAPI(DoScreenWipe) (void);
EXPORT void HWRAPI(DrawIntermissionBG) (void);
EXPORT void HWRAPI(MakeScreenTexture) (void);
EXPORT void HWRAPI(RenderVhsEffect) (fixed_t upbary, fixed_t downbary, UINT8 updistort, UINT8 downdistort, UINT8 barsize);
EXPORT void HWRAPI(MakeScreenFinalTexture) (void);
EXPORT void HWRAPI(DrawScreenFinalTexture) (int width, int height);

#define SCREENVERTS 10
EXPORT void HWRAPI(PostImgRedraw) (float points[SCREENVERTS][SCREENVERTS][2]);

// jimita
EXPORT boolean HWRAPI(LoadShaders) (void);
EXPORT void HWRAPI(KillShaders) (void);
EXPORT void HWRAPI(SetShader) (int shader);
EXPORT void HWRAPI(UnSetShader) (void);

EXPORT void HWRAPI(SetShaderInfo) (hwdshaderinfo_t info, INT32 value);
EXPORT void HWRAPI(LoadCustomShader) (int number, char *shader, size_t size, boolean fragment);
EXPORT boolean HWRAPI(InitCustomShaders) (void);

EXPORT void HWRAPI(StartBatching) (void);
EXPORT void HWRAPI(RenderBatches) (precise_t *sSortTime, precise_t *sDrawTime, int *sNumPolys, int *sNumVerts, int *sNumCalls, int *sNumShaders, int *sNumTextures, int *sNumPolyFlags, int *sNumColors);

EXPORT void HWRAPI(InitPalette) (int flashnum, boolean skiplut);
EXPORT UINT32 HWRAPI(AddLightTable) (UINT8 *lighttable);
EXPORT void HWRAPI(ClearLightTableCache) (void);

// ==========================================================================
//                                      HWR DRIVER OBJECT, FOR CLIENT PROGRAM
// ==========================================================================

#if !defined (_CREATE_DLL_)

struct hwdriver_s
{
	Init                	pfnInit;
	SetupGLInfo             pfnSetupGLInfo;
	SetPalette          	pfnSetPalette;
	FinishUpdate        	pfnFinishUpdate;
	Draw2DLine          	pfnDraw2DLine;
	DrawPolygon         	pfnDrawPolygon;
	SetBlend            	pfnSetBlend;
	ClearBuffer         	pfnClearBuffer;
	SetTexture          	pfnSetTexture;
	UpdateTexture       	pfnUpdateTexture;
	DeleteTexture       	pfnDeleteTexture;
	ReadRect            	pfnReadRect;
	GClipRect           	pfnGClipRect;
	ClearMipMapCache    	pfnClearMipMapCache;
	SetSpecialState     	pfnSetSpecialState;
	DrawModel           	pfnDrawModel;
	CreateModelVBOs     	pfnCreateModelVBOs;
	SetTransform        	pfnSetTransform;
	GetTextureUsed      	pfnGetTextureUsed;
	PostImgRedraw       	pfnPostImgRedraw;
	FlushScreenTextures 	pfnFlushScreenTextures;
	StartScreenWipe     	pfnStartScreenWipe;
	EndScreenWipe       	pfnEndScreenWipe;
	DoScreenWipe        	pfnDoScreenWipe;
	DrawIntermissionBG  	pfnDrawIntermissionBG;
	MakeScreenTexture   	pfnMakeScreenTexture;
	RenderVhsEffect     pfnRenderVhsEffect;
	MakeScreenFinalTexture  pfnMakeScreenFinalTexture;
	DrawScreenFinalTexture  pfnDrawScreenFinalTexture;

	RenderSkyDome 			pfnRenderSkyDome;

	LoadShaders 			pfnLoadShaders;
	KillShaders 			pfnKillShaders;
	SetShader 				pfnSetShader;
	UnSetShader 			pfnUnSetShader;

	SetShaderInfo       	pfnSetShaderInfo;
	LoadCustomShader 		pfnLoadCustomShader;
	InitCustomShaders 		pfnInitCustomShaders;

	StartBatching 			pfnStartBatching;
	RenderBatches 			pfnRenderBatches;
	
	InitPalette 			pfnInitPalette;
	AddLightTable 			pfnAddLightTable;
	ClearLightTableCache 	pfnClearLightTableCache;
};

extern struct hwdriver_s hwdriver;

#define HWD hwdriver

#endif //not defined _CREATE_DLL_

#endif //__HWR_DRV_H__
