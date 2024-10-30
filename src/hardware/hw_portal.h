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
/// \file  hw_portal.h
/// \brief OpenGL renderer portal struct, functions, linked list extern.

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
	GLPORTAL_OFF,
	GLPORTAL_SEARCH,
	GLPORTAL_STENCIL,
	GLPORTAL_DEPTH,
	GLPORTAL_INSIDE,
};

typedef struct gl_portal_s
{
	struct gl_portal_s *next;

	// Viewport.
	fixed_t viewx;
	fixed_t viewy;
	fixed_t viewz;
	angle_t viewangle;

	seg_t *seg; 		// seg that is used for drawing to the stencil buffer
	INT32 clipline;		// Optional clipline for line-based portals

	// angles for the left and right edges of the portal
	// relative to the viewpoint
	angle_t angle1;
	angle_t angle2;
} gl_portal_t;

typedef struct gl_portallist_s
{
	gl_portal_t *base;
	gl_portal_t *cap;
} gl_portallist_t;

extern SINT8 gl_portal_state;

// new thing
extern gl_portallist_t *currentportallist;

static inline void HWR_SetPortalState (SINT8 state)
{
	gl_portal_state = state;
}

// Adds an entry to the clipper for portal rendering
void HWR_PortalClipping(gl_portal_t *portal);

void HWR_Portal_Add2Lines(const INT32 line1, const INT32 line2, seg_t *seg);
void HWR_PortalFrame(gl_portal_t* portal);
void HWR_RenderPortal(gl_portal_t* portal, gl_portal_t* rootportal, const float fpov, player_t *player, int stencil_level);
void HWR_FreePortalList(gl_portallist_t portallist);

boolean HWR_PortalCheckPointSide(fixed_t x, fixed_t y);
boolean HWR_PortalCheckBBox(const fixed_t *bspcoord);

#endif
#endif
