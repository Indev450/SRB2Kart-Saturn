// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2004      by Stephen McGranahan
// Copyright (C) 2015-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  p_slopes.c
/// \brief ZDoom + Eternity Engine Slopes, ported and enhanced by Kalaron

#ifndef P_SLOPES_H__
#define P_SLOPES_H__

#include "doomtype.h"

void P_CalculateSlopeNormal(pslope_t *slope);
void P_ResetDynamicSlopes(void);
void P_RunDynamicSlopes(void);

// P_SpawnSlope_Line
// Creates one or more slopes based on the given line type and front/back
// sectors.
void P_SpawnSlope_Line(int linenum);

//
// P_CopySectorSlope
//
// Searches through tagged sectors and copies
//
void P_CopySectorSlope(line_t *line);

pslope_t *P_SlopeById(UINT16 id);

//
// P_GetZAt
//

// Returns the height of the sloped plane at (x, y) as a fixed_t
FUNCINLINE static ATTRINLINE fixed_t P_GetZAt(pslope_t *z_slope, fixed_t x, fixed_t y)
{
	return z_slope->o.z + FixedMul((FixedMul(x - z_slope->o.x, z_slope->d.x) + FixedMul(y - z_slope->o.y, z_slope->d.y)), z_slope->zdelta);
}

// Returns the height of the sector floor at (x, y)
FUNCINLINE static ATTRINLINE fixed_t P_GetSectorFloorZAt(const sector_t *z_sector, fixed_t x, fixed_t y)
{
	return z_sector->f_slope ? P_GetZAt(z_sector->f_slope, x, y) : z_sector->floorheight;
}

// Returns the height of the sector ceiling at (x, y)
FUNCINLINE static ATTRINLINE fixed_t P_GetSectorCeilingZAt(const sector_t *z_sector, fixed_t x, fixed_t y)
{
	return z_sector->c_slope ? P_GetZAt(z_sector->c_slope, x, y) : z_sector->ceilingheight;
}

// Returns the height of the FOF top at (x, y)
FUNCINLINE static ATTRINLINE fixed_t P_GetFFloorTopZAt(const ffloor_t *z_ffloor, fixed_t x, fixed_t y)
{
	return *z_ffloor->t_slope ? P_GetZAt(*z_ffloor->t_slope, x, y) : *z_ffloor->topheight;
}

// Returns the height of the FOF bottom  at (x, y)
FUNCINLINE static ATTRINLINE fixed_t P_GetFFloorBottomZAt(const ffloor_t *z_ffloor, fixed_t x, fixed_t y)
{
	return *z_ffloor->b_slope ? P_GetZAt(*z_ffloor->b_slope, x, y) : *z_ffloor->bottomheight;
}

// Returns the height of the light list at (x, y)
FUNCINLINE static ATTRINLINE fixed_t P_GetLightZAt(const lightlist_t *z_light, fixed_t x, fixed_t y)
{
	return z_light->slope ? P_GetZAt(z_light->slope, x, y) : z_light->height;
}

// Lots of physics-based bullshit
void P_QuantizeMomentumToSlope(vector3_t *momentum, pslope_t *slope);
void P_ReverseQuantizeMomentumToSlope(vector3_t *momentum, pslope_t *slope);
void P_SlopeLaunch(mobj_t *mo);
void P_HandleSlopeLanding(mobj_t *thing, pslope_t *slope);
void P_ButteredSlope(mobj_t *mo);


// EOF
#endif // #ifdef ESLOPE
