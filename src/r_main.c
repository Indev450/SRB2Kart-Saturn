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
/// \file  r_main.c
/// \brief Rendering main loop and setup functions,
///        utility functions (BSP, geometry, trigonometry).
///        See tables.c, too.

#include "doomdef.h"
#include "g_game.h"
#include "g_input.h"
#include "g_state.h"
#include "r_local.h"
#include "r_splats.h" // faB(21jan): testing
#include "r_sky.h"
#include "st_stuff.h"
#include "p_local.h"
#include "keys.h"
#include "i_video.h"
#include "m_menu.h"
#include "am_map.h"
#include "d_main.h"
#include "v_video.h"
#include "p_spec.h" // skyboxmo
#include "z_zone.h"
#include "m_random.h" // quake camera shake
#include "r_portal.h"
#include "doomstat.h" // MAXSPLITSCREENPLAYERS
#include "r_fps.h" // Frame interpolation/uncapped
#include "tables.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW 2048

// increment every time a check is made
size_t validcount = 1;

INT32 centerx, centery;

fixed_t centerxfrac, centeryfrac;
fixed_t projection;
fixed_t projectiony; // aspect ratio
fixed_t fovtan; // field of view

// just for profiling purposes
size_t framecount;

size_t loopcount;

fixed_t viewx, viewy, viewz;
angle_t viewangle, aimingangle, viewroll;
UINT8 viewssnum;
fixed_t viewcos, viewsin;
boolean skyVisible;
boolean skyVisiblePerPlayer[MAXSPLITSCREENPLAYERS]; // saved values of skyVisible for each splitscreen player
sector_t *viewsector;
player_t *viewplayer;

fixed_t rendertimefrac;
fixed_t rendertimefrac_unpaused;
fixed_t renderdeltatics;
boolean renderisnewtic;

//
// precalculated math tables
//
angle_t clipangle;
angle_t doubleclipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X.
INT32 viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t xtoviewangle[MAXVIDWIDTH+1];

lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t *scalelightfixed[MAXLIGHTSCALE];
lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

// Hack to support extra boom colormaps.
size_t num_extra_colormaps;
extracolormap_t extra_colormaps[MAXCOLORMAPS];

// Performance stats
precise_t ps_prevframetime = 0;
ps_metric_t ps_rendercalltime = {0};
ps_metric_t ps_otherrendertime = {0};
ps_metric_t ps_uitime = {0};
ps_metric_t ps_swaptime = {0};

ps_metric_t ps_skyboxtime = {0};
ps_metric_t ps_bsptime = {0};

ps_metric_t ps_sw_spritecliptime = {0};
ps_metric_t ps_sw_portaltime = {0};
ps_metric_t ps_sw_planetime = {0};
ps_metric_t ps_sw_maskedtime = {0};

ps_metric_t ps_numbspcalls = {0};
ps_metric_t ps_numsprites = {0};
ps_metric_t ps_numdrawnodes = {0};
ps_metric_t ps_numpolyobjects = {0};

static CV_PossibleValue_t drawdist_cons_t[] = {
	/*{256, "256"},*/	{512, "512"},	{768, "768"},
	{1024, "1024"},	{1536, "1536"},	{2048, "2048"},
	{3072, "3072"},	{4096, "4096"},	{6144, "6144"},
	{8192, "8192"},	{12288, "12288"}, {20480, "20480"},
	{24576, "24576"},{0, "Infinite"},	{0, NULL}};

#if defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) || defined(__arm64__) // only for 64bit (idk how else to proper check lmao)
static CV_PossibleValue_t drawdist_precip_cons_t[] = {
	{256, "256"}, {512, "512"}, {768, "768"},
	{1024, "1024"}, {1536, "1536"}, {2048, "2048"},
	{3072, "3072"}, {4096, "4096"}, {0, "None"}, {0, NULL}};
#else
static CV_PossibleValue_t drawdist_precip_cons_t[] = {
	{256, "256"},	{512, "512"},	{768, "768"},
	{1024, "1024"},	{1536, "1536"},	{2048, "2048"},
	{0, "None"},	{0, NULL}};
#endif

static CV_PossibleValue_t maxinterpdist_cons_t[] = {
       /*{256, "256"},*/ {512, "512"}, {768, "768"},
       {1024, "1024"}, {1536, "1536"}, {2048, "2048"},
       {3072, "3072"}, {4096, "4096"}, {6144, "6144"},
       {8192, "8192"}, {0, "Infinite"}, {0, NULL}};

static CV_PossibleValue_t fov_cons_t[] = {{5*FRACUNIT, "MIN"}, {178*FRACUNIT, "MAX"}, {0, NULL}};

static CV_PossibleValue_t translucenthud_cons_t[] = {{0, "MIN"}, {10, "MAX"}, {0, NULL}};
static CV_PossibleValue_t maxportals_cons_t[] = {{0, "MIN"}, {12, "MAX"}, {0, NULL}}; // lmao rendering 32 portals, you're a card
static CV_PossibleValue_t homremoval_cons_t[] = {{0, "No"}, {1, "Yes"}, {2, "Flash"}, {0, NULL}};

static void Fov_OnChange(void);
static void FlipCam_OnChange(void);
static void FlipCam2_OnChange(void);
static void FlipCam3_OnChange(void);
static void FlipCam4_OnChange(void);
void SendWeaponPref(void);
void SendWeaponPref2(void);
void SendWeaponPref3(void);
void SendWeaponPref4(void);

static void Precipstuff_OnChange(void);

