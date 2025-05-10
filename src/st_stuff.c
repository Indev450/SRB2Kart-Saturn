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
/// \file  st_stuff.c
/// \brief Status bar code
///        Does the face/direction indicator animatin.
///        Does palette indicators as well (red pain/berserk, bright pickup)

#include "doomdef.h"
#include "g_game.h"
#include "g_input.h"
#include "k_director.h"
#include "r_local.h"
#include "p_local.h"
#include "f_finale.h"
#include "st_stuff.h"
#include "i_video.h"
#include "v_video.h"
#include "z_zone.h"
#include "hu_stuff.h"
#include "s_sound.h"
#include "i_system.h"
#include "m_menu.h"
#include "m_cheat.h"
#include "p_setup.h" // NiGHTS grading

#include "i_time.h"

#include "k_kart.h" // SRB2kart

//random index
#include "m_random.h"

// item finder
#include "m_cond.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#endif

#include "lua_hud.h"
#include "lua_hudlib_drawlist.h"
#include "lua_hook.h"

#include "r_fps.h"

UINT16 objectsdrawn = 0;

// dumb fade thing for director toggle
tic_t directortoggletimer = 0;

//
// STATUS BAR DATA
//

patch_t *facerankprefix[MAXSKINS]; // ranking
patch_t *facewantprefix[MAXSKINS]; // wanted
patch_t *facemmapprefix[MAXSKINS]; // minimap

patch_t *localfacerankprefix[MAXLOCALSKINS]; // ranking
patch_t *localfacewantprefix[MAXLOCALSKINS]; // wanted
patch_t *localfacemmapprefix[MAXLOCALSKINS]; // minimap

/*char *facerankprefix_name[MAXSKINS]; // ranking
char *facewantprefix_name[MAXSKINS]; // wanted
char *facemmapprefix_name[MAXSKINS]; // minimap

char *localfacerankprefix_name[MAXLOCALSKINS]; // ranking
char *localfacewantprefix_name[MAXLOCALSKINS]; // wanted
char *localfacemmapprefix_name[MAXLOCALSKINS]; // minimap*/

// ------------------------------------------
//             status bar overlay
// ------------------------------------------

// Midnight Channel:
static patch_t *hud_tv1;
static patch_t *hud_tv2;

#ifdef HAVE_DISCORDRPC
// Discord Rich Presence
static patch_t *envelope;
#endif

// current player for overlay drawing
player_t *stplyr;
UINT8 stplyrnum;

// SRB2kart

hudinfo_t hudinfo[NUMHUDITEMS] =
{
	{  34, 176}, // HUD_LIVESNAME
	{  16, 176}, // HUD_LIVESPIC
	{  74, 184}, // HUD_LIVESNUM
	{  38, 186}, // HUD_LIVESX

	{  16,  42}, // HUD_RINGS
	{ 220,  10}, // HUD_RINGSSPLIT
	{ 112,  42}, // HUD_RINGSNUM
	{ 288,  10}, // HUD_RINGSNUMSPLIT

	{  16,  10}, // HUD_SCORE
	{ 128,  10}, // HUD_SCORENUM

	{  17,  26}, // HUD_TIME
	{ 136,  10}, // HUD_TIMESPLIT
	{  88,  26}, // HUD_MINUTES
	{ 188,  10}, // HUD_MINUTESSPLIT
	{  88,  26}, // HUD_TIMECOLON
	{ 188,  10}, // HUD_TIMECOLONSPLIT
	{ 112,  26}, // HUD_SECONDS
	{ 212,  10}, // HUD_SECONDSSPLIT
	{ 112,  26}, // HUD_TIMETICCOLON
	{ 136,  26}, // HUD_TICS

	{ 112,  56}, // HUD_SS_TOTALRINGS
	{ 288,  40}, // HUD_SS_TOTALRINGS_SPLIT

	{ 110,  93}, // HUD_GETRINGS
	{ 160,  93}, // HUD_GETRINGSNUM
	{ 124, 160}, // HUD_TIMELEFT
	{ 168, 176}, // HUD_TIMELEFTNUM
	{ 130,  93}, // HUD_TIMEUP
	{ 152, 168}, // HUD_HUNTPICS
	{ 152,  24}, // HUD_GRAVBOOTSICO
	{ 240, 160}, // HUD_LAP
};

static huddrawlist_h luahuddrawlist_game[MAXSPLITSCREENPLAYERS];

