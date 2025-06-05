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
/// \file  r_plane.c
/// \brief Here is a core component: drawing the floors and ceilings,
///        while maintaining a per column clipping list only.
///        Moreover, the sky areas have to be determined.

#include "doomdef.h"
#include "console.h"
#include "g_game.h"
#include "p_setup.h" // levelflats
#include "p_slopes.h"
#include "r_data.h"
#include "r_local.h"
#include "r_state.h"
#include "r_splats.h" // faB(21jan):testing
#include "r_sky.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "p_tick.h"
#include "r_fps.h"
#include "r_portal.h"
#include "core/thread_pool.h"
#include <algorithm>

static void R_SetSlopePlaneVectors(drawspandata_t* ds, visplane_t *pl, INT32 y, fixed_t xoff, fixed_t yoff, float fudge);
static void R_SetTiltedSpan(drawspandata_t* ds, INT32 span);

//
// opening
//

// Quincunx antialiasing of flats!
//#define QUINCUNX

// good night sweet prince
//#define SHITPLANESPARENCY

visplane_t *visplanes[MAXVISPLANES];
static visplane_t *freetail;
static visplane_t **freehead = &freetail;

visplane_t *floorplane;
visplane_t *ceilingplane;

visffloor_t ffloor[MAXFFLOORS];
INT32 numffloors;

//SoM: 3/23/2000: Boom visplane hashing routine.
#define visplane_hash(picnum,lightlevel,height) \
  ((unsigned)((picnum)*3+(lightlevel)+(height)*7) & VISPLANEHASHMASK)

//SoM: 3/23/2000: Use boom opening limit removal
size_t maxopenings;
INT16 *openings, *lastopening; /// \todo free leak

//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
INT16 *floorclip, *ceilingclip;
fixed_t *frontscale;

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
static INT32 *spanstart;

//added : 10-02-98: yslopetab is what yslope used to be,
//                yslope points somewhere into yslopetab,
//                now (viewheight/2) slopes are calculated above and
//                below the original viewheight for mouselook
//                (this is to calculate yslopes only when really needed)
//                (when mouselookin', yslope is moving into yslopetab)
//                Check R_SetupFrame, R_SetViewSize for more...
fixed_t *yslopetab;
fixed_t *yslope;

fixed_t basexscale, baseyscale;

static INT16 *ffloor_f_clip;
static INT16 *ffloor_c_clip;

static void R_ReallocPlaneBounds(visplane_t *pl)
{
	pl->top_memory    = static_cast<UINT16*>(Z_Realloc(pl->top_memory, sizeof(UINT16) * (viewwidth + 2), PU_STATIC, NULL));
	pl->bottom_memory = static_cast<UINT16*>(Z_Realloc(pl->bottom_memory, sizeof(UINT16) * (viewwidth + 2), PU_STATIC, NULL));
	pl->top = pl->top_memory + 1;
	pl->bottom = pl->bottom_memory + 1;
}

void R_AllocPlaneMemory(void)
{
	visplane_t *check;

	// Alloc visplane top/bottom bounds
	for (unsigned i = 0; i < MAXVISPLANES; i++)
	{
		check = visplanes[i];

		while (check)
		{
			R_ReallocPlaneBounds(check);
			check = check->next;
		}
	}

	// Need to do it for "freed" visplanes too
	check = freetail;
	while (check)
	{
		R_ReallocPlaneBounds(check);
		check = check->next;
	}

	// Alloc ffloor clip tables
	ffloor_f_clip = static_cast<INT16*>(Z_Realloc(ffloor_f_clip, sizeof(*ffloor_f_clip) * (viewwidth * MAXFFLOORS), PU_STATIC, NULL));
	ffloor_c_clip = static_cast<INT16*>(Z_Realloc(ffloor_c_clip, sizeof(*ffloor_c_clip) * (viewwidth * MAXFFLOORS), PU_STATIC, NULL));

	for (unsigned i = 0; i < MAXFFLOORS; i++)
	{
		ffloor[i].f_clip = ffloor_f_clip + (i * viewwidth);
		ffloor[i].c_clip = ffloor_c_clip + (i * viewwidth);
	}

	yslopetab = static_cast<fixed_t*>(Z_Realloc(yslopetab, sizeof(*yslopetab) * (viewheight * 16), PU_STATIC, NULL));
	spanstart = static_cast<fixed_t*>(Z_Realloc(spanstart, sizeof(*spanstart) * viewheight, PU_STATIC, NULL));
}

//
// Water ripple effect!!
// Needs the height of the plane, and the vertical position of the span.
// Sets planeripple.xfrac and planeripple.yfrac, added to ds_xfrac and ds_yfrac, if the span is not tilted.
//

static void R_CalculatePlaneRipple(drawspandata_t* ds, visplane_t *plane, INT32 y, fixed_t plheight, boolean calcfrac)
{
	fixed_t distance = FixedMul(plheight, yslope[y]);
	const INT32 yay = (ds->planeripple.offset + (distance>>9)) & 8191;

	// ripples da water texture
	ds->bgofs = FixedDiv(FINESINE(yay), (1<<12) + (distance>>11))>>FRACBITS;

	if (calcfrac)
	{
		angle_t angle = (plane->viewangle + plane->plangle)>>ANGLETOFINESHIFT;
		angle = (angle + 2048) & 8191; // 90 degrees
		ds->planeripple.xfrac = FixedMul(FINECOSINE(angle), (ds->bgofs<<FRACBITS));
		ds->planeripple.yfrac = FixedMul(FINESINE(angle), (ds->bgofs<<FRACBITS));
	}
}

static void R_UpdatePlaneRipple(drawspandata_t* ds)
{
	ds->waterofs = (leveltime & 1)*16384;
	ds->planeripple.offset = ((leveltime-1)*140) + ((rendertimefrac*140) / FRACUNIT);
}