consvar_t cv_tailspickup = {"tailspickup", "On", CV_NETVAR|CV_NOSHOWHELP, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_chasecam = {"chasecam", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_chasecam2 = {"chasecam2", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_chasecam3 = {"chasecam3", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_chasecam4 = {"chasecam4", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_flipcam = {"flipcam", "No", CV_SAVE|CV_CALL|CV_NOINIT, CV_YesNo, FlipCam_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_flipcam2 = {"flipcam2", "No", CV_SAVE|CV_CALL|CV_NOINIT, CV_YesNo, FlipCam2_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_flipcam3 = {"flipcam3", "No", CV_SAVE|CV_CALL|CV_NOINIT, CV_YesNo, FlipCam3_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_flipcam4 = {"flipcam4", "No", CV_SAVE|CV_CALL|CV_NOINIT, CV_YesNo, FlipCam4_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_shadow = {"shadow", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_shadowoffs = {"offsetshadows", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_skybox = {"skybox", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ffloorclip = {"r_ffloorclip", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_spriteclip = {"r_spriteclip", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_soniccd = {"soniccd", "Off", CV_NETVAR|CV_NOSHOWHELP, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_allowmlook = {"allowmlook", "Yes", CV_NETVAR, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_showhud = {"showhud", "Yes", CV_CALL,  CV_YesNo, R_SetViewSize, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_translucenthud = {"translucenthud", "10", CV_SAVE, translucenthud_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_uncappedhud = {"uncappedhud", "Yes", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_translucency = {"translucency", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_drawdist = {"drawdist", "Infinite", CV_SAVE, drawdist_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_drawdist_precip = {"drawdist_precip", "1024", CV_SAVE|CV_CALL|CV_NOINIT, drawdist_precip_cons_t, Precipstuff_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_lessprecip = {"lessweathereffects", "Off", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, Precipstuff_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_mobjscaleprecip = {"scaleprecipmobjscale", "Off", CV_SAVE|CV_CALL|CV_NOINIT, CV_OnOff, Precipstuff_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_maxinterpdist = {"maxinterpdist", "Infinite", CV_SAVE, maxinterpdist_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_ripplewater = {"waterripples", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// cap fov, fov too high tears software apart.
consvar_t cv_fov = {"fov", "90", CV_FLOAT|CV_CALL|CV_SAVE, fov_cons_t, Fov_OnChange, 0, NULL, NULL, 0, 0, NULL};

// Okay, whoever said homremoval causes a performance hit should be shot.
consvar_t cv_homremoval = {"homremoval", "Yes", CV_SAVE, homremoval_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_maxportals = {"maxportals", "2", CV_SAVE, maxportals_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

void SplitScreen_OnChange(void)
{
	UINT8 i;

	// recompute screen size
	R_ExecuteSetViewSize();

	if (!demo.playback && !botingame)
	{
		for (i = 1; i < MAXSPLITSCREENPLAYERS; i++)
		{
			if (i > splitscreen)
				CL_RemoveSplitscreenPlayer(displayplayers[i]);
			else
				CL_AddSplitscreenPlayer();
		}

		if (server && !netgame)
			multiplayer = splitscreen;
	}
	else
	{
		for (i = 1; i < MAXSPLITSCREENPLAYERS; i++)
			displayplayers[i] = consoleplayer;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && i != consoleplayer)
			{
				UINT8 j;
				for (j = 1; j < MAXSPLITSCREENPLAYERS; j++)
				{
					if (displayplayers[j] == consoleplayer)
					{
						displayplayers[j] = i;
						break;
					}
				}

				if (j == MAXSPLITSCREENPLAYERS)
					break;
			}
		}
	}
}
static void Fov_OnChange(void)
{
	R_SetViewSize();
}

static void FlipCam_OnChange(void)
{
	SendWeaponPref();
}

static void FlipCam2_OnChange(void)
{
	SendWeaponPref2();
}

static void FlipCam3_OnChange(void)
{
	SendWeaponPref3();
}

static void FlipCam4_OnChange(void)
{
	SendWeaponPref4();
}

static void Precipstuff_OnChange(void)
{
	if (gamestate != GS_LEVEL)
		return;

	thinker_t *think;
	thinker_t *next;
	precipmobj_t *precipmobj;

	for (think = thinkercap.next; think != &thinkercap; think = next)
	{
		next = think->next;

		if (think->function.acp1 != (actionf_p1)P_NullPrecipThinker)
			continue; // not a precipmobj thinker

		precipmobj = (precipmobj_t *)think;
		P_FreePrecipMobj(precipmobj);
	}

	P_SpawnPrecipitation();
}

//
// R_PointOnSide
// Traverse BSP (sub) tree,
// check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
// killough 5/2/98: reformatted
//
PUREFUNC INT32 R_PointOnSide(fixed_t x, fixed_t y, const node_t *restrict node)
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

//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table. The +1 size of tantoangle[]
//  is to handle the case when x==y without additional
//  checking.
//
// killough 5/2/98: reformatted, cleaned up

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
	return (y -= viewy, (x -= viewx) || y) ?
	x >= 0 ?
	y >= 0 ?
		(x > y) ? tantoangle[SlopeDiv(y,x)] :                          // octant 0
		ANGLE_90-tantoangle[SlopeDiv(x,y)] :                           // octant 1
		x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :                   // octant 8
		ANGLE_270+tantoangle[SlopeDiv(x,y)] :                          // octant 7
		y >= 0 ? (x = -x) > y ? ANGLE_180-tantoangle[SlopeDiv(y,x)] :  // octant 3
		ANGLE_90 + tantoangle[SlopeDiv(x,y)] :                         // octant 2
		(x = -x) > (y = -y) ? ANGLE_180+tantoangle[SlopeDiv(y,x)] :    // octant 4
		ANGLE_270-tantoangle[SlopeDiv(x,y)] :                          // octant 5
		0;
}

// This version uses 64-bit variables to avoid overflows with large values.
angle_t R_PointToAngle64(INT64 x, INT64 y)
{
	return (y -= viewy, (x -= viewx) || y) ?
	x >= 0 ?
	y >= 0 ?
		(x > y) ? tantoangle[SlopeDivEx(y,x)] :                            // octant 0
		ANGLE_90-tantoangle[SlopeDivEx(x,y)] :                               // octant 1
		x > (y = -y) ? 0-tantoangle[SlopeDivEx(y,x)] :                    // octant 8
		ANGLE_270+tantoangle[SlopeDivEx(x,y)] :                              // octant 7
		y >= 0 ? (x = -x) > y ? ANGLE_180-tantoangle[SlopeDivEx(y,x)] :  // octant 3
		ANGLE_90 + tantoangle[SlopeDivEx(x,y)] :                             // octant 2
		(x = -x) > (y = -y) ? ANGLE_180+tantoangle[SlopeDivEx(y,x)] :    // octant 4
		ANGLE_270-tantoangle[SlopeDivEx(x,y)] :                              // octant 5
		0;
}

angle_t R_PointToAngle2(fixed_t pviewx, fixed_t pviewy, fixed_t x, fixed_t y)
{
	return (y -= pviewy, (x -= pviewx) || y) ?
	x >= 0 ?
	y >= 0 ?
		(x > y) ? tantoangle[SlopeDiv(y,x)] :                          // octant 0
		ANGLE_90-tantoangle[SlopeDiv(x,y)] :                           // octant 1
		x > (y = -y) ? 0-tantoangle[SlopeDiv(y,x)] :                   // octant 8
		ANGLE_270+tantoangle[SlopeDiv(x,y)] :                          // octant 7
		y >= 0 ? (x = -x) > y ? ANGLE_180-tantoangle[SlopeDiv(y,x)] :  // octant 3
		ANGLE_90 + tantoangle[SlopeDiv(x,y)] :                         // octant 2
		(x = -x) > (y = -y) ? ANGLE_180+tantoangle[SlopeDiv(y,x)] :    // octant 4
		ANGLE_270-tantoangle[SlopeDiv(x,y)] :                          // octant 5
		0;
}

angle_t R_PlayerSliptideAngle(player_t *player)
{
	mobj_t *mo;
    spritedef_t *sprdef;
    spriteframe_t *sprframe;
    angle_t ang = 0;

    if (!cv_sliptideroll.value || !player || P_MobjWasRemoved(player->mo))
        return 0;

    mo = player->mo;

    size_t rot = mo->frame & FF_FRAMEMASK;
    boolean papersprite = (mo->frame & FF_PAPERSPRITE);

	if (mo->skin && mo->sprite == SPR_PLAY)
	{
		sprdef = &((skin_t *)mo->skin)->spritedef;

		if (rot >= sprdef->numframes)
			sprdef = &sprites[mo->sprite];
	}
	else
	{
		sprdef = &sprites[mo->sprite];
	}

	if (rot >= sprdef->numframes)
	{
		sprdef = &sprites[states[S_UNKNOWN].sprite];
		rot = states[S_UNKNOWN].frame&FF_FRAMEMASK;
	}

	sprframe = &sprdef->spriteframes[rot];

	// No sprite frame? I guess it is possible
	if (!sprframe) return 0;

	if (sprframe->rotate != SRF_SINGLE || papersprite)
		ang = R_PointToAngle(mo->x, mo->y) - mo->angle;

	return FixedMul(FINECOSINE((ang) >> ANGLETOFINESHIFT), mo->player->sliproll*(mo->player->sliptidemem));
}

INT32 R_GetHudUncap(void)
{
	return cv_uncappedhud.value ? rendertimefrac & FRACMASK : 0;
}

INT32 R_GetMenuUncap(void)
{
	return cv_uncappedhud.value ? rendertimefrac_unpaused & FRACMASK : 0;
}

//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
// killough 5/2/98: reformatted, cleaned up
//
// note: THIS IS USED ONLY FOR WALLS!
fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
	angle_t anglea = ANGLE_90 + (visangle-viewangle);
	angle_t angleb = ANGLE_90 + (visangle-rw_normalangle);
	fixed_t den = FixedMul(rw_distance, FINESINE(anglea>>ANGLETOFINESHIFT));
	// proff 11/06/98: Changed for high-res
	fixed_t num = FixedMul(projectiony, FINESINE(angleb>>ANGLETOFINESHIFT));

	if (den > num>>16)
	{
		num = FixedDiv(num, den);
		if (num > 64*FRACUNIT)
			return 64*FRACUNIT;
		if (num < 256)
			return 256;
		return num;
	}
	return 64*FRACUNIT;
}

//
// R_DoCulling
// Checks viewz and top/bottom heights of an item against culling planes
// Returns true if the item is to be culled, i.e it shouldn't be drawn!
// if ML_NOCLIMB is set, the camera view is required to be in the same area for culling to occur
boolean R_DoCulling(line_t *cullheight, line_t *viewcullheight, fixed_t vz, fixed_t bottomh, fixed_t toph)
{
	fixed_t cullplane;

	if (!cullheight)
		return false;

	cullplane = cullheight->frontsector->floorheight;
	if (cullheight->flags & ML_NOCLIMB) // Group culling
	{
		if (!viewcullheight)
			return false;

		// Make sure this is part of the same group
		if (viewcullheight->frontsector == cullheight->frontsector)
		{
			// OK, we can cull
			if (vz > cullplane && toph < cullplane) // Cull if below plane
				return true;

			if (bottomh > cullplane && vz <= cullplane) // Cull if above plane
				return true;
		}
	}
	else // Quick culling
	{
		if (vz > cullplane && toph < cullplane) // Cull if below plane
			return true;

		if (bottomh > cullplane && vz <= cullplane) // Cull if above plane
			return true;
	}

	return false;
}

// Returns search dimensions within a blockmap, in the direction of viewangle and out to a certain distance.
void R_GetRenderBlockMapDimensions(fixed_t drawdist, INT32 *xl, INT32 *xh, INT32 *yl, INT32 *yh)
{
	const angle_t left = viewangle - clipangle;
	const angle_t right = viewangle + clipangle;

	const fixed_t vxleft = viewx + FixedMul(drawdist, FCOS(left));
	const fixed_t vyleft = viewy + FixedMul(drawdist, FSIN(left));

	const fixed_t vxright = viewx + FixedMul(drawdist, FCOS(right));
	const fixed_t vyright = viewy + FixedMul(drawdist, FSIN(right));

	// Try to narrow the search to within only the field of view
	*xl = (unsigned)(min(viewx, min(vxleft, vxright)) - bmaporgx)>>MAPBLOCKSHIFT;
	*xh = (unsigned)(max(viewx, max(vxleft, vxright)) - bmaporgx)>>MAPBLOCKSHIFT;
	*yl = (unsigned)(min(viewy, min(vyleft, vyright)) - bmaporgy)>>MAPBLOCKSHIFT;
	*yh = (unsigned)(max(viewy, max(vyleft, vyright)) - bmaporgy)>>MAPBLOCKSHIFT;

	if (*xh >= bmapwidth)
		*xh = bmapwidth - 1;

	if (*yh >= bmapheight)
		*yh = bmapheight - 1;

	BMBOUNDFIX(*xl, *xh, *yl, *yh);
}

//
// R_InitTextureMapping
//
static void R_InitTextureMapping(void)
{
	INT32 i;
	INT32 x;
	INT32 t;
	fixed_t focallength;

	// Use tangent table to generate viewangletox:
	//  viewangletox will give the next greatest x
	//  after the view angle.
	//
	// Calc focallength
	//  so FIELDOFVIEW angles covers SCREENWIDTH.
	focallength = FixedDiv(projection,
		FINETANGENT(FINEANGLES/4+/*cv_fov.value*/ FIELDOFVIEW/2));

	focallengthf = FIXED_TO_FLOAT(focallength);

	for (i = 0; i < FINEANGLES/2; i++)
	{
		if (FINETANGENT(i) > fovtan*2)
			t = -1;
		else if (FINETANGENT(i) < -fovtan*2)
			t = viewwidth+1;
		else
		{
			t = FixedMul(FINETANGENT(i), focallength);
			t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

			if (t < -1)
				t = -1;
			else if (t > viewwidth+1)
				t = viewwidth+1;
		}
		viewangletox[i] = t;
	}

	// Scan viewangletox[] to generate xtoviewangle[]:
	//  xtoviewangle will give the smallest view angle
	//  that maps to x.
	for (x = 0; x <= viewwidth;x++)
	{
		i = 0;
		while (viewangletox[i] > x)
			i++;
		xtoviewangle[x] = (i<<ANGLETOFINESHIFT) - ANGLE_90;
	}

	// Take out the fencepost cases from viewangletox.
	for (i = 0; i < FINEANGLES/2; i++)
	{
		if (viewangletox[i] == -1)
			viewangletox[i] = 0;
		else if (viewangletox[i] == viewwidth+1)
			viewangletox[i]  = viewwidth;
	}

	clipangle = xtoviewangle[0];
	doubleclipangle = clipangle*2;
}



//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP 2

static inline void R_InitLightTables(void)
{
	INT32 i;
	INT32 j;
	INT32 level;
	INT32 startmapl;
	INT32 scale;

	// Calculate the light levels to use
	//  for each level / distance combination.
	for (i = 0; i < LIGHTLEVELS; i++)
	{
		startmapl = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j = 0; j < MAXLIGHTZ; j++)
		{
			//added : 02-02-98 : use BASEVIDWIDTH, vid.width is not set already,
			// and it seems it needs to be calculated only once.
			scale = FixedDiv((BASEVIDWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
			scale >>= LIGHTSCALESHIFT;
			level = startmapl - scale/DISTMAP;

			if (level < 0)
				level = 0;

			if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS-1;

			zlight[i][j] = colormaps + level*256;
		}
	}
}

static struct {
	angle_t rollangle; // pre-shifted by fineshift

	fixed_t zoomneeded;
	INT32 *scrmap;
	INT32 scrmapsize;

	INT32 x1; // clip rendering horizontally for efficiency
	INT16 ceilingclip[MAXVIDWIDTH], floorclip[MAXVIDWIDTH];

	boolean use;
} viewmorph = {
	0,

	FRACUNIT,
	NULL,
	0,

	0,
	{}, {},

	false
};

void R_CheckViewMorph(void)
{
	float zoomfactor, rollcos, rollsin;
	float x1, y1, x2, y2;
	fixed_t temp;
	INT32 end, vx, vy, pos, usedpos;
	INT32 usedx, usedy, halfwidth = vid.width/2, halfheight = vid.height/2;

	angle_t rollangle = viewroll;

	rollangle >>= ANGLETOFINESHIFT;
	rollangle = ((rollangle+2) & ~3) & FINEMASK; // Limit the distinct number of angles to reduce recalcs from angles changing a lot.


	if (rollangle == viewmorph.rollangle &&
		viewmorph.scrmapsize == vid.width*vid.height)
		return; // No change

	viewmorph.rollangle = rollangle;

	if (viewmorph.rollangle == 0)
	{
		viewmorph.use = false;
		viewmorph.x1 = 0;
		if (viewmorph.zoomneeded != FRACUNIT)
			R_SetViewSize();
		viewmorph.zoomneeded = FRACUNIT;

		return;
	}

	if (viewmorph.scrmapsize != vid.width*vid.height)
	{
		if (viewmorph.scrmap)
			free(viewmorph.scrmap);
		viewmorph.scrmap = malloc(vid.width*vid.height * sizeof(INT32));
		viewmorph.scrmapsize = vid.width*vid.height;
	}

	temp = FINECOSINE(rollangle);
	rollcos = FIXED_TO_FLOAT(temp);
	temp = FINESINE(rollangle);
	rollsin = FIXED_TO_FLOAT(temp);

	// Calculate maximum zoom needed
	x1 = (vid.width*fabsf(rollcos) + vid.height*fabsf(rollsin)) / vid.width;
	y1 = (vid.height*fabsf(rollcos) + vid.width*fabsf(rollsin)) / vid.height;

	temp = max(x1, y1)*FRACUNIT;
	if (temp < FRACUNIT)
		temp = FRACUNIT;
	else
		temp |= 0x3FFF; // Limit how many times the viewport needs to be recalculated

	//CONS_Printf("Setting zoom to %f\n", FIXED_TO_FLOAT(temp));

	if (temp != viewmorph.zoomneeded)
	{
		viewmorph.zoomneeded = temp;
		R_SetViewSize();
	}

	zoomfactor = FIXED_TO_FLOAT(viewmorph.zoomneeded);

	end = vid.width * vid.height - 1;

	pos = 0;

	// Pre-multiply rollcos and rollsin to use for positional stuff
	rollcos /= zoomfactor;
	rollsin /= zoomfactor;

	x1 = -(halfwidth * rollcos - halfheight * rollsin);
	y1 = -(halfheight * rollcos + halfwidth * rollsin);

	viewmorph.x1 = (INT32)(halfwidth - (halfwidth * fabsf(rollcos) + halfheight * fabsf(rollsin)));
	//CONS_Printf("saving %d cols\n", viewmorph.x1);

	// Set ceilingclip and floorclip
	for (vx = 0; vx < vid.width; vx++)
	{
		viewmorph.ceilingclip[vx] = vid.height;
		viewmorph.floorclip[vx] = -1;
	}
	x2 = x1;
	y2 = y1;
	for (vx = 0; vx < vid.width; vx++)
	{
		INT16 xa, ya, xb, yb;
		xa = x2+halfwidth;
		ya = y2+halfheight-1;
		xb = vid.width-1-xa;
		yb = vid.height-1-ya;

		viewmorph.ceilingclip[xa] = min(viewmorph.ceilingclip[xa], ya);
		viewmorph.floorclip[xa] = max(viewmorph.floorclip[xa], ya);
		viewmorph.ceilingclip[xb] = min(viewmorph.ceilingclip[xb], yb);
		viewmorph.floorclip[xb] = max(viewmorph.floorclip[xb], yb);
		x2 += rollcos;
		y2 += rollsin;
	}
	x2 = x1;
	y2 = y1;
	for (vy = 0; vy < vid.height; vy++)
	{
		INT16 xa, ya, xb, yb;
		xa = x2+halfwidth;
		ya = y2+halfheight;
		xb = vid.width-1-xa;
		yb = vid.height-1-ya;

		viewmorph.ceilingclip[xa] = min(viewmorph.ceilingclip[xa], ya);
		viewmorph.floorclip[xa] = max(viewmorph.floorclip[xa], ya);
		viewmorph.ceilingclip[xb] = min(viewmorph.ceilingclip[xb], yb);
		viewmorph.floorclip[xb] = max(viewmorph.floorclip[xb], yb);
		x2 -= rollsin;
		y2 += rollcos;
	}

	//CONS_Printf("Top left corner is %f %f\n", x1, y1);

	x1 += halfwidth;
	y1 += halfheight;

	for (vy = 0; vy < halfheight; vy++)
	{
		x2 = x1;
		y2 = y1;
		x1 -= rollsin;
		y1 += rollcos;

		for (vx = 0; vx < vid.width; vx++)
		{
			usedx = x2;
			usedy = y2;

			usedpos = usedx + usedy*vid.width;

			viewmorph.scrmap[pos] = usedpos;
			viewmorph.scrmap[end-pos] = end-usedpos;

			x2 += rollcos;
			y2 += rollsin;
			pos++;
		}
	}

	viewmorph.use = true;
}

void R_ApplyViewMorph(void)
{
	UINT8 *tmpscr = screens[4];
	UINT8 *srcscr = screens[0];
	INT32 p, end = vid.width * vid.height;

	if (!viewmorph.use)
		return;

	if (cv_debug & DBG_VIEWMORPH)
	{
		UINT8 border = 32;
		UINT8 grid = 160;
		INT32 ws = vid.width / 4;
		INT32 hs = vid.width * (vid.height / 4);

		memcpy(tmpscr, srcscr, vid.width*vid.height);
		for (p = 0; p < vid.width; p++)
		{
			tmpscr[viewmorph.scrmap[p]] = border;
			tmpscr[viewmorph.scrmap[p + hs]] = grid;
			tmpscr[viewmorph.scrmap[p + hs*2]] = grid;
			tmpscr[viewmorph.scrmap[p + hs*3]] = grid;
			tmpscr[viewmorph.scrmap[end - 1 - p]] = border;
		}
		for (p = vid.width; p < end; p += vid.width)
		{
			tmpscr[viewmorph.scrmap[p]] = border;
			tmpscr[viewmorph.scrmap[p + ws]] = grid;
			tmpscr[viewmorph.scrmap[p + ws*2]] = grid;
			tmpscr[viewmorph.scrmap[p + ws*3]] = grid;
			tmpscr[viewmorph.scrmap[end - 1 - p]] = border;
		}
	}
	else
		for (p = 0; p < end; p++)
			tmpscr[p] = srcscr[viewmorph.scrmap[p]];

	VID_BlitLinearScreen(tmpscr, screens[0],
			vid.width*vid.bpp, vid.height, vid.width*vid.bpp, vid.width);
}

static inline int intsign(int n) {
	return n < 0 ? -1 : n > 0 ? 1 : 0;
}

angle_t R_ViewRollAngle(const player_t *player)
{
	angle_t roll = 0;

	if (gamestate != GS_LEVEL)
	{
		// FIXME: The way this is implemented is totally
		// incompatible with cameras that aren't directly
		// tied to the player. (podium, titlemap,
		// MT_ALTVIEWMAN in general)

		// All of these player variables should affect their
		// camera_t in P_MoveChaseCamera, and then this
		// just returns that variable instead.
		return 0;
	}

	roll = player->viewrollangle;

	if (cv_tilting.value)
	{
		if (!player->spectator && !demo.freecam)
			roll += player->tilt;

		if (cv_actionmovie.value)
		{
			int xs = intsign(quake.x),
			ys = intsign(quake.y),
			zs = intsign(quake.z);
			roll += (xs ^ ys ^ zs) * ANG1;
		}
	}

	return roll;
}

//
// R_SetViewSize
// Do not really change anything here,
// because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean setsizeneeded;

void R_SetViewSize(void)
{
	setsizeneeded = true;
}

//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize(void)
{
	fixed_t dy;
	INT32 i;
	INT32 j;
	INT32 level;
	INT32 startmapl;
	angle_t fov;

	setsizeneeded = false;

	if (rendermode == render_none)
		return;

	// status bar overlay
	st_overlay = cv_showhud.value;

	scaledviewwidth = vid.width;
	viewheight = vid.height;

	if (splitscreen)
		viewheight >>= 1;

	viewwidth = scaledviewwidth;

	if (splitscreen > 1)
	{
		viewwidth >>= 1;
		scaledviewwidth >>= 1;
	}

	centerx = viewwidth/2;
	centery = viewheight/2;
	centerxfrac = centerx<<FRACBITS;
	centeryfrac = centery<<FRACBITS;

	fov = FixedAngle(cv_fov.value/2) + ANGLE_90;
	fovtan = FixedMul(FINETANGENT(fov >> ANGLETOFINESHIFT), viewmorph.zoomneeded);
	if (splitscreen == 1) // Splitscreen FOV should be adjusted to maintain expected vertical view
		fovtan = 17*fovtan/10;

	projection = projectiony = FixedDiv(centerxfrac, fovtan);

	R_InitViewBuffer(scaledviewwidth, viewheight);

	R_InitTextureMapping();

	// why did we calc all the software crap?
#ifdef HWRENDER
	if (rendermode == render_opengl)
	{
		HWR_SetViewSize();
		am_recalc = true;
		return;
	}
#endif

	// thing clipping
	for (i = 0; i < viewwidth; i++)
		screenheightarray[i] = (INT16)viewheight;

	// setup sky scaling
	R_SetSkyScale();

	// planes
	// this is only used for planes rendering in software mode
	j = viewheight*16;
	for (i = 0; i < j; i++)
	{
		dy = (i - viewheight*8)<<FRACBITS;
		dy = FixedMul(abs(dy), fovtan);
		yslopetab[i] = FixedDiv(centerx*FRACUNIT, dy);
	}
		
	if (ds_su)
		Z_Free(ds_su);
	if (ds_sv)
		Z_Free(ds_sv);
	if (ds_sz)
		Z_Free(ds_sz);

	ds_su = ds_sv = ds_sz = NULL;
	ds_sup = ds_svp = ds_szp = NULL;

	memset(scalelight, 0xFF, sizeof(scalelight));

	// Calculate the light levels to use for each level/scale combination.
	for (i = 0; i< LIGHTLEVELS; i++)
	{
		startmapl = ((LIGHTLEVELS - 1 - i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
		for (j = 0; j < MAXLIGHTSCALE; j++)
		{
			level = startmapl - j*vid.width/(viewwidth)/DISTMAP;

			if (level < 0)
				level = 0;

			if (level >= NUMCOLORMAPS)
				level = NUMCOLORMAPS - 1;

			scalelight[i][j] = colormaps + level*256;
		}
	}

	am_recalc = true;
}

//
// R_Init
//

void R_Init(void)
{
	// screensize independent
	//I_OutputMsg("\nR_InitData");
	R_InitData();

	//I_OutputMsg("\nR_InitViewBorder");
	R_InitViewBorder();
	R_SetViewSize(); // setsizeneeded is set true

	//I_OutputMsg("\nR_InitPlanes");
	R_InitPlanes();

	// this is now done by SCR_Recalc() at the first mode set
	//I_OutputMsg("\nR_InitLightTables");
	R_InitLightTables();

	//I_OutputMsg("\nR_InitTranslationTables\n");
	R_InitTranslationTables();

	R_InitDrawNodes();

	framecount = 0;
}

//
// R_PointInSubsector
//
subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
	size_t nodenum = numnodes-1;

	while (!(nodenum & NF_SUBSECTOR))
		nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];

	return &subsectors[nodenum & ~NF_SUBSECTOR];
}

//
// R_IsPointInSubsector, same as above but returns 0 if not in subsector
//
subsector_t *R_IsPointInSubsector(fixed_t x, fixed_t y)
{
	node_t *node;
	INT32 side, i;
	size_t nodenum;
	subsector_t *ret;

	// single subsector is a special case
	//if (numnodes == 0)
		//return subsectors;

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

//
// R_SetupFrame
//

mobj_t *viewmobj;

void R_SkyboxFrame(player_t *player)
{
	camera_t *thiscam = &camera[0];
	UINT8 i;

	if (splitscreen)
	{
		for (i = 0; i <= splitscreen; i++)
		{
			if (player == &players[displayplayers[i]])
			{
				thiscam = &camera[i];
				R_SetViewContext(VIEWCONTEXT_SKY1 + i);
				break;
			}
		}
	}
	else
	{
		R_SetViewContext(VIEWCONTEXT_SKY1);
	}

	// cut-away view stuff
	newview->sky = true;
	viewmobj = skyboxmo[0];
#ifdef PARANOIA
	if (!viewmobj)
	{
		const size_t playeri = (size_t)(player - players);
		I_Error("R_SkyboxFrame: viewmobj null (player %s)", sizeu1(playeri));
	}
#endif
	if (player->awayviewtics)
	{
		newview->aim = player->awayviewaiming;
		newview->angle = player->awayviewmobj->angle;
	}
	else if (thiscam->chase)
	{
		newview->aim = thiscam->aiming;
		newview->angle = thiscam->angle;
	}
	else
	{
		newview->aim = player->aiming;
		newview->angle = player->mo->angle;
		if (!demo.playback && player->playerstate != PST_DEAD)
		{
			if (player == &players[consoleplayer])
			{
				newview->angle = localangle[0]; // WARNING: camera uses this
				newview->aim = localaiming[0];
			}
			else if (splitscreen)
			{
				for (i = 1; i <= splitscreen; i++)
				{
					if (player == &players[displayplayers[i]])
					{
						newview->angle = localangle[i];
						newview->aim = localaiming[i];
						break;
					}
				}
			}
		}
	}
	newview->angle += viewmobj->angle;
	newview->roll = R_ViewRollAngle(player);

	newview->player = player;

	newview->x = viewmobj->x;
	newview->y = viewmobj->y;
	newview->z = 0;
	if (viewmobj->spawnpoint)
		newview->z = ((fixed_t)viewmobj->spawnpoint->angle)<<FRACBITS;

	newview->x += quake.x;
	newview->y += quake.y;
	newview->z += quake.z;

	if (mapheaderinfo[gamemap-1])
	{
		mapheader_t *mh = mapheaderinfo[gamemap-1];
		if (player->awayviewtics)
		{
			if (skyboxmo[1])
			{
				fixed_t x = 0, y = 0;
				if (mh->skybox_scalex > 0)
					x = (player->awayviewmobj->x - skyboxmo[1]->x) / mh->skybox_scalex;
				else if (mh->skybox_scalex < 0)
					x = (player->awayviewmobj->x - skyboxmo[1]->x) * -mh->skybox_scalex;

				if (mh->skybox_scaley > 0)
					y = (player->awayviewmobj->y - skyboxmo[1]->y) / mh->skybox_scaley;
				else if (mh->skybox_scaley < 0)
					y = (player->awayviewmobj->y - skyboxmo[1]->y) * -mh->skybox_scaley;

				if (viewmobj->angle == 0)
				{
					newview->x  += x;
					newview->y += y;
				}
				else if (viewmobj->angle == ANGLE_90)
				{
					newview->x  -= y;
					newview->y += x;
				}
				else if (viewmobj->angle == ANGLE_180)
				{
					newview->x  -= x;
					newview->y -= y;
				}
				else if (viewmobj->angle == ANGLE_270)
				{
					newview->x  += y;
					newview->y -= x;
				}
				else
				{
					angle_t ang = viewmobj->angle>>ANGLETOFINESHIFT;
					newview->x  += FixedMul(x,FINECOSINE(ang)) - FixedMul(y,  FINESINE(ang));
					newview->y += FixedMul(x,  FINESINE(ang)) + FixedMul(y,FINECOSINE(ang));
				}
			}
			if (mh->skybox_scalez > 0)
				newview->z += (player->awayviewmobj->z + 20*FRACUNIT) / mh->skybox_scalez;
			else if (mh->skybox_scalez < 0)
				newview->z += (player->awayviewmobj->z + 20*FRACUNIT) * -mh->skybox_scalez;
		}
		else if (thiscam->chase)
		{
			if (skyboxmo[1])
			{
				fixed_t x = 0, y = 0;
				if (mh->skybox_scalex > 0)
					x = (thiscam->x - skyboxmo[1]->x) / mh->skybox_scalex;
				else if (mh->skybox_scalex < 0)
					x = (thiscam->x - skyboxmo[1]->x) * -mh->skybox_scalex;

				if (mh->skybox_scaley > 0)
					y = (thiscam->y - skyboxmo[1]->y) / mh->skybox_scaley;
				else if (mh->skybox_scaley < 0)
					y = (thiscam->y - skyboxmo[1]->y) * -mh->skybox_scaley;

				if (viewmobj->angle == 0)
				{
					newview->x += x;
					newview->y += y;
				}
				else if (viewmobj->angle == ANGLE_90)
				{
					newview->x -= y;
					newview->y += x;
				}
				else if (viewmobj->angle == ANGLE_180)
				{
					newview->x -= x;
					newview->y -= y;
				}
				else if (viewmobj->angle == ANGLE_270)
				{
					newview->x += y;
					newview->y -= x;
				}
				else
				{
					angle_t ang = viewmobj->angle>>ANGLETOFINESHIFT;
					newview->x += FixedMul(x,FINECOSINE(ang)) - FixedMul(y,  FINESINE(ang));
					newview->y += FixedMul(x,  FINESINE(ang)) + FixedMul(y,FINECOSINE(ang));
				}
			}
			if (mh->skybox_scalez > 0)
				newview->z += (thiscam->z + (thiscam->height>>1)) / mh->skybox_scalez;
			else if (mh->skybox_scalez < 0)
				newview->z += (thiscam->z + (thiscam->height>>1)) * -mh->skybox_scalez;
		}
		else
		{
			if (skyboxmo[1])
			{
				fixed_t x = 0, y = 0;
				if (mh->skybox_scalex > 0)
					x = (player->mo->x - skyboxmo[1]->x) / mh->skybox_scalex;
				else if (mh->skybox_scalex < 0)
					x = (player->mo->x - skyboxmo[1]->x) * -mh->skybox_scalex;
				if (mh->skybox_scaley > 0)
					y = (player->mo->y - skyboxmo[1]->y) / mh->skybox_scaley;
				else if (mh->skybox_scaley < 0)
					y = (player->mo->y - skyboxmo[1]->y) * -mh->skybox_scaley;

				if (viewmobj->angle == 0)
				{
					newview->x += x;
					newview->y += y;
				}
				else if (viewmobj->angle == ANGLE_90)
				{
					newview->x -= y;
					newview->y += x;
				}
				else if (viewmobj->angle == ANGLE_180)
				{
					newview->x -= x;
					newview->y -= y;
				}
				else if (viewmobj->angle == ANGLE_270)
				{
					newview->x += y;
					newview->y -= x;
				}
				else
				{
					angle_t ang = viewmobj->angle>>ANGLETOFINESHIFT;
					newview->x += FixedMul(x,FINECOSINE(ang)) - FixedMul(y,  FINESINE(ang));
					newview->y += FixedMul(x,  FINESINE(ang)) + FixedMul(y,FINECOSINE(ang));
				}
			}
			if (mh->skybox_scalez > 0)
				newview->z += player->viewz / mh->skybox_scalez;
			else if (mh->skybox_scalez < 0)
				newview->z += player->viewz * -mh->skybox_scalez;
		}
	}

	if (viewmobj->subsector)
		newview->sector = viewmobj->subsector->sector;
	else
		newview->sector = R_PointInSubsector(newview->x, newview->y)->sector;

	R_InterpolateView(R_UsingFrameInterpolation() ? (demo.playback && demo.freecam) ? rendertimefrac_unpaused : rendertimefrac : FRACUNIT, false);
}

void R_SetupFrame(player_t *player, boolean skybox)
{
	camera_t *thiscam;
	boolean chasecam = false;

	if (splitscreen > 2 && player == &players[displayplayers[3]])
	{
		thiscam = &camera[3];
		chasecam = (cv_chasecam4.value != 0);
		R_SetViewContext(VIEWCONTEXT_PLAYER4);
		if (thiscam->reset)
		{
			R_ResetViewInterpolation(4);
			thiscam->reset = false;
		}
	}
	else if (splitscreen > 1 && player == &players[displayplayers[2]])
	{
		thiscam = &camera[2];
		chasecam = (cv_chasecam3.value != 0);
		R_SetViewContext(VIEWCONTEXT_PLAYER3);
		if (thiscam->reset)
		{
			R_ResetViewInterpolation(3);
			thiscam->reset = false;
		}
	}
	else if (splitscreen && player == &players[displayplayers[1]])
	{
		thiscam = &camera[1];
		chasecam = (cv_chasecam2.value != 0);
		R_SetViewContext(VIEWCONTEXT_PLAYER2);
		if (thiscam->reset)
		{
			R_ResetViewInterpolation(2);
			thiscam->reset = false;
		}
	}
	else
	{
		thiscam = &camera[0];
		chasecam = (cv_chasecam.value != 0);
		R_SetViewContext(VIEWCONTEXT_PLAYER1);
		if (thiscam->reset)
		{
			R_ResetViewInterpolation(1);
			thiscam->reset = false;
		}
	}

	if (player->spectator) // no spectator chasecam
		chasecam = false; // force chasecam off
	else if (player->playerstate == PST_DEAD || player->exiting)
		chasecam = true; // force chasecam on

	if (chasecam && (thiscam && !thiscam->chase))
	{
		P_ResetCamera(player, thiscam);
		thiscam->chase = true;
	}
	else if (thiscam && !chasecam)
		thiscam->chase = false;

	newview->sky = !skybox;

	if (player->awayviewtics) // cut-away view stuff
	{
		viewmobj = player->awayviewmobj; // should be a MT_ALTVIEWMAN
		I_Assert(viewmobj != NULL);
		newview->z = viewmobj->z + 20*FRACUNIT;
		newview->aim = player->awayviewaiming;
		newview->angle = viewmobj->angle;
	}
	else if (!player->spectator && (thiscam && chasecam)) // use outside cam view
	{
		viewmobj = NULL;
		newview->z = thiscam->z + (thiscam->height>>1);
		newview->aim = thiscam->aiming;
		newview->angle = thiscam->angle;
	}
	else // use the player's eyes view
	{
		newview->z = player->viewz;

		viewmobj = player->mo;
		I_Assert(viewmobj != NULL);

		newview->aim = player->aiming;
		newview->angle = viewmobj->angle;

		if (!demo.playback && player->playerstate != PST_DEAD)
		{
			if (player == &players[consoleplayer])
			{
				newview->angle = localangle[0]; // WARNING: camera uses this
				newview->aim = localaiming[0];
			}
			else if (splitscreen)
			{
				UINT8 i;
				for (i = 1; i <= splitscreen; i++)
				{
					if (player == &players[displayplayers[i]])
					{
						newview->angle = localangle[i];
						newview->aim = localaiming[i];
						break;
					}
				}
			}
		}
	}
	newview->roll = R_ViewRollAngle(player);
	newview->z += quake.z;

	newview->player = player;

	if ((thiscam && chasecam) && !player->awayviewtics && !player->spectator)
	{
		newview->x = thiscam->x;
		newview->y = thiscam->y;
		newview->x += quake.x;
		newview->y += quake.y;

		if (thiscam->subsector && thiscam->subsector->sector)
			newview->sector = thiscam->subsector->sector;
		else
			newview->sector = R_PointInSubsector(newview->x, newview->y)->sector;
	}
	else
	{
		newview->x = viewmobj->x;
		newview->y = viewmobj->y;
		newview->x += quake.x;
		newview->y += quake.y;

		if (!P_MobjWasRemoved(viewmobj) && viewmobj->subsector && thiscam && thiscam->subsector->sector)
			newview->sector = viewmobj->subsector->sector;
		else
			newview->sector = R_PointInSubsector(newview->x, newview->y)->sector;
	}

	R_InterpolateView(R_UsingFrameInterpolation() ? (demo.playback && demo.freecam) ? rendertimefrac_unpaused : rendertimefrac : FRACUNIT, false);
}

static void R_PortalFrame(portal_t *portal)
{
	viewx = portal->viewx;
	viewy = portal->viewy;
	viewz = portal->viewz;

	viewangle = portal->viewangle;
	// viewsin = FINESINE(viewangle>>ANGLETOFINESHIFT);
	// viewcos = FINECOSINE(viewangle>>ANGLETOFINESHIFT);

	portalclipstart = portal->start;
	portalclipend = portal->end;

	if (portal->clipline != -1)
	{
		portalclipline = &lines[portal->clipline];
		portalcullsector = portalclipline->frontsector;
		viewsector = portalclipline->frontsector;
	}
	else
	{
		portalclipline = NULL;
		portalcullsector = NULL;
		viewsector = R_PointInSubsector(viewx, viewy)->sector;
	}
}

// ================
// R_RenderView
// ================

//                     FAB NOTE FOR WIN32 PORT !! I'm not finished already,
// but I suspect network may have problems with the video buffer being locked
// for all duration of rendering, and being released only once at the end..
// I mean, there is a win16lock() or something that lasts all the rendering,
// so maybe we should release screen lock before each netupdate below..?

void R_RenderPlayerView(player_t *player)
{
	const boolean skybox = (skyboxmo[0] && cv_skybox.value);
	UINT8 i;

	// if this is display player 1
	if (cv_homremoval.value && player == &players[displayplayers[0]])
	{
		if (cv_homremoval.value == 1)
			V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31); // No HOM effect!
		else //'development' HOM removal -- makes it blindingly obvious if HOM is spotted.
			V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 128+(timeinmap&15));
	}
	// Draw over the fourth screen so you don't have to stare at a HOM :V
	else if (splitscreen == 2 && player == &players[displayplayers[2]])
	{
		// V_DrawPatchFill, but for the fourth screen only
		patch_t *pat = W_CachePatchName("SRB2BACK", PU_CACHE);
		INT32 dupz = (vid.dupx < vid.dupy ? vid.dupx : vid.dupy);
		INT32 x, y, pw = SHORT(pat->width) * dupz, ph = SHORT(pat->height) * dupz;

		for (x = vid.width>>1; x < vid.width; x += pw)
		{
			for (y = vid.height>>1; y < vid.height; y += ph)
				V_DrawScaledPatch(x, y, V_NOSCALESTART, pat);
		}
	}

	// load previous saved value of skyVisible for the player
	for (i = 0; i <= splitscreen; i++)
	{
		if (player != &players[displayplayers[i]])
			continue;

		skyVisible = skyVisiblePerPlayer[i];
		break;
	}

	Portal_InitList();
	
	PS_START_TIMING(ps_skyboxtime);
	if (skybox && skyVisible)
	{
		R_SkyboxFrame(player);

		R_ClearClipSegs();
		R_ClearDrawSegs();
		R_ClearPlanes();
		R_ClearSprites();
#ifdef FLOORSPLATS
		R_ClearVisibleFloorSplats();
#endif

		R_RenderBSPNode((INT32)numnodes - 1);
		R_ClipSprites();
		R_DrawPlanes();
#ifdef FLOORSPLATS
		R_DrawVisibleFloorSplats();
#endif
		R_DrawMasked();
	}
	PS_STOP_TIMING(ps_skyboxtime);

	R_SetupFrame(player, skybox);
	skyVisible = false;
	framecount++;
	validcount++;

	// Clear buffers.
	R_ClearPlanes();
	if (viewmorph.use)
	{
		portalclipstart = viewmorph.x1;
		portalclipend = viewwidth-viewmorph.x1-1;
		R_PortalClearClipSegs(portalclipstart, portalclipend);
		memcpy(ceilingclip, viewmorph.ceilingclip, sizeof(INT16)*vid.width);
		memcpy(floorclip, viewmorph.floorclip, sizeof(INT16)*vid.width);
	}
	else
	{
		portalclipstart = 0;
		portalclipend = viewwidth;
		R_ClearClipSegs();
	}
	R_ClearDrawSegs();
	R_ClearSprites();
#ifdef FLOORSPLATS
	R_ClearVisibleFloorSplats();
#endif

	// The head node is the last node output.

	ps_numbspcalls.value.i = ps_numpolyobjects.value.i = ps_numdrawnodes.value.i = 0;
	PS_START_TIMING(ps_bsptime);
	R_RenderBSPNode((INT32)numnodes - 1);
	PS_STOP_TIMING(ps_bsptime);
	R_AddPrecipitationSprites();
	PS_START_TIMING(ps_sw_spritecliptime);
	R_ClipSprites();
	PS_STOP_TIMING(ps_sw_spritecliptime);
	
	ps_numsprites.value.i = numvisiblesprites;

	PS_START_TIMING(ps_sw_portaltime);
	// Portal rendering. Hijacks the BSP traversal.
	if (portal_base)
	{
		portal_t *portal;

		for (portal = portal_base; portal; portal = portal_base)
		{
			portalrender = portal->pass; // Recursiveness depth.

			R_ClearFFloorClips();

			// Apply the viewpoint stored for the portal.
			R_PortalFrame(portal);

			// Hack in the clipsegs to delimit the starting
			// clipping for sprites and possibly other similar
			// future items.
			R_PortalClearClipSegs(portal->start, portal->end);

			// Hack in the top/bottom clip values for the window
			// that were previously stored.
			Portal_ClipApply(portal);

			// Render the BSP from the new viewpoint, and clip
			// any sprites with the new clipsegs and window.
			R_RenderBSPNode((INT32)numnodes - 1);
			R_ClipSprites();

			Portal_Remove(portal);

			validcount++;
		}
	}
	PS_STOP_TIMING(ps_sw_portaltime);

	PS_START_TIMING(ps_sw_planetime);
	R_DrawPlanes();
	PS_STOP_TIMING(ps_sw_planetime);
#ifdef FLOORSPLATS
	R_DrawVisibleFloorSplats();
#endif
	// draw mid texture and sprite
	// And now 3D floors/sides!
	PS_START_TIMING(ps_sw_maskedtime);
	R_DrawMasked();
	PS_STOP_TIMING(ps_sw_maskedtime);

	// save value to skyVisiblePerPlayer
	// this is so that P1 can't affect whether P2 can see a skybox or not, or vice versa
	for (i = 0; i <= splitscreen; i++)
	{
		if (player != &players[displayplayers[i]])
			continue;

		skyVisiblePerPlayer[i] = skyVisible;
		break;
	}
}

// =========================================================================
//                    ENGINE COMMANDS & VARS
// =========================================================================

void R_RegisterEngineStuff(void)
{
	CV_RegisterVar(&cv_gravity);
	CV_RegisterVar(&cv_tailspickup);
	CV_RegisterVar(&cv_soniccd);
	CV_RegisterVar(&cv_allowmlook);
	CV_RegisterVar(&cv_homremoval);
	CV_RegisterVar(&cv_flipcam);
	CV_RegisterVar(&cv_flipcam2);
	CV_RegisterVar(&cv_flipcam3);
	CV_RegisterVar(&cv_flipcam4);

	// Enough for dedicated server
	if (dedicated)
		return;

	CV_RegisterVar(&cv_translucency);
	CV_RegisterVar(&cv_drawdist);
	CV_RegisterVar(&cv_drawdist_precip);
	CV_RegisterVar(&cv_lessprecip);
	CV_RegisterVar(&cv_mobjscaleprecip);
	CV_RegisterVar(&cv_fov);

	CV_RegisterVar(&cv_chasecam);
	CV_RegisterVar(&cv_chasecam2);
	CV_RegisterVar(&cv_chasecam3);
	CV_RegisterVar(&cv_chasecam4);
	CV_RegisterVar(&cv_shadow);
	CV_RegisterVar(&cv_shadowoffs);
	CV_RegisterVar(&cv_skybox);
	CV_RegisterVar(&cv_ffloorclip);
	CV_RegisterVar(&cv_spriteclip);

	CV_RegisterVar(&cv_cam_dist);
	CV_RegisterVar(&cv_cam_still);
	CV_RegisterVar(&cv_cam_height);
	CV_RegisterVar(&cv_cam_speed);
	CV_RegisterVar(&cv_cam_rotate);
	CV_RegisterVar(&cv_cam_rotspeed);

	CV_RegisterVar(&cv_cam2_dist);
	CV_RegisterVar(&cv_cam2_still);
	CV_RegisterVar(&cv_cam2_height);
	CV_RegisterVar(&cv_cam2_speed);
	CV_RegisterVar(&cv_cam2_rotate);
	CV_RegisterVar(&cv_cam2_rotspeed);

	CV_RegisterVar(&cv_cam3_dist);
	CV_RegisterVar(&cv_cam3_still);
	CV_RegisterVar(&cv_cam3_height);
	CV_RegisterVar(&cv_cam3_speed);
	CV_RegisterVar(&cv_cam3_rotate);
	CV_RegisterVar(&cv_cam3_rotspeed);

	CV_RegisterVar(&cv_cam4_dist);
	CV_RegisterVar(&cv_cam4_still);
	CV_RegisterVar(&cv_cam4_height);
	CV_RegisterVar(&cv_cam4_speed);
	CV_RegisterVar(&cv_cam4_rotate);
	CV_RegisterVar(&cv_cam4_rotspeed);

	CV_RegisterVar(&cv_tilting);
	CV_RegisterVar(&cv_quaketilt);
	CV_RegisterVar(&cv_tiltsmoothing);
	CV_RegisterVar(&cv_actionmovie);

	CV_RegisterVar(&cv_lookbackmom);

	CV_RegisterVar(&cv_driftsparkpulse);
	CV_RegisterVar(&cv_gravstretch);
	CV_RegisterVar(&cv_sloperoll);
	CV_RegisterVar(&cv_spriteroll);
	CV_RegisterVar(&cv_sliptideroll);
	CV_RegisterVar(&cv_sloperolldist);
	CV_RegisterVar(&cv_sparkroll);

	CV_RegisterVar(&cv_showhud);
	CV_RegisterVar(&cv_translucenthud);
	CV_RegisterVar(&cv_uncappedhud);

	CV_RegisterVar(&cv_maxportals);

	CV_RegisterVar(&cv_maxinterpdist);

	CV_RegisterVar(&cv_ripplewater);

	// Default viewheight is changeable,
	// initialized to standard viewheight
	//CV_RegisterVar(&cv_viewheight);

	// Frame interpolation/uncapped
	CV_RegisterVar(&cv_fpscap);
	CV_RegisterVar(&cv_precipinterp);
}
