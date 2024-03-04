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

//
// STATUS BAR DATA
//

patch_t *facerankprefix[MAXSKINS]; // ranking
patch_t *facewantprefix[MAXSKINS]; // wanted
patch_t *facemmapprefix[MAXSKINS]; // minimap

patch_t *localfacerankprefix[MAXSKINS]; // ranking
patch_t *localfacewantprefix[MAXSKINS]; // wanted
patch_t *localfacemmapprefix[MAXSKINS]; // minimap

char *facerankprefix_name[MAXSKINS]; // ranking
char *facewantprefix_name[MAXSKINS]; // wanted
char *facemmapprefix_name[MAXSKINS]; // minimap

char *localfacerankprefix_name[MAXSKINS]; // ranking
char *localfacewantprefix_name[MAXSKINS]; // wanted
char *localfacemmapprefix_name[MAXSKINS]; // minimap

// ------------------------------------------
//             status bar overlay
// ------------------------------------------

// icons for overlay
patch_t *sboscore; // Score logo
patch_t *sbotime; // Time logo
patch_t *sbocolon; // Colon for time
patch_t *sboperiod; // Period for time centiseconds
patch_t *livesback; // Lives icon background
static patch_t *nrec_timer; // Timer for NiGHTS records
static patch_t *sborings;
static patch_t *sboover;
static patch_t *timeover;
static patch_t *stlivex;
static patch_t *rrings;
static patch_t *getall; // Special Stage HUD
static patch_t *timeup; // Special Stage HUD
static patch_t *hunthoming[6];
static patch_t *itemhoming[6];
static patch_t *race1;
static patch_t *race2;
static patch_t *race3;
static patch_t *racego;
//static patch_t *ttlnum;
static patch_t *nightslink;
static patch_t *count5;
static patch_t *count4;
static patch_t *count3;
static patch_t *count2;
static patch_t *count1;
static patch_t *count0;
static patch_t *curweapon;
static patch_t *normring;
static patch_t *bouncering;
static patch_t *infinityring;
static patch_t *autoring;
static patch_t *explosionring;
static patch_t *scatterring;
static patch_t *grenadering;
static patch_t *railring;
static patch_t *jumpshield;
static patch_t *forceshield;
static patch_t *ringshield;
static patch_t *watershield;
static patch_t *bombshield;
static patch_t *pityshield;
static patch_t *invincibility;
static patch_t *sneakers;
static patch_t *gravboots;
static patch_t *nonicon;
static patch_t *bluestat;
static patch_t *byelstat;
static patch_t *orngstat;
static patch_t *redstat;
static patch_t *yelstat;
static patch_t *nbracket;
static patch_t *nhud[12];
static patch_t *nsshud;
static patch_t *narrow[9];
static patch_t *nredar[8]; // Red arrow
static patch_t *drillbar;
static patch_t *drillfill[3];
static patch_t *capsulebar;
static patch_t *capsulefill;
patch_t *ngradeletters[7];
static patch_t *minus5sec;
static patch_t *minicaps;
static patch_t *gotrflag;
static patch_t *gotbflag;

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

static huddrawlist_h luahuddrawlist_game;

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
	Z_FreeTags(PU_HUDGFX, PU_HUDGFX);
}

