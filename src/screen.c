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
/// \brief Handles multiple resolutions

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

// ------------------
// global video state
// ------------------
viddef_t vid = {};
INT32 setmodeneeded = 0; // video mode change needed if > 0 (the mode number to set + 1)

static CV_PossibleValue_t shittyscreen_cons_t[] = {{0, "Okay"}, {1, "Shitty"}, {2, "Extra Shitty"}, {0, NULL}};

//added : 03-02-98: default screen mode, as loaded/saved in config
consvar_t cv_scr_width = {"scr_width", "1280", CV_SAVE|CV_CALL, CV_Unsigned, VID_RefreshModeList, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_scr_height = {"scr_height", "800", CV_SAVE|CV_CALL, CV_Unsigned, VID_RefreshModeList, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_renderview = {"renderview", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_frameskip = {"frameskip", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_vhseffect = {"vhspause", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_shittyscreen = {"televisionsignal", "Okay", CV_NOSHOWHELP, shittyscreen_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_parallelsoftware = {"parallelsoftware", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_paralleldrawmasked = {"paralleldrawmasked", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t votescale_cons_t[] = {{0, "Vanilla"}, {1, "Adaptive"}, {2, "VerticalFill"}, {3, "HorizontalFill"}, {0, NULL}};
consvar_t cv_votebgscaling = {"votebgscaling", "Adaptive", CV_SAVE, votescale_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static void Highreshudscale_OnChange(void);
static CV_PossibleValue_t highreshudscale_cons_t[] = {{4*FRACUNIT/5, "MIN"}, {5*FRACUNIT/3, "MAX"}, {0, NULL}};
consvar_t cv_highreshudscale = {"highreshudscale", "1", CV_SAVE|CV_FLOAT|CV_CALL|CV_NOINIT|CV_NOSHOWHELP, highreshudscale_cons_t, Highreshudscale_OnChange, 0, NULL, NULL, 0, 0, NULL};

static void Highreshudscale_OnChange(void)
{
	if (!con_startup)
		SCR_Recalc();
}

static void SCR_ChangeFullscreen (void);

static CV_PossibleValue_t fullscreen_cons_t[] = {{0, "No"}, {1, "Yes"}, {2, "Borderless Window"}, {0, NULL}};

consvar_t cv_fullscreen = {"fullscreen", "Yes", CV_SAVE|CV_CALL, fullscreen_cons_t, SCR_ChangeFullscreen, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t accuratefps_cons_t[] = {{0, "Inaccurate"}, {1, "Accurate"}, {0, NULL}};
consvar_t cv_accuratefps = {"fpssampling", "1", CV_SAVE, accuratefps_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// =========================================================================
//                           SCREEN VARIABLES
// =========================================================================

static void SCR_SetDrawFuncs(enum columncontext_e _columncontext)
{
	//
	//  setup the right draw routines
	//

	spanfuncs[BASEDRAWFUNC] = R_DrawSpan;
	spanfuncs[SPANDRAWFUNC_TRANS] = R_DrawTranslucentSpan;
	spanfuncs[SPANDRAWFUNC_TILTED] = R_DrawSpan_Tilted;
	spanfuncs[SPANDRAWFUNC_TILTEDTRANS] = R_DrawTranslucentSpan_Tilted;
	spanfuncs[SPANDRAWFUNC_SPLAT] = R_DrawSplat;
	spanfuncs[SPANDRAWFUNC_TRANSSPLAT] = R_DrawTranslucentSplat;
	spanfuncs[SPANDRAWFUNC_TILTEDSPLAT] = R_DrawSplat_Tilted;
	spanfuncs[SPANDRAWFUNC_TILTEDTRANSSPLAT] = R_DrawTranslucentSpan_Tilted;
	spanfuncs[SPANDRAWFUNC_WATER] = R_DrawTranslucentWaterSpan;
	spanfuncs[SPANDRAWFUNC_TILTEDWATER] = R_DrawTranslucentWaterSpan_Tilted;
	spanfuncs[SPANDRAWFUNC_FOG] = R_DrawFogSpan;
	spanfuncs[SPANDRAWFUNC_TILTEDFOG] = R_DrawFogSpan_Tilted;

	if (_columncontext == COLUMNCONTEXT_FLUSH)
	{
		colfuncs[BASEDRAWFUNC] = R_DrawColumn_Flush;
		colfuncs[COLDRAWFUNC_FUZZY] = R_DrawTranslucentColumn_Flush;
		colfuncs[COLDRAWFUNC_TRANS] = R_DrawTranslatedColumn_Flush;
		colfuncs[COLDRAWFUNC_SHADOWED] = R_DrawColumnShadowed_Flush;
		colfuncs[COLDRAWFUNC_TRANSTRANS] = R_DrawTranslatedTranslucentColumn_Flush;
		colfuncs[COLDRAWFUNC_TWOSMULTIPATCH] = R_Draw2sMultiPatchColumn_Flush;
		colfuncs[COLDRAWFUNC_TWOSMULTIPATCHTRANS] = R_Draw2sMultiPatchTranslucentColumn_Flush;
	}
	else
	{
		colfuncs[BASEDRAWFUNC] = R_DrawColumn;
		colfuncs[COLDRAWFUNC_FUZZY] = R_DrawTranslucentColumn;
		colfuncs[COLDRAWFUNC_TRANS] = R_DrawTranslatedColumn;
		colfuncs[COLDRAWFUNC_SHADOWED] = R_DrawColumnShadowed;
		colfuncs[COLDRAWFUNC_TRANSTRANS] = R_DrawTranslatedTranslucentColumn;
		colfuncs[COLDRAWFUNC_TWOSMULTIPATCH] = R_Draw2sMultiPatchColumn;
		colfuncs[COLDRAWFUNC_TWOSMULTIPATCHTRANS] = R_Draw2sMultiPatchTranslucentColumn;
	}

	// gotta keep a copy of those for R_DrawWallColumn.....
	colfuncs[COLDRAWFUNC_TWOSMULTIPATCH_DIRECT] = R_Draw2sMultiPatchColumn;
	colfuncs[COLDRAWFUNC_TWOSMULTIPATCHTRANS_DIRECT] = R_Draw2sMultiPatchTranslucentColumn;

	colfuncs[COLDRAWFUNC_FOG] = R_DrawFogColumn;

	R_SetColumnFunc(BASEDRAWFUNC);
	R_SetSpanFunc(BASEDRAWFUNC);
}

void SCR_SetMode(void)
{
	if (dedicated)
		return;

	if (!setmodeneeded || WipeInAction)
		return; // should never happen and don't change it during a wipe, BAD!

	if (vid.modenum != setmodeneeded - 1)
		M_StopMovie(); // nope, cry about it

	VID_SetMode(--setmodeneeded);

	V_SetPalette(0);

	SCR_SetDrawFuncs(COLUMNCONTEXT_DIRECT);

	// set the apprpriate drawer for the sky (tall or INT16)
	setmodeneeded = 0;
}

// used to switch between column buffering and drawing them directly to screen
// our sky "plane" drawer cannot handle the buffer system due to multithreading
// (that would require alot of extra complexity for smth with massive diminishing results)
// Our masked drawing step draws things in a very particular order, which results in alot of flushing to screen
// effectively adding massive overhead due to excessive flushing, so we draw our masked thing directly to screen instead
void R_SetColumnContext(enum columncontext_e _columncontext)
{
	SCR_SetDrawFuncs(_columncontext); // set our column drawers
}

void R_SetColumnFunc(size_t id)
{
	I_Assert(id < COLDRAWFUNC_MAX);

	colfunctype = id;
	colfunc = colfuncs[id];
}

void R_SetSpanFunc(size_t id)
{
	I_Assert(id < SPANDRAWFUNC_MAX);
	spanfunc = spanfuncs[id];
}

boolean R_CheckColumnFunc(size_t id)
{
	size_t i;

	if (colfunc == NULL)
	{
		// Shouldn't happen.
		return false;
	}

	for (i = 0; i < COLDRAWFUNC_MAX; i++)
	{
		if (colfunc == colfuncs[id])
		{
			return true;
		}
	}

	return false;
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

	V_Recalc();

	V_Init();
	CV_RegisterVar(&cv_highreshudscale);
	CV_RegisterVar(&cv_ticrate);
	CV_RegisterVar(&cv_accuratefps);
	CV_RegisterVar(&cv_menucaps);
	CV_RegisterVar(&cv_constextsize);

#ifdef BACKWARDSCOMPATCORRECTION
	CV_RegisterVar(&cv_globalgamma);
#endif
	CV_RegisterVar(&cv_globalbrightness);
	CV_RegisterVar(&cv_globalsaturation);

	CV_RegisterVar(&cv_rhue);
	CV_RegisterVar(&cv_yhue);
	CV_RegisterVar(&cv_ghue);
	CV_RegisterVar(&cv_chue);
	CV_RegisterVar(&cv_bhue);
	CV_RegisterVar(&cv_mhue);

	CV_RegisterVar(&cv_rbrightness);
	CV_RegisterVar(&cv_ybrightness);
	CV_RegisterVar(&cv_gbrightness);
	CV_RegisterVar(&cv_cbrightness);
	CV_RegisterVar(&cv_bbrightness);
	CV_RegisterVar(&cv_mbrightness);

	CV_RegisterVar(&cv_rsaturation);
	CV_RegisterVar(&cv_ysaturation);
	CV_RegisterVar(&cv_gsaturation);
	CV_RegisterVar(&cv_csaturation);
	CV_RegisterVar(&cv_bsaturation);
	CV_RegisterVar(&cv_msaturation);

	CV_RegisterVar(&cv_votebgscaling);

	V_SetPalette(0);
}

// Called at new frame, if the video mode has changed
//
void SCR_Recalc(void)
{
	if (dedicated)
		return;

	V_Recalc();

	// toggle off (then back on) the automap because some screensize-dependent values will
	// be calculated next time the automap is activated.
	if (automapactive)
	{
		am_recalc = true;
		AM_Start();
	}

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
		CONS_Printf(M_GetText("Default resolution: %d x %d\n"), cv_scr_width.value, cv_scr_height.value);
		// see note above
		setmodeneeded = VID_GetModeForSize(cv_scr_width.value, cv_scr_height.value) + 1;
	}
}

// sets the modenum as the new default video mode to be saved in the config file
void SCR_SetDefaultMode(void)
{
	// remember the default screen size
	CV_StealthSetValue(&cv_scr_width, vid.width);
	CV_StealthSetValue(&cv_scr_height, vid.height);
	VID_RefreshModeList(); // probably not needed but justin käs
}

// Change fullscreen on/off according to cv_fullscreen
void SCR_ChangeFullscreen(void)
{
	I_SetBorderlessWindow(); // Running this here so we can have borderless window at startup

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
}

boolean SCR_IsAspectCorrect(INT32 width, INT32 height)
{
	return (width % BASEVIDWIDTH == 0 && height % BASEVIDHEIGHT == 0 && width / BASEVIDWIDTH == height / BASEVIDHEIGHT);
}

#define USE_FPS_SAMPLES

#ifdef USE_FPS_SAMPLES
#define MAX_FRAME_TIME (0.05)
#define NUM_FPS_SAMPLES (32) // Number of samples to store

static double total_frame_time = 0.0;
static double fps_samples[NUM_FPS_SAMPLES];
#endif

static double averageFPS = 0.0f;

void SCR_CalculateFPS(void)
{
	static boolean fps_init = false;
	static int frame_index = 0;
	static precise_t fps_enter = 0;
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

static void SCR_DrawOldTicRate(UINT32 cap, UINT32 benchmark, double fps, INT32 fpsflags)
{
	const char *fps_string;
	INT32 ticcntcolor = 0;

	if (fps > (benchmark - 5))
		ticcntcolor = V_GREENMAP;
	else if (fps < 20)
		ticcntcolor = V_REDMAP;

	if (cap != 0)
		fps_string = va("%d/%d\x82", (INT32)fps, cap);
	else
		fps_string = va("%d\x82", (INT32)fps);

	// draw "FPS"
	if (cv_ticrate.value == 3)
		V_DrawRightAlignedString(319, 181, V_YELLOWMAP|fpsflags, "FPS");

	V_DrawRightAlignedString(319, 190, ticcntcolor|fpsflags, fps_string);
}

static void SCR_DrawKartTicRate(UINT32 cap, UINT32 benchmark, double fps, INT32 fpsflags)
{
	UINT8 *ticcntcolor = NULL;
	INT32 x = 318;

	// draw "FPS"
	if (cv_ticrate.value == 1)
	{
		ticcntcolor = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_YELLOW, GTC_CACHE);
		V_DrawFixedPatch(306<<FRACBITS, 183<<FRACBITS, FRACUNIT, fpsflags, framecounter, ticcntcolor);
	}

	if (fps > (benchmark - 5))
		ticcntcolor = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_MINT, GTC_CACHE);
	else if (fps < 20)
		ticcntcolor = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_RASPBERRY, GTC_CACHE);
	else
		ticcntcolor = NULL;

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

void SCR_DisplayTicRate(void)
{
	UINT32 cap, benchmark;
	double fps;
	INT32 fpsflags;

	if (gamestate == GS_NULL)
		return;

	cap = R_GetFramerateCap();
	benchmark = (cap == 0) ? I_GetRefreshRate() : cap;
	fps = round(averageFPS);
	fpsflags = V_LocalTransFlag()|V_SNAPTOBOTTOM|V_SNAPTORIGHT;

	switch (cv_ticrate.value)
	{
		case 1: // new kart counter
		case 2:
			SCR_DrawKartTicRate(cap, benchmark, fps, fpsflags);
			break;
		case 3: // kart v1.0/srb2 counter
		case 4:
			SCR_DrawOldTicRate(cap, benchmark, fps, fpsflags);
			break;
		default:
			break;
	}
}

// SCR_DisplayLocalPing
// Used to draw the user's local ping next to the framerate for a quick check without having to hold TAB for instance. By default, it only shows up if your ping is too high and risks getting you kicked.

void SCR_DisplayLocalPing(void)
{
	UINT32 ping = playerpingtable[consoleplayer];
	INT32 pingflags = V_LocalTransFlag()|V_SNAPTOBOTTOM|V_SNAPTORIGHT;

	if (cv_showping.value == 1 || (cv_showping.value == 2 && ping > servermaxping)) // only show 2 (warning) if our ping is at a bad level
	{
		INT32 dispy = (cv_ticrate.value == 1) ? 165 : ((cv_ticrate.value == 2 || cv_ticrate.value == 4) ? 172 : ((cv_ticrate.value == 3) ? 163 : 181)); // absolute buttpain
		HU_drawPlayerPing(308, dispy, consoleplayer, pingflags); // consoleplayer's ping is everyone's ping in a splitnetgame :P
	}
}
