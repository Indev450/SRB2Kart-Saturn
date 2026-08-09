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
/// \brief convert SRB2 map

#include "../doomdef.h"

#ifdef HWRENDER

#include "hw_main.h"
#include "hw_glob.h"
#include "../z_zone.h"

#ifdef HWR_LOADING_SCREEN
#include "../console.h"
#include "../m_menu.h"
#include "../p_setup.h" // levelfadecol
#include "../i_video.h"
#endif

//#define DEBUG_HWBSP

// --------------------------------------------------------------------------
// This is global data for planes rendering
// --------------------------------------------------------------------------

extrasubsector_t *extrasubsectors = NULL;

// extra subsectors are subsectors without segs, added for the plane polygons
#define NUM_EXTRA_SUBSECTORS 50
static size_t num_alloc_extra_subsector;
#ifdef PARANOIA
size_t num_extra_subsector;
#else
static size_t num_extra_subsector;
#endif

typedef struct {
	float x, y;
	float dx, dy;
	polyvertex_t divpt;
	float divfrac;
} fdivline_t;

// when loading the map, this is set to true if portals are found.
// if no portals are found, the portal scanning phase can be skipped while rendering, saving a bit of time.
boolean gl_maphasportals = false;

// Determine on mapload if current map has any Horizonlines present
// avoids doing a bunch of work during rendering
boolean gl_maphashorizonlines = false;

// ==========================================================================
//                                    FLOOR & CEILING CONVEX POLYS GENERATION
// ==========================================================================

//debug counters
#ifdef DEBUG_HWBSP
static INT32 nobackpoly = 0;
static INT32 skipcut = 0;
static INT32 totalsubsecpolys = 0;
#endif

// --------------------------------------------------------------------------
// Polygon fast alloc / free
// --------------------------------------------------------------------------

static poly_t *HWR_AllocPoly(INT32 numpts)
{
	poly_t *p;
	size_t size = sizeof(poly_t) + sizeof(polyvertex_t) * numpts;
	p = Z_Malloc(size, PU_HWRPLANE, NULL);
	p->numpts = numpts;
	return p;
}

/// TODO: polygons should be freed in reverse order for efficiency,
/// for now don't free because it doesn't free in reverse order
static void HWR_FreePoly(poly_t *poly)
{
	Z_Free(poly);
}

// Return interception along bsp line (partline),
// with the polygon segment

// Return the division in partline div fields.
// divfrac = how far along partline vector is crossing pt
static boolean fracdivline(fdivline_t *partline, polyvertex_t *v1, polyvertex_t *v2)
{
	double frac1, frac2;
	double num; // numerator
	double den; // denominator
	double v1x, v1y, v1dx, v1dy; // polygon side vector, v1->v2
	double v3x, v3y, v3dx, v3dy; // partline vector

	// a segment of a polygon
	v1x  = v1->x;
	v1y  = v1->y;
	v1dx = v2->x - v1->x;
	v1dy = v2->y - v1->y;

	// the bsp partition line
	v3x  = partline->x;
	v3y  = partline->y;
	v3dx = partline->dx;
	v3dy = partline->dy;

	den = v3dy*v1dx - v3dx*v1dy;

	if (fabs(den) < 1.0E-36) // avoid check of float for exact 0
		return false;  // partline and polygon side are effectively parallel

	// first check the frac along the polygon segment,
	// (do not accept hit with the extensions)
	num = (v3x - v1x)*v3dy + (v1y - v3y)*v3dx;
	frac1 = num / den;

	// 0= cross at v1, 1.0= cross at v2
	if (frac1 < 0.0 || frac1 > 1.0)  // double
		return false;  // not within the polygon side

	// now get the frac along the BSP line
	// which is useful to determine what is left, what is right
	num = (v3x - v1x)*v1dy + (v1y - v3y)*v1dx;
	frac2 = num / den;

	partline->divfrac = frac2;  // how far along partline vector

	// [WDJ] find the interception point along the segment.
	// It should be slightly more accurate because it is always closer to the
	// crossing point than arbitrary positions on the partition line.
	partline->divpt.x = v1x + v1dx*frac1;
	partline->divpt.y = v1y + v1dy*frac1;

	return true;
}