static void R_HandleRipplePlane(drawspandata_t* ds, visplane_t *pl)
{
	INT32 top, bottom;
	UINT8 *scr;

	if (!cv_ripplewater.value)
	{
		ds->planeripple.active = false;
		return;
	}

	ds->planeripple.active = true;

	if (spanfunc == R_DrawTranslucentSpan)
	{
		spanfunc = R_DrawTranslucentWaterSpan;

		// Copy the current scene, ugh
		top = pl->high-8;
		bottom = pl->low+8;

		if (top < 0)
			top = 0;
		if (bottom > vid.height)
			bottom = vid.height;

		// Only copy the part of the screen we need
		scr = (renderscreen + (top*vid.width));

		for (UINT8 i = 1; i <= splitscreen; i++)
		{
			if (viewplayer != &players[displayplayers[i]])
				continue;

			switch (i)
			{
				case 1:
					if (splitscreen == 1)
						scr = (renderscreen + (top + viewheight) * vid.width);
					else if (splitscreen > 1)
						scr = (renderscreen + (top * vid.width) + viewwidth);
					break;
				case 2:
					scr = (renderscreen + (top + viewheight) * vid.width);
					break;
				case 3:
					scr = (renderscreen + (top + viewheight) * vid.width + viewwidth);
					break;
				default:
					break;
			}

			break;
		}

		VID_BlitLinearScreen(scr, vid.screens[1]+((top)*vid.width),
											 vid.width, bottom-top,
											 vid.width, vid.width);
	}
}


static bool R_CheckMapPlane(const char* funcname, INT32 y, INT32 x1, INT32 x2)
{
	if (x1 == x2)
		return true;

	if (x1 < x2 && x1 >= 0 && x2 < viewwidth && y >= 0 && y < viewheight)
		return true;

	CONS_Debug(DBG_RENDER, "%s: x1=%d, x2=%d at y=%d\n", funcname, x1, x2, y);
	return false;
}

//
// R_MapPlane
//
// Uses global vars:
//  basexscale
//  baseyscale
//  centerx
//  viewx
//  viewy
//  viewsin
//  viewcos
//  viewheight
static void R_MapPlane(drawspandata_t *ds, void(*spanfunc2)(drawspandata_t*), INT32 y, INT32 x1, INT32 x2, boolean allow_parallel)
{
	angle_t angle, planecos, planesin;
	fixed_t distance = 0, span;
	size_t pindex;

	if (!R_CheckMapPlane(__func__, y, x1, x2))
		return;

	angle = (ds->currentplane->viewangle + ds->currentplane->plangle)>>ANGLETOFINESHIFT;
	planecos = FINECOSINE(angle);
	planesin = FINESINE(angle);

	// [RH] Notice that I dumped the caching scheme used by Doom.
	// It did not offer any appreciable speedup.
	distance = FixedMul(ds->planeheight, yslope[y]);
	span = abs(centery - y);

	if (span) // don't divide by zero
	{
		ds->xstep = FixedMul(planesin, ds->planeheight) / span;
		ds->ystep = FixedMul(planecos, ds->planeheight) / span;
	}
	else
	{
		ds->xstep = FixedMul(distance, basexscale);
		ds->ystep = FixedMul(distance, baseyscale);
	}

	ds->xfrac = ds->xoffs + FixedMul(planecos, distance) + (x1 - centerx) * ds->xstep;
	ds->yfrac = ds->yoffs - FixedMul(planesin, distance) + (x1 - centerx) * ds->ystep;

	if (ds->planeripple.active)
	{
		// Needed for ds_bgofs
		R_CalculatePlaneRipple(ds, ds->currentplane, y, ds->planeheight, (!ds->currentplane->slope));

		ds->xfrac += ds->planeripple.xfrac;
		ds->yfrac += ds->planeripple.yfrac;

		if ((y + ds->bgofs) >= viewheight)
			ds->bgofs = viewheight-y-1;
		if ((y + ds->bgofs) < 0)
			ds->bgofs = -y;
	}

	pindex = distance >> LIGHTZSHIFT;
	if (pindex >= MAXLIGHTZ)
		pindex = MAXLIGHTZ - 1;
	ds->colormap = ds->planezlight[pindex];

	if (encoremap && !ds->currentplane->noencore)
		ds->colormap += COLORMAP_REMAPOFFSET;

	if (ds->currentplane->extra_colormap)
		ds->colormap = ds->currentplane->extra_colormap->colormap + (ds->colormap - colormaps);

	ds->y = y;
	ds->x1 = x1;
	ds->x2 = x2;

	spanfunc2(ds);
}

static void R_MapTiltedPlane(drawspandata_t *ds, void(*spanfunc2)(drawspandata_t*), INT32 y, INT32 x1, INT32 x2, boolean allow_parallel)
{
	if (!R_CheckMapPlane(__func__, y, x1, x2))
		return;

	// Water ripple effect
	if (ds->planeripple.active)
	{
		R_SetTiltedSpan(ds, std::clamp(y, 0, viewheight));

		R_CalculatePlaneRipple(ds, ds->currentplane, y, ds->planeheight, false);
		R_SetSlopePlaneVectors(ds, ds->currentplane, y, (ds->xoffs + ds->planeripple.xfrac), (ds->yoffs + ds->planeripple.yfrac), 0);

		if ((y + ds->bgofs) >= viewheight)
			ds->bgofs = viewheight-y-1;
		if ((y + ds->bgofs) < 0)
			ds->bgofs = -y;
	}

	if (ds->currentplane->extra_colormap)
		ds->colormap = ds->currentplane->extra_colormap->colormap;
	else
		ds->colormap = colormaps;

	ds->fullbright = colormaps;
	if (encoremap && !ds->currentplane->noencore)
	{
		ds->colormap += COLORMAP_REMAPOFFSET;
		ds->fullbright += COLORMAP_REMAPOFFSET;
	}

	ds->y = y;
	ds->x1 = x1;
	ds->x2 = x2;

	spanfunc2(ds);
}