void ST_LoadGraphics(void)
{
	int i;

	// SRB2 border patch
	st_borderpatchnum = W_GetNumForName("GFZFLR01");
	scr_borderpatch = W_CacheLumpNum(st_borderpatchnum, PU_HUDGFX);

	// the original Doom uses 'STF' as base name for all face graphics
	// Graue 04-08-2004: face/name graphics are now indexed by skins
	//                   but load them in R_AddSkins, that gets called
	//                   first anyway
	// cache the status bar overlay icons (fullscreen mode)
	sborings = W_CachePatchName("SBORINGS", PU_HUDGFX);
	sboscore = W_CachePatchName("SBOSCORE", PU_HUDGFX);
	sboover = W_CachePatchName("SBOOVER", PU_HUDGFX);
	timeover = W_CachePatchName("TIMEOVER", PU_HUDGFX);
	stlivex = W_CachePatchName("STLIVEX", PU_HUDGFX);
	livesback = W_CachePatchName("STLIVEBK", PU_HUDGFX);
	rrings = W_CachePatchName("RRINGS", PU_HUDGFX);
	sbotime = W_CachePatchName("SBOTIME", PU_HUDGFX); // Time logo
	sbocolon = W_CachePatchName("SBOCOLON", PU_HUDGFX); // Colon for time
	sboperiod = W_CachePatchName("SBOPERIO", PU_HUDGFX); // Period for time centiseconds
	nrec_timer = W_CachePatchName("NGRTIMER", PU_HUDGFX); // Timer for NiGHTS
	getall = W_CachePatchName("GETALL", PU_HUDGFX); // Special Stage HUD
	timeup = W_CachePatchName("TIMEUP", PU_HUDGFX); // Special Stage HUD
	race1 = W_CachePatchName("RACE1", PU_HUDGFX);
	race2 = W_CachePatchName("RACE2", PU_HUDGFX);
	race3 = W_CachePatchName("RACE3", PU_HUDGFX);
	racego = W_CachePatchName("RACEGO", PU_HUDGFX);
	nightslink = W_CachePatchName("NGHTLINK", PU_HUDGFX);
	count5 = W_CachePatchName("DRWNF0", PU_HUDGFX);
	count4 = W_CachePatchName("DRWNE0", PU_HUDGFX);
	count3 = W_CachePatchName("DRWND0", PU_HUDGFX);
	count2 = W_CachePatchName("DRWNC0", PU_HUDGFX);
	count1 = W_CachePatchName("DRWNB0", PU_HUDGFX);
	count0 = W_CachePatchName("DRWNA0", PU_HUDGFX);

	for (i = 0; i < 6; ++i)
	{
		hunthoming[i] = W_CachePatchName(va("HOMING%d", i+1), PU_HUDGFX);
		itemhoming[i] = W_CachePatchName(va("HOMITM%d", i+1), PU_HUDGFX);
	}

	curweapon = W_CachePatchName("CURWEAP", PU_HUDGFX);
	normring = W_CachePatchName("RINGIND", PU_HUDGFX);
	bouncering = W_CachePatchName("BNCEIND", PU_HUDGFX);
	infinityring = W_CachePatchName("INFNIND", PU_HUDGFX);
	autoring = W_CachePatchName("AUTOIND", PU_HUDGFX);
	explosionring = W_CachePatchName("BOMBIND", PU_HUDGFX);
	scatterring = W_CachePatchName("SCATIND", PU_HUDGFX);
	grenadering = W_CachePatchName("GRENIND", PU_HUDGFX);
	railring = W_CachePatchName("RAILIND", PU_HUDGFX);
	jumpshield = W_CachePatchName("WHTVB0", PU_HUDGFX);
	forceshield = W_CachePatchName("BLTVB0", PU_HUDGFX);
	ringshield = W_CachePatchName("YLTVB0", PU_HUDGFX);
	watershield = W_CachePatchName("ELTVB0", PU_HUDGFX);
	bombshield = W_CachePatchName("BKTVB0", PU_HUDGFX);
	pityshield = W_CachePatchName("GRTVB0", PU_HUDGFX);
	invincibility = W_CachePatchName("PINVB0", PU_HUDGFX);
	sneakers = W_CachePatchName("SHTVB0", PU_HUDGFX);
	gravboots = W_CachePatchName("GBTVB0", PU_HUDGFX);

	tagico = W_CachePatchName("TAGICO", PU_HUDGFX);
	rflagico = W_CachePatchName("RFLAGICO", PU_HUDGFX);
	bflagico = W_CachePatchName("BFLAGICO", PU_HUDGFX);
	rmatcico = W_CachePatchName("RMATCICO", PU_HUDGFX);
	bmatcico = W_CachePatchName("BMATCICO", PU_HUDGFX);
	gotrflag = W_CachePatchName("GOTRFLAG", PU_HUDGFX);
	gotbflag = W_CachePatchName("GOTBFLAG", PU_HUDGFX);
	nonicon = W_CachePatchName("NONICON", PU_HUDGFX);

	// NiGHTS HUD things
	bluestat = W_CachePatchName("BLUESTAT", PU_HUDGFX);
	byelstat = W_CachePatchName("BYELSTAT", PU_HUDGFX);
	orngstat = W_CachePatchName("ORNGSTAT", PU_HUDGFX);
	redstat = W_CachePatchName("REDSTAT", PU_HUDGFX);
	yelstat = W_CachePatchName("YELSTAT", PU_HUDGFX);
	nbracket = W_CachePatchName("NBRACKET", PU_HUDGFX);
	for (i = 0; i < 12; ++i)
		nhud[i] = W_CachePatchName(va("NHUD%d", i+1), PU_HUDGFX);
	nsshud = W_CachePatchName("NSSHUD", PU_HUDGFX);
	minicaps = W_CachePatchName("MINICAPS", PU_HUDGFX);

	for (i = 0; i < 8; ++i)
	{
		narrow[i] = W_CachePatchName(va("NARROW%d", i+1), PU_HUDGFX);
		nredar[i] = W_CachePatchName(va("NREDAR%d", i+1), PU_HUDGFX);
	}

	// non-animated version
	narrow[8] = W_CachePatchName("NARROW9", PU_HUDGFX);

	drillbar = W_CachePatchName("DRILLBAR", PU_HUDGFX);
	for (i = 0; i < 3; ++i)
		drillfill[i] = W_CachePatchName(va("DRILLFI%d", i+1), PU_HUDGFX);
	capsulebar = W_CachePatchName("CAPSBAR", PU_HUDGFX);
	capsulefill = W_CachePatchName("CAPSFILL", PU_HUDGFX);
	minus5sec = W_CachePatchName("MINUS5", PU_HUDGFX);

	for (i = 0; i < 7; ++i)
		ngradeletters[i] = W_CachePatchName(va("GRADE%d", i), PU_HUDGFX);

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
	
	facerankprefix_name[skinnum] = rankstr;
	facewantprefix_name[skinnum] = wantstr;
	facemmapprefix_name[skinnum] = mmapstr;
}