// BOOMEDIT.WAD has a vertex error of .21
#define DIVLINE_VERTEX_DIFF 0.45f

// if two vertice coords have a x and/or y difference
// of less or equal than 1 FRACUNIT, they are considered the same
// point. Note: hardcoded value, 1.0f could be anything else.
static boolean SameVertex(polyvertex_t *p1, polyvertex_t *p2)
{
	if (fabsf(p2->x - p1->x) > DIVLINE_VERTEX_DIFF)
		return false;
	if (fabsf(p2->y - p1->y) > DIVLINE_VERTEX_DIFF)
		return false;

	// p1 and p2 are considered the same vertex
	return true;
}

// split a _CONVEX_ polygon in two convex polygons
// outputs:
//   frontpoly : polygon on right side of bsp line
//   backpoly  : polygon on left side
//
// Called from: WalkBSPNode
static void SplitPoly (fdivline_t *dlnp,        // splitting parametric line
                       poly_t *poly,            // the convex poly we split
                       poly_t **frontpoly,      // return one poly here
                       poly_t **backpoly)       // return the other here
{
	INT32 i, j;
	polyvertex_t *pv;

	INT32 ps, pe; // poly start, end
	INT32 ps_online, pe_online;
	INT32 nptfront, nptback;
	polyvertex_t vs = {};
	polyvertex_t ve = {};
	polyvertex_t lastpv = {};
	float ps_frac = 0.0f, pe_frac = 0.0f;        // used to tell which poly is on
	                                             // the front side of the bsp partition line
	ps = pe = -1;
    ps_online = pe_online = 0;

	for (i = 0; i < poly->numpts; i++)
	{
		// i, j are one side of the poly
		j = i+1;
		if (j == poly->numpts)
			j = 0;  // wrap poly

		// start & end points
		if (!fracdivline(dlnp, &poly->pts[i], &poly->pts[j]))
			continue;

		// have dividing pt
		if (ps < 0)
		{
			// first point
			ps = i;
			vs = dlnp->divpt;
			ps_frac = dlnp->divfrac;
		}
		else
		{
			// the partition line can traverse a junction between two segments
			// or the two points are so close, they can be considered as one
			// thus, don't accept, since split 2 must be another vertex
			if (SameVertex(&dlnp->divpt, &lastpv))
			{
				if (pe < 0)
				{
					ps = i;
					ps_online = 1;
				}
				else
				{
					pe = i;
					pe_online = 1;
				}
			}
			else
			{
				if (pe < 0)
				{
					pe = i;
					ve = dlnp->divpt;
					pe_frac = dlnp->divfrac;
				}
				else
				{
					// a frac, not same vertice as last one
					// we already got pt2 so pt 2 is not on the line,
					// so we probably got back to the start point
					// which is on the line
					if (SameVertex(&dlnp->divpt, &vs))
						ps_online = 1;

					break;
				}
			}
		}

		// remember last point intercept to detect identical points
		lastpv = dlnp->divpt;
	}

	// no split: the partition line is either parallel and
	// aligned with one of the poly segments, or the line is totally
	// out of the polygon and doesn't traverse it (happens if the bsp
	// is fooled by some trick where the sidedefs don't point to
	// the right sectors)
	if (ps < 0)
	{
		//I_Error("SplitPoly: did not split polygon (%d %d)\n"
		//        "debugpos %d",ps,pe,debugpos);

		// this eventually happens with 'broken' BSP's that accept
		// linedefs where each side point the same sector, that is:
		// the deep water effect with the original Doom

		//TODO: make sure front poly is to front of partition line?

		*frontpoly = poly;
		*backpoly  = NULL;
		return;
	}

	//if (ps >= 0 && pe < 0)
	if (pe < 0)
	{
		//I_Error("SplitPoly: only one point for split line (%d %d)", ps, pe);
		*frontpoly = poly;
		*backpoly  = NULL;
		return;
	}

	if (pe <= ps)
	{
		//I_Error("SplitPoly: invalid splitting line (%d %d)", ps, pe);
		*frontpoly = poly;
		*backpoly  = NULL;
		return;
	}

	// Number of points on each side, _not_ counting those
	// that may lie just on the line.
	nptback  = pe - ps - pe_online;
	nptfront = poly->numpts - pe_online - ps_online - nptback;

	if (nptback > 0)
		*backpoly = HWR_AllocPoly(2 + nptback);
	else
		*backpoly = NULL;

	if (nptfront > 0)
		*frontpoly = HWR_AllocPoly(2 + nptfront);
	else
		*frontpoly = NULL;

	// generate FRONT poly
	if (*frontpoly)
	{
		pv = (*frontpoly)->pts;
		*pv++ = vs;
		*pv++ = ve;
		i = pe;

		do {
			if (++i == poly->numpts)
				i = 0;
			*pv++ = poly->pts[i];
		} while (i != ps && --nptfront);
	}

	// generate BACK poly
	if (*backpoly)
	{
		pv = (*backpoly)->pts;
		*pv++ = ve;
		*pv++ = vs;
		i = ps;

		do {
			if (++i == poly->numpts)
				i = 0;
			*pv++ = poly->pts[i];
		} while (i != pe && --nptback);
	}

	// make sure frontpoly is the one on the 'right' side
	// of the partition line
	if (ps_frac > pe_frac)
	{
		poly_t*      swappoly;
		swappoly   = *backpoly;
		*backpoly  = *frontpoly;
		*frontpoly = swappoly;
	}

	HWR_FreePoly(poly);
}