void R_ClearFFloorClips(void)
{
	INT32 i, p;

	// opening / clipping determination
	for (i = 0; i < viewwidth; i++)
	{
		for (p = 0; p < MAXFFLOORS; p++)
		{
			ffloor[p].f_clip[i] = (INT16)viewheight;
			ffloor[p].c_clip[i] = -1;
		}
	}

	numffloors = 0;
}

//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes(void)
{
	INT32 i, p;
	angle_t angle;

	// opening / clipping determination
	for (i = 0; i < viewwidth; i++)
	{
		floorclip[i] = (INT16)viewheight;
		ceilingclip[i] = -1;
		frontscale[i] = INT32_MAX;
		for (p = 0; p < MAXFFLOORS; p++)
		{
			ffloor[p].f_clip[i] = (INT16)viewheight;
			ffloor[p].c_clip[i] = -1;
		}
	}

	for (i = 0; i < MAXVISPLANES; i++)
		for (*freehead = visplanes[i], visplanes[i] = NULL;
			freehead && *freehead ;)
		{
			freehead = &(*freehead)->next;
		}

	lastopening = openings;

	// left to right mapping
	angle = (viewangle-ANGLE_90)>>ANGLETOFINESHIFT;

	// scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (FINECOSINE(angle), centerxfrac);
	baseyscale = -FixedDiv (FINESINE(angle), centerxfrac);
}

static visplane_t *new_visplane(unsigned hash)
{
	visplane_t *check = freetail;
	if (!check)
	{
		check = static_cast<visplane_t*>(calloc(1, sizeof (*check)));
		if (check == NULL)
			I_Error("new_visplane: Out of memory");
		check->top_memory = static_cast<UINT16*>(Z_Malloc(sizeof(UINT16) * (viewwidth + 2), PU_STATIC, NULL));
		check->bottom_memory = static_cast<UINT16*>(Z_Malloc(sizeof(UINT16) * (viewwidth + 2), PU_STATIC, NULL));
		check->top = check->top_memory + 1;
		check->bottom = check->bottom_memory + 1;
	}
	else
	{
		freetail = freetail->next;
		if (!freetail)
			freehead = &freetail;
	}
	check->next = visplanes[hash];
	visplanes[hash] = check;
	return check;
}

//
// R_FindPlane: Seek a visplane having the identical values:
//              Same height, same flattexture, same lightlevel.
//              If not, allocates another of them.
//
visplane_t *R_FindPlane(fixed_t height, INT32 picnum, INT32 lightlevel,
	fixed_t xoff, fixed_t yoff, angle_t plangle, extracolormap_t *planecolormap,
	ffloor_t *pfloor
			, polyobj_t *polyobj
			, pslope_t *slope
			, boolean noencore
			, boolean reverseLight, const sector_t *lighting_sector)
{
	visplane_t *check;
	unsigned hash;

	if (slope); else // Don't mess with this right now if a slope is involved
	{
		xoff += viewx;
		yoff -= viewy;
		if (plangle != 0)
		{
			// Add the view offset, rotated by the plane angle.
			fixed_t cosinecomponent = FINECOSINE(plangle>>ANGLETOFINESHIFT);
			fixed_t sinecomponent = FINESINE(plangle>>ANGLETOFINESHIFT);
			fixed_t oldxoff = xoff;
			xoff = FixedMul(xoff,cosinecomponent)+FixedMul(yoff,sinecomponent);
			yoff = -FixedMul(oldxoff,sinecomponent)+FixedMul(yoff,cosinecomponent);
		}
	}

	if (polyobj)
	{
		if (polyobj->angle != 0)
		{
			float ang = ANG2RAD(polyobj->angle);
			float x = FixedToFloat(polyobj->centerPt.x);
			float y = FixedToFloat(polyobj->centerPt.y);
			xoff -= FloatToFixed(x * cos(ang) + y * sin(ang));
			yoff -= FloatToFixed(x * sin(ang) - y * cos(ang));
		}
		else
		{
			xoff -= polyobj->centerPt.x;
			yoff += polyobj->centerPt.y;
		}
	}

	if (slope != NULL && P_ApplyLightOffset(lightlevel >> LIGHTSEGSHIFT, lighting_sector))
	{
		if (reverseLight)
		{
			lightlevel -= slope->lightOffset * 8;
		}
		else
		{
			lightlevel += slope->lightOffset * 8;
		}
	}

	// This appears to fix the Nimbus Ruins sky bug.
	if (picnum == skyflatnum && pfloor)
	{
		height = 0; // all skies map together
		lightlevel = 0;
	}

	if (!pfloor)
	{
		hash = visplane_hash(picnum, lightlevel, height);
		for (check = visplanes[hash]; check; check = check->next)
		{
			if (polyobj != check->polyobj)
				continue;
			if (height == check->height && picnum == check->picnum
				&& lightlevel == check->lightlevel
				&& xoff == check->xoffs && yoff == check->yoffs
				&& planecolormap == check->extra_colormap
				&& check->viewx == viewx && check->viewy == viewy && check->viewz == viewz
				&& check->viewangle == viewangle
				&& check->plangle == plangle
				&& check->slope == slope
				&& check->noencore == noencore)
			{
				return check;
			}
		}
	}
	else
	{
		hash = MAXVISPLANES - 1;
	}

	check = new_visplane(hash);

	check->height = height;
	check->picnum = picnum;
	check->lightlevel = lightlevel;
	check->minx = vid.width;
	check->maxx = -1;
	check->xoffs = xoff;
	check->yoffs = yoff;
	check->extra_colormap = planecolormap;
	check->ffloor = pfloor;
	check->viewx = viewx;
	check->viewy = viewy;
	check->viewz = viewz;
	check->viewangle = viewangle;
	check->plangle = plangle;
	check->polyobj = polyobj;
	check->slope = slope;
	check->noencore = noencore;

	memset(check->top, 0xff, sizeof(*check->top) * viewwidth);
	memset(check->bottom, 0x00, sizeof(*check->bottom) * viewwidth);

	return check;
}

