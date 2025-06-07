// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  r_draw.h
/// \brief Low-level span/column drawer functions

#ifndef __R_DRAW__
#define __R_DRAW__

#ifdef __cplusplus
extern "C" {
#endif

#include "r_defs.h"
#include "r_things.h"
#include "r_skins.h"

// -------------------------------
// COMMON STUFF FOR 8bpp AND 16bpp
// -------------------------------
extern UINT8 *renderscreen;
extern INT32 linesize;

FUNCINLINE static ATTRINLINE UINT8 *R_Address(INT32 px, INT32 py)
{
	return renderscreen + (py + viewwindowy) * linesize + (viewwindowx + px);
}


typedef struct {
	float x, y, z;
} floatv3_t;

typedef struct
{
	INT32 y;
	INT32 x1;
	INT32 x2;
	lighttable_t* colormap;
	lighttable_t* translation;

	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t xstep;
	fixed_t ystep;
	INT32 waterofs;
	INT32 bgofs;

	fixed_t xoffs;
	fixed_t yoffs;

	visplane_t *currentplane;
	UINT8 *source;
	UINT8 *transmap;

	float zeroheight;

	// Vectors for Software's tilted slope drawers
	floatv3_t sup;
	floatv3_t svp;
	floatv3_t szp;
	floatv3_t slope_origin;
	floatv3_t slope_u;
	floatv3_t slope_v;

	// Variable flat sizes
	UINT32 nflatxshift;
	UINT32 nflatyshift;
	UINT32 nflatshiftup;
	UINT32 nflatmask;

	fixed_t planeheight;
	lighttable_t **planezlight;

	//
	// Water ripple effect
	// Needs the height of the plane, and the vertical position of the span.
	// Sets planeripple.xfrac and planeripple.yfrac, added to ds_xfrac and ds_yfrac, if the span is not tilted.
	//
	struct
	{
		INT32 offset;
		fixed_t xfrac, yfrac;
		boolean active;
	} planeripple;
} drawspandata_t;

extern drawspandata_t g_ds;

// Draws a single visplane.
void R_DrawSinglePlane(drawspandata_t* ds, visplane_t *pl, boolean allow_parallel);

// Vectors for Software's tilted slope drawers
extern floatv3_t *ds_su, *ds_sv, *ds_sz;

extern float focallengthf;


typedef void (coldrawfunc_t)(drawcolumndata_t*);
typedef void (spandrawfunc_t)(drawspandata_t*);

#define BASEDRAWFUNC 0

enum
{
	COLDRAWFUNC_BASE = BASEDRAWFUNC,
	COLDRAWFUNC_FUZZY,
	COLDRAWFUNC_TRANS,
	COLDRAWFUNC_SHADOWED,
	COLDRAWFUNC_TRANSTRANS,
	COLDRAWFUNC_TWOSMULTIPATCH,
	COLDRAWFUNC_TWOSMULTIPATCHTRANS,
	COLDRAWFUNC_FOG,

	COLDRAWFUNC_MAX
};

extern int colfunctype;
extern coldrawfunc_t *colfunc;
extern coldrawfunc_t *colfuncs[COLDRAWFUNC_MAX];

enum
{
	SPANDRAWFUNC_BASE = BASEDRAWFUNC,
	SPANDRAWFUNC_TRANS,
	SPANDRAWFUNC_TILTED,
	SPANDRAWFUNC_TILTEDTRANS,

	SPANDRAWFUNC_SPLAT,
	SPANDRAWFUNC_TRANSSPLAT,
	SPANDRAWFUNC_TILTEDSPLAT,

	SPANDRAWFUNC_WATER,
	SPANDRAWFUNC_TILTEDWATER,

	SPANDRAWFUNC_FOG,

	SPANDRAWFUNC_MAX
};

extern spandrawfunc_t *spanfunc;
extern spandrawfunc_t *spanfuncs[SPANDRAWFUNC_MAX];

void R_DrawMaskedColumn(drawcolumndata_t* dc, column_t *column);

/// \brief Top border
#define BRDR_T 0
/// \brief Bottom border
#define BRDR_B 1
/// \brief Left border
#define BRDR_L 2
/// \brief Right border
#define BRDR_R 3
/// \brief Topleft border
#define BRDR_TL 4
/// \brief Topright border
#define BRDR_TR 5
/// \brief Bottomleft border
#define BRDR_BL 6
/// \brief Bottomright border
#define BRDR_BR 7

extern lumpnum_t viewborderlump[8];

// ------------------------------------------------
// r_draw.c COMMON ROUTINES FOR BOTH 8bpp and 16bpp
// ------------------------------------------------

#define GTC_CACHE 1
#define GTC_MENUCACHE GTC_CACHE
//@TODO Add a separate caching mechanism for menu colormaps distinct from in-level GTC_CACHE. For now this is still preferable to memory leaks...

#define TC_DEFAULT    -1
#define TC_BOSS       -2
#define TC_METALSONIC -3 // For Metal Sonic battle
#define TC_ALLWHITE   -4 // For Cy-Brak-demon
#define TC_RAINBOW    -5 // For invincibility power
#define TC_BLINK      -6 // For item blinking

// Initialize color translation tables, for player rendering etc.
UINT8* R_GetTranslationColormap(INT32 skinnum, skincolors_t color, UINT8 flags);
UINT8* R_GetLocalTranslationColormap(skin_t *skin, skin_t *localskin, skincolors_t color, UINT8 flags, boolean local);
patch_t* R_GetSkinFaceRank(player_t* ply);
patch_t* R_GetSkinFaceWant(player_t* ply);
patch_t* R_GetSkinFaceMini(player_t* ply);
void R_FlushTranslationColormapCache(void);
UINT8 R_GetColorByName(const char *name);

extern UINT8 *transtables; // translucency tables, should be (*transtables)[5][256][256]

enum
{
	blendtab_add,
	blendtab_subtract,
	blendtab_reversesubtract,
	blendtab_modulate,
	NUMBLENDMAPS
};

extern UINT8 *blendtables[NUMBLENDMAPS];

void R_InitTranslucencyTables(void);
void R_GenerateBlendTables(void);

UINT8 *R_GetTranslucencyTable(INT32 alphalevel);
UINT8 *R_GetBlendTable(int style, INT32 alphalevel);

boolean R_BlendLevelVisible(INT32 blendmode, INT32 alphalevel);

// Custom player skin translation
void R_InitViewBuffer(INT32 width, INT32 height);
void R_InitViewBorder(void);
void R_VideoErase(size_t ofs, INT32 count);

// Rendering function.
#if 0
void R_FillBackScreen(void);

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder(void);
#endif

#define TRANSPARENTPIXEL 247

// -----------------
// DRAWING CODE
// -----------------

// column drawers
void R_DrawColumn(drawcolumndata_t* dc);
void R_DrawColumnShadowed(drawcolumndata_t* dc);

void R_DrawTranslucentColumn(drawcolumndata_t* dc);

void R_DrawTranslatedColumn(drawcolumndata_t* dc);
void R_DrawTranslatedTranslucentColumn(drawcolumndata_t* dc);

void R_Draw2sMultiPatchColumn(drawcolumndata_t* dc);
void R_Draw2sMultiPatchTranslucentColumn(drawcolumndata_t* dc);

void R_DrawFogColumn(drawcolumndata_t* dc);

// span drawers
void R_DrawSpan(drawspandata_t* ds);

void R_DrawSpan_Tilted(drawspandata_t* ds);
void R_DrawTranslucentSpan_Tilted(drawspandata_t* ds);
void R_DrawTranslucentWaterSpan_Tilted(drawspandata_t* ds);

void R_DrawTranslucentSpan(drawspandata_t* ds);
void R_DrawTranslucentWaterSpan(drawspandata_t* ds);

void R_DrawFogSpan(drawspandata_t* ds);

void R_DrawSplat_Tilted(drawspandata_t* ds);
void R_DrawSplat(drawspandata_t* ds);
void R_DrawTranslucentSplat(drawspandata_t* ds);

#ifdef __cplusplus
} // extern "C"
#endif

// =========================================================================
#endif  // __R_DRAW__