// checks if any line is a portal or horizonline, for optimization
static inline void CheckForPortalsAndHorizonLines(seg_t *lseg, const line_t *line)
{
	// portal check
	if (!gl_maphasportals && lseg->side == 0 && line->special == PORTALSPECIAL)
	{
		// Find the other side!
		INT32 line2 = P_FindSpecialLineFromTag(PORTALSPECIAL, line->tag, -1);

		if (line == &lines[line2])
			line2 = P_FindSpecialLineFromTag(PORTALSPECIAL, line->tag, line2);

		if (line2 >= 0) // found it!
			gl_maphasportals = 1;
	}

	// horizonline check
	if (!gl_maphashorizonlines && line->special == HORIZONSPECIAL)
		gl_maphashorizonlines = true;
}

// Use each seg of the poly as a partition line, keep only the
// part of the convex poly to the front of the seg (that is,
// the part inside the sector). The part behind the seg, is
// the void space and is cut out.
//
//  poly : surrounding convex polygon, non-destructive
//  lseg : array of seg
//  segcount : number of seg in the array
// Return frontpoly, which may be new or ptr to poly.
// Called from: HWR_SubsecPoly
static poly_t *CutOutSubsecPoly(seg_t *lseg, INT32 segcount, poly_t *poly)
{
	INT32 i, j;
	poly_t* frontpoly = poly;  // default, return same poly
	polyvertex_t *pv;
	INT32 poly_num_pts = 0, ps, pe;
	polyvertex_t vs = {}, ve = {},
				 p1 = {}, p2 = {};
	float ps_frac = 0.0f;
	vertex_t *v1, *v2;
	fdivline_t cutseg; // x, y, dx, dy as start of node_t struct

	// for each seg of the subsector
	for (; segcount--; lseg++)
	{
		// x, y, dx, dy (like a divline)
		const line_t *line = lseg->linedef;

		if (!line)
			continue;

		CheckForPortalsAndHorizonLines(lseg, line);

		if (line->sidenum[1] != NO_INDEX)
		{
			if (sides[line->sidenum[0]].sector == sides[line->sidenum[1]].sector)
			{
				// Segs that are self-ref linedef do not cutout the subsector.
#ifdef DEBUG_HWBSP
				CONS_Debug(DBG_RENDER, "CutOutSubsecPoly: self ref line %i\n", line - lines);
#endif
				continue;
			}
		}

		if (lseg->side) // side 1
		{
			v1 = line->v2;
			v2 = line->v1;
		}
		else // side 0
		{
			v1 = line->v1;
			v2 = line->v2;
		}

		p1.x = FixedToFloat(v1->x);
		p1.y = FixedToFloat(v1->y);
		p2.x = FixedToFloat(v2->x);
		p2.y = FixedToFloat(v2->y);

		cutseg.x = p1.x;
		cutseg.y = p1.y;
		cutseg.dx = p2.x - p1.x;
		cutseg.dy = p2.y - p1.y;

		// see if it cuts the convex poly
		ps = -1;
		pe = -1;

		for (i = 0; i < poly->numpts; i++)
		{
			// i, j are one side of the poly
			j = i+1;
			if (j == poly->numpts)
				j = 0;

			if (!fracdivline(&cutseg, &poly->pts[i], &poly->pts[j]))
				continue;

			// have dividing pt
			if (ps < 0)
			{
				ps = i;
				vs = cutseg.divpt;
				ps_frac = cutseg.divfrac;
				continue;
			}

			// frac 1 on previous segment,
			//     0 on the next,
			// the split line goes through one of the convex poly
			// vertices, happens quite often since the convex
			// poly is already adjacent to the subsector segs
			// on most borders
			if (SameVertex(&cutseg.divpt, &vs))
				continue;

			if (ps_frac <= cutseg.divfrac)
			{
				poly_num_pts = 2 + poly->numpts - (i-ps);
				pe = ps;
				ps = i;
				ve = cutseg.divpt;
			}
			else
			{
				poly_num_pts = 2 + (i-ps);
				pe = i;
				ve = vs;
				vs = cutseg.divpt;
			}

			// found 2nd point
			break;
		}

		// there was a split
		if (ps >= 0)
		{
			// need 2 points
			if (pe >= 0)
			{
				// generate FRONT poly
				frontpoly = HWR_AllocPoly(poly_num_pts);

				pv = frontpoly->pts;
				*pv++ = vs;
				*pv++ = ve;

				do {
					if (++ps == poly->numpts)  // poly wrap
						ps = 0;
					*pv++ = poly->pts[ps];
				} while (ps != pe);

				HWR_FreePoly(poly);
				poly = frontpoly;
			}
#ifdef DEBUG_HWBSP
			else
			{
				// hmmm... maybe we should NOT accept this, but this happens
				// only when the cut is not needed it seems (when the cut
				// line is aligned to one of the borders of the poly, and
				// only some times..)
				// [WDJ] This happens often (27 times in Doom2 map01).
				skipcut++;
				//I_Error("CutOutPoly: only one point for split line (%d %d) %d", ps, pe, debugpos);
			}
#endif
		}
	}

	return frontpoly;
}

