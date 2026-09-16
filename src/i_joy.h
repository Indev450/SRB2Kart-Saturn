// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_joy.h
/// \brief share joystick information with game control code

#ifndef __I_JOY_H__
#define __I_JOY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "g_input.h"

/*!
  \brief	-JOYAXISRANGE to +JOYAXISRANGE for each axis

	(1024-1) so we can do a right shift instead of division
	(doesnt matter anyway, just give enough precision)
	a gamepad will return -1, 0, or 1 in the event data
	an analog type joystick will return a value
	from -JOYAXISRANGE to +JOYAXISRANGE for each axis
*/

#define JOYAXISRANGE 1023

// detect a bug if we increase JOYBUTTONS above DIJOYSTATE's number of buttons
#if (JOYBUTTONS > 64)
"JOYBUTTONS is greater than INT64 bits can hold"
#endif

#define DigitalGamepadStyle(i) (cv_joyscale[i].value == 0)

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __I_JOY_H__
