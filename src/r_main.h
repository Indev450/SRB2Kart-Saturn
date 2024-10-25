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
/// \file  r_main.h
/// \brief Rendering variables, consvars, defines

#ifndef __R_MAIN__
#define __R_MAIN__

#include "d_player.h"
#include "r_data.h"
#include "m_perfstats.h"

//
// POV related.
//
extern fixed_t viewcos, viewsin;
extern INT32 viewheight;
extern INT32 centerx, centery;

extern fixed_t centerxfrac, centeryfrac;
extern fixed_t projection, projectiony;
extern fixed_t fovtan; // field of view

extern size_t validcount, linecount, loopcount, framecount;

// The fraction of a tic being drawn (for interpolation between two tics)
extern fixed_t rendertimefrac;
// Same as rendertimefrac but not suspended when the game is paused
extern fixed_t rendertimefrac_unpaused;
// Evaluated delta tics for this frame (how many tics since the last frame)
extern fixed_t renderdeltatics;
// The current render is a new logical tic
extern boolean renderisnewtic;

extern mobj_t *viewmobj;

//
// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).
//

// Lighting constants.
// Now with 32 levels.
#define LIGHTLEVELS 32
#define LIGHTSEGSHIFT 3

#define MAXLIGHTSCALE 48
#define LIGHTSCALESHIFT 12
#define MAXLIGHTZ 128
#define LIGHTZSHIFT 20

#define LIGHTRESOLUTIONFIX (640*fovtan/vid.width)

extern lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern lighttable_t *scalelightfixed[MAXLIGHTSCALE];
extern lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS 32

#define COLORMAP_SIZE (256*LIGHTLEVELS)
#define COLORMAP_REMAPOFFSET COLORMAP_SIZE

// Utility functions.

//
// R_PointOnSide
// Traverse BSP (sub) tree,
// check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//
FUNCINLINE static ATTRINLINE PUREFUNC INT32 R_PointOnSide(fixed_t x, fixed_t y, const node_t *restrict node)
{
	if (!node->dx)
		return x <= node->x ? node->dy > 0 : node->dy < 0;

	if (!node->dy)
		return y <= node->y ? node->dx < 0 : node->dx > 0;

	x -= node->x;
	y -= node->y;

	// Try to quickly decide by looking at sign bits.
	// also use a mask to avoid branch prediction
	INT32 mask = (node->dy ^ node->dx ^ x ^ y) >> 31;
	return (mask & ((node->dy ^ x) < 0)) |  // (left is negative)
	(~mask & (FixedMul(y, node->dx>>FRACBITS) >= FixedMul(node->dy>>FRACBITS, x)));
}

FUNCINLINE static ATTRINLINE PUREFUNC INT32 R_PointOnSideRender(fixed_t x, fixed_t y, const node_t *node)
{
	// use cross product to determine side quickly
	return (INT64)(y - node->y) * node->dx - (INT64)(x - node->x) * node->dy > 0;
}

FUNCINLINE static ATTRINLINE PUREFUNC INT32 R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t *line)
{
    fixed_t lx = line->v1->x;
    fixed_t ly = line->v1->y;
    fixed_t ldx = line->v2->x - lx;
    fixed_t ldy = line->v2->y - ly;

	// use cross product to determine side quickly
	return (INT64)(y - ly) * ldx - (INT64)(x - lx) * ldy > 0;
}

angle_t R_PointToAngle(fixed_t x, fixed_t y);
angle_t R_PointToAngle64(INT64 x, INT64 y);
angle_t R_PointToAngle2(fixed_t px2, fixed_t py2, fixed_t px1, fixed_t py1);
angle_t R_PlayerSliptideAngle(player_t *player);

fixed_t R_ScaleFromGlobalAngle(angle_t visangle);
boolean R_IsPointInSector(sector_t *sector, fixed_t x, fixed_t y);

//
// R_PointInSubsector
//
FUNCINLINE static ATTRINLINE subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
	size_t nodenum = numnodes-1;

	while (!(nodenum & NF_SUBSECTOR))
		nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];

	return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_IsPointInSubsector, same as above but returns 0 if not in subsector