// At this point, the poly should be convex and the exact
// layout of the subsector. It is not always the case,
// so continue to cut off the poly into smaller parts with
// each seg of the subsector.
//
//  ssindex : subsec index, 0..(numsubsectors-1)
//  poly : surrounding convex polygon, non-destructive
// Called from WalkBSPNode
static inline void HWR_SubsecPoly(INT32 ssindex, poly_t *poly)
{
	INT16 segcount;
	subsector_t *sub;
	seg_t *lseg;

	sub = &subsectors[ssindex];
	segcount = sub->numlines;
	lseg = &segs[sub->firstline];

	if (poly)
	{
		poly = CutOutSubsecPoly(lseg, segcount, poly);
#ifdef DEBUG_HWBSP
		totalsubsecpolys++;
#endif
		//extra data for this subsector
		extrasubsectors[ssindex].planepoly = poly;
	}
}

// The bsp divline does not have enough precision.
// Search for the segs source of this divline.
static inline void SetDivline(node_t *bsp, fdivline_t *divline)
{
	// MAR - If you don't use the same partition line that the BSP uses,
	// the front/back polys won't match the subsectors in the BSP!
	divline->x  = FixedToFloat(bsp->x);
	divline->y  = FixedToFloat(bsp->y);
	divline->dx = FixedToFloat(bsp->dx);
	divline->dy = FixedToFloat(bsp->dy);
}

