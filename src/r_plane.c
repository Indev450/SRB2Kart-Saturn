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

//
// opening
//

// Quincunx antialiasing of flats!
//#define QUINCUNX

// good night sweet prince
//#define SHITPLANESPARENCY

//SoM: 3/23/2000: Use Boom visplane hashing.
#define VISPLANEHASHBITS 9
#define VISPLANEHASHMASK ((1<<VISPLANEHASHBITS)-1)
// the last visplane list is outside of the hash table and is used for fof planes
#define MAXVISPLANES ((1<<VISPLANEHASHBITS)+1)

static visplane_t *visplanes[MAXVISPLANES];
static visplane_t *freetail;
static visplane_t **freehead = &freetail;

visplane_t *floorplane;
visplane_t *ceilingplane;
static visplane_t *currentplane;

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
INT16 floorclip[MAXVIDWIDTH], ceilingclip[MAXVIDWIDTH];
fixed_t frontscale[MAXVIDWIDTH];

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
static INT32 spanstart[MAXVIDHEIGHT];

//
// texture mapping
//
lighttable_t **planezlight;
static fixed_t planeheight;

//added : 10-02-98: yslopetab is what yslope used to be,
//                yslope points somewhere into yslopetab,
//                now (viewheight/2) slopes are calculated above and
//                below the original viewheight for mouselook
//                (this is to calculate yslopes only when really needed)
//                (when mouselookin', yslope is moving into yslopetab)
//                Check R_SetupFrame, R_SetViewSize for more...
fixed_t yslopetab[MAXVIDHEIGHT*16];
fixed_t *yslope;

fixed_t basexscale, baseyscale;

fixed_t cachedheight[MAXVIDHEIGHT];
fixed_t cacheddistance[MAXVIDHEIGHT];
fixed_t cachedxstep[MAXVIDHEIGHT];
fixed_t cachedystep[MAXVIDHEIGHT];

static fixed_t xoffs, yoffs;
static floatv3_t ds_slope_origin, ds_slope_u, ds_slope_v;

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes(void)
{
	// FIXME: unused
}

// R_PortalStoreClipValues
// Saves clipping values for later. -Red
void R_PortalStoreClipValues(INT32 start, INT32 end, INT16 *ceil, INT16 *floor, fixed_t *scale)
{
	INT32 i;
	for (i = 0; i < end-start; i++)
	{
		*ceil = ceilingclip[start+i];
		ceil++;
		*floor = floorclip[start+i];
		floor++;
		*scale = frontscale[start+i];
		scale++;
	}
}

// R_PortalRestoreClipValues
// Inverse of the above. Restores the old value!
void R_PortalRestoreClipValues(INT32 start, INT32 end, INT16 *ceil, INT16 *floor, fixed_t *scale)
{
	INT32 i;
	for (i = 0; i < end-start; i++)
	{
		ceilingclip[start+i] = *ceil;
		ceil++;
		floorclip[start+i] = *floor;
		floor++;
		frontscale[start+i] = *scale;
		scale++;
	}

	// HACKS FOLLOW
	for (i = 0; i < start; i++)
	{
		floorclip[i] = -1;
		ceilingclip[i] = (INT16)viewheight;
	}
	for (i = end; i < vid.width; i++)
	{
		floorclip[i] = -1;
		ceilingclip[i] = (INT16)viewheight;
	}
}

//
// Water ripple effect!!
// Needs the height of the plane, and the vertical position of the span.
// Sets planeripple.xfrac and planeripple.yfrac, added to ds_xfrac and ds_yfrac, if the span is not tilted.
//

INT32 ds_bgofs;
INT32 ds_waterofs;

struct
{
	INT32 offset;
	fixed_t xfrac, yfrac;
	boolean active;
} planeripple;

// ripples da water texture
static fixed_t R_CalculateRippleOffset(INT32 y)
{
	fixed_t distance = FixedMul(planeheight, yslope[y]);
	const INT32 yay = (planeripple.offset + (distance>>9)) & 8191;
	return FixedDiv(FINESINE(yay), (1<<12) + (distance>>11));
}
	
