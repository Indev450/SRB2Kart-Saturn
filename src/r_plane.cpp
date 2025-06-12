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

#include <algorithm>

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

#ifdef HAVE_THREADS
#include "core/thread_pool.h"
#endif

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

static void R_SetTiltedSpan(drawspandata_t* ds, INT32 span);
static void R_SetSlopePlaneVectors(drawspandata_t* ds, visplane_t *pl, INT32 y, fixed_t xoff, fixed_t yoff);

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
static fixed_t R_CalculateRippleOffset(drawspandata_t* ds, INT32 y)
{
	fixed_t distance = FixedMul(ds->planeheight, yslope[y]);
	const INT32 yay = (ds->planeripple.offset + (distance>>9)) & 8191;
	return FixedDiv(FINESINE(yay), (1<<12) + (distance>>11));
}

static void R_CalculatePlaneRipple(drawspandata_t* ds, angle_t angle)
{
	angle >>= ANGLETOFINESHIFT;
	angle = (angle + 2048) & 8191; // 90 degrees
	ds->planeripple.xfrac = FixedMul(FINECOSINE(angle), ds->bgofs);
	ds->planeripple.yfrac = FixedMul(FINESINE(angle), ds->bgofs);
}

static void R_UpdatePlaneRipple(drawspandata_t* ds)
{
	ds->waterofs = (leveltime & 1)*16384;
	ds->planeripple.offset = ((leveltime-1)*140) + ((rendertimefrac*140) / FRACUNIT);
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
static void R_MapPlane(drawspandata_t *ds, spandrawfunc_t *localspanfunc, INT32 y, INT32 x1, INT32 x2, boolean allow_parallel)
{
	angle_t angle, planecos, planesin;
	fixed_t distance = 0, span;
	size_t pindex;

	(void)allow_parallel;

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

	// [RH] Instead of using the xtoviewangle array, I calculated the fractional values
	// at the middle of the screen, then used the calculated ds_xstep and ds_ystep
	// to step from those to the proper texture coordinate to start drawing at.
	// That way, the texture coordinate is always calculated by its position
	// on the screen and not by its position relative to the edge of the visplane.
	ds->xfrac = ds->xoffs + FixedMul(planecos, distance) + (x1 - centerx) * ds->xstep;
	ds->yfrac = ds->yoffs - FixedMul(planesin, distance) + (x1 - centerx) * ds->ystep;

	if (ds->planeripple.active)
	{
		ds->bgofs = R_CalculateRippleOffset(ds, y);

		R_CalculatePlaneRipple(ds, ds->currentplane->viewangle + ds->currentplane->plangle);

		ds->xfrac += ds->planeripple.xfrac;
		ds->yfrac += ds->planeripple.yfrac;
		ds->bgofs >>= FRACBITS;

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

	localspanfunc(ds);
}

static void R_MapTiltedPlane(drawspandata_t *ds, spandrawfunc_t *localspanfunc, INT32 y, INT32 x1, INT32 x2, boolean allow_parallel)
{
	(void)allow_parallel;

	if (!R_CheckMapPlane(__func__, y, x1, x2))
		return;

	// Water ripple effect
	if (ds->planeripple.active)
	{
		ds->bgofs = R_CalculateRippleOffset(ds, y);

		R_SetTiltedSpan(ds, std::clamp(y, 0, viewheight));

		R_CalculatePlaneRipple(ds, ds->currentplane->viewangle + ds->currentplane->plangle);
		R_SetSlopePlaneVectors(ds, ds->currentplane, y, (ds->xoffs + ds->planeripple.xfrac), (ds->yoffs + ds->planeripple.yfrac));

		ds->bgofs >>= FRACBITS;

		if ((y + ds->bgofs) >= viewheight)
			ds->bgofs = viewheight-y-1;
		if ((y + ds->bgofs) < 0)
			ds->bgofs = -y;
	}

	if (ds->currentplane->extra_colormap)
		ds->colormap = ds->currentplane->extra_colormap->colormap;
	else
		ds->colormap = colormaps;

	if (encoremap && !ds->currentplane->noencore)
	{
		ds->colormap += COLORMAP_REMAPOFFSET;
	}

	ds->y = y;
	ds->x1 = x1;
	ds->x2 = x2;

	localspanfunc(ds);
}

void R_ClearFFloorClips(void)
{
	INT32 p;

	// opening / clipping determination
	for (p = 0; p < MAXFFLOORS; p++)
	{
		visffloor_t *fffloor = &ffloor[p];
		std::fill(fffloor->f_clip, fffloor->f_clip + viewwidth, static_cast<INT16>(viewheight));
		std::fill(fffloor->c_clip, fffloor->c_clip + viewwidth, static_cast<INT16>(-1));
	}

	numffloors = 0;
}

//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes(void)
{
	INT32 i;
	angle_t angle;

	// opening / clipping determination
	std::fill(floorclip, floorclip + viewwidth, static_cast<INT16>(viewheight));
	std::fill(ceilingclip, ceilingclip + viewwidth, static_cast<INT16>(-1));
	std::fill(frontscale, frontscale + viewwidth, INT32_MAX);

	R_ClearFFloorClips();

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
	fixed_t xoff, fixed_t yoff, angle_t plangle, extracolormap_t *planecolormap
			, ffloor_t *pfloor
			, polyobj_t *polyobj
			, pslope_t *slope
			, boolean noencore
			, boolean reverseLight, const sector_t *lighting_sector)
{
	visplane_t *check;
	unsigned hash;

	if (!slope) // Don't mess with this right now if a slope is involved
	{
		if (plangle != 0)
		{
			// Must use 64-bit math to avoid an overflow!
			INT64 vx = xoff + viewx;
			INT64 vy = yoff - viewy;

			// Add the view offset, rotated by the plane angle.
			float ang = ANG2RAD(plangle);
			float x = vx / (float)FRACUNIT;
			float y = vy / (float)FRACUNIT;

			vx = (x * cos(ang) + y * sin(ang)) * FRACUNIT;
			vy = (-x * sin(ang) + y * cos(ang)) * FRACUNIT;

			xoff = vx;
			yoff = vy;
		}
		else
		{
			xoff += viewx;
			yoff -= viewy;
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
// This function basically expands the visplane.
// The reason for this is that when creating 3D floor planes, there is no
// need to create new ones with R_CheckPlane, because 3D floor planes
// are created by subsector and there is no way a subsector can graphically
// overlap.
void R_ExpandPlane(visplane_t *pl, INT32 start, INT32 stop)
{
	// Don't expand polyobject planes here - we do that on our own.
	if (pl->polyobj)
		return;

	if (pl->minx > start) pl->minx = start;
	if (pl->maxx < stop)  pl->maxx = stop;
}

//
// R_MakeSpans
//
static void R_MakeSpans(void (*mapfunc)(drawspandata_t* ds, void(*spanfunc)(drawspandata_t*), INT32, INT32, INT32, boolean), spandrawfunc_t* localspanfunc, drawspandata_t* ds, INT32 x, INT32 t1, INT32 b1, INT32 t2, INT32 b2, boolean allow_parallel)
{
	const INT32 vidheight = vid.height;

	//    Alam: from r_splats's R_RenderFloorSplat
	if (t1 >= vidheight) t1 = vidheight-1;
	if (b1 >= vidheight) b1 = vidheight-1;
	if (t2 >= vidheight) t2 = vidheight-1;
	if (b2 >= vidheight) b2 = vidheight-1;
	if (x-1 >= vid.width) x = vid.width;

	drawspandata_t dc_copy = *ds;

#ifdef HAVE_THREADS
	// We want to draw N spans per subtask to ensure the work is
	// coarse enough to not be too slow due to task scheduling overhead.
	// To safely do this, we need to copy part of spanstart to a local.
	// This is essentially loop unrolling across threads.
	constexpr const int kSpanTaskGranularity = 8;
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
				mapfunc(&dc_copy, localspanfunc, t1 + i, spanstartcopy[i], x - 1, false);
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
				mapfunc(&dc_copy, localspanfunc, b1 - i, spanstartcopy[i], x - 1, false);
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
#else
	(void)allow_parallel;

	while (t1 < t2 && t1 <= b1)
	{
		mapfunc(&dc_copy, localspanfunc, t1, spanstart[t1], x - 1, false);
		t1++;
	}
	while (b1 > b2 && b1 >= t1)
	{
		mapfunc(&dc_copy, localspanfunc, b1, spanstart[b1], x - 1, false);
		b1--;
	}
#endif

	while (t2 < t1 && t2 <= b2)
		spanstart[t2++] = x;
	while (b2 > b1 && b2 >= t2)
		spanstart[b2--] = x;
}

void R_DrawPlanes(void)
{
	visplane_t *pl;
	INT32 i;
	drawspandata_t ds = {};
#ifdef HAVE_THREADS
	srb2::ThreadPool::Sema tp_sema;
#endif

	R_UpdatePlaneRipple(&ds);

#ifdef HAVE_THREADS
	srb2::g_main_threadpool->begin_sema();
#endif
	for (i = 0; i < MAXVISPLANES; i++, pl++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			if (pl->ffloor != NULL || pl->polyobj != NULL)
				continue;

			R_DrawSinglePlane(&ds, pl, cv_parallelsoftware.value);
		}
	}
#ifdef HAVE_THREADS
	tp_sema = srb2::g_main_threadpool->end_sema();
	srb2::g_main_threadpool->notify_sema(tp_sema);
	srb2::g_main_threadpool->wait_sema(tp_sema);
#endif
}

static void R_DrawSkyPlane(visplane_t *pl, void(*colfunc2)(drawcolumndata_t*), boolean allow_parallel);

void R_DrawSkyPlanes(void)
{
	visplane_t *pl;
	INT32 i;

#ifdef HAVE_THREADS
	srb2::ThreadPool::Sema tp_sema;
	srb2::g_main_threadpool->begin_sema();
#endif
	for (i = 0; i < MAXVISPLANES; i++, pl++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			if (pl->picnum != skyflatnum || pl->ffloor || pl->polyobj)
				continue;

			R_DrawSkyPlane(pl, colfunc, cv_parallelsoftware.value);
		}
	}
#ifdef HAVE_THREADS
	tp_sema = srb2::g_main_threadpool->end_sema();
	srb2::g_main_threadpool->notify_sema(tp_sema);
	srb2::g_main_threadpool->wait_sema(tp_sema);
#endif
}

static void R_DrawSkyPlane(visplane_t *pl, void(*colfunc2)(drawcolumndata_t*), boolean allow_parallel)
{
	INT32 x;
	drawcolumndata_t dc = {};

	if (!(pl->minx <= pl->maxx))
		return;

	// If we're not supposed to draw the sky (e.g. for skyboxes), don't do anything!
	// This probably utterly ruins sky rendering for FOFs and polyobjects, unfortunately
	if (!newview->sky)
	{
		return;
	}

	// Reset column drawer function (note: couldn't we just call walldrawerfunc directly?)
	// (that is, unless we'll need to switch drawers in future for some reason)
	R_SetColumnFunc(BASEDRAWFUNC);

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

#ifdef HAVE_THREADS
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
#else
	(void)allow_parallel;

	for (x = pl->minx; x <= pl->maxx; x++)
	{
		dc.yl = pl->top[x];
		dc.yh = pl->bottom[x];

		if (dc.yl > dc.yh)
		{
			continue;
		}

		INT32 angle = (pl->viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
		angle -= (skytextureoffset >> FRACBITS);

		dc.iscale = FixedMul(skyscale, FINECOSINE(xtoviewangle[x]>>ANGLETOFINESHIFT));
		dc.x = x;
		dc.source =
		R_GetColumn(texturetranslation[skytexture],
					-angle); // get negative of angle for each column to display sky correct way round! --Monster Iestyn 27/01/18

		colfunc2(&dc);
	}
#endif
}

// Returns the height of the sloped plane at (x, y) as a 32.16 fixed_t
static INT64 R_GetSlopeZAt(const pslope_t *slope, fixed_t x, fixed_t y)
{
	INT64 x64 = ((INT64)x - (INT64)slope->o.x);
	INT64 y64 = ((INT64)y - (INT64)slope->o.y);

	x64 = (x64 * (INT64)slope->d.x) / FRACUNIT;
	y64 = (y64 * (INT64)slope->d.y) / FRACUNIT;

	return (INT64)slope->o.z + ((x64 + y64) * (INT64)slope->zdelta) / FRACUNIT;
}

// Sets the texture origin vector of the sloped plane.
static void R_SetSlopePlaneOrigin(drawspandata_t *ds, pslope_t *slope, fixed_t xpos, fixed_t ypos, fixed_t zpos, fixed_t xoff, fixed_t yoff, fixed_t angle)
{
	floatv3_t *p = &ds->slope_origin;

	INT64 vx = (INT64)xpos + (INT64)xoff;
	INT64 vy = (INT64)ypos - (INT64)yoff;

	float vxf = vx / (float)FRACUNIT;
	float vyf = vy / (float)FRACUNIT;
	float ang = ANG2RAD(ANGLE_270 - angle);

	// p is the texture origin in view space
	// Don't add in the offsets at this stage, because doing so can result in
	// errors if the flat is rotated.
	p->x = vxf * cos(ang) - vyf * sin(ang);
	p->z = vxf * sin(ang) + vyf * cos(ang);
	p->y = (R_GetSlopeZAt(slope, -xoff, yoff) - zpos) / (float)FRACUNIT;
}

// This function calculates all of the vectors necessary for drawing a sloped plane.
static void R_SetSlopePlane(drawspandata_t* ds, pslope_t *slope, fixed_t xpos, fixed_t ypos, fixed_t zpos, fixed_t xoff, fixed_t yoff, angle_t angle, angle_t plangle)
{
	// Potentially override other stuff for now cus we're mean. :< But draw a slope plane!
	// I copied ZDoom's code and adapted it to SRB2... -Red
	floatv3_t *m = &ds->slope_v, *n = &ds->slope_u;
	fixed_t height, temp;
	float ang;

	R_SetSlopePlaneOrigin(ds, slope, xpos, ypos, zpos, xoff, yoff, angle);
	height = P_GetSlopeZAt(slope, xpos, ypos);
	ds->zeroheight = FixedToFloat(height - zpos);

	// m is the v direction vector in view space
	ang = ANG2RAD(ANGLE_180 - (angle + plangle));
	m->x = cos(ang);
	m->z = sin(ang);

	// n is the u direction vector in view space
	n->x = sin(ang);
	n->z = -cos(ang);

	plangle >>= ANGLETOFINESHIFT;
	temp = P_GetSlopeZAt(slope, xpos + FINESINE(plangle), ypos + FINECOSINE(plangle));
	m->y = FixedToFloat(temp - height);
	temp = P_GetSlopeZAt(slope, xpos + FINECOSINE(plangle), ypos - FINESINE(plangle));
	n->y = FixedToFloat(temp - height);
}

static void R_CalculateSlopeVectors(drawspandata_t* ds)
{
	float sfmult = 65536.f;

	// Eh. I tried making this stuff fixed-point and it exploded on me. Here's a macro for the only floating-point vector function I recall using.
#define CROSS(d, v1, v2) \
d.x = (v1.y * v2.z) - (v1.z * v2.y);\
d.y = (v1.z * v2.x) - (v1.x * v2.z);\
d.z = (v1.x * v2.y) - (v1.y * v2.x)
	CROSS(ds->sup, ds->slope_origin, ds->slope_v);
	CROSS(ds->svp, ds->slope_origin, ds->slope_u);
	CROSS(ds->szp, ds->slope_v, ds->slope_u);
#undef CROSS

	ds->sup.z *= focallengthf;
	ds->svp.z *= focallengthf;
	ds->szp.z *= focallengthf;

	// Premultiply the texture vectors with the scale factors
	sfmult *= (1 << ds->nflatshiftup);

	ds->sup.x *= sfmult;
	ds->sup.y *= sfmult;
	ds->sup.z *= sfmult;
	ds->svp.x *= sfmult;
	ds->svp.y *= sfmult;
	ds->svp.z *= sfmult;
}

void R_SetTiltedSpan(drawspandata_t* ds, INT32 span)
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

static void R_SetSlopePlaneVectors(drawspandata_t* ds, visplane_t *pl, INT32 y, fixed_t xoff, fixed_t yoff)
{
	R_SetTiltedSpan(ds, y);
	R_SetSlopePlane(ds, pl->slope, pl->viewx, pl->viewy, pl->viewz, xoff, yoff, pl->viewangle, pl->plangle);
	R_CalculateSlopeVectors(ds);
}

static inline void R_AdjustSlopeCoordinates(drawspandata_t* ds, vector3_t *origin)
{
	const fixed_t modmask = ((1 << (32-ds->nflatshiftup)) - 1);

	fixed_t ox = (origin->x & modmask);
	fixed_t oy = -(origin->y & modmask);

	ds->xoffs &= modmask;
	ds->yoffs &= modmask;

	ds->xoffs -= (origin->x - ox);
	ds->yoffs += (origin->y + oy);
}

void R_DrawSinglePlane(drawspandata_t* ds, visplane_t *pl, boolean allow_parallel)
{
	INT32 light = 0;
	INT32 x;
	INT32 stop;
	size_t size;
	INT32 spanfunctype = BASEDRAWFUNC;
	ffloor_t *rover;
	void (*mapfunc)(drawspandata_t*, void(*)(drawspandata_t*), INT32, INT32, INT32, boolean) = R_MapPlane;

	if (!(pl->minx <= pl->maxx))
		return;

	R_UpdatePlaneRipple(ds);

	// sky flat
	if (pl->picnum == skyflatnum)
	{
		// horrific
		if (!newview->sky)
		{
			skyVisible = true;
		}

		return;
	}

	ds->planeripple.active = false;
	R_SetSpanFunc(BASEDRAWFUNC);

	if (pl->polyobj && pl->polyobj->translucency != 0)
	{
		spanfunctype = SPANDRAWFUNC_TRANS;

		// Hacked up support for alpha value in software mode Tails 09-24-2002 (sidenote: ported to polys 10-15-2014, there was no time travel involved -Red)
		if (pl->polyobj->translucency >= NUMTRANSMAPS)
			return; // Don't even draw it
		else if (pl->polyobj->translucency > 0)
			ds->transmap = R_GetTranslucencyTable(pl->polyobj->translucency);
		else // Opaque, but allow transparent flat pixels
			spanfunctype = SPANDRAWFUNC_SPLAT;

#ifdef SHITPLANESPARENCY
		if (spanfunc == spanfuncs[SPANDRAWFUNC_SPLAT] || (pl->extra_colormap && pl->extra_colormap->fog))
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
				if ((pl->ffloor->flags & FF_CUTEXTRA) && (rover->flags & FF_EXTRA))
				{
					if (!(rover->flags & FF_EXTRA))
						continue;

					// The plane is from an extra 3D floor... Check the flags so
					// there are no undesired cuts.
					if (((pl->ffloor->flags & (FF_FOG|FF_SWIMMABLE)) == (rover->flags & (FF_FOG|FF_SWIMMABLE)))
						&& pl->height < *rover->topheight
						&& pl->height > *rover->bottomheight)
						return;
				}
			}

			if (pl->ffloor->flags & FF_TRANSLUCENT)
			{
				spanfunctype = SPANDRAWFUNC_TRANS;

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
						spanfunctype = SPANDRAWFUNC_SPLAT; // Opaque, but allow transparent flat pixels
				}

#ifdef SHITPLANESPARENCY
				if (spanfunc == spanfuncs[SPANDRAWFUNC_SPLAT] || (pl->extra_colormap && pl->extra_colormap->fog))
#else
				if (!pl->extra_colormap || !(pl->extra_colormap->fog & 2))
#endif
					light = (pl->lightlevel >> LIGHTSEGSHIFT);
				else
					light = LIGHTLEVELS-1;
			}
			else if (pl->ffloor->flags & FF_FOG)
			{
				spanfunctype = SPANDRAWFUNC_FOG;
				light = (pl->lightlevel >> LIGHTSEGSHIFT);
			}
			else light = (pl->lightlevel >> LIGHTSEGSHIFT);

			if (pl->ffloor->flags & FF_RIPPLE && cv_ripplewater.value)
			{
				INT32 top, bottom;

				ds->planeripple.active = true;

				if (spanfunctype == SPANDRAWFUNC_TRANS)
				{
					spanfunctype = SPANDRAWFUNC_WATER;

					// Copy the current scene, ugh
					top = pl->high-8;
					bottom = pl->low+8;

					if (top < 0)
						top = 0;
					if (bottom > vid.height)
						bottom = vid.height;

					// Only copy the part of the screen we need
					UINT8 i = R_GetViewNumber();
					INT32 scrx = 0;
					INT32 scry = top;
					INT32 offset;

					if (splitscreen == 1)
					{
						if (i & 1)
						{
							scry += viewheight;
						}
					}
					else
					{
						if (i & 1)
						{
							scrx += viewwidth;
						}

						if (i / 2)
						{
							scry += viewheight;
						}
					}

					offset = (scry*vid.width) + scrx;

					// No idea if this works
					VID_BlitLinearScreen(renderscreen + offset,
										 vid.screens[1] + (top*vid.width), // intentionally not +offset
										 viewwidth, bottom-top,
										 vid.width, vid.width);
				}
			}
		}
		else light = (pl->lightlevel >> LIGHTSEGSHIFT);
	}

	// Don't mess with angle on slopes! We'll handle this ourselves later
	if (!pl->slope && viewangle != pl->viewangle+pl->plangle)
	{
		viewangle = pl->viewangle+pl->plangle;
	}

	ds->currentplane = pl;

	ds->source = (UINT8 *)W_CacheLumpNum(levelflats[pl->picnum].lumpnum, PU_STATIC); // Stay here until Z_ChangeTag
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

	if (light >= LIGHTLEVELS)
		light = LIGHTLEVELS-1;

	if (light < 0)
		light = 0;

	if (pl->slope)
	{
		mapfunc = R_MapTiltedPlane;

		if (!pl->plangle)
		{
			R_AdjustSlopeCoordinates(ds, &pl->slope->o);
		}

		if (ds->planeripple.active)
		{
			ds->planeheight = abs(P_GetSlopeZAt(pl->slope, pl->viewx, pl->viewy) - pl->viewz);

			R_PlaneBounds(pl);

			for (x = pl->high; x < pl->low; x++)
			{
				ds->bgofs = R_CalculateRippleOffset(ds, x);
				R_CalculatePlaneRipple(ds, pl->viewangle + pl->plangle);
				R_SetSlopePlaneVectors(ds, pl, x, (ds->xoffs + ds->planeripple.xfrac), (ds->yoffs + ds->planeripple.yfrac));
			}
		}
		else
			R_SetSlopePlaneVectors(ds, pl, 0, ds->xoffs, ds->yoffs);

		switch (spanfunctype)
		{
			case SPANDRAWFUNC_WATER:
				spanfunctype = SPANDRAWFUNC_TILTEDWATER;
				break;
			case SPANDRAWFUNC_TRANS:
				spanfunctype = SPANDRAWFUNC_TILTEDTRANS;
				break;
			case SPANDRAWFUNC_SPLAT:
				spanfunctype = SPANDRAWFUNC_TILTEDSPLAT;
				break;
			case SPANDRAWFUNC_FOG:
				spanfunctype = SPANDRAWFUNC_TILTEDFOG;
				break;
			default:
				spanfunctype = SPANDRAWFUNC_TILTED;
				break;
		}

		ds->planezlight = scalelight[light];
	}
	else
	{
		ds->planeheight = abs(pl->height - pl->viewz);
		ds->planezlight = zlight[light];
	}

	R_SetSpanFunc(spanfunctype);

	// set the maximum value for unsigned
	pl->top[pl->maxx+1] = 0xffff;
	pl->top[pl->minx-1] = 0xffff;
	pl->bottom[pl->maxx+1] = 0x0000;
	pl->bottom[pl->minx-1] = 0x0000;

	stop = pl->maxx + 1;

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
	if (spanfunc == spanfuncs[BASEDRAWFUNC])
	{
		INT32 i;

		ds->transmap = R_GetTranslucencyTable(tr_trans50);

		R_SetSpanFunc(SPANDRAWFUNC_TRANS);

		for (i = 0; i < 4; i++)
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