//Hurdler: implement a loading status
#ifdef HWR_LOADING_SCREEN
static size_t ls_count = 0;
static UINT8 ls_percent = 0;

static void loading_status(void)
{
	char s[16];
	int x, y;

	/*if (g_reloadinggamestate)
	{
		return;
	}*/

	I_OsPolling();
	CON_Drawer();
	snprintf(s, sizeof(s), "%d%%", (++ls_percent)<<1);

	x = BASEVIDWIDTH/2;
	y = BASEVIDHEIGHT/2;
	V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, levelfadecol); // Black background to match fade in effect
	//V_DrawPatchFill(srb2back); // SRB2 background, ehhh too bright.
	M_DrawTextBox(x-58, y-8, 13, 1);
	V_DrawString(x-50, y, V_YELLOWMAP, "Loading...");
	V_DrawRightAlignedString(x+50, y, V_YELLOWMAP, s);

	// Is this really necessary at this point..?
	V_DrawCenteredString(BASEVIDWIDTH/2, 40, V_YELLOWMAP, "OPENGL MODE IS INCOMPLETE AND SOME");
	V_DrawCenteredString(BASEVIDWIDTH/2, 50, V_YELLOWMAP, "VISUAL FEATURES ARE MISSING/BROKEN.");
	V_DrawCenteredString(BASEVIDWIDTH/2, 70, V_YELLOWMAP, "USE AT SONIC'S RISK.");

	I_UpdateNoVsync();
}
#endif

// poly : the convex polygon that encloses all child subsectors
static void WalkBSPNode(INT32 bspnum, poly_t *poly, UINT16 *leafnode, fixed_t *bbox)
{
	node_t *bsp;
	poly_t *backpoly, *frontpoly;
	fdivline_t fdivline;
	polyvertex_t *pt;
	INT32 i;

	// Found a subsector?
	if (bspnum & NF_SUBSECTOR)
	{
		if (bspnum == -1)
		{
			// BP: i think this code is useless and wrong because
			// - bspnum==-1 happens only when numsubsectors == 0
			// - it can't happens in bsp recursive call since bspnum is a INT32 and children is UINT16
			// - the BSP is complet !! (there just can have subsector without segs) (i am not sure of this point)

			// do we have a valid polygon ?
			if (poly && poly->numpts > 2)
			{
				CONS_Debug(DBG_RENDER, "Adding a new subsector\n");

				if (num_extra_subsector == numsubsectors + NUM_EXTRA_SUBSECTORS)
					I_Error("WalkBSPNode: not enough num_extra_subsectors\n");
				else if (num_extra_subsector > 0x7fff)
					I_Error("WalkBSPNode: num_extra_subsector > 0x7fff\n");

				*leafnode = (UINT16)((UINT16)num_extra_subsector | NF_SUBSECTOR);
				extrasubsectors[num_extra_subsector].planepoly = poly;
				num_extra_subsector++;
			}

			//add subsectors without segs here?
			//HWR_SubsecPoly(0, NULL);
		}
		else
		{
			HWR_SubsecPoly(bspnum & ~NF_SUBSECTOR, poly);

#ifdef HWR_LOADING_SCREEN
			//Hurdler: implement a loading status
			if (ls_count-- <= 0)
			{
				ls_count = numsubsectors/50;
				loading_status();
			}
#endif
		}

		M_ClearBox(bbox);
		poly = extrasubsectors[bspnum & ~NF_SUBSECTOR].planepoly;

		for (i = 0, pt = poly->pts; i < poly->numpts; i++,pt++)
			M_AddToBox(bbox, FLOAT_TO_FIXED(pt->x), FLOAT_TO_FIXED(pt->y));

		return;
	}

	bsp = &nodes[bspnum];
	SetDivline(bsp, &fdivline);
	SplitPoly(&fdivline, poly, &frontpoly, &backpoly);
	poly = NULL;

#ifdef DEBUG_HWBSP
	//debug
	if (!backpoly)
		nobackpoly++;
#endif

	// Recursively divide front space.
	if (LIKELY(frontpoly))
	{
		WalkBSPNode(bsp->children[0], frontpoly, &bsp->children[0],bsp->bbox[0]);

		// copy child bbox
		memcpy(bbox, bsp->bbox[0], 4*sizeof (fixed_t));
	}
	else
		I_Error("WalkBSPNode: no front poly?");

	// Recursively divide back space.
	if (backpoly)
	{
		// Correct back bbox to include floor/ceiling convex polygon
		WalkBSPNode(bsp->children[1], backpoly, &bsp->children[1], bsp->bbox[1]);

		// enlarge bbox with second child
		M_AddToBox(bbox, bsp->bbox[1][BOXLEFT  ],
		                 bsp->bbox[1][BOXTOP   ]);
		M_AddToBox(bbox, bsp->bbox[1][BOXRIGHT ],
		                 bsp->bbox[1][BOXBOTTOM]);
	}
}

