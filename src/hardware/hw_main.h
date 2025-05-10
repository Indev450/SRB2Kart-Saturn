// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1998-2000 by DooM Legacy Team.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//-----------------------------------------------------------------------------
/// \file
/// \brief 3D render mode functions

#ifndef __HWR_MAIN_H__
#define __HWR_MAIN_H__

#include "hw_gl.h"
#include "hw_glob.h"
#include "hw_data.h"
#include "hw_defs.h"
#include "hw_portal.h"

#include "../am_map.h"
#include "../d_player.h"
#include "../r_defs.h"
#include "../m_perfstats.h"
#include "../v_video.h"

#define GLENCORE

// Startup & Shutdown the hardware mode renderer
void HWR_Startup(void);
void HWR_Shutdown(void);

extern float gl_viewwidth, gl_viewheight, gl_baseviewwindowx, gl_baseviewwindowy;

extern float gl_basewindowcenterx, gl_basewindowcentery;

extern unsigned msaa;
extern boolean a2c;

extern FTransform atransform;
extern float gl_viewsin, gl_viewcos;

extern boolean gl_drawing_stencil;

extern seg_t *gl_curline;
extern side_t *gl_sidedef;
extern line_t *gl_linedef;
extern sector_t *gl_frontsector;
extern sector_t *gl_backsector;

enum
{
	HWR_STENCIL_NORMAL,
	HWR_STENCIL_BEGIN,
	HWR_STENCIL_REVERSE,
	HWR_STENCIL_DEPTH,
	HWR_STENCIL_SKY
};

// Performance stats
extern ps_metric_t ps_hw_nodesorttime;
extern ps_metric_t ps_hw_nodedrawtime;
extern ps_metric_t ps_hw_spritesorttime;
extern ps_metric_t ps_hw_spritedrawtime;

// Performance stats for batching
extern ps_metric_t ps_hw_numpolys;
extern ps_metric_t ps_hw_numverts;
extern ps_metric_t ps_hw_numcalls;
extern ps_metric_t ps_hw_numshaders;
extern ps_metric_t ps_hw_numtextures;
extern ps_metric_t ps_hw_numpolyflags;
extern ps_metric_t ps_hw_numcolors;
extern ps_metric_t ps_hw_batchsorttime;
extern ps_metric_t ps_hw_batchdrawtime;

extern boolean gl_shadersavailable;

// hw_draw.c
void HWR_DrawPatch(patch_t *gpatch, INT32 x, INT32 y, INT32 option);
void HWR_DrawStretchyFixedPatch(patch_t *gpatch, fixed_t x, fixed_t y, fixed_t pscale, fixed_t vscale, INT32 option, const UINT8 *colormap, INT32 bflags);
void HWR_DrawCroppedPatch(patch_t *gpatch, fixed_t x, fixed_t y, fixed_t pscale, INT32 option, fixed_t sx, fixed_t sy, fixed_t w, fixed_t h);
void HWR_DrawFill(INT32 x, INT32 y, INT32 w, INT32 h, INT32 color);
void HWR_DrawConsoleFill(INT32 x, INT32 y, INT32 w, INT32 h, UINT32 color, INT32 options);	// Lat: separate flags from color since color needs to be an uint to work right.
void HWR_DrawDiag(INT32 x, INT32 y, INT32 wh, INT32 color);
void HWR_drawAMline(const fline_t *fl, INT32 color);
void HWR_FadeScreenMenuBack(UINT16 color, UINT8 strength);
void HWR_DrawConsoleBack(UINT32 color, INT32 height);
void HWR_DrawViewBorder(INT32 clearlines);
void HWR_DrawFlatFill(INT32 x, INT32 y, INT32 w, INT32 h, lumpnum_t flatlumpnum);

UINT8 *HWR_GetScreenshot(void);
boolean HWR_Screenshot(const char *lbmname);

// hw_main.c
void HWR_SetViewSize(void);
void HWR_AddCommands(void);

void HWR_RenderPlayerView(void);
void HWR_RenderViewpoint(gl_portal_t *rootportal, const float fpov, player_t *player, int stencil_level, boolean allow_portals);

void HWR_ClearSkyDome(void);
void HWR_BuildSkyDome(void);

void HWR_SetTransform(float fpov);
void HWR_ClearClipper(void);
void HWR_SetStencilState(int state, int level);

