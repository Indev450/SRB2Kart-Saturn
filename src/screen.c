// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  screen.c
/// \brief Handles multiple resolutions, 8bpp/16bpp(highcolor) modes

#include "doomdef.h"
#include "doomstat.h"
#include "screen.h"
#include "console.h"
#include "am_map.h"
#include "i_time.h"
#include "i_system.h"
#include "i_video.h"
#include "r_local.h"
#include "r_sky.h"
#include "m_argv.h"
#include "m_misc.h"
#include "v_video.h"
#include "st_stuff.h"
#include "hu_stuff.h"
#include "z_zone.h"
#include "d_main.h"
#include "d_clisrv.h"
#include "f_finale.h"

// SRB2Kart
#include "r_fps.h" // R_GetFramerateCap

// --------------------------------------------
// assembly or c drawer routines for 8bpp/16bpp
// --------------------------------------------
void (*wallcolfunc)(void); // new wall column drawer to draw posts >128 high
void (*colfunc)(void); // standard column, up to 128 high posts

void (*basecolfunc)(void);
void (*fuzzcolfunc)(void); // standard fuzzy effect column drawer
void (*transcolfunc)(void); // translation column drawer
void (*shadecolfunc)(void); // smokie test..
void (*spanfunc)(void); // span drawer, use a 64x64 tile
void (*splatfunc)(void); // span drawer w/ transparency
void (*basespanfunc)(void); // default span func for color mode
void (*transtransfunc)(void); // translucent translated column drawer
void (*twosmultipatchfunc)(void); // for cols with transparent pixels
void (*twosmultipatchtransfunc)(void); // for cols with transparent pixels AND translucency

// ------------------
// global video state
// ------------------
viddef_t vid;
INT32 setmodeneeded; //video mode change needed if > 0 (the mode number to set + 1)

static CV_PossibleValue_t scr_depth_cons_t[] = {{8, "8 bits"}, {16, "16 bits"}, {24, "24 bits"}, {32, "32 bits"}, {0, NULL}};

static CV_PossibleValue_t shittyscreen_cons_t[] = {{0, "Okay"}, {1, "Shitty"}, {2, "Extra Shitty"}, {0, NULL}};