// FIXME: use Z_Malloc() STATIC ?
void HWR_FreeExtraSubsectors(void)
{
	if (extrasubsectors)
		free(extrasubsectors);
	extrasubsectors = NULL;
}

#define MAXDIST (1.5f)
// BP: can't move vertex: DON'T change polygon geometry! (convex)
//#define MOVEVERTEX

// Is vertex va  within the seg v1, v2
static boolean PointInSeg(polyvertex_t *va, polyvertex_t *v1, polyvertex_t *v2)
{
	register float ax, ay, bx, by, cx, cy, d, norm;

	// check bbox of the seg first (without altering v1, v2)
	if (v2->x > v1->x)
	{
		// check if x within seg box  v1..v2
		if ((va->x + MAXDIST) < v1->x) return false;
		if ((va->x - MAXDIST) > v2->x) return false;
	}
	else
	{
		// check if x within seg box  v2..v1
		if ((va->x + MAXDIST) < v2->x) return false;
		if ((va->x - MAXDIST) > v1->x) return false;
	}

	if (v2->y > v1->y)
	{
		// check if x within seg box  v1..v2
		if ((va->y + MAXDIST) < v1->y) return false;
		if ((va->y - MAXDIST) > v2->y) return false;
	}
	else
	{
		// check if x within seg box  v2..v1
		if ((va->y + MAXDIST) < v2->y) return false;
		if ((va->y - MAXDIST) > v1->y) return false;
	}

	// v1 = origin
	ax = v2->x-v1->x;
	ay = v2->y-v1->y;
	norm = hypotf(ax, ay); // length of seg

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
	if (norm != 0) // yes, this can be exactly 0
#pragma GCC diagnostic pop
	{
		ax /= norm;
		ay /= norm; // unit vector along seg, v1->v2
	}

	bx = va->x - v1->x;
	by = va->y - v1->y;  // vector v1->va

	// d = (a DOT b),  (product of lengths * cosine( angle ))
	d = ax*bx+ay*by;

	// bound of the seg
	if (d < 0 || d > norm)
	{
		// Also excludes some va within MAXDIST of v1 or v2
		return false;
	}

	// Cross product.
	if (((by * ax) - (bx * ay)) <= 0)
	{
		// The vertex is to the rightside of the seg, so adding
		// it to the polygon would worsen the crack.
		return false;
	}

	// measure the error in vector bx,by as difference squared sum
	//c= (d * unit_vector_seg) - b
	cx = ax*d-bx;
	cy = ay*d-by;

#ifdef MOVEVERTEX
	if (cx*cx+cy*cy <= MAXDIST*MAXDIST)
	{
		// ajust a little the point position
		va->x = ax*d+v1->x;
		va->y = ay*d+v1->y;
		// anyway the correction is not enouth
		return true;
	}
	return false;
#else
	return cx*cx+cy*cy <= MAXDIST*MAXDIST;
#endif
}

