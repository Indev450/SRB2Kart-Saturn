// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2022 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_time.c
/// \brief Timing for the system layer.

#include "i_time.h"

#include <math.h>

#include "command.h"
#include "doomtype.h"
#include "m_fixed.h"
#include "i_system.h"

static CV_PossibleValue_t timescale_cons_t[] = {{FRACUNIT/20, "MIN"}, {20*FRACUNIT, "MAX"}, {0, NULL}};
consvar_t cv_timescale = {"timescale", "1.0", CV_NETVAR|CV_CHEAT|CV_FLOAT, timescale_cons_t, NULL, FRACUNIT, NULL, NULL, 0, 0, NULL};

static precise_t baseprecise;
static precise_t oldenterprecise;

static tic_t g_time;

static fixed_t I_GetTimeScale(void)
{
	return cv_timescale.value;
}

// get the current time that has passed since gamestart
tic_t I_GetTime(void)
{
	const double ticratescaled = (double)TICRATE * FixedToDouble(I_GetTimeScale());

	// stoopid gcc
	const precise_t elapsed = I_GetPreciseTime() - baseprecise;
	const UINT64 precision = I_GetPrecisePrecision();

	// explictily do this in double precision
	const double totaltime = (double)elapsed / (double)precision;
	const double timeinticks = totaltime * ticratescaled;

	return (tic_t)(timeinticks);
}

tic_t I_GetGlobalTime(void)
{
	return g_time;
}

fixed_t I_GetTimeFrac(void)
{
	const double ticratescaled = (double)TICRATE * FixedToDouble(I_GetTimeScale());

	// stoopid gcc
	const precise_t elapsed = I_GetPreciseTime() - baseprecise;
	const UINT64 precision = I_GetPrecisePrecision();

	// explictily do this in double precision
	const double totaltime = (double)elapsed / (double)precision;
	const double timeinticks = totaltime * ticratescaled;

	double integral;
	const double fractional = modf(timeinticks, &integral);

	fixed_t outfrac = DoubleToFixed(fractional);
	outfrac = CLAMP(outfrac, 0, FRACUNIT);

	if (outfrac > FRACUNIT)
		outfrac = FRACUNIT;

	return outfrac;
}

void I_InitializeTime(void)
{
	CV_RegisterVar(&cv_timescale);

	// I_StartupTimer is preserved for potential subsystems that need to setup
	// timing information for I_GetPreciseTime and sleeping
	I_StartupTimer();

	g_time = 0;

	baseprecise = I_GetPreciseTime();
	oldenterprecise = baseprecise;
}


// Used to track the time between two calls
// uhhh pretty much just for wipes and things like that
// probably would be better to not use a time global
// but i dont think it really matters too much in those cases
void I_UpdateTime(void)
{
	static tic_t oldentertics = 0;

	// get real tics
	const double ticratescaled = (double)TICRATE * FixedToDouble(I_GetTimeScale());

	const precise_t elapsed = I_GetPreciseTime() - baseprecise;
	const UINT64 precision = I_GetPrecisePrecision();
	const double totaltime = (double)elapsed / (double)precision;
	const tic_t entertic = (tic_t)(totaltime * ticratescaled);

	const tic_t realtics = entertic - oldentertics;
	oldentertics = entertic;

	// Update global time state
	g_time += realtics;
}