// variable to stop mayonaka static from flickering
consvar_t cv_lessflicker = {"lessflicker", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_stagetitle = {"maptitle", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

//
// STATUS BAR CODE
//

boolean ST_SameTeam(player_t *a, player_t *b)
{
	// Just pipe team messages to everyone in co-op or race.
	if (!G_BattleGametype())
		return true;

	// Spectator chat.
	if (a->spectator && b->spectator)
		return true;

	// Team chat.
	if (G_GametypeHasTeams())
		return a->ctfteam == b->ctfteam;

	if (G_TagGametype())
		return ((a->pflags & PF_TAGIT) == (b->pflags & PF_TAGIT));

	return false;
}

static boolean st_stopped = true;

void ST_Ticker(void)
{
	if (st_stopped)
		return;
}

// 0 is default, any others are special palettes.
INT32 st_palette = 0;

void ST_doPaletteStuff(void)
{
	INT32 palette;

	if (stplyr && stplyr->flashcount)
		palette = stplyr->flashpal;
	else
		palette = 0;

#ifdef HWRENDER
	if (rendermode == render_opengl && !HWR_PalRenderFlashpal())
		palette = 0; // No flashpals here in OpenGL
#endif

	palette = min(max(palette, 0), 13);

	if (palette != st_palette)
	{
		st_palette = palette;

#ifdef HWRENDER
		if (rendermode == render_soft || (rendermode == render_opengl && HWR_PalRenderFlashpal()))
#else
		if (rendermode != render_none)
#endif
		{
			//V_SetPaletteLump(GetPalette()); // Reset the palette -- is this needed?
			if (!splitscreen)
				V_SetPalette(palette);
		}
	}
}

void ST_UnloadGraphics(void)
{
	Patch_FreeTag(PU_HUDGFX);
}

void ST_LoadGraphics(void)
{
	// SRB2 border patch
	//st_borderpatchnum = W_GetNumForName("GFZFLR01");
	//scr_borderpatch = W_CacheLumpNum(st_borderpatchnum, PU_HUDGFX);

	// the original Doom uses 'STF' as base name for all face graphics
	// Graue 04-08-2004: face/name graphics are now indexed by skins
	//                   but load them in R_AddSkins, that gets called
	//                   first anyway
	// cache the status bar overlay icons (fullscreen mode)

	rflagico = W_CachePatchName("RFLAGICO", PU_HUDGFX);
	bflagico = W_CachePatchName("BFLAGICO", PU_HUDGFX);
	rmatcico = W_CachePatchName("RMATCICO", PU_HUDGFX);
	bmatcico = W_CachePatchName("BMATCICO", PU_HUDGFX);

	K_LoadKartHUDGraphics();

	// Midnight Channel:
	hud_tv1 = W_CachePatchName("HUD_TV1", PU_HUDGFX);
	hud_tv2 = W_CachePatchName("HUD_TV2", PU_HUDGFX);

#ifdef HAVE_DISCORDRPC
	// Discord Rich Presence
	envelope = W_CachePatchName("K_REQUES", PU_HUDGFX);
#endif
}

// made separate so that skins code can reload custom face graphics
void ST_LoadFaceGraphics(char *rankstr, char *wantstr, char *mmapstr, INT32 skinnum)
{
	facerankprefix[skinnum] = W_CachePatchName(rankstr, PU_HUDGFX);
	facewantprefix[skinnum] = W_CachePatchName(wantstr, PU_HUDGFX);
	facemmapprefix[skinnum] = W_CachePatchName(mmapstr, PU_HUDGFX);

	/*facerankprefix_name[skinnum] = rankstr;
	facewantprefix_name[skinnum] = wantstr;
	facemmapprefix_name[skinnum] = mmapstr;*/
}

void ST_LoadLocalFaceGraphics(char *rankstr, char *wantstr, char *mmapstr, INT32 skinnum)
{
	localfacerankprefix[skinnum] = W_CachePatchName(rankstr, PU_HUDGFX);
	localfacewantprefix[skinnum] = W_CachePatchName(wantstr, PU_HUDGFX);
	localfacemmapprefix[skinnum] = W_CachePatchName(mmapstr, PU_HUDGFX);

	/*localfacerankprefix_name[skinnum] = rankstr;
	localfacewantprefix_name[skinnum] = wantstr;
	localfacemmapprefix_name[skinnum] = mmapstr;*/

	//CONS_Printf("Added rank prefix %s\n", rankstr);
	//CONS_Printf("Added want prefix %s\n", wantstr);
	//CONS_Printf("Added mmap prefix %s\n", mmapstr);
}

void ST_ReloadSkinFaceGraphics(void)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
		ST_LoadFaceGraphics(skins[i].facerank, skins[i].facewant, skins[i].facemmap, i);

	for (i = 0; i < numlocalskins; i++)
		ST_LoadLocalFaceGraphics(localskins[i].facerank, localskins[i].facewant, localskins[i].facemmap, i);
}

static inline void ST_InitData(void)
{
	// 'link' the statusbar display to a player, which could be
	// another player than consoleplayer, for example, when you
	// change the view in a multiplayer demo with F12.
	stplyr = &players[displayplayers[0]];

	st_palette = -1;
}

static inline void ST_Stop(void)
{
	if (st_stopped)
		return;

#ifdef HWRENDER
	if (rendermode != render_opengl)
#endif
		V_SetPalette(0);

	st_stopped = true;
}

void ST_Start(void)
{
	if (!st_stopped)
		ST_Stop();

	ST_InitData();

	if (!dedicated)
		st_stopped = false;
}

//
// Initializes the status bar, sets the defaults border patch for the window borders.
//

// used by OpenGL mode, holds lumpnum of flat used to fill space around the viewwindow
lumpnum_t st_borderpatchnum;

void ST_Init(void)
{
	if (dedicated)
		return;

	ST_LoadGraphics();

	for (int i = 0; i < MAXSPLITSCREENPLAYERS; i++)
		luahuddrawlist_game[i] = LUA_HUD_CreateDrawList();
}

// change the status bar too, when pressing F12 while viewing a demo.
void ST_changeDemoView(void)
{
	// the same routine is called at multiplayer deathmatch spawn
	// so it can be called multiple times
	ST_Start();
}

// =========================================================================
//                         STATUS BAR OVERLAY
// =========================================================================

boolean st_overlay;

// =========================================================================
//                          INTERNAL DRAWING
// =========================================================================
#define ST_DrawOverlayNum(x,y,n)           V_DrawTallNum(x, y, V_NOSCALESTART|V_HUDTRANS, n)
#define ST_DrawPaddedOverlayNum(x,y,n,d)   V_DrawPaddedTallNum(x, y, V_NOSCALESTART|V_HUDTRANS, n, d)
#define ST_DrawOverlayPatch(x,y,p)         V_DrawScaledPatch(x, y, V_NOSCALESTART|V_HUDTRANS, p)
#define ST_DrawMappedOverlayPatch(x,y,p,c) V_DrawMappedScaledPatch(x, y, V_NOSCALESTART|V_HUDTRANS, p, c)
#define ST_DrawNumFromHud(h,n)        V_DrawTallNum(SCX(hudinfo[h].x), SCY(hudinfo[h].y), V_NOSCALESTART|V_HUDTRANS, n)
#define ST_DrawPadNumFromHud(h,n,q)   V_DrawPaddedTallNum(SCX(hudinfo[h].x), SCY(hudinfo[h].y), V_NOSCALESTART|V_HUDTRANS, n, q)
#define ST_DrawPatchFromHud(h,p)      V_DrawScaledPatch(SCX(hudinfo[h].x), SCY(hudinfo[h].y), V_NOSCALESTART|V_HUDTRANS, p)
#define ST_DrawNumFromHudWS(h,n)      V_DrawTallNum(SCX(hudinfo[h+!!splitscreen].x), SCY(hudinfo[h+!!splitscreen].y), V_NOSCALESTART|V_HUDTRANS, n)
#define ST_DrawPadNumFromHudWS(h,n,q) V_DrawPaddedTallNum(SCX(hudinfo[h+!!splitscreen].x), SCY(hudinfo[h+!!splitscreen].y), V_NOSCALESTART|V_HUDTRANS, n, q)
#define ST_DrawPatchFromHudWS(h,p)    V_DrawScaledPatch(SCX(hudinfo[h+!!splitscreen].x), SCY(hudinfo[h+!!splitscreen].y), V_NOSCALESTART|V_HUDTRANS, p)

// Devmode information
static void ST_drawDebugInfo(void)
{
	INT32 height = 192;

	if (!stplyr->mo)
		return;

	if (cv_debug & DBG_BASIC)
	{
		const fixed_t d = AngleFixed(stplyr->mo->angle);
		V_DrawRightAlignedString(320, 168, V_MONOSPACE, va("X: %6d", stplyr->mo->x>>FRACBITS));
		V_DrawRightAlignedString(320, 176, V_MONOSPACE, va("Y: %6d", stplyr->mo->y>>FRACBITS));
		V_DrawRightAlignedString(320, 184, V_MONOSPACE, va("Z: %6d", stplyr->mo->z>>FRACBITS));
		V_DrawRightAlignedString(320, 192, V_MONOSPACE, va("A: %6d", FixedInt(d)));

		height = 152;
	}

	if (cv_debug & DBG_DETAILED)
	{
		V_DrawRightAlignedString(320, height - 96,  V_MONOSPACE, va("SCALE: %5d%%", (stplyr->mo->scale*100)/FRACUNIT));

		V_DrawRightAlignedString(320, height - 64, V_MONOSPACE, va("CEILZ: %6d", stplyr->mo->ceilingz>>FRACBITS));
		V_DrawRightAlignedString(320, height - 56, V_MONOSPACE, va("FLOORZ: %6d", stplyr->mo->floorz>>FRACBITS));

		V_DrawRightAlignedString(320, height - 48, V_MONOSPACE, va("CNVX: %6d", stplyr->cmomx>>FRACBITS));
		V_DrawRightAlignedString(320, height - 40, V_MONOSPACE, va("CNVY: %6d", stplyr->cmomy>>FRACBITS));
		V_DrawRightAlignedString(320, height - 32, V_MONOSPACE, va("PLTZ: %6d", stplyr->mo->pmomz>>FRACBITS));

		V_DrawRightAlignedString(320, height - 24, V_MONOSPACE, va("MOMX: %6d", stplyr->rmomx>>FRACBITS));
		V_DrawRightAlignedString(320, height - 16, V_MONOSPACE, va("MOMY: %6d", stplyr->rmomy>>FRACBITS));
		V_DrawRightAlignedString(320, height - 8,  V_MONOSPACE, va("MOMZ: %6d", stplyr->mo->momz>>FRACBITS));
		V_DrawRightAlignedString(320, height,      V_MONOSPACE, va("SPEED: %6d", stplyr->speed>>FRACBITS));

		height -= 120;
	}

	if (cv_debug & DBG_RANDOMIZER) // randomizer testing
	{
		fixed_t peekres = P_RandomPeek();
		peekres *= 10000;     // Change from fixed point
		peekres >>= FRACBITS; // to displayable decimal

		V_DrawRightAlignedString(320, height - 16, V_MONOSPACE, va("Init: %08x", P_GetInitSeed()));
		V_DrawRightAlignedString(320, height - 8,  V_MONOSPACE, va("Seed: %08x", P_GetRandSeed()));
		V_DrawRightAlignedString(320, height,      V_MONOSPACE, va("==  :    .%04d", peekres));

		height -= 32;
	}

	if (cv_debug & DBG_MEMORY)
		V_DrawRightAlignedString(320, height,     V_MONOSPACE, va("Heap used: %7sKB", sizeu1(Z_TagsUsage(0, INT32_MAX)>>10)));
}

static void ST_drawLevelTitle(void)
{
	char *lvlttl = mapheaderinfo[gamemap-1]->lvlttl;
	char *subttl = mapheaderinfo[gamemap-1]->subttl;
	char *zonttl = mapheaderinfo[gamemap-1]->zonttl; // SRB2kart
	char *actnum = mapheaderinfo[gamemap-1]->actnum;
	INT32 lvlttlxpos;
	INT32 ttlnumxpos;
	INT32 zonexpos;
	INT32 dupcalc = (vid.width/vid.dupx);
	UINT8 gtc = G_GetGametypeColor(gametype);
	INT32 sub = 0;
	INT32 bary = (splitscreen)
		? BASEVIDHEIGHT/2
		: 163;
	INT32 lvlw;

	if (!cv_stagetitle.value)
		return;

	if (timeinmap > 113)
		return;

	lvlw = V_LevelNameWidth(lvlttl);

	if (actnum[0])
		lvlttlxpos = ((BASEVIDWIDTH/2) - (lvlw/2)) - V_LevelNameWidth(actnum);
	else
		lvlttlxpos = ((BASEVIDWIDTH/2) - (lvlw/2));

	zonexpos = ttlnumxpos = lvlttlxpos + lvlw;
	if (!(mapheaderinfo[gamemap-1]->levelflags & LF_NOZONE))
	{
		if (zonttl[0])
			zonexpos -= V_LevelNameWidth(zonttl); // SRB2kart
		else
			zonexpos -= V_LevelNameWidth(M_GetText("Zone"));
	}

	if (lvlttlxpos < 0)
		lvlttlxpos = 0;

	if (timeinmap > 105)
	{
		INT32 count = (113 - (INT32)(timeinmap));

		// uh... "fill in the bits" of sub, or something... no idea what I came up with
		// VERY janky, but doesn't require m_easing
		INT32 frac = (FixedMul(R_GetHudUncap(), FixedDiv(dupcalc, BASEVIDWIDTH)) >> (count + 4))/13; // these two magic numbers seem to do the trick

		sub = dupcalc;
		while (count-- > 0)
			sub >>= 1;
		sub = -sub - frac;
	}

	{
		dupcalc = (dupcalc - BASEVIDWIDTH)>>1;
		V_DrawFill(sub - dupcalc, bary+9, ttlnumxpos+dupcalc + 1, 2, 31|V_SNAPTOBOTTOM);
		V_DrawDiag(sub + ttlnumxpos + 1, bary, 11, 31|V_SNAPTOBOTTOM);
		V_DrawFill(sub - dupcalc, bary, ttlnumxpos+dupcalc, 10, gtc|V_SNAPTOBOTTOM);
		V_DrawDiag(sub + ttlnumxpos, bary, 10, gtc|V_SNAPTOBOTTOM);

		if (subttl[0])
			V_DrawRightAlignedString(sub + zonexpos - 8, bary+1, V_ALLOWLOWERCASE|V_SNAPTOBOTTOM, subttl);
		//else
			//V_DrawRightAlignedString(sub + zonexpos - 8, bary+1, V_ALLOWLOWERCASE, va("%s Mode", gametype_cons_t[gametype].strvalue));
	}

	ttlnumxpos += sub;
	lvlttlxpos += sub;
	zonexpos += sub;

	V_DrawLevelTitle(lvlttlxpos, bary-18, V_SNAPTOBOTTOM, lvlttl);

	if (strlen(zonttl) > 0)
		V_DrawLevelTitle(zonexpos, bary+6, V_SNAPTOBOTTOM, zonttl);
	else if (!(mapheaderinfo[gamemap-1]->levelflags & LF_NOZONE))
		V_DrawLevelTitle(zonexpos, bary+6, V_SNAPTOBOTTOM, M_GetText("Zone"));

	if (actnum[0])
		V_DrawLevelTitle(ttlnumxpos+12, bary+6, V_SNAPTOBOTTOM, actnum);
}

// returns the actual button name for any gamecontrol
// if unbound is set, it returns "Unbound" as a string should there be no button bound to a gamecontrol
// if gamectrl is set, it adds an extra "-" for cases where theres also a hardcoded button like accelerate
static const char *ST_GetButtonName(INT32 control, const char *inputtext, boolean unbound, boolean gamectrl)
{
	static char buttname[32] = "";
	const char *butt1 = (gamecontrol[0][control][0] != 0 ? G_KeynumToString(gamecontrol[0][control][0]) : NULL);
	const char *butt2 = (gamecontrol[0][control][1] != 0 ? G_KeynumToString(gamecontrol[0][control][1]) : NULL); // alternative bind

	if (butt1 == NULL && butt2 == NULL) // not bound to a button
		snprintf(buttname, 32, (unbound ? "%s - %s" : (gamectrl ? "-%s - %s" : "%s - %s")), (unbound ? "Unbound" : ""), inputtext);
	else if (butt1 != NULL && butt2 != NULL) // bound to two buttons
		snprintf(buttname, 32, "%s/%s - %s", butt1, butt2, inputtext);
	else // bound to only one
		snprintf(buttname, 32, (gamectrl ? "-%s - %s" : "%s - %s"), (butt1 != NULL ? butt1 : butt2 != NULL ? butt2 : ""), inputtext);

	return buttname;
}

// Draw the status bar overlay, customisable: the user chooses which
// kind of information to overlay
//
static void ST_overlayDrawer(void)
{
	//hu_showscores = auto hide score/time/rings when tab rankings are shown
	if (!(hu_showscores && (netgame || multiplayer)))
	{
		K_drawKartHUD();
	}

	if (!hu_showscores) // hide the following if TAB is held
	{
		// TODO: splitscreen support!
		if (cv_showdirectorhud.value && !splitscreen && !P_IsLocalPlayer(stplyr) && K_DirectorIsAvailable())
		{
			char directortext[20] = {0};

			snprintf(directortext, 20, "Director: %s", cv_director.value ? "On" : "Off");

			if ((!demo.playback && directortoggletimer < 13*TICRATE) || (demo.playback && directortoggletimer < 4*TICRATE))
			{
				if (renderisnewtic)
					directortoggletimer++;

				if (directortoggletimer < 4*TICRATE)
					V_DrawString(1, BASEVIDHEIGHT-8-1, V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_HUDTRANSHALF|V_ALLOWLOWERCASE, directortext);
				else if (cv_translucenthud.value != 0)
					V_DrawString(1, BASEVIDHEIGHT-8-1, V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_80TRANS|V_ALLOWLOWERCASE, directortext); // idk if V_80TRANS is good?
			}
		}
		else
		{
			directortoggletimer = 0;
		}

		if (cv_showviewpointtext.value)
		{
			if (!(multiplayer && demo.playback))
			{
				if(!P_IsLocalPlayer(stplyr))
				{
					/*char name[MAXPLAYERNAME+1];
					// shorten the name if its more than twelve characters.
					strlcpy(name, player_names[stplyr-players], 13);*/

					// Show name of player being displayed
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-40, V_SNAPTOBOTTOM|V_HUDTRANS, M_GetText("VIEWPOINT:"));
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-32, V_SNAPTOBOTTOM|V_HUDTRANS|V_ALLOWLOWERCASE, player_names[stplyr-players]);
				}
			}
			else if (!demo.title && !camera[stplyrnum].freecam)
			{
				if (!splitscreen)
				{
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-40, V_SNAPTOBOTTOM|V_HUDTRANSHALF, M_GetText("VIEWPOINT:"));
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-32, V_SNAPTOBOTTOM|V_HUDTRANSHALF|V_ALLOWLOWERCASE, player_names[stplyr-players]);
				}
				else if (splitscreen == 1)
				{
					char name[MAXPLAYERNAME+12];

					INT32 y = (stplyrnum == 0) ? 4 : BASEVIDHEIGHT/2-12;
					sprintf(name, "VIEWPOINT: %s", player_names[stplyr-players]);
					V_DrawRightAlignedThinString(BASEVIDWIDTH-40, y, V_HUDTRANSHALF|V_ALLOWLOWERCASE|K_calcSplitFlags(V_SNAPTOTOP|V_SNAPTOBOTTOM|V_SNAPTORIGHT), name);
				}
				else if (splitscreen)
				{
					V_DrawCenteredThinString((vid.width/vid.dupx)/4, BASEVIDHEIGHT/2 - 12, V_HUDTRANSHALF|V_ALLOWLOWERCASE|K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT), player_names[stplyr-players]);
				}
			}
		}
	}

	if ((!(netgame || multiplayer) || !hu_showscores) && !forceshowhud)
	{
		if (renderisnewtic)
		{
			LUA_HUDHOOK(game, luahuddrawlist_game[stplyrnum]);
		}
	}

	// draw level title Tails
	if (*mapheaderinfo[gamemap-1]->lvlttl != '\0' && !(hu_showscores && (netgame || multiplayer) && !mapreset) && LUA_HudEnabled(hud_stagetitle) && !forceshowhud)
		ST_drawLevelTitle();

	if (!hu_showscores && netgame && !mapreset)
	{
		if (stplyr->spectator && LUA_HudEnabled(hud_textspectator))
		{
			const char *itemtxt = M_GetText("Item - Join Game");

			if (stplyr->powers[pw_flashing])
				itemtxt = M_GetText("Item - . . .");
			else if (stplyr->pflags & PF_WANTSTOJOIN)
				itemtxt = M_GetText("Item - Cancel Join");
			else if (G_GametypeHasTeams())
				itemtxt = M_GetText("Item - Join Team");

			if (cv_ingamecap.value)
			{
				UINT8 numingame = 0;
				UINT8 i;

				for (i = 0; i < MAXPLAYERS; i++)
					if (playeringame[i] && !players[i].spectator)
						numingame++;

				itemtxt = va("%s (%s: %d)", itemtxt, M_GetText("Slots left"), max(0, cv_ingamecap.value - numingame));
			}

			// SRB2kart: changed positions & text
			if (splitscreen)
			{
				INT32 splitflags = K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT);
				V_DrawThinString(2, (BASEVIDHEIGHT/2)-20, V_HUDTRANSHALF|V_YELLOWMAP|splitflags, M_GetText("- SPECTATING -"));
				V_DrawThinString(2, (BASEVIDHEIGHT/2)-10, V_HUDTRANSHALF|splitflags, itemtxt);
			}
			else
			{
				if (cv_showdirectorhud.value)
				{
					V_DrawString(2, BASEVIDHEIGHT-50, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF|V_YELLOWMAP, M_GetText("- SPECTATING -"));
					V_DrawString(2, BASEVIDHEIGHT-40, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, itemtxt);
					V_DrawString(2, BASEVIDHEIGHT-30, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, va("Accelerate %s", ST_GetButtonName(gc_camfloat, "Float", false, true)));
					V_DrawString(2, BASEVIDHEIGHT-20, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, va("Brake %s", ST_GetButtonName(gc_camsink, "Sink", false, true)));
					V_DrawString(2, BASEVIDHEIGHT-10, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, ST_GetButtonName(gc_director, "Toggle Director", true, false));
				}
				else
				{
					V_DrawString(2, BASEVIDHEIGHT-40, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF|V_YELLOWMAP, M_GetText("- SPECTATING -"));
					V_DrawString(2, BASEVIDHEIGHT-30, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, itemtxt);
					V_DrawString(2, BASEVIDHEIGHT-20, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, va("Accelerate %s", ST_GetButtonName(gc_camfloat, "Float", false, true)));
					V_DrawString(2, BASEVIDHEIGHT-10, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, va("Brake %s", ST_GetButtonName(gc_camsink, "Sink", false, true)));
				}
			}
		}
	}

	// Replay manual-save stuff
	if (demo.recording && multiplayer && demo.savebutton && demo.savebutton + 3*TICRATE < leveltime)
	{
		switch (demo.savemode)
		{
		case DSM_NOTSAVING:
			V_DrawRightAlignedThinString(BASEVIDWIDTH - 2, 2, V_HUDTRANS|V_SNAPTOTOP|V_SNAPTORIGHT|V_ALLOWLOWERCASE|(G_BattleGametype() ? V_REDMAP : V_SKYMAP), "Look Backward: Save replay");
			break;

		case DSM_WILLAUTOSAVE:
			V_DrawRightAlignedThinString(BASEVIDWIDTH - 2, 2, V_HUDTRANS|V_SNAPTOTOP|V_SNAPTORIGHT|V_ALLOWLOWERCASE|(G_BattleGametype() ? V_REDMAP : V_SKYMAP), "Replay will be saved. (Look Backward: Change title)");
			break;

		case DSM_WILLSAVE:
			V_DrawRightAlignedThinString(BASEVIDWIDTH - 2, 2, V_HUDTRANS|V_SNAPTOTOP|V_SNAPTORIGHT|V_ALLOWLOWERCASE|(G_BattleGametype() ? V_REDMAP : V_SKYMAP), "Replay will be saved.");
			break;

		case DSM_TITLEENTRY:
			ST_DrawDemoTitleEntry();
			break;

		default: // Don't render anything
			break;
		}
	}
}

