// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 2024 by Vivian "toastergrl" Grannell.
// Copyright (C) 2024 by Kart Krew.
// Copyright (C) 2020 by Sonic Team Junior.
// Copyright (C) 2000 by DooM Legacy Team.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  am_map.h
/// \brief Code for the 'automap', former Doom feature used for DEVMODE testing

#ifndef __AMMAP_H__
#define __AMMAP_H__

#include "d_event.h"

typedef struct
{
	INT32 x, y;
} fpoint_t;

typedef struct
{
	fpoint_t a, b;
} fline_t;

extern boolean am_recalc; // true if screen size changes
extern boolean automapactive; // In AutoMap mode?

// Called by main loop.
boolean AM_Responder(event_t *ev);

// Called by main loop.
void AM_Ticker(void);

// Called by main loop, instead of view drawer if automap is active.
void AM_Drawer(void);

// Enables the automap.
void AM_Start(void);

// Called to force the automap to quit if the level is completed while it is up.
void AM_Stop(void);

typedef struct
{
	INT32 w, h;
	UINT8 *buf;
} minigen_t;

// Minimap generation
minigen_t *AM_MinimapGenerate(INT32 mul);

#endif