//
// R_CheckPlane: return same visplane or alloc a new one if needed
//
visplane_t *R_CheckPlane(visplane_t *pl, INT32 start, INT32 stop)
{
	INT32 intrl, intrh;
	INT32 unionl, unionh;
	INT32 x;

	if (start < pl->minx)
	{
		intrl = pl->minx;
		unionl = start;
	}
	else
	{
		unionl = pl->minx;
		intrl = start;
	}

	if (stop > pl->maxx)
	{
		intrh = pl->maxx;
		unionh = stop;
	}
	else
	{
		unionh = pl->maxx;
		intrh = stop;
	}

	// 0xff is not equal to -1 with shorts...
	for (x = intrl; x <= intrh; x++)
		if (pl->top[x] != 0xffff || pl->bottom[x] != 0x0000)
			break;

	if (x > intrh) /* Can use existing plane; extend range */
	{
		pl->minx = unionl;
		pl->maxx = unionh;
	}
	else /* Cannot use existing plane; create a new one */
	{
		visplane_t *new_pl;
		if (pl->ffloor)
		{
			new_pl = new_visplane(MAXVISPLANES - 1);
		}
		else
		{
			unsigned hash = visplane_hash(pl->picnum, pl->lightlevel, pl->height);
			new_pl = new_visplane(hash);
		}

		new_pl->height = pl->height;
		new_pl->picnum = pl->picnum;
		new_pl->lightlevel = pl->lightlevel;
		new_pl->xoffs = pl->xoffs;
		new_pl->yoffs = pl->yoffs;
		new_pl->extra_colormap = pl->extra_colormap;
		new_pl->ffloor = pl->ffloor;
		new_pl->viewx = pl->viewx;
		new_pl->viewy = pl->viewy;
		new_pl->viewz = pl->viewz;
		new_pl->viewangle = pl->viewangle;
		new_pl->plangle = pl->plangle;
		new_pl->polyobj = pl->polyobj;
		new_pl->slope = pl->slope;
		new_pl->noencore = pl->noencore;
		pl = new_pl;
		pl->minx = start;
		pl->maxx = stop;
		memset(pl->top, 0xff, sizeof(*pl->top) * viewwidth);
		memset(pl->bottom, 0x00, sizeof(*pl->bottom) * viewwidth);
	}
	return pl;
}


//
// R_ExpandPlane
//
// This function basically expands the visplane or I_Errors.
// The reason for this is that when creating 3D floor planes, there is no
// need to create new ones with R_CheckPlane, because 3D floor planes
// are created by subsector and there is no way a subsector can graphically
// overlap.
void R_ExpandPlane(visplane_t *pl, INT32 start, INT32 stop)
{
	INT32 unionl, unionh;
//	INT32 x;

	// Don't expand polyobject planes here - we do that on our own.
	if (pl->polyobj)
		return;

	if (start < pl->minx)
	{
		unionl = start;
	}
	else
	{
		unionl = pl->minx;
	}

	if (stop > pl->maxx)
	{
		unionh = stop;
	}
	else
	{
		unionh = pl->maxx;
	}
/*
	for (x = start; x <= stop; x++)
		if (pl->top[x] != 0xffff || pl->bottom[x] != 0x0000)
			break;

	if (x <= stop)
		I_Error("R_ExpandPlane: planes in same subsector overlap?!\nminx: %d, maxx: %d, start: %d, stop: %d\n", pl->minx, pl->maxx, start, stop);
*/
	pl->minx = unionl, pl->maxx = unionh;
}

//
// R_MakeSpans
//
static void R_MakeSpans(void (*mapfunc)(drawspandata_t* ds, void(*spanfunc)(drawspandata_t*), INT32, INT32, INT32, boolean), void(*spanfunc2)(drawspandata_t*), drawspandata_t* ds, INT32 x, INT32 t1, INT32 b1, INT32 t2, INT32 b2, boolean allow_parallel)
{
	//    Alam: from r_splats's R_RenderFloorSplat
	if (t1 >= vid.height) t1 = vid.height-1;
	if (b1 >= vid.height) b1 = vid.height-1;
	if (t2 >= vid.height) t2 = vid.height-1;
	if (b2 >= vid.height) b2 = vid.height-1;
	if (x-1 >= vid.width) x = vid.width;

	// We want to draw N spans per subtask to ensure the work is
	// coarse enough to not be too slow due to task scheduling overhead.
	// To safely do this, we need to copy part of spanstart to a local.
	// This is essentially loop unrolling across threads.
	constexpr const int kSpanTaskGranularity = 8;
	drawspandata_t dc_copy = *ds;
	while (t1 < t2 && t1 <= b1)
	{
		INT32 spanstartcopy[kSpanTaskGranularity] = {0};
		INT32 taskspans = 0;
		for (int i = 0; i < kSpanTaskGranularity; i++)
		{
			if (!((t1 + i) < t2 && (t1 + i) <= b1))
			{
				break;
			}
			spanstartcopy[i] = spanstart[t1 + i];
			taskspans += 1;
		}
		auto task = [=]() mutable -> void {
			for (int i = 0; i < taskspans; i++)
			{
				mapfunc(&dc_copy, spanfunc2, t1 + i, spanstartcopy[i], x - 1, false);
			}
		};
		if (allow_parallel)
		{
			srb2::g_main_threadpool->schedule(std::move(task));
		}
		else
		{
			(task)();
		}
		t1 += taskspans;
	}
	while (b1 > b2 && b1 >= t1)
	{
		INT32 spanstartcopy[kSpanTaskGranularity] = {0};
		INT32 taskspans = 0;
		for (int i = 0; i < kSpanTaskGranularity; i++)
		{
			if (!((b1 - i) > b2 && (b1 - i) >= t1))
			{
				break;
			}
			spanstartcopy[i] = spanstart[b1 - i];
			taskspans += 1;
		}
		auto task = [=]() mutable -> void {
			for (int i = 0; i < taskspans; i++)
			{
				mapfunc(&dc_copy, spanfunc2, b1 - i, spanstartcopy[i], x - 1, false);
			}
		};
		if (allow_parallel)
		{
			srb2::g_main_threadpool->schedule(std::move(task));
		}
		else
		{
			(task)();
		}
		b1 -= taskspans;
	}

	while (t2 < t1 && t2 <= b2)
		spanstart[t2++] = x;
	while (b2 > b1 && b2 >= t2)
		spanstart[b2--] = x;
}