// My original intention was to split hw_main.c
// into files like hw_bsp.c, hw_sprites.c...

// hw_main.c: Lighting and fog
void HWR_Lighting(FSurfaceInfo *Surface, INT32 light_level, extracolormap_t *colormap, const boolean directional);

UINT8 HWR_GetTranstableAlpha(INT32 transtablenum);
FBITFIELD HWR_GetBlendModeFlag(INT32 ast);
FBITFIELD HWR_SurfaceBlend(INT32 style, INT32 transtablenum, FSurfaceInfo *pSurf);
FBITFIELD HWR_TranstableToAlpha(INT32 transtablenum, FSurfaceInfo *pSurf);

// Get amount of memory used by gpu textures in bytes;
FUNCINLINE static ATTRINLINE INT32 HWR_GetTextureUsed(void)
{
	return GL_GetTextureUsed();
}

// hw_main.c: Post-rendering
void HWR_DoWipe(UINT8 wipenum, UINT8 scrnnum);

FUNCINLINE static ATTRINLINE void HWR_StartScreenWipe(void)
{
	GL_MakeScreenTexture(HWD_SCREENTEXTURE_WIPE_START);
}

FUNCINLINE static ATTRINLINE void HWR_EndScreenWipe(void)
{
	GL_MakeScreenTexture(HWD_SCREENTEXTURE_WIPE_END);
}

FUNCINLINE static ATTRINLINE void HWR_DrawIntermissionBG(void)
{
	GL_DrawScreenTexture(HWD_SCREENTEXTURE_GENERIC1, NULL, 0);
}

FUNCINLINE static ATTRINLINE void HWR_RenderVhsEffect(fixed_t upbary, fixed_t downbary, UINT8 updistort, UINT8 downdistort, UINT8 barsize)
{
	GL_RenderVhsEffect(upbary, downbary, updistort, downdistort, barsize);
}

FUNCINLINE static ATTRINLINE void HWR_MakeScreenFinalTexture(void)
{
	GL_MakeScreenTexture(HWD_SCREENTEXTURE_GENERIC2);
}

FUNCINLINE static ATTRINLINE void HWR_DrawScreenFinalTexture(INT32 width, INT32 height, boolean useshader)
{
	GL_DrawScreenFinalTexture(HWD_SCREENTEXTURE_GENERIC2, width, height, useshader);
}

// hw_main.c: Segs
void HWR_ProcessSeg(void); // Sort of like GLWall::Process in GZDoom

// hw_bsp.c
void HWR_CreatePlanePolygons(INT32 bspnum);
extern boolean gl_maphasportals;

// Console variables
extern CV_PossibleValue_t glanisotropicmode_cons_t[];

extern consvar_t cv_gltexturedepth;

extern consvar_t cv_glscreentextures;
#ifdef USE_FBO_OGL
extern consvar_t cv_glframebuffer;
#endif

extern consvar_t cv_glmdls;
extern consvar_t cv_glfallbackplayermodel;

extern consvar_t cv_glspritebillboarding;
extern consvar_t cv_glshearing;

extern consvar_t cv_glfakecontrast;
extern consvar_t cv_glslopecontrast;

extern consvar_t cv_glshaders;

extern consvar_t cv_gllightdither;
extern consvar_t cv_glsecbright;

extern consvar_t cv_glfiltermode;
extern consvar_t cv_glanisotropicmode;

extern consvar_t cv_glsolvetjoin;

extern consvar_t cv_glbatching;

extern consvar_t cv_glrenderdistance;

extern consvar_t cv_glhorizonlines;
extern consvar_t cv_glportals;

extern consvar_t cv_glpaletterendering;
extern consvar_t cv_glpalettedepth;
extern consvar_t cv_glflashpal;

FUNCINLINE static ATTRINLINE boolean HWR_UseShader(void)
{
	return (cv_glshaders.value && gl_shadersavailable);
}

FUNCINLINE static ATTRINLINE boolean HWR_ShouldUsePaletteRendering(void)
{
	return (cv_glpaletterendering.value && (pLocalPalette != NULL) && HWR_UseShader());
}

FUNCINLINE static ATTRINLINE boolean HWR_PalRenderFlashpal(void)
{
	return (HWR_ShouldUsePaletteRendering() && cv_glflashpal.value);
}

#endif