//added : 03-02-98: default screen mode, as loaded/saved in config
consvar_t cv_scr_width = {"scr_width", "1280", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_scr_height = {"scr_height", "800", CV_SAVE, CV_Unsigned, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_scr_depth = {"scr_depth", "16 bits", CV_SAVE, scr_depth_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_renderview = {"renderview", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_vhseffect = {"vhspause", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_shittyscreen = {"televisionsignal", "Okay", CV_NOSHOWHELP, shittyscreen_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static void Highreshudscale_OnChange(void);
static CV_PossibleValue_t highreshudscale_cons_t[] = {{4*FRACUNIT/5, "MIN"}, {6*FRACUNIT/5, "MAX"}, {0, NULL}};
consvar_t cv_highreshudscale = {"highreshudscale", "1", CV_SAVE|CV_FLOAT|CV_CALL|CV_NOINIT|CV_NOSHOWHELP, highreshudscale_cons_t, Highreshudscale_OnChange, 0, NULL, NULL, 0, 0, NULL};

static void Highreshudscale_OnChange(void)
{
	if (!con_startup)
		SCR_Recalc();
}

static void SCR_ChangeFullscreen (void);

consvar_t cv_fullscreen = {"fullscreen", "Yes", CV_SAVE|CV_CALL, CV_YesNo, SCR_ChangeFullscreen, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t accuratefps_cons_t[] = {{0, "Inaccurate"}, {1, "Accurate"}, {0, NULL}};
consvar_t cv_accuratefps = {"fpssampling", "1", CV_SAVE, accuratefps_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// =========================================================================
//                           SCREEN VARIABLES
// =========================================================================

INT32 scr_bpp; // current video mode bytes per pixel
UINT8 *scr_borderpatch; // flat used to fill the reduced view borders set at ST_Init()

// =========================================================================

//  Short and Tall sky drawer, for the current color mode
void (*walldrawerfunc)(void);

void SCR_SetMode(void)
{
	if (dedicated)
		return;

	if (!setmodeneeded || WipeInAction)
		return; // should never happen and don't change it during a wipe, BAD!

	VID_SetMode(--setmodeneeded);

	V_SetPalette(0);

	//
	//  setup the right draw routines for either 8bpp or 16bpp
	//
	if (true)//vid.bpp == 1) //Always run in 8bpp. todo: remove all 16bpp code?
	{
		spanfunc = basespanfunc = R_DrawSpan_8;
		splatfunc = R_DrawSplat_8;
		transcolfunc = R_DrawTranslatedColumn_8;
		transtransfunc = R_DrawTranslatedTranslucentColumn_8;

		colfunc = basecolfunc = R_DrawColumn_8;
		shadecolfunc = R_DrawShadeColumn_8;
		fuzzcolfunc = R_DrawTranslucentColumn_8;
		walldrawerfunc = R_DrawWallColumn_8;
		twosmultipatchfunc = R_Draw2sMultiPatchColumn_8;
		twosmultipatchtransfunc = R_Draw2sMultiPatchTranslucentColumn_8;
	}
/*	else if (vid.bpp > 1)
	{
		I_OutputMsg("using highcolor mode\n");
		spanfunc = basespanfunc = R_DrawSpan_16;
		transcolfunc = R_DrawTranslatedColumn_16;
		transtransfunc = R_DrawTranslucentColumn_16; // No 16bit operation for this function

		colfunc = basecolfunc = R_DrawColumn_16;
		shadecolfunc = NULL; // detect error if used somewhere..
		fuzzcolfunc = R_DrawTranslucentColumn_16;
		walldrawerfunc = R_DrawWallColumn_16;
	}*/
	else
		I_Error("unknown bytes per pixel mode %d\n", vid.bpp);
/*
	if (SCR_IsAspectCorrect(vid.width, vid.height))
		CONS_Alert(CONS_WARNING, M_GetText("Resolution is not aspect-correct!\nUse a multiple of %dx%d\n"), BASEVIDWIDTH, BASEVIDHEIGHT);
*/
	// set the apprpriate drawer for the sky (tall or INT16)
	setmodeneeded = 0;
}

// do some initial settings for the game loading screen
//
void SCR_Startup(void)
{
	if (dedicated)
	{
		V_Init();
		V_SetPalette(0);
		return;
	}

	vid.modenum = 0;

	vid.dupx = vid.width / BASEVIDWIDTH;
	vid.dupy = vid.height / BASEVIDHEIGHT;
	vid.dupx = vid.dupy = (vid.dupx < vid.dupy ? vid.dupx : vid.dupy);
	vid.fdupx = FixedDiv(vid.width*FRACUNIT, BASEVIDWIDTH*FRACUNIT);
	vid.fdupy = FixedDiv(vid.height*FRACUNIT, BASEVIDHEIGHT*FRACUNIT);

//#ifdef HWRENDER
	//if (rendermode != render_opengl && rendermode != render_none) // This was just placing it incorrectly at non aspect correct resolutions in opengl
//#endif
		vid.fdupx = vid.fdupy = (vid.fdupx < vid.fdupy ? vid.fdupx : vid.fdupy);

	vid.meddupx = (UINT8)(vid.dupx >> 1) + 1;
	vid.meddupy = (UINT8)(vid.dupy >> 1) + 1;
#ifdef HWRENDER
	vid.fmeddupx = vid.meddupx*FRACUNIT;
	vid.fmeddupy = vid.meddupy*FRACUNIT;
#endif

	vid.smalldupx = (UINT8)(vid.dupx / 3) + 1;
	vid.smalldupy = (UINT8)(vid.dupy / 3) + 1;
#ifdef HWRENDER
	vid.fsmalldupx = vid.smalldupx*FRACUNIT;
	vid.fsmalldupy = vid.smalldupy*FRACUNIT;
#endif

	vid.baseratio = FRACUNIT;

	V_Init();
	CV_RegisterVar(&cv_highreshudscale);
	CV_RegisterVar(&cv_ticrate);
	CV_RegisterVar(&cv_accuratefps);
	CV_RegisterVar(&cv_menucaps);
	CV_RegisterVar(&cv_constextsize);

	CV_RegisterVar(&cv_globalgamma);
	CV_RegisterVar(&cv_globalsaturation);

	CV_RegisterVar(&cv_rhue);
	CV_RegisterVar(&cv_yhue);
	CV_RegisterVar(&cv_ghue);
	CV_RegisterVar(&cv_chue);
	CV_RegisterVar(&cv_bhue);
	CV_RegisterVar(&cv_mhue);

	CV_RegisterVar(&cv_rgamma);
	CV_RegisterVar(&cv_ygamma);
	CV_RegisterVar(&cv_ggamma);
	CV_RegisterVar(&cv_cgamma);
	CV_RegisterVar(&cv_bgamma);
	CV_RegisterVar(&cv_mgamma);

	CV_RegisterVar(&cv_rsaturation);
	CV_RegisterVar(&cv_ysaturation);
	CV_RegisterVar(&cv_gsaturation);
	CV_RegisterVar(&cv_csaturation);
	CV_RegisterVar(&cv_bsaturation);
	CV_RegisterVar(&cv_msaturation);

	V_SetPalette(0);
}

// Called at new frame, if the video mode has changed
//
void SCR_Recalc(void)
{
	if (dedicated)
		return;

	// bytes per pixel quick access
	scr_bpp = vid.bpp;

	// scale 1,2,3 times in x and y the patches for the menus and overlays...
	// calculated once and for all, used by routines in v_video.c
	vid.dupx = vid.width / BASEVIDWIDTH;
	vid.dupy = vid.height / BASEVIDHEIGHT;
	vid.dupx = vid.dupy = (vid.dupx < vid.dupy ? vid.dupx : vid.dupy);
	vid.fdupx = FixedDiv(vid.width*FRACUNIT, BASEVIDWIDTH*FRACUNIT);
	vid.fdupy = FixedDiv(vid.height*FRACUNIT, BASEVIDHEIGHT*FRACUNIT);

	if ((vid.width > 720) && (vid.height > 1280)) // ehhhh well this thing has so many issues, so ill lock it to higher resolutions instead
	{
		vid.dupx = FixedDiv(vid.dupx, cv_highreshudscale.value);
		vid.dupy = FixedDiv(vid.dupy, cv_highreshudscale.value);
		vid.fdupx = FixedDiv(vid.fdupx, cv_highreshudscale.value);
		vid.fdupy = FixedDiv(vid.fdupy, cv_highreshudscale.value);
	}

#ifdef HWRENDER
	//if (rendermode != render_opengl && rendermode != render_none) // This was just placing it incorrectly at non aspect correct resolutions in opengl
	// 13/11/18:
	// The above is no longer necessary, since we want OpenGL to be just like software now
	// -- Monster Iestyn
#endif
		vid.fdupx = vid.fdupy = (vid.fdupx < vid.fdupy ? vid.fdupx : vid.fdupy);

	//vid.baseratio = FixedDiv(vid.height << FRACBITS, BASEVIDHEIGHT << FRACBITS);
	vid.baseratio = FRACUNIT;

	vid.meddupx = (UINT8)(vid.dupx >> 1) + 1;
	vid.meddupy = (UINT8)(vid.dupy >> 1) + 1;
#ifdef HWRENDER
	vid.fmeddupx = vid.meddupx*FRACUNIT;
	vid.fmeddupy = vid.meddupy*FRACUNIT;
#endif

	vid.smalldupx = (UINT8)(vid.dupx / 3) + 1;
	vid.smalldupy = (UINT8)(vid.dupy / 3) + 1;
#ifdef HWRENDER
	vid.fsmalldupx = vid.smalldupx*FRACUNIT;
	vid.fsmalldupy = vid.smalldupy*FRACUNIT;
#endif

	// toggle off automap because some screensize-dependent values will
	// be calculated next time the automap is activated.
	if (automapactive)
		AM_Stop();

	// set the screen[x] ptrs on the new vidbuffers
	V_Init();

	// scr_viewsize doesn't change, neither detailLevel, but the pixels
	// per screenblock is different now, since we've changed resolution.
	R_SetViewSize(); //just set setsizeneeded true now ..

	// vid.recalc lasts only for the next refresh...
	con_recalc = true;
	am_recalc = true;
}

// Check for screen cmd-line parms: to force a resolution.
//
// Set the video mode to set at the 1st display loop (setmodeneeded)
//

void SCR_CheckDefaultMode(void)
{
	INT32 scr_forcex, scr_forcey; // resolution asked from the cmd-line

	if (dedicated)
		return;

	// 0 means not set at the cmd-line
	scr_forcex = scr_forcey = 0;

	if (M_CheckParm("-width") && M_IsNextParm())
		scr_forcex = atoi(M_GetNextParm());

	if (M_CheckParm("-height") && M_IsNextParm())
		scr_forcey = atoi(M_GetNextParm());

	if (scr_forcex && scr_forcey)
	{
		CONS_Printf(M_GetText("Using resolution: %d x %d\n"), scr_forcex, scr_forcey);
		// returns -1 if not found, thus will be 0 (no mode change) if not found
		setmodeneeded = VID_GetModeForSize(scr_forcex, scr_forcey) + 1;
	}
	else
	{
		CONS_Printf(M_GetText("Default resolution: %d x %d (%d bits)\n"), cv_scr_width.value,
			cv_scr_height.value, cv_scr_depth.value);
		// see note above
		setmodeneeded = VID_GetModeForSize(cv_scr_width.value, cv_scr_height.value) + 1;
	}
}

// sets the modenum as the new default video mode to be saved in the config file
void SCR_SetDefaultMode(void)
{
	// remember the default screen size
	CV_SetValue(&cv_scr_width, vid.width);
	CV_SetValue(&cv_scr_height, vid.height);
	//CV_SetValue(&cv_scr_depth, vid.bpp*8);
}

// Change fullscreen on/off according to cv_fullscreen
void SCR_ChangeFullscreen(void)
{
#ifdef DIRECTFULLSCREEN
	// allow_fullscreen is set by VID_PrepareModeList
	// it is used to prevent switching to fullscreen during startup
	if (!allow_fullscreen)
		return;

	if (graphics_started)
	{
		VID_PrepareModeList();
		setmodeneeded = VID_GetModeForSize(vid.width, vid.height) + 1;
	}
	return;
#endif
}

boolean SCR_IsAspectCorrect(INT32 width, INT32 height)
{
	return (width % BASEVIDWIDTH == 0 && height % BASEVIDHEIGHT == 0 && width / BASEVIDWIDTH == height / BASEVIDHEIGHT);
}

double averageFPS = 0.0f;

#define USE_FPS_SAMPLES

#ifdef USE_FPS_SAMPLES
#define MAX_FRAME_TIME (0.05)
#define NUM_FPS_SAMPLES (16) // Number of samples to store

static double total_frame_time = 0.0;
static int frame_index;

static double fps_samples[NUM_FPS_SAMPLES];
#endif

static boolean fps_init = false;
static precise_t fps_enter = 0;

void SCR_CalculateFPS(void)
{
	precise_t fps_finish = 0;

	double frameElapsed = 0.0;

	if (fps_init == false)
	{
		fps_enter = I_GetPreciseTime();
		fps_init = true;
	}

	fps_finish = I_GetPreciseTime();
	frameElapsed = (double)((INT64)(fps_finish - fps_enter)) / I_GetPrecisePrecision();
	fps_enter = fps_finish;

#ifdef USE_FPS_SAMPLES
	total_frame_time += frameElapsed;

	if (cv_accuratefps.value)
	{
		if (frame_index++ >= NUM_FPS_SAMPLES || total_frame_time >= MAX_FRAME_TIME)
		{
			averageFPS = 1.0 / (total_frame_time / frame_index);
			total_frame_time = 0.0;
			frame_index = 0;
		}
	}
	else
	{
		if (total_frame_time >= MAX_FRAME_TIME)
		{
			static int sampleIndex = 0;

			fps_samples[sampleIndex] = frameElapsed;

			sampleIndex++;
			if (sampleIndex >= NUM_FPS_SAMPLES)
				sampleIndex = 0;

			averageFPS = 0.0;
			for (int i = 0; i < NUM_FPS_SAMPLES; i++)
			{
				averageFPS += fps_samples[i];
			}

			if (averageFPS > 0.0)
			{
				averageFPS = 1.0 / (averageFPS / NUM_FPS_SAMPLES);
			}
		}

		while (total_frame_time >= MAX_FRAME_TIME)
		{
			total_frame_time -= MAX_FRAME_TIME;
		}
	}
#else
	// Direct, unsampled counter.
	averageFPS = 1.0 / frameElapsed;
#endif
}

void SCR_DisplayTicRate(void)
{
	const UINT8 *ticcntcolor = NULL;
	UINT32 cap = R_GetFramerateCap();
	UINT32 benchmark = (cap == 0) ? I_GetRefreshRate() : cap;
	INT32 x = 318;
	double fps = round(averageFPS);
	INT32 fpsflags = V_LocalTransFlag()|V_SNAPTOBOTTOM|V_SNAPTORIGHT;
	const char *fps_string;
	
	INT32 ticcntcolor2 = 0;
	
	if (gamestate == GS_NULL)
		return;

	// new kart counter
	if (cv_ticrate.value == 1 || cv_ticrate.value == 2)
	{
		// draw "FPS"
		if (cv_ticrate.value == 1)
			V_DrawFixedPatch(306<<FRACBITS, 183<<FRACBITS, FRACUNIT, fpsflags, framecounter, R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_YELLOW, GTC_CACHE));
			
		if (fps > (benchmark - 5))
			ticcntcolor = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_MINT, GTC_CACHE);
		else if (fps < 20)
			ticcntcolor = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_RASPBERRY, GTC_CACHE);

		if (cap != 0)
		{
			UINT32 digits = 1;
			UINT32 c2 = cap;

			while (c2 > 0)
			{
				c2 = c2 / 10;
				digits++;
			}

			// draw total frame:
			V_DrawPingNum(x, 190, fpsflags, cap, ticcntcolor);
			
			x -= digits * 4;

			// draw "/"	
			V_DrawFixedPatch(x<<FRACBITS, 190<<FRACBITS, FRACUNIT, fpsflags, frameslash, ticcntcolor);
		}

		// draw our actual framerate
		V_DrawPingNum(x, 190, fpsflags, fps, ticcntcolor);
	}
	else if (cv_ticrate.value == 3 || cv_ticrate.value == 4) // kart v1.0/srb2 counter
	{
		if (fps > (benchmark - 5))
			ticcntcolor2 = V_GREENMAP;
		else if (fps < 20)
			ticcntcolor2 = V_REDMAP;

		if (cap != 0)
			fps_string = va("%d/%d\x82", (INT32)fps, cap);
		else
			fps_string = va("%d\x82", (INT32)fps);
	
		if (cv_ticrate.value == 3)
			V_DrawRightAlignedString(319, 181, V_YELLOWMAP|fpsflags, "FPS");
			
		V_DrawRightAlignedString(319, 190, ticcntcolor2|fpsflags, fps_string);
	}
}

// SCR_DisplayLocalPing
// Used to draw the user's local ping next to the framerate for a quick check without having to hold TAB for instance. By default, it only shows up if your ping is too high and risks getting you kicked.

void SCR_DisplayLocalPing(void)
{
	UINT32 ping = playerpingtable[consoleplayer];	// consoleplayer's ping is everyone's ping in a splitnetgame :P
	INT32 pingflags = V_LocalTransFlag()|V_SNAPTOBOTTOM|V_SNAPTORIGHT;
	
	if (cv_showping.value == 1 || (cv_showping.value == 2 && ping > servermaxping))	// only show 2 (warning) if our ping is at a bad level
	{
		INT32 dispy = (cv_ticrate.value == 1) ? 165 : ((cv_ticrate.value == 2 || cv_ticrate.value == 4) ? 172 : ((cv_ticrate.value == 3) ? 163 : 181)); // absolute buttpain
		
		HU_drawPing(308, dispy, ping, pingflags);
	}
}
