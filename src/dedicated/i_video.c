// Emacs style mode select   -*- C++ -*-
// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
// Portions Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 2014-2018 by Sonic Team Junior.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//-----------------------------------------------------------------------------
/// \file
/// \brief SRB2 graphics stuff for dedicated

#include "../doomdef.h"
#include "../i_system.h"
#include "../i_video.h"
#include "../console.h"
#include "../command.h"
#include "../i_system.h"
#include "../screen.h"

rendermode_t rendermode = render_none;

// synchronize page flipping with screen refresh
consvar_t cv_vidwait = {"vid_wait", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t keyboardlayout_cons_t[] = {{1,"Default US"}, {2, "Native"}, {3, "AZERTY"}, {0, NULL}};
consvar_t cv_keyboardlayout = {"keyboardlayout", "Default US", CV_SAVE, keyboardlayout_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_alwaysgrabmouse = {"alwaysgrabmouse", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

INT32 numcontrollers = 0;

UINT8 graphics_started = 0; // Is used in console.c and screen.c

// To disable fullscreen at startup; is set in VID_PrepareModeList
boolean     allow_fullscreen = false;
UINT16      realwidth = BASEVIDWIDTH;
UINT16      realheight = BASEVIDHEIGHT;

extern boolean consolevent;

void I_GetConsoleEvents(void);

void I_GetEvent(void)
{
}

void I_StartupMouse(void)
{
}

//
// I_OsPolling
//
void I_OsPolling(void)
{
	if (consolevent)
		I_GetConsoleEvents();
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit(void)
{
}

//
// I_FinishUpdate
//
void I_FinishUpdate(void)
{
}

//
// I_UpdateNoVsync
//
void I_UpdateNoVsync(void)
{
}

//
// I_ReadScreen
//
void I_ReadScreen(UINT8 *scr)
{
	(void)scr;
}

//
// I_SetPalette
//
void I_SetPalette(RGBA_t *palette)
{
	(void)palette;
}

// return number of fullscreen + X11 modes
INT32 VID_NumModes(void)
{
	return 0;
}

const char *VID_GetModeName(INT32 modenum)
{
	(void)modenum;
	return NULL;
}

INT32 VID_GetModeForSize(INT32 w, INT32 h)
{
	(void)w;
	(void)h;
	return -1;
}

void VID_PrepareModeList(void)
{
}

INT32 VID_SetMode(INT32 modeNum)
{
	(void)modeNum;
	return true;
}

void I_StartupGraphics(void)
{
}

void I_ShutdownGraphics(void)
{
}

UINT32 I_GetRefreshRate(void)
{
	return 0;
}

boolean I_UseNativeKeyboard(void)
{
	return false;
}

void I_SetBorderlessWindow(void)
{

}