void ST_LoadLocalFaceGraphics(char *rankstr, char *wantstr, char *mmapstr, INT32 skinnum)
{
	localfacerankprefix[skinnum] = W_CachePatchName(rankstr, PU_HUDGFX);
	localfacewantprefix[skinnum] = W_CachePatchName(wantstr, PU_HUDGFX);
	localfacemmapprefix[skinnum] = W_CachePatchName(mmapstr, PU_HUDGFX);
	
	localfacerankprefix_name[skinnum] = rankstr;
	localfacewantprefix_name[skinnum] = wantstr;
	localfacemmapprefix_name[skinnum] = mmapstr;

	//CONS_Printf("Added rank prefix %s\n", rankstr);
	//CONS_Printf("Added want prefix %s\n", wantstr);
	//CONS_Printf("Added mmap prefix %s\n", mmapstr);
}

void ST_ReloadSkinFaceGraphics(void)
{
	INT32 i;

	for (i = 0; i < numskins; i++)
		ST_LoadFaceGraphics(skins[i].facerank, skins[i].facewant, skins[i].facemmap, i);
	
	for (i = 0; i < numlocalskins; i++) {
		ST_LoadLocalFaceGraphics(localskins[i].facerank, localskins[i].facewant, localskins[i].facemmap, i);
	}
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

	luahuddrawlist_game = LUA_HUD_CreateDrawList();
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
		//V_DrawRightAlignedString(320, height - 104, V_MONOSPACE, va("SHIELD: %5x", stplyr->powers[pw_shield]));
		V_DrawRightAlignedString(320, height - 96,  V_MONOSPACE, va("SCALE: %5d%%", (stplyr->mo->scale*100)/FRACUNIT));
		//V_DrawRightAlignedString(320, height - 88,  V_MONOSPACE, va("DASH: %3d/%3d", stplyr->dashspeed>>FRACBITS, FixedMul(stplyr->maxdash,stplyr->mo->scale)>>FRACBITS));
		//V_DrawRightAlignedString(320, height - 80,  V_MONOSPACE, va("AIR: %4d, %3d", stplyr->powers[pw_underwater], stplyr->powers[pw_spacetime]));

		// Flags
		//V_DrawRightAlignedString(304-64, height - 72, V_MONOSPACE, "Flags:");
		//V_DrawString(304-60,             height - 72, (stplyr->jumping) ? V_GREENMAP : V_REDMAP, "JM");
		//V_DrawString(304-40,             height - 72, (stplyr->pflags & PF_JUMPED) ? V_GREENMAP : V_REDMAP, "JD");
		//V_DrawString(304-20,             height - 72, (stplyr->pflags & PF_SPINNING) ? V_GREENMAP : V_REDMAP, "SP");
		//V_DrawString(304,                height - 72, (stplyr->pflags & PF_STARTDASH) ? V_GREENMAP : V_REDMAP, "ST");

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

/*
static void ST_drawScore(void)
{
	// SCORE:
	ST_DrawPatchFromHud(HUD_SCORE, sboscore);
	if (objectplacing)
	{
		if (op_displayflags > UINT16_MAX)
			ST_DrawOverlayPatch(SCX(hudinfo[HUD_SCORENUM].x-tallminus->width), SCY(hudinfo[HUD_SCORENUM].y), tallminus);
		else
			ST_DrawNumFromHud(HUD_SCORENUM, op_displayflags);
	}
	else
		ST_DrawNumFromHud(HUD_SCORENUM, stplyr->score);
}
*/

/*
static void ST_drawTime(void)
{
	INT32 seconds, minutes, tictrn, tics;

	// TIME:
	ST_DrawPatchFromHudWS(HUD_TIME, sbotime);

	if (objectplacing)
	{
		tics    = objectsdrawn;
		seconds = objectsdrawn%100;
		minutes = objectsdrawn/100;
		tictrn  = 0;
	}
	else
	{
		tics = stplyr->realtime;
		seconds = G_TicsToSeconds(tics);
		minutes = G_TicsToMinutes(tics, true);
		tictrn  = G_TicsToCentiseconds(tics);
	}

	if (cv_timetic.value == 1) // Tics only -- how simple is this?
		ST_DrawNumFromHudWS(HUD_SECONDS, tics);
	else
	{
		ST_DrawNumFromHudWS(HUD_MINUTES, minutes); // Minutes
		ST_DrawPatchFromHudWS(HUD_TIMECOLON, sbocolon); // Colon
		ST_DrawPadNumFromHudWS(HUD_SECONDS, seconds, 2); // Seconds

		if (!splitscreen && (cv_timetic.value == 2 || modeattacking)) // there's not enough room for tics in splitscreen, don't even bother trying!
		{
			ST_DrawPatchFromHud(HUD_TIMETICCOLON, sboperiod); // Period
			ST_DrawPadNumFromHud(HUD_TICS, tictrn, 2); // Tics
		}
	}
}
*/

/*
static inline void ST_drawRings(void) // SRB2kart - unused.
{
	INT32 ringnum = max(stplyr->health-1, 0);

	ST_DrawPatchFromHudWS(HUD_RINGS, ((stplyr->health <= 1 && leveltime/5 & 1) ? rrings : sborings));

	if (objectplacing)
		ringnum = op_currentdoomednum;
	else if (!useNightsSS && G_IsSpecialStage(gamemap))
	{
		INT32 i;
		ringnum = 0;
		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].mo && players[i].mo->health > 1)
				ringnum += players[i].mo->health - 1;
	}

	ST_DrawNumFromHudWS(HUD_RINGSNUM, ringnum);
}
*/

/*
static void ST_drawLives(void) // SRB2kart - unused.
{
	const INT32 v_splitflag = (splitscreen && stplyr == &players[displayplayers[0]] ? V_SPLITSCREEN : 0);

	if (!stplyr->skincolor)
		return; // Just joined a server, skin isn't loaded yet!

	// face background
	V_DrawSmallScaledPatch(hudinfo[HUD_LIVESPIC].x, hudinfo[HUD_LIVESPIC].y + (v_splitflag ? -12 : 0),
		V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_HUDTRANS|v_splitflag, livesback);

	// face
	if (stplyr->mo && stplyr->mo->color)
	{
		// skincolor face/super
		UINT8 *colormap = R_GetTranslationColormap(stplyr->skin, stplyr->mo->color, GTC_CACHE);
		patch_t *face = facerankprefix[stplyr->skin];
		if (stplyr->powers[pw_super] || stplyr->pflags & PF_NIGHTSMODE)
			face = facewantprefix[stplyr->skin];
		V_DrawSmallMappedPatch(hudinfo[HUD_LIVESPIC].x, hudinfo[HUD_LIVESPIC].y + (v_splitflag ? -12 : 0),
			V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_HUDTRANS|v_splitflag,face, colormap);
	}
	else if (stplyr->skincolor)
	{
		// skincolor face
		UINT8 *colormap = R_GetTranslationColormap(stplyr->skin, stplyr->skincolor, GTC_CACHE);
		V_DrawSmallMappedPatch(hudinfo[HUD_LIVESPIC].x, hudinfo[HUD_LIVESPIC].y + (v_splitflag ? -12 : 0),
			V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_HUDTRANS|v_splitflag,facerankprefix[stplyr->skin], colormap);
	}

	// name
	if (strlen(skins[stplyr->skin].hudname) > 8)
		V_DrawThinString(hudinfo[HUD_LIVESNAME].x, hudinfo[HUD_LIVESNAME].y + (v_splitflag ? -12 : 0),
			V_HUDTRANS|V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_MONOSPACE|V_YELLOWMAP|v_splitflag, skins[stplyr->skin].hudname);
	else
		V_DrawString(hudinfo[HUD_LIVESNAME].x, hudinfo[HUD_LIVESNAME].y + (v_splitflag ? -12 : 0),
			V_HUDTRANS|V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_MONOSPACE|V_YELLOWMAP|v_splitflag, skins[stplyr->skin].hudname);
	// x
	V_DrawScaledPatch(hudinfo[HUD_LIVESX].x, hudinfo[HUD_LIVESX].y + (v_splitflag ? -4 : 0),
		V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_HUDTRANS|v_splitflag, stlivex);
	// lives
	V_DrawRightAlignedString(hudinfo[HUD_LIVESNUM].x, hudinfo[HUD_LIVESNUM].y + (v_splitflag ? -4 : 0),
		V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_HUDTRANS|v_splitflag, va("%d",stplyr->lives));
}
*/

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
		sub = dupcalc;
		while (count-- > 0)
			sub >>= 1;
		sub = -sub;
	}

	{
		dupcalc = (dupcalc - BASEVIDWIDTH)>>1;
		V_DrawFill(sub - dupcalc, bary+9, ttlnumxpos+dupcalc + 1, 2, 31);
		V_DrawDiag(sub + ttlnumxpos + 1, bary, 11, 31);
		V_DrawFill(sub - dupcalc, bary, ttlnumxpos+dupcalc, 10, gtc);
		V_DrawDiag(sub + ttlnumxpos, bary, 10, gtc);

		if (subttl[0])
			V_DrawRightAlignedString(sub + zonexpos - 8, bary+1, V_ALLOWLOWERCASE, subttl);
		//else
			//V_DrawRightAlignedString(sub + zonexpos - 8, bary+1, V_ALLOWLOWERCASE, va("%s Mode", gametype_cons_t[gametype].strvalue));
	}

	ttlnumxpos += sub;
	lvlttlxpos += sub;
	zonexpos += sub;

	V_DrawLevelTitle(lvlttlxpos, bary-18, 0, lvlttl);

	if (strlen(zonttl) > 0)
		V_DrawLevelTitle(zonexpos, bary+6, 0, zonttl);
	else if (!(mapheaderinfo[gamemap-1]->levelflags & LF_NOZONE))
		V_DrawLevelTitle(zonexpos, bary+6, 0, M_GetText("Zone"));

	if (actnum[0])
		V_DrawLevelTitle(ttlnumxpos+12, bary+6, 0, actnum);
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
		// Countdown timer for Race Mode
		// ...moved to k_kart.c so we can take advantage of the LAPS_Y value

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
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-40, V_HUDTRANS, M_GetText("VIEWPOINT:"));
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-32, V_HUDTRANS|V_ALLOWLOWERCASE, player_names[stplyr-players]);
				}
			}
			else if (!demo.title)
			{
				if (!splitscreen)
				{
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-40, V_HUDTRANSHALF, M_GetText("VIEWPOINT:"));
					V_DrawCenteredString((BASEVIDWIDTH/2), BASEVIDHEIGHT-32, V_HUDTRANSHALF|V_ALLOWLOWERCASE, player_names[stplyr-players]);
				}
				else if (splitscreen == 1)
				{
					char name[MAXPLAYERNAME+12];

					INT32 y = (stplyr == &players[displayplayers[0]]) ? 4 : BASEVIDHEIGHT/2-12;
					sprintf(name, "VIEWPOINT: %s", player_names[stplyr-players]);
					V_DrawRightAlignedThinString(BASEVIDWIDTH-40, y, V_HUDTRANSHALF|V_ALLOWLOWERCASE|K_calcSplitFlags(V_SNAPTOTOP|V_SNAPTOBOTTOM|V_SNAPTORIGHT), name);
				}
				else if (splitscreen)
				{
					V_DrawCenteredThinString((vid.width/vid.dupx)/4, BASEVIDHEIGHT/2 - 12, V_HUDTRANSHALF|V_ALLOWLOWERCASE|K_calcSplitFlags(V_SNAPTOBOTTOM|V_SNAPTOLEFT), player_names[stplyr-players]);
				}
			}
		}

		// This is where we draw all the fun cheese if you have the chasecam off!
		/*if ((stplyr == &players[displayplayers[0]] && !camera[0].chase)
			|| ((splitscreen && stplyr == &players[displayplayers[1]]) && !camera[1].chase)
			|| ((splitscreen > 1 && stplyr == &players[displayplayers[2]]) && !camera[2].chase)
			|| ((splitscreen > 2 && stplyr == &players[displayplayers[3]]) && !camera[3].chase))
		{
			ST_drawFirstPersonHUD();
		}*/
	}

	if (!(netgame || multiplayer) || !hu_showscores)
	{
		if (renderisnewtic)
		{
			LUAh_GameHUD(stplyr, luahuddrawlist_game);
		}
	}

	// draw level title Tails
	if (*mapheaderinfo[gamemap-1]->lvlttl != '\0' && !(hu_showscores && (netgame || multiplayer) && !mapreset) && LUA_HudEnabled(hud_stagetitle))
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
				V_DrawString(2, BASEVIDHEIGHT-40, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF|V_YELLOWMAP, M_GetText("- SPECTATING -"));
				V_DrawString(2, BASEVIDHEIGHT-30, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, itemtxt);
				V_DrawString(2, BASEVIDHEIGHT-20, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, M_GetText("Accelerate - Float"));
				V_DrawString(2, BASEVIDHEIGHT-10, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_HUDTRANSHALF, M_GetText("Brake - Sink"));
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
	static UINT8 skullAnimCounter = 0;
	char *nametodraw;

	skullAnimCounter++;
	skullAnimCounter %= 8;

	nametodraw = demo.titlename;
	while (V_StringWidth(nametodraw, 0) > MAXSTRINGLENGTH*8 - 8)
		nametodraw++;

#define x (BASEVIDWIDTH/2 - 139)
#define y (BASEVIDHEIGHT/2)
	M_DrawTextBox(x, y + 4, MAXSTRINGLENGTH, 1);
	V_DrawString(x + 8, y + 12, V_ALLOWLOWERCASE, nametodraw);
	if (skullAnimCounter < 4)
		V_DrawCharacter(x + 8 + V_StringWidth(nametodraw, 0), y + 12,
			'_' | 0x80, false);

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

	// Doom's status bar only updated if necessary.
	// However, ours updates every frame regardless, so the "refresh" param was removed
	//(void)refresh;

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
			LUA_HUD_ClearDrawList(luahuddrawlist_game);
		}

		// No deadview!
		for (i = 0; i <= splitscreen; i++)
		{
			stplyr = &players[displayplayers[i]];
			stplyrnum = i;
			ST_overlayDrawer();
		}

		LUA_HUD_DrawList(luahuddrawlist_game);

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