static void R_CalculatePlaneRipple(angle_t angle)
{
	angle >>= ANGLETOFINESHIFT;
	angle = (angle + 2048) & 8191; // 90 degrees
	planeripple.xfrac = FixedMul(FINECOSINE(angle), ds_bgofs);
	planeripple.yfrac = FixedMul(FINESINE(angle), ds_bgofs);
}

static void R_UpdatePlaneRipple(void)
{
	ds_waterofs = (leveltime & 1)*16384;
	planeripple.offset = ((leveltime-1)*140) + ((rendertimefrac*140) / FRACUNIT);
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
static void R_MapPlane(INT32 y, INT32 x1, INT32 x2)
{
	angle_t angle, planecos, planesin;
	fixed_t distance = 0, span;
	size_t pindex;

#ifdef RANGECHECK
	if (x2 < x1 || x1 < 0 || x2 >= viewwidth || y > viewheight)
		I_Error("R_MapPlane: %d, %d at %d", x1, x2, y);
#endif

	if (x1 >= vid.width)
		x1 = vid.width - 1;

	angle = (currentplane->viewangle + currentplane->plangle)>>ANGLETOFINESHIFT;
	planecos = FINECOSINE(angle);
	planesin = FINESINE(angle);

	if (planeheight != cachedheight[y])
	{
		cachedheight[y] = planeheight;
		cacheddistance[y] = distance = FixedMul(planeheight, yslope[y]);
		span = abs(centery - y);

		if (span) // don't divide by zero
		{
			ds_xstep = FixedMul(planesin, planeheight) / span;
			ds_ystep = FixedMul(planecos, planeheight) / span;
		}
		else
		{
			ds_xstep = FixedMul(distance, basexscale);
			ds_ystep = FixedMul(distance, baseyscale);
		}

		cachedxstep[y] = ds_xstep;
		cachedystep[y] = ds_ystep;
	}
	else
	{
		distance = cacheddistance[y];
		ds_xstep = cachedxstep[y];
		ds_ystep = cachedystep[y];
	}

	ds_xfrac = xoffs + FixedMul(planecos, distance) + (x1 - centerx) * ds_xstep;
	ds_yfrac = yoffs - FixedMul(planesin, distance) + (x1 - centerx) * ds_ystep;

	// Water ripple effect
	if (planeripple.active)
	{
		ds_bgofs = R_CalculateRippleOffset(y);

		R_CalculatePlaneRipple(currentplane->viewangle + currentplane->plangle);

		ds_xfrac += planeripple.xfrac;
		ds_yfrac += planeripple.yfrac;
		ds_bgofs >>= FRACBITS;

		if ((y + ds_bgofs) >= viewheight)
			ds_bgofs = viewheight-y-1;
		if ((y + ds_bgofs) < 0)
			ds_bgofs = -y;
	}

	pindex = distance >> LIGHTZSHIFT;
	if (pindex >= MAXLIGHTZ)
		pindex = MAXLIGHTZ - 1;

	ds_colormap = planezlight[pindex];

	if (encoremap && !currentplane->noencore)
		ds_colormap += (256*32);

	if (currentplane->extra_colormap)
		ds_colormap = currentplane->extra_colormap->colormap + (ds_colormap - colormaps);

	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;

	spanfunc();
}

static void R_MapTiltedPlane(INT32 y, INT32 x1, INT32 x2)
{
#ifdef RANGECHECK
	if (x2 < x1 || x1 < 0 || x2 >= viewwidth || y > viewheight)
		I_Error("R_MapTiltedPlane: %d, %d at %d", x1, x2, y);
#endif

	if (x1 >= vid.width)
		x1 = vid.width - 1;

	// Water ripple effect
	if (planeripple.active)
	{
		ds_bgofs = R_CalculateRippleOffset(y);

		ds_sup = &ds_su[y];
		ds_svp = &ds_sv[y];
		ds_szp = &ds_sz[y];

		ds_bgofs >>= FRACBITS;

		if ((y + ds_bgofs) >= viewheight)
			ds_bgofs = viewheight-y-1;
		if ((y + ds_bgofs) < 0)
			ds_bgofs = -y;
	}
	
	if (encoremap && !currentplane->noencore)
		ds_colormap += (256*32);

	if (currentplane->extra_colormap)
		ds_colormap = currentplane->extra_colormap->colormap;
	else
		ds_colormap = colormaps;

	ds_y = y;
	ds_x1 = x1;
	ds_x2 = x2;

	spanfunc();
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

	numffloors = 0;

	for (i = 0; i < MAXVISPLANES; i++)
	for (*freehead = visplanes[i], visplanes[i] = NULL;
		freehead && *freehead ;)
	{
		freehead = &(*freehead)->next;
	}

	lastopening = openings;

	// texture calculation
	memset(cachedheight, 0, sizeof (cachedheight));

	// left to right mapping
	angle = (viewangle-ANGLE_90)>>ANGLETOFINESHIFT;

	// scale will be unit scale at SCREENWIDTH/2 distance
	basexscale = FixedDiv (FINECOSINE(angle),centerxfrac);
	baseyscale = -FixedDiv (FINESINE(angle),centerxfrac);
}

static visplane_t *new_visplane(unsigned hash)
{
	visplane_t *check = freetail;
	if (!check)
	{
		check = malloc(sizeof (*check));
		if (check == NULL) I_Error("%s: Out of memory", "new_visplane"); // FIXME: ugly
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
			, boolean noencore)
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
			float ang = ANG2RAD(plangle);
			float x = FixedToFloat(xoff);
			float y = FixedToFloat(yoff);
			xoff = FloatToFixed(x * cos(ang) + y * sin(ang));
			yoff = FloatToFixed(-x * sin(ang) + y * cos(ang));
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

	memset(check->top, 0xff, sizeof (check->top));
	memset(check->bottom, 0x00, sizeof (check->bottom));

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
			unsigned hash =
				visplane_hash(pl->picnum, pl->lightlevel, pl->height);
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
		memset(pl->top, 0xff, sizeof pl->top);
		memset(pl->bottom, 0x00, sizeof pl->bottom);
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

static void R_MakeSpans(INT32 x, INT32 t1, INT32 b1, INT32 t2, INT32 b2)
{
	//    Alam: from r_splats's R_RenderFloorSplat
	if (t1 >= vid.height) t1 = vid.height-1;
	if (b1 >= vid.height) b1 = vid.height-1;
	if (t2 >= vid.height) t2 = vid.height-1;
	if (b2 >= vid.height) b2 = vid.height-1;
	if (x-1 >= vid.width) x = vid.width;

	while (t1 < t2 && t1 <= b1)
	{
		R_MapPlane(t1, spanstart[t1], x - 1);
		t1++;
	}
	while (b1 > b2 && b1 >= t1)
	{
		R_MapPlane(b1, spanstart[b1], x - 1);
		b1--;
	}

	while (t2 < t1 && t2 <= b2)
		spanstart[t2++] = x;
	while (b2 > b1 && b2 >= t2)
		spanstart[b2--] = x;
}

static void R_MakeTiltedSpans(INT32 x, INT32 t1, INT32 b1, INT32 t2, INT32 b2)
{
	//    Alam: from r_splats's R_RasterizeFloorSplat
	if (t1 >= vid.height) t1 = vid.height-1;
	if (b1 >= vid.height) b1 = vid.height-1;
	if (t2 >= vid.height) t2 = vid.height-1;
	if (b2 >= vid.height) b2 = vid.height-1;
	if (x-1 >= vid.width) x = vid.width;

	while (t1 < t2 && t1 <= b1)
	{
		R_MapTiltedPlane(t1, spanstart[t1], x - 1);
		t1++;
	}
	while (b1 > b2 && b1 >= t1)
	{
		R_MapTiltedPlane(b1, spanstart[b1], x - 1);
		b1--;
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

	spanfunc = basespanfunc;
	wallcolfunc = walldrawerfunc;
	R_UpdatePlaneRipple();

	for (i = 0; i < MAXVISPLANES; i++, pl++)
	{
		for (pl = visplanes[i]; pl; pl = pl->next)
		{
			if (pl->ffloor != NULL || pl->polyobj != NULL)
				continue;

			R_DrawSinglePlane(pl);
		}
	}
}

static void R_DrawSkyPlane(visplane_t *pl)
{
	INT32 x;
	INT32 angle;

	if (!newview->sky)
	{
		skyVisible = true;
		return;
	}

	wallcolfunc = walldrawerfunc;

	// use correct aspect ratio scale
	dc_iscale = skyscale;
	// Sky is always drawn full bright,
	//  i.e. colormaps[0] is used.
	// Because of this hack, sky is not affected
	//  by INVUL inverse mapping.
	dc_colormap = colormaps;
	if (encoremap)
		dc_colormap += (256*32);
	dc_texturemid = skytexturemid;
	dc_texheight = textureheight[skytexture]
		>>FRACBITS;
	for (x = pl->minx; x <= pl->maxx; x++)
	{
		dc_yl = pl->top[x];
		dc_yh = pl->bottom[x];
		if (dc_yl <= dc_yh)
		{
			angle = (pl->viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
			dc_iscale = FixedMul(skyscale, FINECOSINE(xtoviewangle[x]>>ANGLETOFINESHIFT));
			dc_x = x;
			dc_source =
				R_GetColumn(texturetranslation[skytexture],
					angle);
			wallcolfunc();
		}
	}
}

// Sets the texture origin vector of the sloped plane.
static void R_SetSlopePlaneOrigin(pslope_t *slope, fixed_t xpos, fixed_t ypos, fixed_t zpos, fixed_t xoff, fixed_t yoff, fixed_t angle)
{
	floatv3_t *p = &ds_slope_origin;

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
	p->y = (R_GetZAt(slope, -xoff, yoff) - zpos) / (float)FRACUNIT;
}

// This function calculates all of the vectors necessary for drawing a sloped plane.
void R_SetSlopePlane(pslope_t *slope, fixed_t xpos, fixed_t ypos, fixed_t zpos, fixed_t xoff, fixed_t yoff, angle_t angle, angle_t plangle)
{
	// Potentially override other stuff for now cus we're mean. :< But draw a slope plane!
	// I copied ZDoom's code and adapted it to SRB2... -Red
	floatv3_t *m = &ds_slope_v, *n = &ds_slope_u;
	fixed_t height, temp;
	float ang;

	R_SetSlopePlaneOrigin(slope, xpos, ypos, zpos, xoff, yoff, angle);
	height = P_GetZAt(slope, xpos, ypos);
	zeroheight = FixedToFloat(height - zpos);

	// m is the v direction vector in view space
	ang = ANG2RAD(ANGLE_180 - (angle + plangle));
	m->x = cos(ang);
	m->z = sin(ang);

	// n is the u direction vector in view space
	n->x = sin(ang);
	n->z = -cos(ang);

	plangle >>= ANGLETOFINESHIFT;
	temp = P_GetZAt(slope, xpos + FINESINE(plangle), ypos + FINECOSINE(plangle));
	m->y = FixedToFloat(temp - height);
	temp = P_GetZAt(slope, xpos + FINECOSINE(plangle), ypos - FINESINE(plangle));
	n->y = FixedToFloat(temp - height);
}

// This function calculates all of the vectors necessary for drawing a sloped and scaled plane.
void R_SetScaledSlopePlane(pslope_t *slope, fixed_t xpos, fixed_t ypos, fixed_t zpos, fixed_t xs, fixed_t ys, fixed_t xoff, fixed_t yoff, angle_t angle, angle_t plangle)
{
	floatv3_t *m = &ds_slope_v, *n = &ds_slope_u;
	fixed_t height, temp;

	float xscale = FixedToFloat(xs);
	float yscale = FixedToFloat(ys);
	float ang;

	R_SetSlopePlaneOrigin(slope, xpos, ypos, zpos, xoff, yoff, angle);
	height = P_GetZAt(slope, xpos, ypos);
	zeroheight = FixedToFloat(height - zpos);

	// m is the v direction vector in view space
	ang = ANG2RAD(ANGLE_180 - (angle + plangle));
	m->x = yscale * cos(ang);
	m->z = yscale * sin(ang);

	// n is the u direction vector in view space
	n->x = xscale * sin(ang);
	n->z = -xscale * cos(ang);

	ang = ANG2RAD(plangle);
	temp = P_GetZAt(slope, xpos + FloatToFixed(yscale * sin(ang)), ypos + FloatToFixed(yscale * cos(ang)));
	m->y = FixedToFloat(temp - height);
	temp = P_GetZAt(slope, xpos + FloatToFixed(xscale * cos(ang)), ypos - FloatToFixed(xscale * sin(ang)));
	n->y = FixedToFloat(temp - height);
}

void R_CalculateSlopeVectors(void)
{
	float sfmult = 65536.f;

	// Eh. I tried making this stuff fixed-point and it exploded on me. Here's a macro for the only floating-point vector function I recall using.
#define CROSS(d, v1, v2) \
d->x = (v1.y * v2.z) - (v1.z * v2.y);\
d->y = (v1.z * v2.x) - (v1.x * v2.z);\
d->z = (v1.x * v2.y) - (v1.y * v2.x)
	CROSS(ds_sup, ds_slope_origin, ds_slope_v);
	CROSS(ds_svp, ds_slope_origin, ds_slope_u);
	CROSS(ds_szp, ds_slope_v, ds_slope_u);
#undef CROSS

	ds_sup->z *= focallengthf;
	ds_svp->z *= focallengthf;
	ds_szp->z *= focallengthf;

	// Premultiply the texture vectors with the scale factors
	sfmult *= (1 << nflatshiftup);

	ds_sup->x *= sfmult;
	ds_sup->y *= sfmult;
	ds_sup->z *= sfmult;
	ds_svp->x *= sfmult;
	ds_svp->y *= sfmult;
	ds_svp->z *= sfmult;
}

void R_SetTiltedSpan(INT32 span)
{
	if (ds_su == NULL)
		ds_su = Z_Malloc(sizeof(*ds_su) * vid.height, PU_STATIC, NULL);
	if (ds_sv == NULL)
		ds_sv = Z_Malloc(sizeof(*ds_sv) * vid.height, PU_STATIC, NULL);
	if (ds_sz == NULL)
		ds_sz = Z_Malloc(sizeof(*ds_sz) * vid.height, PU_STATIC, NULL);

	ds_sup = &ds_su[span];
	ds_svp = &ds_sv[span];
	ds_szp = &ds_sz[span];
}

static void R_SetSlopePlaneVectors(visplane_t *pl, INT32 y, fixed_t xoff, fixed_t yoff)
{
	R_SetTiltedSpan(y);
	R_SetSlopePlane(pl->slope, pl->viewx, pl->viewy, pl->viewz, xoff, yoff, pl->viewangle, pl->plangle);
	R_CalculateSlopeVectors();
}

static inline void R_AdjustSlopeCoordinates(vector3_t *origin)
{
	const fixed_t modmask = ((1 << (32-nflatshiftup)) - 1);

	fixed_t ox = (origin->x & modmask);
	fixed_t oy = -(origin->y & modmask);

	xoffs &= modmask;
	yoffs &= modmask;

	xoffs -= (origin->x - ox);
	yoffs += (origin->y + oy);
}

void R_DrawSinglePlane(visplane_t *pl)
{
	INT32 light = 0;
	INT32 x;
	INT32 stop, angle;
	size_t size;
	ffloor_t *rover;

	if (!(pl->minx <= pl->maxx))
		return;

	// sky flat
	if (pl->picnum == skyflatnum)
	{
		R_DrawSkyPlane(pl);
		return;
	}

	planeripple.active = false;
	spanfunc = basespanfunc;

	if (pl->polyobj && pl->polyobj->translucency != 0) {
		spanfunc = R_DrawTranslucentSpan_8;

		// Hacked up support for alpha value in software mode Tails 09-24-2002 (sidenote: ported to polys 10-15-2014, there was no time travel involved -Red)
		if (pl->polyobj->translucency >= 10)
			return; // Don't even draw it
		else if (pl->polyobj->translucency > 0)
			ds_transmap = transtables + ((pl->polyobj->translucency-1)<<FF_TRANSSHIFT);
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

	} else
	if (pl->ffloor)
	{
		// Don't draw planes that shouldn't be drawn.
		for (rover = pl->ffloor->target->ffloors; rover; rover = rover->next)
		{
			if ((pl->ffloor->flags & FF_CUTEXTRA) && (rover->flags & FF_EXTRA))
			{
				if (pl->ffloor->flags & FF_EXTRA)
				{
					// The plane is from an extra 3D floor... Check the flags so
					// there are no undesired cuts.
					if (((pl->ffloor->flags & (FF_FOG|FF_SWIMMABLE)) == (rover->flags & (FF_FOG|FF_SWIMMABLE)))
						&& pl->height < *rover->topheight
						&& pl->height > *rover->bottomheight)
						return;
				}
			}
		}

		if (pl->ffloor->flags & FF_TRANSLUCENT)
		{
			spanfunc = R_DrawTranslucentSpan_8;

			// Hacked up support for alpha value in software mode Tails 09-24-2002
			if (pl->ffloor->alpha < 12)
				return; // Don't even draw it
			else if (pl->ffloor->alpha < 38)
				ds_transmap = transtables + ((tr_trans90-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 64)
				ds_transmap = transtables + ((tr_trans80-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 89)
				ds_transmap = transtables + ((tr_trans70-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 115)
				ds_transmap = transtables + ((tr_trans60-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 140)
				ds_transmap = transtables + ((tr_trans50-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 166)
				ds_transmap = transtables + ((tr_trans40-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 192)
				ds_transmap = transtables + ((tr_trans30-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 217)
				ds_transmap = transtables + ((tr_trans20-1)<<FF_TRANSSHIFT);
			else if (pl->ffloor->alpha < 243)
				ds_transmap = transtables + ((tr_trans10-1)<<FF_TRANSSHIFT);
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
		else if (pl->ffloor->flags & FF_FOG)
		{
			spanfunc = R_DrawFogSpan_8;
			light = (pl->lightlevel >> LIGHTSEGSHIFT);
		}
		else light = (pl->lightlevel >> LIGHTSEGSHIFT);

		if (pl->ffloor->flags & FF_RIPPLE)
		{
			INT32 top, bottom;
			UINT8 *scr;

			planeripple.active = true;
			
			if (spanfunc == R_DrawTranslucentSpan_8)
			{
				spanfunc = R_DrawTranslucentWaterSpan_8;

				// Copy the current scene, ugh
				top = pl->high-8;
				bottom = pl->low+8;

				if (top < 0)
					top = 0;
				if (bottom > vid.height)
					bottom = vid.height;

				if (splitscreen > 2 && viewplayer == &players[displayplayers[3]]) // Only copy the part of the screen we need
					scr = (screens[0] + (top+(viewheight))*vid.width + viewwidth);
				else if ((splitscreen == 1 && viewplayer == &players[displayplayers[1]])
					|| (splitscreen > 1 && viewplayer == &players[displayplayers[2]]))
					scr = (screens[0] + (top+(viewheight))*vid.width);
				else if (splitscreen > 1 && viewplayer == &players[displayplayers[1]])
					scr = (screens[0] + ((top)*vid.width) + viewwidth);
				else
					scr = (screens[0] + ((top)*vid.width));

				VID_BlitLinearScreen(scr, screens[1]+((top)*vid.width),
				                     vid.width, bottom-top,
				                     vid.width, vid.width);
			}
		}
	}
	else light = (pl->lightlevel >> LIGHTSEGSHIFT);

	if (!pl->slope // Don't mess with angle on slopes! We'll handle this ourselves later
		&& viewangle != pl->viewangle+pl->plangle)
	{
		memset(cachedheight, 0, sizeof (cachedheight));
		angle = (pl->viewangle+pl->plangle-ANGLE_90)>>ANGLETOFINESHIFT;
		basexscale = FixedDiv(FINECOSINE(angle),centerxfrac);
		baseyscale = -FixedDiv(FINESINE(angle),centerxfrac);
		viewangle = pl->viewangle+pl->plangle;
	}

	currentplane = pl;

	ds_source = (UINT8 *)
		W_CacheLumpNum(levelflats[pl->picnum].lumpnum,
			PU_STATIC); // Stay here until Z_ChangeTag

	size = W_LumpLength(levelflats[pl->picnum].lumpnum);

	switch (size)
	{
		case 4194304: // 2048x2048 lump
			nflatmask = 0x3FF800;
			nflatxshift = 21;
			nflatyshift = 10;
			nflatshiftup = 5;
			break;
		case 1048576: // 1024x1024 lump
			nflatmask = 0xFFC00;
			nflatxshift = 22;
			nflatyshift = 12;
			nflatshiftup = 6;
			break;
		case 262144:// 512x512 lump'
			nflatmask = 0x3FE00;
			nflatxshift = 23;
			nflatyshift = 14;
			nflatshiftup = 7;
			break;
		case 65536: // 256x256 lump
			nflatmask = 0xFF00;
			nflatxshift = 24;
			nflatyshift = 16;
			nflatshiftup = 8;
			break;
		case 16384: // 128x128 lump
			nflatmask = 0x3F80;
			nflatxshift = 25;
			nflatyshift = 18;
			nflatshiftup = 9;
			break;
		case 1024: // 32x32 lump
			nflatmask = 0x3E0;
			nflatxshift = 27;
			nflatyshift = 22;
			nflatshiftup = 11;
			break;
		default: // 64x64 lump
			nflatmask = 0xFC0;
			nflatxshift = 26;
			nflatyshift = 20;
			nflatshiftup = 10;
			break;
	}

	xoffs = pl->xoffs;
	yoffs = pl->yoffs;

	if (light >= LIGHTLEVELS)
		light = LIGHTLEVELS-1;

	if (light < 0)
		light = 0;

	if (pl->slope)
	{
		if (!pl->plangle)
		{
			R_AdjustSlopeCoordinates(&pl->slope->o);
		}
		
		if (planeripple.active)
		{
			planeheight = abs(P_GetZAt(pl->slope, pl->viewx, pl->viewy) - pl->viewz);

			R_PlaneBounds(pl);

			for (x = pl->high; x < pl->low; x++)
			{
				ds_bgofs = R_CalculateRippleOffset(x);
				R_CalculatePlaneRipple(pl->viewangle + pl->plangle);
				R_SetSlopePlaneVectors(pl, x, (xoffs + planeripple.xfrac), (yoffs + planeripple.yfrac));
			}
		}
		else
			R_SetSlopePlaneVectors(pl, 0, xoffs, yoffs);

		if (spanfunc == R_DrawTranslucentWaterSpan_8)
			spanfunc = R_DrawTiltedTranslucentWaterSpan_8;
		else if (spanfunc == R_DrawTranslucentSpan_8)
			spanfunc = R_DrawTiltedTranslucentSpan_8;
		else if (spanfunc == splatfunc)
			spanfunc = R_DrawTiltedSplat_8;
		else
			spanfunc = R_DrawTiltedSpan_8;

		planezlight = scalelight[light];
	} 
	else
	{
		planeheight = abs(pl->height - pl->viewz);
		planezlight = zlight[light];
	}

	// set the maximum value for unsigned
	pl->top[pl->maxx+1] = 0xffff;
	pl->top[pl->minx-1] = 0xffff;
	pl->bottom[pl->maxx+1] = 0x0000;
	pl->bottom[pl->minx-1] = 0x0000;

	stop = pl->maxx + 1;

	if (pl->slope)
	{
		for (x = pl->minx; x <= stop; x++)
			R_MakeTiltedSpans(x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x]);
	}
	else
	{
		for (x = pl->minx; x <= stop; x++)
			R_MakeSpans(x, pl->top[x-1], pl->bottom[x-1], pl->top[x], pl->bottom[x]);
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
	if (spanfunc == R_DrawSpan_8)
	{
		INT32 i;
		ds_transmap = transtables + ((tr_trans50-1)<<FF_TRANSSHIFT);
		spanfunc = R_DrawTranslucentSpan_8;
		for (i=0; i<4; i++)
		{
			xoffs = pl->xoffs;
			yoffs = pl->yoffs;

			switch(i)
			{
				case 0:
					xoffs -= FRACUNIT/4;
					yoffs -= FRACUNIT/4;
					break;
				case 1:
					xoffs -= FRACUNIT/4;
					yoffs += FRACUNIT/4;
					break;
				case 2:
					xoffs += FRACUNIT/4;
					yoffs -= FRACUNIT/4;
					break;
				case 3:
					xoffs += FRACUNIT/4;
					yoffs += FRACUNIT/4;
					break;
			}
			planeheight = abs(pl->height - pl->viewz);

			if (light >= LIGHTLEVELS)
				light = LIGHTLEVELS-1;

			if (light < 0)
				light = 0;

			planezlight = zlight[light];

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

	Z_ChangeTag(ds_source, PU_CACHE);
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
