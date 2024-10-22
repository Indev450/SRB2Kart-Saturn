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
/// \file  d_main.h
/// \brief game startup, and main loop code, system specific interface stuff.

#ifndef __D_MAIN__
#define __D_MAIN__

#include "d_event.h"
#include "w_wad.h"   // for MAX_WADFILES

// make sure not to write back the config until it's been correctly loaded
extern tic_t rendergametic;

extern boolean loaded_config;

extern char srb2home[256]; //Alam: My Home
extern boolean usehome; //Alam: which path?
extern const char *pandf; //Alam: how to path?
extern char srb2path[256]; //Alam: SRB2's Home

extern boolean found_extra_kart; // for use in k_kart.c
extern boolean found_extra2_kart; // for use in k_kart.c
extern boolean found_extra3_kart; // for use in k_kart.c

extern boolean xtra_speedo; // extra speedometer check
extern boolean xtra_speedo_clr; // extra speedometer colour check
extern boolean xtra_speedo3; // 80x11 extra speedometer check
extern boolean xtra_speedo_clr3; // 80x11 extra speedometer colour check
extern boolean achi_speedo; // achiiro speedometer check
extern boolean achi_speedo_clr; // extra speedometer colour check
extern boolean clr_hud; // colour hud check
extern boolean big_lap; // bigger lap counter
extern boolean big_lap_color; // bigger lap counter but colour
extern boolean kartzspeedo; // kartZ speedo
extern boolean statdp; // stat display for extended player setup
extern boolean nametaggfx; // Nametag stuffs
extern boolean driftgaugegfx;

// autoload stuff
extern boolean autoloading;
extern boolean autoloaded;
extern boolean postautoloaded;

void D_AddPostloadFiles(void);

extern char *autoloadwadfilespost[MAX_WADFILES];
extern char *autoloadwadfiles[MAX_WADFILES];

// the infinite loop of D_SRB2Loop() called from win_main for windows version
void D_SRB2Loop(void) FUNCNORETURN;

//
// D_SRB2Main()
// Not a globally visible function, just included for source reference,
// calls all startup code, parses command line options.
//
void D_SRB2Main(void);

// Called by IO functions when input is detected.
void D_PostEvent(const event_t *ev);

void D_ProcessEvents(void);

void D_CleanFile(char **filearray);

const char *D_Home(void);

//
// BASE LEVEL
//
void D_StartTitle(void);

#endif //__D_MAIN__