void ST_DrawDemoTitleEntry(void)
{
#define x (BASEVIDWIDTH/2 - 139)
#define y (BASEVIDHEIGHT/2)
	M_DrawTextBox(x, y + 4, MAXSTRINGLENGTH, 1);

	M_DrawTextInputScroll(x + 8, y + 12, &demo.titlenameinput, 0, (MAXSTRINGLENGTH-1)*8);

	M_DrawTextBox(x + 30, y - 24, 26, 1);
	V_DrawString(x + 38, y - 16, V_ALLOWLOWERCASE, "Enter the name of the replay.");

	M_DrawTextBox(x + 50, y + 20, 20, 1);
	V_DrawThinString(x + 58, y + 28, V_ALLOWLOWERCASE, "Escape - Cancel");
	V_DrawRightAlignedThinString(x + 220, y + 28, V_ALLOWLOWERCASE, "Enter - Confirm");
#undef x
#undef y
}

// MayonakaStatic: draw Midnight Channel's TV-like borders
static void ST_MayonakaStatic(void)
{
	INT32 flag;
	if (cv_lessflicker.value)
		flag = V_70TRANS;
	else
		flag = (leveltime%2) ? V_90TRANS : V_70TRANS;

	V_DrawFixedPatch(0, 0, FRACUNIT, V_SNAPTOTOP|V_SNAPTOLEFT|flag, hud_tv1, NULL);
	V_DrawFixedPatch(320<<FRACBITS, 0, FRACUNIT, V_SNAPTOTOP|V_SNAPTORIGHT|V_FLIP|flag, hud_tv1, NULL);
	V_DrawFixedPatch(0, 142<<FRACBITS, FRACUNIT, V_SNAPTOBOTTOM|V_SNAPTOLEFT|flag, hud_tv2, NULL);
	V_DrawFixedPatch(320<<FRACBITS, 142<<FRACBITS, FRACUNIT, V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_FLIP|flag, hud_tv2, NULL);
}