void R_DrawPlanes(void)
{
	visplane_t *pl;
	INT32 i;
	drawspandata_t ds = {0};

	spanfunc = basespanfunc;
	wallcolfunc = walldrawerfunc;

	for (i = 0; i < MAXVISPLANES; i++, pl++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			if (pl->ffloor != NULL || pl->polyobj != NULL)
				continue;

			R_DrawSinglePlane(&ds, pl, true);
		}
	}

	R_UpdatePlaneRipple(&ds);
}

static void R_DrawSkyPlane(visplane_t *pl, void(*colfunc2)(drawcolumndata_t*), boolean allow_parallel)
{
	INT32 x;
	drawcolumndata_t dc = {0};

	if (!newview->sky)
	{
		skyVisible = true;
		return;
	}

	wallcolfunc = walldrawerfunc;

	// use correct aspect ratio scale
	dc.iscale = skyscale;
	// Sky is always drawn full bright,
	//  i.e. colormaps[0] is used.
	// Because of this hack, sky is not affected
	//  by INVUL inverse mapping.
	dc.colormap = colormaps;

	if (encoremap)
		dc.colormap += COLORMAP_REMAPOFFSET;

	dc.texturemid = skytexturemid;
	dc.texheight = textureheight[skytexture] >>FRACBITS;
	dc.sourcelength = dc.texheight;

	x = pl->minx;

	// Precache the texture so we don't corrupt the zoned heap off-main thread
	if (!texturecache[texturetranslation[skytexture]])
	{
		R_GenerateTexture(texturetranslation[skytexture]);
	}

	while (x <= pl->maxx)
	{
		// Tune concurrency granularity here to maximize throughput
		// The cheaper colfunc is, the more coarse the task should be
		constexpr const int kSkyPlaneMacroColumns = 8;

		auto thunk = [=]() mutable -> void {
			for (int i = 0; i < kSkyPlaneMacroColumns && i + x <= pl->maxx; i++)
			{
				dc.yl = pl->top[x + i];
				dc.yh = pl->bottom[x + i];

				if (dc.yl > dc.yh)
				{
					continue;
				}

				INT32 angle = (pl->viewangle + xtoviewangle[x + i])>>ANGLETOSKYSHIFT;
				dc.iscale = FixedMul(skyscale, FINECOSINE(xtoviewangle[x + i]>>ANGLETOFINESHIFT));
				dc.x = x + i;
				dc.source =
					R_GetColumn(texturetranslation[skytexture],
						-angle); // get negative of angle for each column to display sky correct way round! --Monster Iestyn 27/01/18

				colfunc2(&dc);
			}
		};

		if (allow_parallel)
		{
			srb2::g_main_threadpool->schedule(std::move(thunk));
		}
		else
		{
			(thunk)();
		}

		x += kSkyPlaneMacroColumns;
	}

}

// Potentially override other stuff for now cus we're mean. :< But draw a slope plane!
// I copied ZDoom's code and adapted it to SRB2... -Red
static void R_CalculateSlopeVectors(drawspandata_t* ds, pslope_t *slope, fixed_t planeviewx, fixed_t planeviewy, fixed_t planeviewz, fixed_t planexscale, fixed_t planeyscale, fixed_t planexoffset, fixed_t planeyoffset, angle_t planeviewangle, angle_t planeangle, float fudge)
{
	floatv3_t p, m, n;
	float ang;
	float vx, vy, vz;
	float xscale = FIXED_TO_FLOAT(planexscale);
	float yscale = FIXED_TO_FLOAT(planeyscale);
	// compiler complains when P_GetSlopeZAt is used in FLOAT_TO_FIXED directly
	// use this as a temp var to store P_GetSlopeZAt's return value each time
	fixed_t temp;

	vx = FIXED_TO_FLOAT(planeviewx+planexoffset);
	vy = FIXED_TO_FLOAT(planeviewy-planeyoffset);
	vz = FIXED_TO_FLOAT(planeviewz);

	temp = P_GetSlopeZAt(slope, planeviewx, planeviewy);
	ds->zeroheight = FIXED_TO_FLOAT(temp);

	// p is the texture origin in view space
	// Don't add in the offsets at this stage, because doing so can result in
	// errors if the flat is rotated.
	ang = ANG2RAD(ANGLE_270 - planeviewangle);
	p.x = vx * cos(ang) - vy * sin(ang);
	p.z = vx * sin(ang) + vy * cos(ang);
	temp = P_GetSlopeZAt(slope, -planexoffset, planeyoffset);
	p.y = FIXED_TO_FLOAT(temp) - vz;

	// m is the v direction vector in view space
	ang = ANG2RAD(ANGLE_180 - (planeviewangle + planeangle));
	m.x = yscale * cos(ang);
	m.z = yscale * sin(ang);

	// n is the u direction vector in view space
	n.x = xscale * sin(ang);
	n.z = -xscale * cos(ang);

	ang = ANG2RAD(planeangle);
	temp = P_GetSlopeZAt(slope, planeviewx + FLOAT_TO_FIXED(yscale * sin(ang)), planeviewy + FLOAT_TO_FIXED(yscale * cos(ang)));
	m.y = FIXED_TO_FLOAT(temp) - ds->zeroheight;
	temp = P_GetSlopeZAt(slope, planeviewx + FLOAT_TO_FIXED(xscale * cos(ang)), planeviewy - FLOAT_TO_FIXED(xscale * sin(ang)));
	n.y = FIXED_TO_FLOAT(temp) - ds->zeroheight;

	m.x /= fudge;
	m.y /= fudge;
	m.z /= fudge;

	n.x *= fudge;
	n.y *= fudge;
	n.z *= fudge;

	// Eh. I tried making this stuff fixed-point and it exploded on me. Here's a macro for the only floating-point vector function I recall using.
#define CROSS(d, v1, v2) \
d.x = (v1.y * v2.z) - (v1.z * v2.y);\
d.y = (v1.z * v2.x) - (v1.x * v2.z);\
d.z = (v1.x * v2.y) - (v1.y * v2.x)
		CROSS(ds->sup, p, m);
		CROSS(ds->svp, p, n);
		CROSS(ds->szp, m, n);
#undef CROSS

	ds->sup.z *= focallengthf;
	ds->svp.z *= focallengthf;
	ds->szp.z *= focallengthf;

	// Premultiply the texture vectors with the scale factors
#define SFMULT 65536.f
	ds->sup.x *= (SFMULT * (1<<ds->nflatshiftup));
	ds->sup.y *= (SFMULT * (1<<ds->nflatshiftup));
	ds->sup.z *= (SFMULT * (1<<ds->nflatshiftup));
	ds->svp.x *= (SFMULT * (1<<ds->nflatshiftup));
	ds->svp.y *= (SFMULT * (1<<ds->nflatshiftup));
	ds->svp.z *= (SFMULT * (1<<ds->nflatshiftup));
#undef SFMULT
}

