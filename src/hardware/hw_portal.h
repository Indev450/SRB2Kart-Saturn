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
/// \file  r_portal.h
/// \brief Software renderer portal struct, functions, linked list extern.

#ifndef __HW_PORTAL__
#define __HW_PORTAL__

#ifdef HWRENDER

#include "../doomstat.h"
#include "../r_data.h"
#include "../r_bsp.h"

/** Portal structure.
 */
enum
{
	GRPORTAL_OFF,
	GRPORTAL_SEARCH,
	GRPORTAL_STENCIL,
	GRPORTAL_DEPTH,
	GRPORTAL_INSIDE,
};

extern SINT8 gr_portal;

typedef struct gl_portal_s
{
	struct gl_portal_s *next;

	// Viewport.
	fixed_t viewx;
	fixed_t viewy;
	fixed_t viewz;
	angle_t viewangle;

	UINT8 pass;			/**< Keeps track of the portal's recursion depth. */
	INT32 startline;
	INT32 clipline;		/**< Optional clipline for line-based portals. */
	INT32 drawcount;	/**< For OpenGL. */

	seg_t *seg;
} gl_portal_t;

typedef struct gl_portallist_s
{
	gl_portal_t *base;
	gl_portal_t *cap;
} gl_portallist_t;

// new thing
extern gl_portallist_t *currentportallist;

// Adds an entry to the clipper for portal rendering
void HWR_PortalClipping(gl_portal_t *portal);

void HWR_Portal_Add2Lines(const INT32 line1, const INT32 line2, seg_t *seg);
void HWR_PortalFrame(gl_portal_t* portal);
void HWR_RenderPortalSeg(gl_portal_t* portal, SINT8 state);
void HWR_RenderPortal(gl_portal_t* portal, gl_portal_t* rootportal, const float fpov, player_t *player, int stencil_level);
void HWR_FreePortalList(gl_portallist_t portallist);

boolean HWR_PortalCheckPointSide(fixed_t x, fixed_t y);
boolean HWR_PortalCheckBBox(const fixed_t *bspcoord);

#endif
#endif