#ifdef HAVE_DISCORDRPC
void ST_AskToJoinEnvelope(void)
{
	const tic_t freq = TICRATE/2;

	if (menuactive)
		return;

	if ((leveltime % freq) < freq/2)
		return;

	V_DrawFixedPatch(296*FRACUNIT, 2*FRACUNIT, FRACUNIT, V_SNAPTOTOP|V_SNAPTORIGHT, envelope, NULL);
	// maybe draw number of requests with V_DrawPingNum ?
}
#endif

void ST_Drawer(void)
{
	UINT8 i;

#ifdef SEENAMES
	if (cv_seenames.value && cv_allowseenames.value && displayplayers[0] == consoleplayer && seenplayer && seenplayer->mo && !mapreset)
	{
		if (cv_seenames.value == 1)
			V_DrawCenteredString(BASEVIDWIDTH/2, BASEVIDHEIGHT/2 + 15, V_HUDTRANSHALF, player_names[seenplayer-players]);
		else if (cv_seenames.value == 2)
			V_DrawCenteredString(BASEVIDWIDTH/2, BASEVIDHEIGHT/2 + 15, V_HUDTRANSHALF,
			va("%s%s", G_GametypeHasTeams() ? ((seenplayer->ctfteam == 1) ? "\x85" : "\x84") : "", player_names[seenplayer-players]));
		else //if (cv_seenames.value == 3)
			V_DrawCenteredString(BASEVIDWIDTH/2, BASEVIDHEIGHT/2 + 15, V_HUDTRANSHALF,
			va("%s%s", !G_BattleGametype() || (G_GametypeHasTeams() && players[consoleplayer].ctfteam == seenplayer->ctfteam)
			 ? "\x83" : "\x85", player_names[seenplayer-players]));
	}
#endif

	// force a set of the palette by using doPaletteStuff()
	if (vid.recalc)
		st_palette = -1;

	// Do red-/gold-shifts from damage/items
#ifdef HWRENDER
	//25/08/99: Hurdler: palette changes is done for all players,
	//                   not only player1! That's why this part
	//                   of code is moved somewhere else.
	if (rendermode == render_soft || (rendermode == render_opengl && HWR_PalRenderFlashpal()))
#endif
	if (rendermode != render_none) ST_doPaletteStuff();

	if (st_overlay)
	{
		if (renderisnewtic)
		{
			for (i = 0; i <= splitscreen; i++)
				LUA_HUD_ClearDrawList(luahuddrawlist_game[i]);
		}

		// No deadview!
		for (i = 0; i <= splitscreen; i++)
		{
			stplyr = &players[displayplayers[i]];
			stplyrnum = i;
			ST_overlayDrawer();
		}

		for (i = 0; i <= splitscreen; i++)
			LUA_HUD_DrawList(luahuddrawlist_game[i]);

		// draw Midnight Channel's overlay ontop
		if (mapheaderinfo[gamemap-1]->typeoflevel & TOL_TV)	// Very specific Midnight Channel stuff.
			ST_MayonakaStatic();
	}

	// Draw a white fade on level opening
	if (timeinmap < 15)
	{
		if (timeinmap <= 5)
			V_DrawFill(0,0,BASEVIDWIDTH,BASEVIDHEIGHT,120); // Pure white on first few frames, to hide SRB2's awful level load artifacts
		else
			V_DrawFadeScreen(120, 15-timeinmap); // Then gradually fade out from there
	}
	ST_drawDebugInfo();
}