static INT32 numsplitpoly;

static void SearchSegInBSP(INT32 bspnum, polyvertex_t *p, poly_t *poly)
{
	poly_t  *q;
	INT32 j, k;

	if (bspnum & NF_SUBSECTOR)
	{
		if (bspnum != -1)
		{
			bspnum &= ~NF_SUBSECTOR;
			q = extrasubsectors[bspnum].planepoly;

			if (poly == q || !q)
				return;

			const INT32 numpts = q->numpts;

			for (j = 0; j < numpts; j++)
			{
				k = j+1;
				if (k == numpts)
					k = 0;

				if (!SameVertex(p, &q->pts[j]) &&
					!SameVertex(p, &q->pts[k]) &&
					 PointInSeg(p, &q->pts[j], &q->pts[k]))
				{
					INT32 n;
					poly_t *newpoly = HWR_AllocPoly(numpts+1);

					for (n = 0; n <= j; n++)
						newpoly->pts[n] = q->pts[n];

					newpoly->pts[k] = *p;

					for (n = k+1; n < newpoly->numpts; n++)
						newpoly->pts[n] = q->pts[n-1];

					numsplitpoly++;
					extrasubsectors[bspnum].planepoly = newpoly;
					HWR_FreePoly(q);
					return;
				}
			}
		}

		return;
	}

	// Not a subsector, visit left and right children.
	if ((FixedToFloat(nodes[bspnum].bbox[0][BOXBOTTOM])-MAXDIST <= p->y) &&
		(FixedToFloat(nodes[bspnum].bbox[0][BOXTOP   ])+MAXDIST >= p->y) &&
		(FixedToFloat(nodes[bspnum].bbox[0][BOXLEFT  ])-MAXDIST <= p->x) &&
		(FixedToFloat(nodes[bspnum].bbox[0][BOXRIGHT ])+MAXDIST >= p->x))
		SearchSegInBSP(nodes[bspnum].children[0], p, poly);

	if ((FixedToFloat(nodes[bspnum].bbox[1][BOXBOTTOM])-MAXDIST <= p->y) &&
		(FixedToFloat(nodes[bspnum].bbox[1][BOXTOP   ])+MAXDIST >= p->y) &&
		(FixedToFloat(nodes[bspnum].bbox[1][BOXLEFT  ])-MAXDIST <= p->x) &&
		(FixedToFloat(nodes[bspnum].bbox[1][BOXRIGHT ])+MAXDIST >= p->x))
		SearchSegInBSP(nodes[bspnum].children[1], p, poly);
}

// search for T-intersection problem
// BP : It can be mush more faster doing this at the same time of the splitpoly
// but we must use a different structure : polygone pointing on segs
// segs pointing on polygone and on vertex (too mush complicated, well not
// realy but i am soo lasy), the methode discibed is also better for segs presition
static void SolveTProblem(void)
{
	poly_t *p;
	INT32 i, numpts;
	size_t ssnum;

	if (!cv_glsolvetjoin.value)
		return;

	CONS_Debug(DBG_RENDER, "Solving T-joins. This may take a while. Please wait...\n");

#ifdef HWR_LOADING_SCREEN
	CON_Drawer(); //let the user know what we are doing
	I_FinishUpdate(); // page flip or blit buffer
#endif

	numsplitpoly = 0;

	for (ssnum = 0; ssnum < num_extra_subsector; ssnum++)
	{
		p = extrasubsectors[ssnum].planepoly;

		if (!p || p->numpts == 0)
			continue;

		numpts = p->numpts;

		for (i = 0; i < numpts; i++)
			SearchSegInBSP((INT32)numnodes-1, &p->pts[i], p);
	}

#ifdef DEBUG_HWBSP
	CONS_Debug(DBG_RENDER, "numsplitpoly %d\n", numsplitpoly);
	CONS_Debug(DBG_RENDER, "%d point divides a polygon line\n", numsplitpoly);
#endif
}