static void R_SetTiltedSpan(drawspandata_t* ds, INT32 span)
{
	if (ds_su == NULL)
		ds_su = static_cast<floatv3_t*>(Z_Calloc(sizeof(*ds_su) * vid.height, PU_STATIC, NULL));
	if (ds_sv == NULL)
		ds_sv = static_cast<floatv3_t*>(Z_Calloc(sizeof(*ds_sv) * vid.height, PU_STATIC, NULL));
	if (ds_sz == NULL)
		ds_sz = static_cast<floatv3_t*>(Z_Calloc(sizeof(*ds_sz) * vid.height, PU_STATIC, NULL));

	ds->sup = ds_su[span];
	ds->svp = ds_sv[span];
	ds->szp = ds_sz[span];
}

static void R_SetSlopePlaneVectors(drawspandata_t* ds, visplane_t *pl, INT32 y, fixed_t xoff, fixed_t yoff, float fudge)
{
	R_SetTiltedSpan(ds, y);
	R_CalculateSlopeVectors(ds, pl->slope, pl->viewx, pl->viewy, pl->viewz, FRACUNIT, FRACUNIT, xoff, yoff, pl->viewangle, pl->plangle, fudge);
}

void R_DrawSinglePlane(drawspandata_t* ds, visplane_t *pl, boolean allow_parallel)
{
	INT32 light = 0;
	INT32 x;
	INT32 stop, angle;
	size_t size;
	ffloor_t *rover;
	void (*mapfunc)(drawspandata_t*, void(*)(drawspandata_t*), INT32, INT32, INT32, boolean) = R_MapPlane;

	if (!(pl->minx <= pl->maxx))
		return;

	// sky flat
	if (pl->picnum == skyflatnum)
	{
		R_DrawSkyPlane(pl, colfunc, allow_parallel);
		return;
	}

	ds->planeripple.active = false;
	spanfunc = basespanfunc;

	if (pl->polyobj && pl->polyobj->translucency != 0)
	{
		spanfunc = R_DrawTranslucentSpan;

		// Hacked up support for alpha value in software mode Tails 09-24-2002 (sidenote: ported to polys 10-15-2014, there was no time travel involved -Red)
		if (pl->polyobj->translucency >= 10)
			return; // Don't even draw it
		else if (pl->polyobj->translucency > 0)
			ds->transmap = R_GetTranslucencyTable(pl->polyobj->translucency);
		else // Opaque, but allow transparent flat pixels
			spanfunc = splatfunc;

#ifdef SHITPLANESPARENCY
		if (spanfunc == splatfunc || (pl->extra_colormap && pl->extra_colormap->fog))
#else
		if (!pl->extra_colormap || !(pl->extra_colormap->fog & 2))
#endif
			light = (pl->lightlevel >> LIGHTSEGSHIFT);
		else
			light = LIGHTLEVELS-1;

	}
	else
	{
		if (pl->ffloor)
		{
			// Don't draw planes that shouldn't be drawn.
			for (rover = pl->ffloor->target->ffloors; rover; rover = rover->next)
			{
				if (!((pl->ffloor->flags & FF_CUTEXTRA) && (rover->flags & FF_EXTRA)))
					continue;

				// The plane is from an extra 3D floor... Check the flags so
				// there are no undesired cuts.
				if (((pl->ffloor->flags & (FF_FOG|FF_SWIMMABLE)) == (rover->flags & (FF_FOG|FF_SWIMMABLE)))
					&& pl->height < *rover->topheight
					&& pl->height > *rover->bottomheight)
					return;
			}

			if (pl->ffloor->flags & FF_TRANSLUCENT)
			{
				spanfunc = R_DrawTranslucentSpan;

				// Hacked up support for alpha value in software mode Tails 09-24-2002
				// ...unhacked by toaster 04-01-2021, re-hacked a little by sphere 19-11-2021
				// and mercilessly shoved into saturn by chearii 02-02-2025
				{
					INT32 trans = (10*((256+12) - pl->ffloor->alpha))/255;
					if (trans >= 10)
						return; // Don't even draw it
					if (pl->ffloor->blend) // additive, (reverse) subtractive, modulative
						ds->transmap = R_GetBlendTable(pl->ffloor->blend, trans);
					else if (!(ds->transmap = R_GetTranslucencyTable(trans)) || trans == 0)
						spanfunc = splatfunc; // Opaque, but allow transparent flat pixels
				}

#ifdef SHITPLANESPARENCY
				if (spanfunc == splatfunc || (pl->extra_colormap && pl->extra_colormap->fog))
#else
				if (!pl->extra_colormap || !(pl->extra_colormap->fog & 2))
#endif
					light = (pl->lightlevel >> LIGHTSEGSHIFT);
				else
					light = LIGHTLEVELS-1;
			}
			else if (pl->ffloor->flags & FF_FOG)
			{
				spanfunc = R_DrawFogSpan;
				light = (pl->lightlevel >> LIGHTSEGSHIFT);
			}
			else light = (pl->lightlevel >> LIGHTSEGSHIFT);

			if (pl->ffloor->flags & FF_RIPPLE)
			{
				R_HandleRipplePlane(ds, pl);
			}
		}
		else light = (pl->lightlevel >> LIGHTSEGSHIFT);
	}

	// Don't mess with angle on slopes! We'll handle this ourselves later
	if (!pl->slope && viewangle != pl->viewangle+pl->plangle)
	{
		angle = (pl->viewangle+pl->plangle-ANGLE_90)>>ANGLETOFINESHIFT;
		basexscale = FixedDiv(FINECOSINE(angle),centerxfrac);
		baseyscale = -FixedDiv(FINESINE(angle),centerxfrac);
		viewangle = pl->viewangle+pl->plangle;
	}

	ds->currentplane = pl;

	ds->source = (UINT8 *)
		W_CacheLumpNum(levelflats[pl->picnum].lumpnum,
			PU_STATIC); // Stay here until Z_ChangeTag

	size = W_LumpLength(levelflats[pl->picnum].lumpnum);

	switch (size)
	{
		case 4194304: // 2048x2048 lump
			ds->nflatmask = 0x3FF800;
			ds->nflatxshift = 21;
			ds->nflatyshift = 10;
			ds->nflatshiftup = 5;
			break;
		case 1048576: // 1024x1024 lump
			ds->nflatmask = 0xFFC00;
			ds->nflatxshift = 22;
			ds->nflatyshift = 12;
			ds->nflatshiftup = 6;
			break;
		case 262144:// 512x512 lump'
			ds->nflatmask = 0x3FE00;
			ds->nflatxshift = 23;
			ds->nflatyshift = 14;
			ds->nflatshiftup = 7;
			break;
		case 65536: // 256x256 lump
			ds->nflatmask = 0xFF00;
			ds->nflatxshift = 24;
			ds->nflatyshift = 16;
			ds->nflatshiftup = 8;
			break;
		case 16384: // 128x128 lump
			ds->nflatmask = 0x3F80;
			ds->nflatxshift = 25;
			ds->nflatyshift = 18;
			ds->nflatshiftup = 9;
			break;
		case 1024: // 32x32 lump
			ds->nflatmask = 0x3E0;
			ds->nflatxshift = 27;
			ds->nflatyshift = 22;
			ds->nflatshiftup = 11;
			break;
		default: // 64x64 lump
			ds->nflatmask = 0xFC0;
			ds->nflatxshift = 26;
			ds->nflatyshift = 20;
			ds->nflatshiftup = 10;
			break;
	}

	ds->xoffs = pl->xoffs;
	ds->yoffs = pl->yoffs;
	ds->planeheight = abs(pl->height - pl->viewz);

	if (light >= LIGHTLEVELS)
		light = LIGHTLEVELS-1;

	if (light < 0)
		light = 0;

	if (pl->slope)
	{
		float fudgecanyon = 0;
		fixed_t temp;
		// Okay, look, don't ask me why this works, but without this setup there's a disgusting-looking misalignment with the textures. -fickle
		fudgecanyon = ((1<<ds->nflatshiftup)+1.0f)/(1<<ds->nflatshiftup);

		angle_t hack = (pl->plangle & (ANGLE_90-1));

		mapfunc = R_MapTiltedPlane;

		if (hack)
		{
			/*
			Essentially: We can't & the components along the regular axes when the plane is rotated.
			This is because the distance on each regular axis in order to loop is different.
			We rotate them, & the components, add them together, & them again, and then rotate them back.
			These three seperate & operations are done per axis in order to prevent overflows.
			toast 10/04/17
			---
			...of coooourse, this still isn't perfect. but it looks... merely kind of grody, rather than
			completely wrong? idk. i'm just backporting this to kart right now. if anyone else wants to
			ever try dig around: it's drifting towards 0,0, and no, multiplying by fudge doesn't fix it.
			toast 27/09/18
			*/

			const fixed_t cosinecomponent = FINECOSINE(hack>>ANGLETOFINESHIFT);
			const fixed_t sinecomponent = FINESINE(hack>>ANGLETOFINESHIFT);

			const fixed_t modmask = ((1 << (32-ds->nflatshiftup)) - 1);

			fixed_t ox = (FixedMul(pl->slope->o.x,cosinecomponent) & modmask) - (FixedMul(pl->slope->o.y,sinecomponent) & modmask);
			fixed_t oy = (-FixedMul(pl->slope->o.x,sinecomponent) & modmask) - (FixedMul(pl->slope->o.y,cosinecomponent) & modmask);

			temp = ox & modmask;
			oy &= modmask;
			ox = FixedMul(temp,cosinecomponent)+FixedMul(oy,-sinecomponent); // negative sine for opposite direction
			oy = -FixedMul(temp,-sinecomponent)+FixedMul(oy,cosinecomponent);

			temp = ds->xoffs;
			ds->xoffs = (FixedMul(temp,cosinecomponent) & modmask) + (FixedMul(ds->yoffs, sinecomponent) & modmask);
			ds->yoffs = (-FixedMul(temp,sinecomponent) & modmask) + (FixedMul(ds->yoffs, cosinecomponent) & modmask);

			temp = ds->xoffs & modmask;
			ds->yoffs &= modmask;
			ds->xoffs = FixedMul(temp,cosinecomponent)+FixedMul(ds->yoffs, -sinecomponent); // ditto
			ds->yoffs = -FixedMul(temp,-sinecomponent)+FixedMul(ds->yoffs, cosinecomponent);

			ds->xoffs -= (pl->slope->o.x - ox);
			ds->yoffs += (pl->slope->o.y + oy);
		}
		else
		{
			ds->xoffs &= ((1 << (32-ds->nflatshiftup))-1);
			ds->yoffs &= ((1 << (32-ds->nflatshiftup))-1);
			ds->xoffs -= (pl->slope->o.x + (1 << (31-ds->nflatshiftup))) & ~((1 << (32-ds->nflatshiftup))-1);
			ds->yoffs += (pl->slope->o.y + (1 << (31-ds->nflatshiftup))) & ~((1 << (32-ds->nflatshiftup))-1);
		}

		ds->xoffs = (fixed_t)(ds->xoffs*fudgecanyon);
		ds->yoffs = (fixed_t)(ds->yoffs/fudgecanyon);

		if (ds->planeripple.active)
		{
			fixed_t plheight = abs(P_GetSlopeZAt(pl->slope, pl->viewx, pl->viewy) - pl->viewz);

			R_PlaneBounds(pl);

			for (x = pl->high; x < pl->low; x++)
			{
				R_CalculatePlaneRipple(ds, pl, x, plheight, true);
				R_SetSlopePlaneVectors(ds, pl, x, (ds->xoffs + ds->planeripple.xfrac), (ds->yoffs + ds->planeripple.yfrac), fudgecanyon);
			}
		}
		else
			R_SetSlopePlaneVectors(ds, pl, 0, ds->xoffs, ds->yoffs, fudgecanyon);

		if (spanfunc == R_DrawTranslucentWaterSpan)
			spanfunc = R_DrawTiltedTranslucentWaterSpan;
		else if (spanfunc == R_DrawTranslucentSpan)
			spanfunc = R_DrawTiltedTranslucentSpan;
		else if (spanfunc == splatfunc)
			spanfunc = R_DrawTiltedSplat;
		else
			spanfunc = R_DrawTiltedSpan;

		ds->planezlight = scalelight[light];
	}
	else
		ds->planezlight = zlight[light];

	// set the maximum value for unsigned
	pl->top[pl->maxx+1] = 0xffff;
	pl->top[pl->minx-1] = 0xffff;
	pl->bottom[pl->maxx+1] = 0x0000;
	pl->bottom[pl->minx-1] = 0x0000;

	stop = pl->maxx + 1;

	if (viewx != pl->viewx || viewy != pl->viewy)
	{
		viewx = pl->viewx;
		viewy = pl->viewy;
	}
	if (viewz != pl->viewz)
		viewz = pl->viewz;

	for (x = pl->minx; x <= stop; x++)
	{
		R_MakeSpans(mapfunc, spanfunc, ds, x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x], allow_parallel);
	}