//
FUNCINLINE static ATTRINLINE subsector_t *R_IsPointInSubsector(fixed_t x, fixed_t y)
{
	node_t *node;
	INT32 side, i;
	size_t nodenum;
	subsector_t *ret;

	nodenum = numnodes - 1;

	while (!(nodenum & NF_SUBSECTOR))
	{
		node = &nodes[nodenum];
		side = R_PointOnSide(x, y, node);
		nodenum = node->children[side];
	}

	ret = &subsectors[nodenum & ~NF_SUBSECTOR];
	for (i = 0; i < ret->numlines; i++)
		if (P_PointOnLineSide(x, y, segs[ret->firstline + i].linedef) != segs[ret->firstline + i].side)
			return 0;

	return ret;
}

#define R_PointToDist(x, y) R_PointToDist2(viewx, viewy, x, y)
#define R_PointToDist2(px2, py2, px1, py1) FixedHypot((px1) - (px2), (py1) - (py2))

boolean R_DoCulling(line_t *cullheight, line_t *viewcullheight, fixed_t vz, fixed_t bottomh, fixed_t toph);
void R_GetRenderBlockMapDimensions(fixed_t drawdist, INT32 *xl, INT32 *xh, INT32 *yl, INT32 *yh);


typedef struct portal_pair
{
    INT32 line1;
    INT32 line2;
    UINT8 pass;
    struct portal_pair *next;

    fixed_t viewx;
    fixed_t viewy;
    fixed_t viewz;
    angle_t viewangle;

    INT32 start;
    INT32 end;
    INT16 *ceilingclip;
    INT16 *floorclip;
    fixed_t *frontscale;
} portal_pair;


// Performance stats
extern precise_t ps_prevframetime;// time when previous frame was rendered
extern ps_metric_t ps_rendercalltime;
extern ps_metric_t ps_otherrendertime;
extern ps_metric_t ps_uitime;
extern ps_metric_t ps_swaptime;

extern ps_metric_t ps_skyboxtime;
extern ps_metric_t ps_bsptime;

extern ps_metric_t ps_sw_spritecliptime;
extern ps_metric_t ps_sw_portaltime;
extern ps_metric_t ps_sw_planetime;
extern ps_metric_t ps_sw_maskedtime;

extern ps_metric_t ps_numbspcalls;
extern ps_metric_t ps_numsprites;
extern ps_metric_t ps_numdrawnodes;
extern ps_metric_t ps_numpolyobjects;

//
// REFRESH - the actual rendering functions.
//

extern consvar_t cv_showhud, cv_translucenthud, cv_uncappedhud;
extern consvar_t cv_homremoval;
extern consvar_t cv_chasecam[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_flipcam[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_shadow, cv_shadowoffs;
extern consvar_t cv_ffloorclip, cv_spriteclip;
extern consvar_t cv_translucency;
extern consvar_t cv_drawdist, cv_drawdist_precip, cv_lessprecip, cv_mobjscaleprecip;
extern consvar_t cv_fov;
extern consvar_t cv_skybox;
extern consvar_t cv_tailspickup;
extern consvar_t cv_grmaxinterpdist;
extern consvar_t cv_ripplewater;

// Called by startup code.
void R_Init(void);

void R_CheckViewMorph(void);
void R_ApplyViewMorph(void);
angle_t R_ViewRollAngle(const player_t *player);

// just sets setsizeneeded true
extern boolean setsizeneeded;
void R_SetViewSize(void);

// do it (sometimes explicitly called)
void R_ExecuteSetViewSize(void);

void R_SkyboxFrame(int s);
void R_SetupFrame(int s);

// Called by G_Drawer.
void R_RenderPlayerView(player_t *player);

// add commands related to engine, at game startup
void R_RegisterEngineStuff(void);

// return multiplier for HUD uncap
INT32 R_GetHudUncap(void);
// same as above but keeps interpolation during pause
INT32 R_GetMenuUncap(void);
#endif