// call this routine after the BSP of a Doom wad file is loaded,
// and it will generate all the convex polys for the hardware renderer
void HWR_CreatePlanePolygons(INT32 bspnum)
{
	poly_t *rootp;
	polyvertex_t *rootpv;
	size_t i;
	fixed_t rootbbox[4];

	CONS_Debug(DBG_RENDER, "Creating polygons, please wait...\n");
#ifdef HWR_LOADING_SCREEN
	ls_count = ls_percent = 0; // reset the loading status
	CON_Drawer(); //let the user know what we are doing
	I_FinishUpdate(); // page flip or blit buffer
#endif

	// reset the portal and horizonline flag
	gl_maphasportals = gl_maphashorizonlines = false;

	// find min/max boundaries of map
#ifdef DEBUG_HWBSP
	CONS_Debug(DBG_RENDER, "Looking for boundaries of map...\n");
#endif
	M_ClearBox(rootbbox);
	for (i = 0; i < numvertexes; i++)
		M_AddToBox(rootbbox, vertexes[i].x, vertexes[i].y);

#ifdef DEBUG_HWBSP
	CONS_Debug(DBG_RENDER, "Generating subsector polygons... %d subsectors\n", numsubsectors);
#endif

	HWR_FreeExtraSubsectors();

	// allocate extra data for each subsector present in map
	num_alloc_extra_subsector = numsubsectors + NUM_EXTRA_SUBSECTORS;
	extrasubsectors = (extrasubsector_t*)calloc(num_alloc_extra_subsector, sizeof(*extrasubsectors));
	if (UNLIKELY(!extrasubsectors))
		I_Error("couldn't malloc extrasubsectors num_alloc_extra_subsector %s\n", sizeu1(num_alloc_extra_subsector));

	// number of the first new subsector that might be added
	num_extra_subsector = numsubsectors;

	// construct the initial convex poly that encloses the full map
	rootp = HWR_AllocPoly(4);
	rootpv = rootp->pts;

	// clockwise polygon
	rootpv->x = FixedToFloat(rootbbox[BOXLEFT  ]);
	rootpv->y = FixedToFloat(rootbbox[BOXBOTTOM]);  //lr
	rootpv++;
	rootpv->x = FixedToFloat(rootbbox[BOXLEFT  ]);
	rootpv->y = FixedToFloat(rootbbox[BOXTOP   ]);  //ur
	rootpv++;
	rootpv->x = FixedToFloat(rootbbox[BOXRIGHT ]);
	rootpv->y = FixedToFloat(rootbbox[BOXTOP   ]);  //ul
	rootpv++;
	rootpv->x = FixedToFloat(rootbbox[BOXRIGHT ]);
	rootpv->y = FixedToFloat(rootbbox[BOXBOTTOM]);  //ll
	rootpv++;

	WalkBSPNode(bspnum, rootp, NULL, rootbbox);

	SolveTProblem();

#ifdef DEBUG_HWBSP
	//debug debug..
	if (nobackpoly)
	    CONS_Debug(DBG_RENDER, "no back polygon %u times\n",nobackpoly);
	//"(should happen only with the deep water trick)"
	if (skipcut)
	    CONS_Debug(DBG_RENDER, "%u cuts were skipped because of only one point\n",skipcut);

	CONS_Debug(DBG_RENDER, "done: %u total subsector convex polygons\n", totalsubsecpolys);
#endif
}

#endif //HWRENDER