/*
QUINCUNX anti-aliasing technique (sort of)

Normally, Quincunx antialiasing staggers pixels
in a 5-die pattern like so:

o   o
  o
o   o

To simulate this, we offset the plane by
FRACUNIT/4 in each direction, and draw
at 50% translucency. The result is
a 'smoothing' of the texture while
using the palette colors.
*/
#ifdef QUINCUNX
	if (spanfunc == R_DrawSpan)
	{
		INT32 i;
		ds_transmap = R_GetTranslucencyTable(tr_trans50);
		spanfunc = R_DrawTranslucentSpan;
		for (i=0; i<4; i++)
		{
			ds->xoffs = pl->xoffs;
			ds->yoffs = pl->yoffs;

			switch(i)
			{
				case 0:
					ds->xoffs -= FRACUNIT/4;
					ds->yoffs -= FRACUNIT/4;
					break;
				case 1:
					ds->xoffs -= FRACUNIT/4;
					ds->yoffs += FRACUNIT/4;
					break;
				case 2:
					ds->xoffs += FRACUNIT/4;
					ds->yoffs -= FRACUNIT/4;
					break;
				case 3:
					ds->xoffs += FRACUNIT/4;
					ds->yoffs += FRACUNIT/4;
					break;
			}
			ds->planeheight = abs(pl->height - pl->viewz);

			if (light >= LIGHTLEVELS)
				light = LIGHTLEVELS-1;

			if (light < 0)
				light = 0;

			ds->planezlight = zlight[light];

			// set the maximum value for unsigned
			pl->top[pl->maxx+1] = 0xffff;
			pl->top[pl->minx-1] = 0xffff;
			pl->bottom[pl->maxx+1] = 0x0000;
			pl->bottom[pl->minx-1] = 0x0000;

			stop = pl->maxx + 1;

			for (x = pl->minx; x <= stop; x++)
				R_MakeSpans(x, pl->top[x-1], pl->bottom[x-1],
					pl->top[x], pl->bottom[x]);
		}
	}
#endif

	Z_ChangeTag(ds->source, PU_CACHE);
}

void R_PlaneBounds(visplane_t *plane)
{
	INT32 i;
	INT32 hi, low;

	hi = plane->top[plane->minx];
	low = plane->bottom[plane->minx];

	for (i = plane->minx + 1; i <= plane->maxx; i++)
	{
		if (plane->top[i] < hi)
			hi = plane->top[i];
		if (plane->bottom[i] > low)
			low = plane->bottom[i];
	}
	plane->high = hi;
	plane->low = low;
}
