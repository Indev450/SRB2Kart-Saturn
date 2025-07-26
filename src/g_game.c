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
/// \file  g_game.c
/// \brief game loop functions, events handling

#include "doomdef.h"
#include "console.h"
#include "d_main.h"
#include "d_clisrv.h"
#include "d_player.h"
#include "f_finale.h"
#include "filesrch.h" // for refreshdirmenu
#include "p_setup.h"
#include "p_saveg.h"
#include "i_time.h"
#include "i_system.h"
#include "am_map.h"
#include "m_random.h"
#include "p_local.h"
#include "r_draw.h"
#include "r_main.h"
#include "s_sound.h"
#include "g_game.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_argv.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "z_zone.h"
#include "i_video.h"
#include "byteptr.h"
#include "i_joy.h"
#include "r_local.h"
#include "r_things.h"
#include "y_inter.h"
#include "v_video.h"
#include "dehacked.h" // get_number (for ghost thok)
#include "lua_script.h"	// LUA_ArchiveDemo and LUA_UnArchiveDemo
#include "lua_hook.h"
#include "lua_libs.h"	// gL (Lua state)
#include "b_bot.h"
#include "m_cond.h" // condition sets
#include "md5.h" // demo checksums
#include "k_director.h" // SRB2kart
#include "k_kart.h" // SRB2kart
#include "k_stats.h" // SRB2kart
#include "r_fps.h" // frame interpolation/uncapped

#ifdef HAVE_DISCORDRPC
#include "discord.h"
#endif

gameaction_t gameaction;
gamestate_t gamestate = GS_NULL;
UINT8 ultimatemode = false;

boolean botingame;
UINT8 botskin;
UINT8 botcolor;

JoyType_t Joystick[MAXSPLITSCREENPLAYERS];

// 1024 bytes is plenty for a savegame
#define SAVEGAMESIZE (1024)

// SRB2kart
char gamedatafilename[64] = "kartdata.dat";
char timeattackfolder[64] = "kart";
char customversionstring[32] = "\0";

static void G_DoCompleted(void);
static void G_DoStartContinue(void);
static void G_DoContinued(void);
static void G_DoWorldDone(void);
static void G_DoStartVote(void);

music_t mapmusic;

INT16 gamemap = 1;
INT16 maptol;
UINT8 globalweather = 0;
INT32 curWeather = PRECIP_NONE;
INT32 cursaveslot = -1; // Auto-save 1p savegame slot
INT16 lastmapsaved = 0; // Last map we auto-saved at
boolean gamecomplete = false;

UINT16 mainwads = 0;
boolean modifiedgame = false; // Set if homebrew PWAD stuff has been added.
boolean majormods = false; // Set if Lua/Gameplay SOC/replacement map has been added.
boolean savemoddata = false;
UINT8 paused;
UINT8 modeattacking = ATTACKING_NONE;
boolean imcontinuing = false;
boolean runemeraldmanager = false;

boolean netgame; // only true if packets are broadcast
boolean multiplayer;
boolean playeringame[MAXPLAYERS];
boolean addedtogame;
player_t players[MAXPLAYERS];

INT32 consoleplayer; // player taking events and displaying
INT32 displayplayers[MAXSPLITSCREENPLAYERS]; // view being displayed

tic_t gametic;
tic_t levelstarttic; // gametic at level start
UINT32 totalrings; // for intermission
INT16 lastmap; // last level you were at (returning from special stages)
tic_t timeinmap; // Ticker for time spent in level (used for levelcard display)

INT16 spstage_start;
INT16 sstage_start;
INT16 sstage_end;

boolean looptitle = true;
boolean useNightsSS = false;

UINT8 skincolor_redteam = SKINCOLOR_RED;
UINT8 skincolor_blueteam = SKINCOLOR_BLUE;
UINT8 skincolor_redring = SKINCOLOR_RED;
UINT8 skincolor_bluering = SKINCOLOR_STEEL;

tic_t countdowntimer = 0;
boolean countdowntimeup = false;

cutscene_t *cutscenes[128];

INT16 nextmapoverride;
boolean skipstats;

// Pointers to each CTF flag
mobj_t *redflag;
mobj_t *blueflag;
// Pointers to CTF spawn location
mapthing_t *rflagpoint;
mapthing_t *bflagpoint;

struct quake quake;

// Map Header Information
mapheader_t* mapheaderinfo[NUMMAPS] = {NULL};

static boolean exitgame = false;
static boolean retrying = false;

UINT8 stagefailed; // Used for GEMS BONUS? Also to see if you beat the stage.

UINT16 emeralds;
UINT32 token; // Number of tokens collected in a level
UINT32 tokenlist; // List of tokens collected
INT32 tokenbits; // Used for setting token bits

// Old Special Stage
INT32 sstimer; // Time allotted in the special stage

boolean gamedataloaded = false;

// Time attack data for levels
// These are dynamically allocated for space reasons now
recorddata_t *mainrecords[NUMMAPS]   = {NULL};
UINT8 mapvisited[NUMMAPS];

UINT32 bluescore, redscore; // CTF and Team Match team scores

// ring count... for PERFECT!
INT32 nummaprings = 0;

// box respawning in battle mode
INT32 nummapboxes = 0;
INT32 numgotboxes = 0;

// Elminates unnecessary searching.
boolean CheckForBustableBlocks;
boolean CheckForBouncySector;
boolean CheckForQuicksand;
boolean CheckForMarioBlocks;
boolean CheckForFloatBob;
boolean CheckForReverseGravity;

// Powerup durations
UINT16 invulntics = 20*TICRATE;
UINT16 sneakertics = 20*TICRATE;
UINT16 flashingtics = 3*TICRATE/2; // SRB2kart
UINT16 tailsflytics = 8*TICRATE;
UINT16 underwatertics = 30*TICRATE;
UINT16 spacetimetics = 11*TICRATE + (TICRATE/2);
UINT16 extralifetics = 4*TICRATE;

// SRB2kart
const tic_t introtime = 108+5; // plus 5 for white fade
const tic_t starttime = 6*TICRATE + (3*TICRATE/4);
const tic_t raceexittime = 5*TICRATE + (2*TICRATE/3);
const tic_t battleexittime = 8*TICRATE;
const INT32 hyudorotime = 7*TICRATE;
const INT32 stealtime = TICRATE/2;
const INT32 sneakertime = TICRATE + (TICRATE/3);
const INT32 itemtime = 8*TICRATE;
const INT32 comebacktime = 10*TICRATE;
const INT32 bumptime = 6;
const INT32 wipeoutslowtime = 20;
const INT32 wantedreduce = 5*TICRATE;
const INT32 wantedfrequency = 10*TICRATE;

INT32 gameovertics = 15*TICRATE;

UINT8 use1upSound = 0;
UINT8 maxXtraLife = 2; // Max extra lives from rings

UINT8 introtoplay;
UINT8 creditscutscene;

// Emerald locations
mobj_t *hunt1;
mobj_t *hunt2;
mobj_t *hunt3;

tic_t racecountdown, exitcountdown; // for racing

fixed_t gravity;
fixed_t mapobjectscale;

struct maplighting maplighting;

INT16 autobalance; //for CTF team balance
INT16 teamscramble; //for CTF team scramble
INT16 scrambleplayers[MAXPLAYERS]; //for CTF team scramble
INT16 scrambleteams[MAXPLAYERS]; //for CTF team scramble
INT16 scrambletotal; //for CTF team scramble
INT16 scramblecount; //for CTF team scramble

INT32 cheats; //for multiplayer cheat commands

// SRB2Kart
// Cvars that we don't want changed mid-game
UINT8 gamespeed; // Game's current speed (or difficulty, or cc, or etc); 0 for easy, 1 for normal, 2 for hard
boolean encoremode = false; // Encore Mode currently enabled?
boolean prevencoremode;
boolean franticitems; // Frantic items currently enabled?
boolean comeback; // Battle Mode's karma comeback is on/off

// Voting system
INT16 votelevels[4][2]; // Levels that were rolled by the host
SINT8 votes[MAXPLAYERS]; // Each player's vote
SINT8 pickedvote; // What vote the host rolls

// Server-sided, synched variables
SINT8 battlewanted[4]; // WANTED players in battle, worth x2 points
tic_t wantedcalcdelay; // Time before it recalculates WANTED
tic_t indirectitemcooldown; // Cooldown before any more Shrink, SPB, or any other item that works indirectly is awarded
tic_t hyubgone; // Cooldown before hyudoro is allowed to be rerolled
tic_t mapreset; // Map reset delay when enough players have joined an empty game
UINT8 nospectategrief; // How many players need to be in-game to eliminate last; for preventing spectate griefing
boolean thwompsactive; // Thwomps activate on lap 2
SINT8 spbplace; // SPB exists, give the person behind better items
boolean startedInFreePlay; // Map was started in free play

// Client-sided, unsynched variables (NEVER use in anything that needs to be synced with other players)
boolean legitimateexit; // Did this client actually finish the match?
boolean comebackshowninfo; // Have you already seen the "ATTACK OR PROTECT" message?
static INT16 randmapbuffer[NUMMAPS+1]; // Buffer for maps RandMap is allowed to roll

tic_t hidetime;

// Grading
UINT32 timesBeaten;
UINT32 timesBeatenWithEmeralds;
//UINT32 timesBeatenUltimate;

INT16 prevmap, nextmap;

// Analog Control
void SendWeaponPref(void);
void SendWeaponPref2(void);
void SendWeaponPref3(void);
void SendWeaponPref4(void);

// don't mind me putting these here, I was lazy to figure out where else I could put those without blowing up the compiler.

// chat timer thingy
static CV_PossibleValue_t chattime_cons_t[] = {{1, "MIN"}, {999, "MAX"}, {0, NULL}};
consvar_t cv_chattime = {"chattime", "8", CV_SAVE, chattime_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// chatwidth
static CV_PossibleValue_t chatwidth_cons_t[] = {{64, "MIN"}, {150, "MAX"}, {0, NULL}};
consvar_t cv_chatwidth = {"chatwidth", "150", CV_SAVE, chatwidth_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// chatheight
static CV_PossibleValue_t chatheight_cons_t[] = {{6, "MIN"}, {22, "MAX"}, {0, NULL}};
consvar_t cv_chatheight = {"chatheight", "8", CV_SAVE, chatheight_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// chat notifications (do you want to hear beeps? I'd understand if you didn't.)
consvar_t cv_chatnotifications = {"chatnotifications", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// chat spam protection (why would you want to disable that???)
consvar_t cv_chatspamprotection = {"chatspamprotection", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// minichat text background
consvar_t cv_chatbacktint = {"chatbacktint", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// old shit console chat. (mostly exists for stuff like terminal, not because I cared if anyone liked the old chat.)
static CV_PossibleValue_t consolechat_cons_t[] = {{0, "Window"}, {1, "Console"}, {2, "Window (Hidden)"}, {0, NULL}};
consvar_t cv_consolechat = {"chatmode", "Window", CV_SAVE, consolechat_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// Pause game upon window losing focus
consvar_t cv_pauseifunfocused = {"pauseifunfocused", "Yes", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

// Display song credits
static CV_PossibleValue_t songcredits_cons_t[] = {{0, "Off"}, {1, "Default"}, {2, "Box"}, {0, NULL}};
consvar_t cv_songcredits = {"songcredits", "Default", CV_SAVE, songcredits_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// Show "FREE PLAY" when you're alone. :(
consvar_t cv_showfreeplay = { "showfreeplay", "Yes", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

// We can disable special tunes!
static CV_PossibleValue_t powermusic_cons_t[] = {{0, "Off"}, {1, "On"}, {2, "SFX"}, {0, NULL}};
consvar_t cv_growmusic  = {"growmusic",  "On", CV_SAVE, powermusic_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_supermusic = {"supermusic", "On", CV_SAVE, powermusic_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_invertmouse = {"invertmouse", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t joyaxis_cons_t[] = {{0, "None"},
{1, "Left X"}, {2, "Left Y"}, {-1, "Left X-"}, {-2, "Left Y-"},
#if JOYAXISSET > 1
{3, "Right X"}, {4, "Right Y"}, {-3, "Right X-"}, {-4, "Right Y-"},
#endif
#if JOYAXISSET > 2
{5, "L Trigger"}, {6, "R Trigger"}, {-5, "L Trigger-"}, {-6, "R Trigger-"},
#endif
#if JOYAXISSET > 3
{7, "U-Axis"}, {8, "V-Axis"}, {-7, "U-Axis-"}, {-8, "V-Axis-"},
#endif
 {0, NULL}};
#if JOYAXISSET > 4
"More Axis Sets"
#endif

static CV_PossibleValue_t deadzone_cons_t[] = {{FRACUNIT/16, "MIN"}, {FRACUNIT, "MAX"}, {0, NULL}};

consvar_t cv_turnaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_turn",  "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_turn", "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_turn", "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_turn", "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_moveaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_move",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_move", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_move", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_move", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_camstrafeaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_camstrafe",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_camstrafe", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_camstrafe", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_camstrafe", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_camturnaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_camturn",  "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_camturn", "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_camturn", "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_camturn", "Left X", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_brakeaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_brake",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_brake", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_brake", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_brake", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_aimaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_aim",  "Left Y", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_aim", "Left Y", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_aim", "Left Y", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_aim", "Left Y", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_lookaxis[MAXSPLITSCREENPLAYERS] = {
	 {"joyaxis_look",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	 {"joyaxis2_look", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	 {"joyaxis3_look", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	 {"joyaxis4_look", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_fireaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_fire",  "L Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_fire", "L Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_fire", "L Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_fire", "L Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_driftaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_drift",  "R Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_drift", "R Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_drift", "R Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_drift", "R Trigger", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_lookbackaxis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_lookback",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_lookback", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_lookback", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_lookback", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_custom1axis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_custom1",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_custom1", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_custom1", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_custom1", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_custom2axis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_custom2",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_custom2", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_custom2", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_custom2", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_custom3axis[MAXSPLITSCREENPLAYERS] = {
	{"joyaxis_custom3",  "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis2_custom3", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis3_custom3", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joyaxis4_custom3", "None", CV_SAVE, joyaxis_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_xdeadzone[MAXSPLITSCREENPLAYERS] = {
	{"joy_xdeadzone",  "0.3", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joy2_xdeadzone", "0.3", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joy3_xdeadzone", "0.3", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joy4_xdeadzone", "0.3", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

consvar_t cv_ydeadzone[MAXSPLITSCREENPLAYERS] = {
	{"joy_ydeadzone",  "0.5", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joy2_ydeadzone", "0.5", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joy3_ydeadzone", "0.5", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL},
	{"joy4_ydeadzone", "0.5", CV_FLOAT|CV_SAVE, deadzone_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL}
};

static CV_PossibleValue_t driftsparkpulse_t[] = {{0, "MIN"}, {FRACUNIT*3, "MAX"}, {0, NULL}};
consvar_t cv_driftsparkpulse = {"driftsparkpulse", "1.4", CV_FLOAT | CV_SAVE, driftsparkpulse_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t cechotoggle_t[] = {{0, "Off"}, {1, "On"}, {2, "Console"}, {0, NULL}};
consvar_t cv_cechotoggle = {"show_cecho", "On", CV_SAVE, cechotoggle_t, NULL, 0, NULL, NULL, 0, 0, NULL};

#if MAXPLAYERS > 16
#error "please update player_name table using the new value for MAXPLAYERS"
#endif

#ifdef SEENAMES
player_t *seenplayer; // player we're aiming at right now
#endif

char player_names[MAXPLAYERS][MAXPLAYERNAME+1] =
{
	"Player 1",
	"Player 2",
	"Player 3",
	"Player 4",
	"Player 5",
	"Player 6",
	"Player 7",
	"Player 8",
	"Player 9",
	"Player 10",
	"Player 11",
	"Player 12",
	"Player 13",
	"Player 14",
	"Player 15",
	"Player 16"
}; // SRB2kart - removed Players 17 through 32

INT32 player_name_changes[MAXPLAYERS];

INT16 rw_maximums[NUM_WEAPONS] =
{
	800, // MAX_INFINITY
	400, // MAX_AUTOMATIC
	100, // MAX_BOUNCE
	50,  // MAX_SCATTER
	100, // MAX_GRENADE
	50,  // MAX_EXPLOSION
	50   // MAX_RAIL
};

// Allocation for time and nights data
void G_AllocMainRecordData(INT16 i)
{
	if (!mainrecords[i])
		mainrecords[i] = Z_Malloc(sizeof(recorddata_t), PU_STATIC, NULL);
	memset(mainrecords[i], 0, sizeof(recorddata_t));
}

// MAKE SURE YOU SAVE DATA BEFORE CALLING THIS
void G_ClearRecords(void)
{
	INT16 i;
	for (i = 0; i < NUMMAPS; ++i)
	{
		if (mainrecords[i])
		{
			Z_Free(mainrecords[i]);
			mainrecords[i] = NULL;
		}
	}
}

tic_t G_GetBestTime(INT16 map)
{
	if (!mainrecords[map-1] || mainrecords[map-1]->time <= 0)
		return (tic_t)UINT32_MAX;

	return mainrecords[map-1]->time;
}

// kinda hacky way to do this, but this sets the game to use a seperate savefile if you have addons loaded
static void G_SetSaveGameModified(void)
{
	size_t filenamelen;

	if (savemoddata)
		return;

	// save vanilla data just to be sure
	G_SaveGameData(true);

	savemoddata = true;

	strlcpy(gamedatafilename, "modkartdata.dat", sizeof (gamedatafilename));
	strlwr(gamedatafilename);

	// Also save a time attack folder
	filenamelen = strlen(gamedatafilename)-4;  // Strip off the extension
	filenamelen = min(filenamelen, sizeof (timeattackfolder));
	memcpy(timeattackfolder, gamedatafilename, filenamelen);
	timeattackfolder[min(filenamelen, sizeof (timeattackfolder) - 1)] = '\0';

	strcpy(savegamename, timeattackfolder);
	strlcat(savegamename, "%u.ssg", sizeof(savegamename));
	// can't use sprintf since there is %u in savegamename
	strcatbf(savegamename, srb2home, PATHSEP);

	G_LoadGameData();

	// unlock EVERYTHING.
	for (UINT8 i = 0; i < MAXUNLOCKABLES; i++)
	{
		if (!unlockables[i].conditionset)
			continue;
		if (!unlockables[i].unlocked)
		{
			unlockables[i].unlocked = true;
		}
	}
}

// for consistency among messages: this modifies the game and removes savemoddata.
void G_SetGameModified(boolean silent, boolean major)
{
	if ((majormods && modifiedgame) || !mainwads || (refreshdirmenu & REFRESHDIR_GAMEDATA)) // new gamedata amnesty?
		return;

	modifiedgame = true;

	if (!major)
		return;

	//savemoddata = false; -- there is literally no reason to do this anymore.
	majormods = true;

	// should this only be done when you load a "major" gameplay modifieng addon?
	G_SetSaveGameModified();

	if (!silent)
		CONS_Alert(CONS_NOTICE, M_GetText("Record Attack data will be saved to a seperate save file.\n"));

	// If in record attack recording, cancel it.
	if (modeattacking)
		M_EndModeAttackRun();
}

void G_SetWadModified(boolean silent, boolean major, UINT16 wadnum)
{
	// now that we have a wad that is actually well, a gameplay changing mod
	// (for later)
	wadfiles[wadnum]->majormod = true;

	// set our game to be marked as modified.
	G_SetGameModified(silent, major);
}

/** Builds an original game map name from a map number.
  * The complexity is due to MAPA0-MAPZZ.
  *
  * \param map Map number.
  * \return Pointer to a static buffer containing the desired map name.
  * \sa M_MapNumber
  */
const char *G_BuildMapName(INT32 map)
{
	static char mapname[10] = "MAPXX"; // internal map name (wad resource name)

	I_Assert(map >= 0);
	I_Assert(map <= NUMMAPS);

	if (map == 0) // hack???
	{
		if (gamestate == GS_TITLESCREEN)
			map = -1;
		else if (gamestate == GS_LEVEL)
			map = gamemap-1;
		else
			map = prevmap;
		map = G_RandMap(G_TOLFlag(cv_newgametype.value), map, false, 0, false, NULL)+1;
	}


	if (map < 100 && map >= 0) // ...but why use signed integer in first place? idk but this prevents warning (and potential buffer overflow lol)
		sprintf(&mapname[3], "%.2d", map);
	else
	{
		mapname[3] = (char)('A' + (char)((map - 100) / 36));
		if ((map - 100) % 36 < 10)
			mapname[4] = (char)('0' + (char)((map - 100) % 36));
		else
			mapname[4] = (char)('A' + (char)((map - 100) % 36) - 10);
		mapname[5] = '\0';
	}

	return mapname;
}

/** Clips the console player's mouse aiming to the current view.
  * Used whenever the player view is changed manually.
  *
  * \param aiming Pointer to the vertical angle to clip.
  * \return Short version of the clipped angle for building a ticcmd.
  */
INT16 G_ClipAimingPitch(INT32 *aiming)
{
	static const INT32 limitangle = ANGLE_90 - 1;

	if (*aiming > limitangle)
		*aiming = limitangle;
	else if (*aiming < -limitangle)
		*aiming = -limitangle;

	return (INT16)((*aiming)>>16);
}

INT16 G_SoftwareClipAimingPitch(INT32 *aiming)
{
	// note: the current software mode implementation doesn't have true perspective
	static const INT32 limitangle = ANGLE_90 - ANG10; // Some viewing fun, but not too far down...

	if (*aiming > limitangle)
		*aiming = limitangle;
	else if (*aiming < -limitangle)
		*aiming = -limitangle;

	return (INT16)((*aiming)>>16);
}

INT32 JoyAxis(axis_input_e axissel, UINT8 player)
{
	INT32 retaxis;
	INT32 axisval;
	boolean flp = false;

	UINT8 pnum = player-1;

	//find what axis to get
	switch (axissel)
	{
		case AXISTURN:
			axisval = cv_turnaxis[pnum].value;
			break;
		case AXISMOVE:
			axisval = cv_moveaxis[pnum].value;
			break;
		case AXISCAMTURN:
			axisval = cv_camturnaxis[pnum].value;
			break;
		case AXISCAMSTRAFE:
			axisval = cv_camstrafeaxis[pnum].value;
			break;
		case AXISBRAKE:
			axisval = cv_brakeaxis[pnum].value;
			break;
		case AXISAIM:
			axisval = cv_aimaxis[pnum].value;
			break;
		case AXISLOOK:
			axisval = cv_lookaxis[pnum].value;
			break;
		case AXISFIRE:
			axisval = cv_fireaxis[pnum].value;
			break;
		case AXISDRIFT:
			axisval = cv_driftaxis[pnum].value;
			break;
		case AXISLOOKBACK:
			axisval = cv_lookbackaxis[pnum].value;
			break;
		case AXISCUSTOM1:
			axisval = cv_custom1axis[pnum].value;
			break;
		case AXISCUSTOM2:
			axisval = cv_custom2axis[pnum].value;
			break;
		case AXISCUSTOM3:
			axisval = cv_custom3axis[pnum].value;
			break;
		default:
			return 0;
	}

	if (axisval < 0) //odd -axises
	{
		axisval = -axisval;
		flp = true;
	}

	if (axisval > JOYAXISSET*2 || axisval == 0) //not there in array or None
		return 0;

	if (axisval % 2)
	{
		axisval /= 2;
		retaxis = joyxmove[pnum][axisval];

		if (retaxis < (-JOYAXISRANGE))
			retaxis = -JOYAXISRANGE;
		if (retaxis > (+JOYAXISRANGE))
			retaxis = +JOYAXISRANGE;
		if (!Joystick[pnum].bGamepadStyle && axissel < AXISDEAD)
		{
			const INT32 jdeadzone = ((JOYAXISRANGE-1) * cv_xdeadzone[pnum].value) >> FRACBITS;
			if (abs(retaxis) <= jdeadzone)
				return 0;
		}

		if (flp)
			retaxis = -retaxis; // flip it around

		return retaxis;
	}
	else
	{
		axisval--;
		axisval /= 2;
		retaxis = joyymove[pnum][axisval];

		if (retaxis < (-JOYAXISRANGE))
			retaxis = -JOYAXISRANGE;
		if (retaxis > (+JOYAXISRANGE))
			retaxis = +JOYAXISRANGE;
		if (!Joystick[pnum].bGamepadStyle && axissel < AXISDEAD)
		{
			const INT32 jdeadzone = ((JOYAXISRANGE-1) * cv_ydeadzone[pnum].value) >> FRACBITS;
			if (abs(retaxis) <= jdeadzone)
				return 0;
		}

		if (flp)
			retaxis = -retaxis; // flip it around

		return retaxis;
	}
}

boolean InputDown(INT32 gc, UINT8 p)
{
	switch (p)
	{
		case 2:
			return PLAYER2INPUTDOWN(gc);
		case 3:
			return PLAYER3INPUTDOWN(gc);
		case 4:
			return PLAYER4INPUTDOWN(gc);
		default:
			return PLAYER1INPUTDOWN(gc);
	}
}

INT32 localaiming[MAXSPLITSCREENPLAYERS];
angle_t localangle[MAXSPLITSCREENPLAYERS];
boolean camspin[MAXSPLITSCREENPLAYERS];

static fixed_t forwardmove[2] = {25<<FRACBITS>>16, 50<<FRACBITS>>16};
static fixed_t sidemove[2] = {2<<FRACBITS>>16, 4<<FRACBITS>>16};
static fixed_t angleturn[3] = {KART_FULLTURN/2, KART_FULLTURN, KART_FULLTURN/4}; // + slow turn

//
// G_BuildLocalTiccmd
// extremely basic cut down ticcmd builder
// for spectator and freecam
// this does not make the player move at all but keeps important things working
//
static void G_BuildLocalTiccmd(ticcmd_t *cmd, UINT8 ssplayer, boolean freecam)
{
	boolean moveinput = false;
	INT32 axis = 0;
	const boolean usejoystick = (cv_usejoystick[(ssplayer-1)].value);

	// check for inputs and return button commands
	// for stuff like joining with item button, saltyhop, honking, etc.
#define CHECKINPUT(button, AXIS, buttflag) \
	axis = JoyAxis(AXIS, ssplayer);        \
	if (InputDown(button, ssplayer) || (usejoystick && axis > 0)) cmd->buttons |= buttflag;

	CHECKINPUT(gc_fire, AXISFIRE, BT_ATTACK);
	CHECKINPUT(gc_drift, AXISDRIFT, BT_DRIFT);
	CHECKINPUT(gc_custom1, AXISCUSTOM1, BT_CUSTOM1);
	CHECKINPUT(gc_custom2, AXISCUSTOM2, BT_CUSTOM2);
	CHECKINPUT(gc_custom3, AXISCUSTOM3, BT_CUSTOM3);

	// we dont need the rest of this if were in freecam state
	if (freecam)
	{
		return;
	}

	CHECKINPUT(gc_accelerate, AXISMOVE, BT_ACCELERATE);
	CHECKINPUT(gc_brake, AXISBRAKE, BT_BRAKE);

#undef CHECKINPUT

	moveinput = (InputDown(gc_turnleft, ssplayer) || InputDown(gc_turnright, ssplayer)
	|| InputDown(gc_aimforward, ssplayer) || InputDown(gc_aimbackward, ssplayer) ||
	(usejoystick && JoyAxis(AXISAIM, ssplayer) != 0) || (usejoystick && JoyAxis(AXISTURN, ssplayer) != 0));

	axis = JoyAxis(AXISLOOKBACK, ssplayer);
	camspin[ssplayer-1] = (InputDown(gc_lookback, ssplayer) || (usejoystick && axis > 0));

	// Reset to our spec player if we watch someone else.
	if ((moveinput || cmd->buttons)
		&& displayplayers[0] != consoleplayer && ssplayer == 1)
	{
		if (cv_director.value)
			CV_SetValue(&cv_director, 0);

		displayplayers[0] = consoleplayer;
		R_ResetViewInterpolation(0);
		camera[0].reset_aiming = true;
	}
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void G_BuildTiccmd(ticcmd_t *cmd, INT32 realtics, UINT8 ssplayer)
{
	INT32 laim, th, tspeed, forward, side, axis;

	// these ones used for multiple conditions
	boolean turnleft, turnright;
	boolean usejoystick, rd;
	angle_t lang;

	static INT32 turnheld[MAXSPLITSCREENPLAYERS]; // for accelerative turning
	static boolean resetdown[MAXSPLITSCREENPLAYERS]; // don't cam reset every frame

	if (demo.playback)
		return;

	const UINT8 forplayer = (ssplayer-1);
	player_t *player = ((ssplayer == 1) ? &players[consoleplayer] : &players[displayplayers[forplayer]]);

	camera_t *thiscam = &camera[forplayer];
	const boolean freecam = camera[forplayer].freecam;

	const boolean analogjoystickmove = cv_usejoystick[forplayer].value && !Joystick[forplayer].bGamepadStyle;
	const boolean gamepadjoystickmove = cv_usejoystick[forplayer].value && Joystick[forplayer].bGamepadStyle;

	lang = localangle[forplayer];
	laim = localaiming[forplayer];
	th = turnheld[forplayer];
	rd = resetdown[forplayer];

	switch (ssplayer)
	{
		case 2:
			G_CopyTiccmd(cmd, I_BaseTiccmd2(), 1);
			break;
		case 3:
			G_CopyTiccmd(cmd, I_BaseTiccmd3(), 1);
			break;
		case 4:
			G_CopyTiccmd(cmd, I_BaseTiccmd4(), 1);
			break;
		case 1:
		default:
			G_CopyTiccmd(cmd, I_BaseTiccmd(), 1); // empty, or external driver
			break;
	}

	// why build a ticcmd if we're paused?
	// Or, for that matter, if we're being reborn.
	// Kart, don't build a ticcmd if someone is resynching or the server is stopped too so we don't fly off course in bad conditions
	if (paused || P_AutoPause() || (gamestate == GS_LEVEL && player->playerstate == PST_REBORN) || hu_resynching)
	{
		cmd->angleturn = (INT16)(lang >> 16);
		cmd->aiming = G_ClipAimingPitch(&laim);
		return;
	}

	// dumbass thing so we can use a few buttons but dont accidentally drive away
	if (player->spectator || freecam)
	{
		cmd->angleturn = (INT16)(lang >> 16);
		G_BuildLocalTiccmd(cmd, ssplayer, freecam);

		// let lua override everything
		if (gamestate == GS_LEVEL)
			LUA_HookTiccmd(player, cmd, HOOK(PlayerCmd));

		return;
	}

	usejoystick = (analogjoystickmove || gamepadjoystickmove);
	turnright = InputDown(gc_turnright, ssplayer);
	turnleft = InputDown(gc_turnleft, ssplayer);

	axis = JoyAxis(AXISTURN, ssplayer);

	if (encoremode)
	{
		turnright ^= turnleft; // swap these using three XORs
		turnleft ^= turnright;
		turnright ^= turnleft;
		axis = -axis;
	}

	if (gamepadjoystickmove && axis != 0)
	{
		turnright = turnright || (axis > 0);
		turnleft = turnleft || (axis < 0);
	}
	forward = side = 0;

	// use two stage accelerative turning
	// on the keyboard and joystick
	if (turnleft || turnright)
		th += realtics;
	else
		th = 0;

	if (th < SLOWTURNTICS)
		tspeed = cv_turnsmooth.value == 2 ? 2 : 0; // slow turn
	else
		tspeed = 1;

	cmd->driftturn = 0;

	// let movement keys cancel each other out
	if (turnright && !(turnleft))
	{
		cmd->angleturn = (INT16)(cmd->angleturn - (angleturn[tspeed]));
		cmd->driftturn = (INT16)(cmd->driftturn - (angleturn[tspeed]));
		side += sidemove[1];
	}
	else if (turnleft && !(turnright))
	{
		cmd->angleturn = (INT16)(cmd->angleturn + (angleturn[tspeed]));
		cmd->driftturn = (INT16)(cmd->driftturn + (angleturn[tspeed]));
		side -= sidemove[1];
	}

	if (analogjoystickmove && axis != 0)
	{
		// JOYAXISRANGE should be 1023 (divide by 1024)
		cmd->angleturn = (INT16)(cmd->angleturn - (((axis * angleturn[1]) >> 10))); // ANALOG!
		cmd->driftturn = (INT16)(cmd->driftturn - (((axis * angleturn[1]) >> 10)));
		side += ((axis * sidemove[0]) >> 10);
	}

	if (cv_mouseturn.value)
	{
		//THIS WORKS WTF????????
		cmd->angleturn = (INT16)(cmd->angleturn - ((mousex*(encoremode ? -1 : 1)*8)));
		cmd->driftturn = (INT16)(cmd->driftturn - ((mousex*(encoremode ? -1 : 1)*8)));
	}

	if (objectplacing) // SRB2Kart: spectators need special controls // not anymore huehuehue
	{
		axis = JoyAxis(AXISMOVE, ssplayer);
		if (InputDown(gc_accelerate, ssplayer) || (usejoystick && axis > 0))
			cmd->buttons |= BT_ACCELERATE;
		axis = JoyAxis(AXISBRAKE, ssplayer);
		if (InputDown(gc_brake, ssplayer) || (usejoystick && axis > 0))
			cmd->buttons |= BT_BRAKE;
		axis = JoyAxis(AXISAIM, ssplayer);
		if (InputDown(gc_aimforward, ssplayer) || (usejoystick && axis < 0))
			forward += forwardmove[1];
		if (InputDown(gc_aimbackward, ssplayer) || (usejoystick && axis > 0))
			forward -= forwardmove[1];
	}
	else
	{
		// forward with key or button // SRB2kart - we use an accel/brake instead of forward/backward.
		axis = JoyAxis(AXISMOVE, ssplayer);
		if (InputDown(gc_accelerate, ssplayer) || (gamepadjoystickmove && axis > 0) || player->kartstuff[k_sneakertimer])
		{
			cmd->buttons |= BT_ACCELERATE;
			forward = forwardmove[1];	// 50
		}
		else if (analogjoystickmove && axis > 0)
		{
			cmd->buttons |= BT_ACCELERATE;
			// JOYAXISRANGE is supposed to be 1023 (divide by 1024)
			forward += ((axis * forwardmove[1]) / (JOYAXISRANGE-1));
		}

		axis = JoyAxis(AXISBRAKE, ssplayer);
		if (InputDown(gc_brake, ssplayer) || (gamepadjoystickmove && axis > 0))
		{
			cmd->buttons |= BT_BRAKE;
			forward -= forwardmove[0];	// 25 - Halved value so clutching is possible
		}
		else if (analogjoystickmove && axis > 0)
		{
			cmd->buttons |= BT_BRAKE;
			// JOYAXISRANGE is supposed to be 1023 (divide by 1024)
			forward -= ((axis * forwardmove[0]) / (JOYAXISRANGE-1));
		}

		// But forward/backward IS used for aiming.
		axis = JoyAxis(AXISAIM, ssplayer);
		if (InputDown(gc_aimforward, ssplayer) || (usejoystick && axis < 0))
			cmd->buttons |= BT_FORWARD;
		if (InputDown(gc_aimbackward, ssplayer) || (usejoystick && axis > 0))
			cmd->buttons |= BT_BACKWARD;
	}

	// fire with any button/key
	axis = JoyAxis(AXISFIRE, ssplayer);
	if (InputDown(gc_fire, ssplayer) || (usejoystick && axis > 0))
		cmd->buttons |= BT_ATTACK;

	// drift with any button/key
	axis = JoyAxis(AXISDRIFT, ssplayer);
	if (InputDown(gc_drift, ssplayer) || (usejoystick && axis > 0))
		cmd->buttons |= BT_DRIFT;

	// Lua scriptable buttons
	axis = JoyAxis(AXISCUSTOM1, ssplayer);
	if (InputDown(gc_custom1, ssplayer) || (usejoystick && axis > 0))
		cmd->buttons |= BT_CUSTOM1;
	axis = JoyAxis(AXISCUSTOM2, ssplayer);
	if (InputDown(gc_custom2, ssplayer) || (usejoystick && axis > 0))
		cmd->buttons |= BT_CUSTOM2;
	axis = JoyAxis(AXISCUSTOM3, ssplayer);
	if (InputDown(gc_custom3, ssplayer) || (usejoystick && axis > 0))
		cmd->buttons |= BT_CUSTOM3;

	// Reset camera
	if (InputDown(gc_camreset, ssplayer))
	{
		if (thiscam->chase && !rd)
			P_ResetCamera(player, thiscam);
		rd = true;
	}
	else
		rd = false;

	cmd->aiming = G_ClipAimingPitch(&laim);

	mousex = mousey = mlooky = 0;

	if (forward > MAXPLMOVE)
		forward = MAXPLMOVE;
	else if (forward < -MAXPLMOVE)
		forward = -MAXPLMOVE;

	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	if (forward || side)
	{
		cmd->forwardmove = (SINT8)(cmd->forwardmove + forward);
		cmd->sidemove = (SINT8)(cmd->sidemove + side);
	}

	//{ SRB2kart - Drift support
	// Not grouped with the rest of turn stuff because it needs to know what buttons you're pressing for rubber-burn turn
	// limit turning to angleturn[1] to stop mouselook letting you look too fast
	if (cmd->angleturn > (angleturn[1]))
		cmd->angleturn = (angleturn[1]);
	else if (cmd->angleturn < (-angleturn[1]))
		cmd->angleturn = (-angleturn[1]);

	if (cmd->driftturn > (angleturn[1]))
		cmd->driftturn = (angleturn[1]);
	else if (cmd->driftturn < (-angleturn[1]))
		cmd->driftturn = (-angleturn[1]);

	if (player->mo)
		cmd->angleturn = K_GetKartTurnValue(player, cmd->angleturn);

	cmd->angleturn *= realtics;

	// SRB2kart - no additional angle if not moving
	if (((player->mo && player->speed > 0) // Moving
		|| (leveltime > starttime && (cmd->buttons & BT_ACCELERATE && cmd->buttons & BT_BRAKE)) // Rubber-burn turn
		|| (player->kartstuff[k_respawn]) // Respawning
		|| (player->spectator || objectplacing))) // Not a physical player
		lang += (cmd->angleturn<<16);

	cmd->angleturn = (INT16)(lang >> 16);
	cmd->latency = modeattacking ? 0 : (leveltime & 0xFF); // Send leveltime when this tic was generated to the server for control lag calculations

	if (!hu_stopped)
	{
		localangle[forplayer] = lang;
		localaiming[forplayer] = laim;
		turnheld[forplayer] = th;
		resetdown[forplayer] = rd;
		axis = JoyAxis(AXISLOOKBACK, ssplayer);
		camspin[forplayer] = (InputDown(gc_lookback, ssplayer) || (usejoystick && axis > 0));
	}

	/* 	Lua: Allow this hook to overwrite ticcmd.
		We check if we're actually in a level because for some reason this Hook would run in menus and on the titlescreen otherwise.
		Be aware that within this hook, nothing but this player's cmd can be edited (otherwise we'd run in some pretty bad synching problems since this is clientsided, or something)

		Possible usages for this are:
			-Forcing the player to perform an action, which could otherwise require terrible, terrible hacking to replicate.
			-Preventing the player to perform an action, which would ALSO require some weirdo hacks.
			-Making some galaxy brain autopilot Lua if you're a masochist
			-Making a Mario Kart 8 Deluxe tier baby mode that steers you away from walls and whatnot. You know what, do what you want!
	*/
	if (gamestate == GS_LEVEL)
		LUA_HookTiccmd(player, cmd, HOOK(PlayerCmd));

	//Reset away view if a command is given.
	if (displayplayers[0] != consoleplayer && ssplayer == 1
	&& (cmd->forwardmove || cmd->sidemove || cmd->buttons))
	{
		displayplayers[0] = consoleplayer;
		G_FixCamera(1);
	}
}

//
// G_DoLoadLevel
//
static void G_DoLoadLevel(boolean resetplayer)
{
	INT32 i;

	// Saturn Music Feature stuffs
	S_ResetKeepAndSpecialMus();
	S_KeepMusic();

	// Make sure objectplace is OFF when you first start the level!
	OP_ResetObjectplace();

	levelstarttic = gametic; // for time calculation

	if (wipegamestate == GS_LEVEL)
		wipegamestate = -1; // force a wipe

	if (gamestate == GS_INTERMISSION)
		Y_EndIntermission();
	if (gamestate == GS_VOTING)
		Y_EndVote();

	G_SetGamestate(GS_LEVEL);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (resetplayer || (playeringame[i] && players[i].playerstate == PST_DEAD))
			players[i].playerstate = PST_REBORN;
	}

	// Setup the level.
	if (!P_SetupLevel(false,false))
	{
		// fail so reset game stuff
		Command_ExitGame_f();
		return;
	}

	if (!resetplayer)
		P_FindEmerald();

	displayplayers[0] = consoleplayer; // view the guy you are playing

	for (i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (i > 0 && !(i == 1 && botingame) && splitscreen < i)
			displayplayers[i] = consoleplayer;
	}

	gameaction = ga_nothing;
#ifdef PARANOIA
	Z_CheckHeap(-2);
#endif

	for (i = 0; i <= splitscreen; i++)
	{
		if (camera[i].chase)
			P_ResetCamera(&players[displayplayers[i]], &camera[i]);
	}

	// clear cmd building stuff
	memset(gamekeydown, 0, sizeof(gamekeydown));
	memset(joyxmove, 0, sizeof(joyxmove));
	memset(joyymove, 0, sizeof(joyymove));
	mousex = mousey = 0;

	// clear hud messages remains (usually from game startup)
	CON_ClearHUD();

	server_lagless = !cv_gentlemens.value;

	G_ResetAllDeviceRumbles();
}

static INT32 pausedelay = 0;
static INT32 camtoggledelay[MAXSPLITSCREENPLAYERS] = {0,0,0,0};
static INT32 spectatedelay[MAXSPLITSCREENPLAYERS] = {0,0,0,0};

static void G_ToggleSpectate(INT32 player)
{
	switch (player)
	{
		case 0:
			COM_ImmedExecute("changeteam spectator");
			break;
		case 1:
			COM_ImmedExecute("changeteam2 spectator");
			break;
		case 2:
			COM_ImmedExecute("changeteam3 spectator");
			break;
		case 3:
			COM_ImmedExecute("changeteam4 spectator");
			break;
	}
}

static boolean G_LevelResponder(event_t *ev)
{
	// allow spy mode changes even during the demo
	if (ev->type == ev_keydown
		&& (ev->data1 == KEY_F12
		|| ev->data1 == gamecontrol[0][gc_viewpoint][0]
		|| ev->data1 == gamecontrol[0][gc_viewpoint][1]))
	{
		if (!demo.playback && (splitscreen || !netgame))
			displayplayers[0] = consoleplayer;
		else
		{
			G_AdjustView(1, 1, true);

			// change statusbar also if playing back demo
			if (demo.quitafterplaying)
				ST_changeDemoView();

			return true;
		}
	}

	if (ev->type == ev_keydown && multiplayer && demo.playback)
	{
		for (INT32 i = 1; i <= splitscreen; i++)
		{
			if (ev->data1 == gamecontrol[i][gc_viewpoint][0]
				|| ev->data1 == gamecontrol[i][gc_viewpoint][1])
			{
				G_AdjustView(i+1, 1, true);
				return true;
			}
		}

		// Allow pausing
		if (ev->data1 == gamecontrol[0][gc_pause][0]
			|| ev->data1 == gamecontrol[0][gc_pause][1]
			|| ev->data1 == KEY_PAUSE
		)
		{
			paused = !paused;

			if (demo.rewinding)
			{
				G_ConfirmRewind(leveltime);
				paused = true;
				S_PauseAudio();
			}
			else if ((paused) && (!cv_pausemusic.value))
				S_PauseAudio();
			else
				S_ResumeAudio();

			return true;
		}

		// open menu but only w/ esc
		if (ev->data1 == 32)
		{
			M_StartControlPanel();
			return true;
		}
	}

	return false;
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
boolean G_Responder(event_t *ev)
{
	if (demo.playback && demo.title)
	{
		// Title demo uses intro responder
		if (F_IntroResponder(ev))
		{
			// stop the title demo
			G_CheckDemoStatus();
			return true;
		}

		return false;
	}
	else if (gameaction == ga_nothing
		&& !demo.quitafterplaying
		&& ((demo.playback && !modeattacking && !multiplayer) || gamestate == GS_TITLESCREEN))
	{
		// any other key pops up menu if in demos
		if (ev->type == ev_keydown && ev->data1 != 301)
		{
			M_StartControlPanel();
			return true;
		}

		return false;
	}

	if (Playing())
	{
		// If you're playing, chat is real.
		// Neatly sidesteps a class of bugs where whenever we add a
		// new gamestate accessible in netplay, chat was console-only.
		if (HU_Responder(ev))
		{
			return true; // chat ate the event
		}
	}

	switch (gamestate)
	{
		case GS_LEVEL:
			if (AM_Responder(ev))
				return true; // automap ate it
			// map the event (key/mouse/joy) to a gamecontrol
			if (G_LevelResponder(ev))
				return true;
			break;
		case GS_INTRO:
			if (F_IntroResponder(ev))
			{
				D_StartTitle();
				return true;
			}
			break;
		case GS_CUTSCENE:
			if (F_CutsceneResponder(ev))
			{
				D_StartTitle();
				return true;
			}
			break;
		case GS_CREDITS:
			if (F_CreditResponder(ev))
			{
				F_StartGameEvaluation();
				return true;
			}
			break;
		case GS_CONTINUING:
			if (F_ContinueResponder(ev))
				return true;
			break;
		case GS_GAMEEND:
		case GS_EVALUATION:
			return true; // Demo End
			break;
		default:
			break;
	}

	// update keys current state
	if (!menuactive)
		G_MapEventsToControls(ev);

	switch (ev->type)
	{
		case ev_keydown:
			if (ev->data1 == gamecontrol[0][gc_pause][0]
				|| ev->data1 == gamecontrol[0][gc_pause][1]
				|| ev->data1 == KEY_PAUSE)
			{
				if (!pausedelay)
				{
					// don't let busy scripts prevent pausing
					pausedelay = NEWTICRATE/7;

					// command will handle all the checks for us
					COM_ImmedExecute("pause");
					return true;
				}
				else
					pausedelay = NEWTICRATE/7;
			}

			// no splitscreen support for director here
			if (ev->data1 == gamecontrol[0][gc_director][0]
				|| ev->data1 == gamecontrol[0][gc_director][1])
			{
				K_ToggleDirector();
			}

			// absolutely horrid
			for (INT32 i = 0; i <= splitscreen; i++)
			{
				if (ev->data1 == gamecontrol[i][gc_camtoggle][0]
					|| ev->data1 == gamecontrol[i][gc_camtoggle][1])
				{
					if (!camtoggledelay[i])
					{
						camtoggledelay[i] = NEWTICRATE / 7;
						CV_SetValue(&cv_chasecam[i], (cv_chasecam[i].value ^ 1));
					}
				}

				if (ev->data1 == gamecontrol[i][gc_spectate][0]
					|| ev->data1 == gamecontrol[i][gc_spectate][1])
				{
					if (!spectatedelay[i])
					{
						spectatedelay[i] = NEWTICRATE / 7;
						G_ToggleSpectate(i);
					}
				}

				if (ev->data1 == gamecontrol[i][gc_freecam][0]
					|| ev->data1 == gamecontrol[i][gc_freecam][1])
				{
					P_ToggleDemoCamera(i);
				}
			}

			return true;

		case ev_keyup:
			return false; // always let key up events filter down
		case ev_mouse:
			return true; // eat events
		case ev_joystick:
			return true; // eat events
		case ev_joystick2:
			return true; // eat events
		case ev_joystick3:
			return true; // eat events
		case ev_joystick4:
			return true; // eat events
		default:
			break;
	}

	return false;
}

//
// G_CouldView
// Return whether a player could be viewed by any means.
//
boolean G_CouldView(INT32 playernum)
{
	player_t *player;

	if (playernum < 0 || playernum > MAXPLAYERS-1)
		return false;

	if (!playeringame[playernum])
		return false;

	player = &players[playernum];

	if (player->spectator)
		return false;

	// SRB2Kart: Only go through players who are actually playing
	if (player->exiting || (player->pflags & PF_TIMEOVER))
		return false;

	// I don't know if we want this actually, but I'll humor the suggestion anyway
	if (G_BattleGametype() && !demo.playback)
	{
		if (player->kartstuff[k_bumper] <= 0)
			return false;
	}

	return true;
}

//
// G_CanView
// Return whether a player can be viewed on a particular view (splitscreen).
//
boolean G_CanView(INT32 playernum, UINT8 viewnum, boolean onlyactive)
{
	UINT8 splits;
	UINT8 viewd;
	INT32 *displayplayerp;

	if (!(onlyactive ? G_CouldView(playernum) : (playeringame[playernum] && !players[playernum].spectator)))
		return false;

	splits = splitscreen+1;
	if (viewnum > splits)
		viewnum = splits;

	for (viewd = 1; viewd < viewnum; ++viewd)
	{
		displayplayerp = (&displayplayers[viewd-1]);
		if ((*displayplayerp) == playernum)
			return false;
	}

	for (viewd = viewnum + 1; viewd <= splits; ++viewd)
	{
		displayplayerp = (&displayplayers[viewd-1]);
		if ((*displayplayerp) == playernum)
			return false;
	}

	return true;
}

//
// G_FindView
// Return the next player that can be viewed on a view, wraps forward.
// An out of range startview is corrected.
//
INT32 G_FindView(INT32 startview, UINT8 viewnum, boolean onlyactive, boolean reverse)
{
	INT32 i, dir = reverse ? -1 : 1;
	startview = min(max(startview, 0), MAXPLAYERS);

	for (i = startview; i < MAXPLAYERS && i >= 0; i += dir)
	{
		if (G_CanView(i, viewnum, onlyactive))
			return i;
	}

	for (i = (reverse ? MAXPLAYERS-1 : 0); i != startview; i += dir)
	{
		if (G_CanView(i, viewnum, onlyactive))
			return i;
	}

	return -1;
}

INT32 G_CountPlayersPotentiallyViewable(boolean active)
{
	INT32 total = 0;
	INT32 i;
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (active ? G_CouldView(i) : (playeringame[i] && !players[i].spectator))
			total++;
	}
	return total;
}

//
// G_FixCamera
// Reset camera position, angle and interpolation on a view
// after changing state.
//
void G_FixCamera(UINT8 view)
{
	player_t *player = &players[displayplayers[view - 1]];

	// The order of displayplayers can change, which would
	// invalidate localangle.
	if (!P_MobjWasRemoved(player->mo))
		localangle[view - 1] = player->mo->angle;

	P_ResetCamera(player, &camera[view - 1]);

	// Make sure the viewport doesn't interpolate at all into
	// its new position -- just snap instantly into place.
	R_ResetViewInterpolation(view);
}

//
// G_ResetView
// Correct a viewpoint to playernum or the next available, wraps forward.
// Also promotes splitscreen up to available viewable players.
// An out of range playernum is corrected.
//
void G_ResetView(UINT8 viewnum, INT32 playernum, boolean onlyactive)
{
	UINT8 splits;
	UINT8 viewd;

	INT32 playernumd;

	INT32 *displayplayerp;

	INT32 olddisplayplayer;
	INT32 playersviewable;

	splits = splitscreen+1;

	/* Promote splits */
	if (viewnum > splits)
	{
		playersviewable = G_CountPlayersPotentiallyViewable(onlyactive);
		if (playersviewable < splits)/* do not demote */
			return;

		if (viewnum > playersviewable)
			viewnum = playersviewable;

		splitscreen = viewnum-1;

		R_ExecuteSetViewSize();
	}

	for (viewd = min(splits+1, viewnum); viewd <= viewnum; ++viewd)
	{
		playernumd = (viewd == viewnum) ? playernum : displayplayers[viewd-1];
		displayplayerp = (&displayplayers[viewd-1]);
		olddisplayplayer = (*displayplayerp);

		/* Check if anyone is available to view. */
		if ((playernumd = G_FindView(playernumd, viewd, onlyactive, playernumd < olddisplayplayer)) == -1)
			continue;

		/* Focus our target view first so that we don't take its player. */
		(*displayplayerp) = playernumd;

		/* If a viewpoint changes, reset the camera to clear uninitialized memory. */
		if (viewnum > splits)
		{
			G_FixCamera(viewd);
		}
		else if ((*displayplayerp) != olddisplayplayer)
		{
			G_FixCamera(viewnum);
		}
	}

	if (demo.playback && viewnum == 1)
		consoleplayer = displayplayers[0];
}

//
// G_AdjustView
// Increment a viewpoint by offset from the current player. A negative value
// decrements.
//
void G_AdjustView(UINT8 viewnum, INT32 offset, boolean onlyactive)
{
	INT32 *displayplayerp, oldview;
	displayplayerp = &displayplayers[viewnum-1];
	oldview = (*displayplayerp);

	// turn off the freecam
	camera[viewnum-1].freecam = false;

	G_ResetView(viewnum, ( (*displayplayerp) + offset ), onlyactive);

	// If no other view could be found, go back to what we had.
	if ((*displayplayerp) == -1)
		(*displayplayerp) = oldview;
}

//
// G_ResetViews
// Ensures all viewpoints are valid
// Also demotes splitscreen down to one player.
//
void G_ResetViews(void)
{
	UINT8 splits;
	UINT8 viewd;

	INT32 playersviewable;

	splits = splitscreen+1;

	playersviewable = G_CountPlayersPotentiallyViewable(false);
	/* Demote splits */
	if (playersviewable < splits)
	{
		splits = playersviewable;
		splitscreen = max(splits-1, 0);
		R_ExecuteSetViewSize();
	}

	/*
	Consider installing a method to focus the last
	view elsewhere if all players spectate?
	*/
	for (viewd = 1; viewd <= splits; ++viewd)
	{
		G_AdjustView(viewd, 0, false);
	}
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker(boolean run)
{
	UINT32 i;
	INT32 buf;
	ticcmd_t *cmd;
	UINT32 ra_timeskip = (modeattacking && !demo.playback && leveltime < starttime - TICRATE*4) ? 0 : (starttime - TICRATE*4 - 1);
	// starttime - TICRATE*4 is where we want RA to start when we PLAY IT, so we will loop the main thinker on RA start to get it to this point,
	// the reason this is done is to ensure that ghosts won't look out of synch with other map elements (objects, moving platforms...)
	// when we REPLAY, don't skip, let the camera spin, do its thing etc~

	// also the -1 is to ensure that the thinker runs in the loop below.

	P_MapStart();

	// do player reborns if needed
	if (gamestate == GS_LEVEL)
	{
		// Or, alternatively, retry.
		if (!(netgame || multiplayer) && G_GetRetryFlag())
		{
			G_ClearRetryFlag();
			D_MapChange(gamemap, gametype, cv_kartencore.value, true, 1, false, false);
		}

		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].playerstate == PST_REBORN)
				G_DoReborn(i);
	}

	P_MapEnd();

	// do things to change the game state
	while (gameaction != ga_nothing)
	{
		switch (gameaction)
		{
			case ga_completed: G_DoCompleted();     break;
			case ga_startcont: G_DoStartContinue(); break;
			case ga_continued: G_DoContinued();     break;
			case ga_worlddone: G_DoWorldDone();     break;
			case ga_startvote: G_DoStartVote();     break;
			case ga_nothing: break;
			default: I_Error("gameaction = %d\n", gameaction);
		}
	}

	buf = gametic % BACKUPTICS;

	if (!demo.playback)
	{
		for (i = 0; i < MAXPLAYERS; i++) // read/write demo and check turbo cheat
		{
			cmd = &players[i].cmd;

			if (!playeringame[i])
				continue;

			//@TODO all this throwdir stuff shouldn't be here! But it stays for now to maintain 1.0.4 compat...
			// Remove for 1.1!

			// SRB2kart
			// Save the dir the player is holding
			//  to allow items to be thrown forward or backward.
			if (cmd->buttons & BT_FORWARD)
				players[i].kartstuff[k_throwdir] = 1;
			else if (cmd->buttons & BT_BACKWARD)
				players[i].kartstuff[k_throwdir] = -1;
			else
				players[i].kartstuff[k_throwdir] = 0;

			G_CopyTiccmd(cmd, &netcmds[buf][i], 1);

			// Use the leveltime sent in the player's ticcmd to determine control lag
			cmd->latency = modeattacking ? 0 : min(((leveltime & 0xFF) - cmd->latency) & 0xFF, MAXPREDICTTICS-1); //@TODO add a cvar to allow setting this max
		}
	}

	// do main actions
	switch (gamestate)
	{
		case GS_LEVEL:

			for (; ra_timeskip < starttime - TICRATE*4; ra_timeskip++)	// this looks weird but this is done to not break compability with older demos for now.
			{
				if (demo.title)
					F_TitleDemoTicker();
				P_Ticker(run); // tic the game
				ST_Ticker();
				AM_Ticker();
				HU_Ticker();
			}
			break;

		case GS_INTERMISSION:
			if (run)
				Y_Ticker();
			HU_Ticker();
			break;

		case GS_VOTING:
			if (run)
				Y_VoteTicker();
			HU_Ticker();
			break;

		case GS_TIMEATTACK:
			break;

		case GS_INTRO:
			if (run)
				F_IntroTicker();
			break;

		case GS_CUTSCENE:
			if (run)
				F_CutsceneTicker();
			HU_Ticker();
			break;

		case GS_GAMEEND:
			if (run)
				F_GameEndTicker();
			break;

		case GS_EVALUATION:
			if (run)
				F_GameEvaluationTicker();
			break;

		case GS_CONTINUING:
			if (run)
				F_ContinueTicker();
			break;

		case GS_CREDITS:
			if (run)
				F_CreditTicker();
			HU_Ticker();
			break;

		case GS_TITLESCREEN:
			F_TitleScreenTicker(run);
			HU_TickSongCredits();
			break;
		case GS_WAITINGPLAYERS:
			if (netgame)
				F_WaitingPlayersTicker();
			HU_Ticker();
			break;

		case GS_DEDICATEDSERVER:
		case GS_NULL:
			break; // do nothing
	}

	if (run)
	{
		if (G_GametypeHasSpectators()
			&& (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_VOTING // definitely good
			|| gamestate == GS_WAITINGPLAYERS)) // definitely a problem if we don't do it at all in this gamestate, but might need more protection?
		{
			K_CheckSpectateStatus();
		}

		if (pausedelay)
			pausedelay--;

		for (UINT8 j = 0; j < MAXSPLITSCREENPLAYERS;j++)
		{
			if (camtoggledelay[j])
				camtoggledelay[j]--;

			if (spectatedelay[j])
				spectatedelay[j]--;
		}

		if (gametic % NAMECHANGERATE == 0)
		{
			memset(player_name_changes, 0, sizeof player_name_changes);
		}
	}
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
static inline void G_PlayerFinishLevel(INT32 player)
{
	player_t *p;

	p = &players[player];

	memset(p->powers, 0, sizeof (p->powers));
	memset(p->kartstuff, 0, sizeof (p->kartstuff)); // SRB2kart
	p->ringweapons = 0;

	p->mo->flags2 &= ~MF2_SHADOW; // cancel invisibility
	P_FlashPal(p, 0, 0); // Resets
	p->starpostangle = 0;
	p->starposttime = 0;
	p->starpostx = 0;
	p->starposty = 0;
	p->starpostz = 0;
	p->starpostnum = 0;

	// SRB2kart: Increment the "matches played" counter.
	if (player == consoleplayer)
	{
		if (legitimateexit && !demo.playback && !mapreset) // (yes you're allowed to unlock stuff this way when the game is modified)
		{
			kartstats.matchesplayed++;
			if (M_UpdateUnlockablesAndExtraEmblems(true))
				S_StartSound(NULL, sfx_ncitem);
			G_SaveGameData(true);
		}

		legitimateexit = false;
	}
}

//
// G_PlayerReborn
// Called after a player dies. Almost everything is cleared and initialized.
//
void G_PlayerReborn(INT32 player)
{
	player_t *p;
	INT32 score, marescore;
	INT32 lives;
	INT32 continues;
	// SRB2kart
	UINT8 kartspeed;
	UINT8 kartweight;
	//
	INT32 charflags;
	INT32 pflags;
	INT32 ctfteam;
	INT32 starposttime;
	INT16 starpostx;
	INT16 starposty;
	INT16 starpostz;
	INT32 starpostnum;
	INT32 starpostangle;
	INT32 exiting;
	INT16 numboxes;
	INT16 totalring;
	UINT8 laps;
	UINT8 mare;
	UINT8 skincolor;
	INT32 skin;
	int localskin;
	boolean skinlocal;
	tic_t jointime;
	UINT8 splitscreenindex;
	boolean spectator;
	INT16 bot;
	SINT8 pity;

	// SRB2kart
	INT32 starpostwp;
	INT32 itemtype;
	INT32 itemamount;
	INT32 itemroulette;
	INT32 roulettetype;
	INT32 growshrinktimer;
	INT32 bumper;
	INT32 comebackpoints;
	INT32 wanted;
	INT32 respawnflip;
	boolean songcredit = false;
	tic_t spectatorreentry;
	tic_t grieftime;
	UINT8 griefstrikes;

	boolean fade;
	boolean playing;

	tic_t laptime[LAP__MAX];

	INT32 i;

	score = players[player].score;
	marescore = players[player].marescore;
	lives = players[player].lives;
	continues = players[player].continues;
	ctfteam = players[player].ctfteam;
	exiting = players[player].exiting;
	jointime = players[player].jointime;
	splitscreenindex = players[player].splitscreenindex;
	spectator = players[player].spectator;
	pflags = (players[player].pflags & (PF_TIMEOVER|PF_FLIPCAM|PF_TAGIT|PF_TAGGED|PF_ANALOGMODE|PF_WANTSTOJOIN));

	// As long as we're not in multiplayer, carry over cheatcodes from map to map
	if (!(netgame || multiplayer))
		pflags |= (players[player].pflags & (PF_GODMODE|PF_NOCLIP|PF_INVIS));

	numboxes = players[player].numboxes;
	laps = players[player].laps;
	totalring = players[player].totalring;

	skincolor = players[player].skincolor;
	skin = players[player].skin;
	localskin = players[player].localskin;
	skinlocal = players[player].skinlocal;
	// SRB2kart
	kartspeed = players[player].kartspeed;
	kartweight = players[player].kartweight;
	//
	charflags = players[player].charflags;

	starposttime = players[player].starposttime;
	starpostx = players[player].starpostx;
	starposty = players[player].starposty;
	starpostz = players[player].starpostz;
	starpostnum = players[player].starpostnum;
	respawnflip = players[player].kartstuff[k_starpostflip];	//SRB2KART
	starpostangle = players[player].starpostangle;

	mare = players[player].mare;
	bot = players[player].bot;
	pity = players[player].pity;

	// SRB2kart
	if (leveltime <= starttime || spectator == true)
	{
		itemroulette = 0;
		roulettetype = 0;
		itemtype = 0;
		itemamount = 0;
		growshrinktimer = 0;
		bumper = (G_BattleGametype() ? cv_kartbumpers.value : 0);
		comebackpoints = 0;
		wanted = 0;
		starpostwp = 0;

		starposttime = 0;
		starpostx = 0;
		starposty = 0;
		starpostz = 0;
		starpostnum = 0;
		respawnflip = 0;
		starpostangle = 0;

		for (i = 0; i < LAP__MAX; i++)
		{
			laptime[i] = 0;
		}
	}
	else
	{
		starpostwp = players[player].kartstuff[k_starpostwp];

		itemroulette = (players[player].kartstuff[k_itemroulette] > 0 ? 1 : 0);
		roulettetype = players[player].kartstuff[k_roulettetype];

		if (players[player].kartstuff[k_itemheld])
		{
			itemtype = 0;
			itemamount = 0;
		}
		else
		{
			itemtype = players[player].kartstuff[k_itemtype];
			itemamount = players[player].kartstuff[k_itemamount];
		}

		// Keep Shrink status, remove Grow status
		if (players[player].kartstuff[k_growshrinktimer] < 0)
			growshrinktimer = players[player].kartstuff[k_growshrinktimer];
		else
			growshrinktimer = 0;

		bumper = players[player].kartstuff[k_bumper];
		comebackpoints = players[player].kartstuff[k_comebackpoints];
		wanted = players[player].kartstuff[k_wanted];

		for (i = 0; i < LAP__MAX; i++)
		{
			laptime[i] = players[player].laptime[i];
		}
	}

	spectatorreentry = players[player].spectatorreentry;

	grieftime = players[player].grieftime;
	griefstrikes = players[player].griefstrikes;

	p = &players[player];
	memset(p, 0, sizeof (*p));

	p->score = score;
	p->marescore = marescore;
	p->lives = lives;
	p->continues = continues;
	p->pflags = pflags;
	p->ctfteam = ctfteam;
	p->jointime = jointime;
	p->splitscreenindex = splitscreenindex;
	p->spectator = spectator;

	// save player config truth reborn
	p->skincolor = skincolor;
	p->skin = skin;
	p->localskin = localskin;
	p->skinlocal = skinlocal;
	// SRB2kart
	p->kartspeed = kartspeed;
	p->kartweight = kartweight;
	//
	p->charflags = charflags;

	p->starposttime = starposttime;
	p->starpostx = starpostx;
	p->starposty = starposty;
	p->starpostz = starpostz;
	p->starpostnum = starpostnum;
	p->starpostangle = starpostangle;
	p->exiting = exiting;

	p->numboxes = numboxes;
	p->laps = laps;
	p->totalring = totalring;

	for (i = 0; i < LAP__MAX; i++)
	{
		p->laptime[i] = laptime[i];
	}

	p->mare = mare;
	if (bot)
		p->bot = 1; // reset to AI-controlled
	p->pity = pity;

	// SRB2kart
	p->kartstuff[k_starpostwp] = starpostwp; // TODO: get these out of kartstuff, it causes desync
	p->kartstuff[k_itemroulette] = itemroulette;
	p->kartstuff[k_roulettetype] = roulettetype;
	p->kartstuff[k_itemtype] = itemtype;
	p->kartstuff[k_itemamount] = itemamount;
	p->kartstuff[k_growshrinktimer] = growshrinktimer;
	p->kartstuff[k_bumper] = bumper;
	p->kartstuff[k_comebackpoints] = comebackpoints;
	p->kartstuff[k_comebacktimer] = comebacktime;
	p->kartstuff[k_wanted] = wanted;
	p->kartstuff[k_eggmanblame] = -1;
	p->kartstuff[k_starpostflip] = respawnflip;

	p->spectatorreentry = spectatorreentry;
	p->grieftime = grieftime;
	p->griefstrikes = griefstrikes;

	// Don't do anything immediately
	p->pflags |= PF_USEDOWN;
	p->pflags |= PF_ATTACKDOWN;
	p->pflags |= PF_JUMPDOWN;

	p->playerstate = PST_LIVE;
	p->health = 1; // 0 rings
	p->panim = PA_IDLE; // standing animation

	if ((netgame || multiplayer) && !p->spectator)
		p->powers[pw_flashing] = K_GetKartFlashing(p)-1; // Babysitting deterrent

	if (p-players == consoleplayer)
	{
		if (mapmusic.flags & MUSIC_RELOADRESET)
		{
			S_HandleReloadResetMusic();
			songcredit = true;
		}
	}

	/* I'm putting this here because lol */
	fade = (cv_birdmusic.value && cv_fading.value && P_IsLocalPlayer(p));

	if (fade)
	{
		playing = S_MusicPlaying();

		/*
		Fade it in with the same call to avoid
		max volume for a few milliseconds (?).
		*/
		if (!playing)
			S_SetRestoreMusicFadeInCvar(&cv_respawnfademusicback);
	}

	P_RestoreMusic(p);

	if (fade)
	{
		/* mid-way fading out, fade back up */
		if (playing)
			S_FadeMusic(100, cv_respawnfademusicback.value);
	}

	if (songcredit)
		S_ShowMusicCredit();

	if (leveltime > (starttime + (TICRATE/2)) && !p->spectator)
		p->kartstuff[k_respawn] = 48; // Respawn effect

	if (gametype == GT_COOP)
		P_FindEmerald(); // scan for emeralds to hunt for

	// Reset Nights score and max link to 0 on death
	p->maxlink = 0;

	// If NiGHTS, find lowest mare to start with.
	p->mare = 0;
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//
static boolean G_CheckSpot(INT32 playernum, mapthing_t *mthing)
{
	fixed_t x;
	fixed_t y;
	INT32 i;

	// maybe there is no player start
	if (!mthing)
		return false;

	if (!players[playernum].mo)
	{
		// first spawn of level
		for (i = 0; i < playernum; i++)
			if (playeringame[i] && players[i].mo
				&& players[i].mo->x == mthing->x << FRACBITS
				&& players[i].mo->y == mthing->y << FRACBITS)
			{
				return false;
			}
		return true;
	}

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	if (!K_CheckPlayersRespawnColliding(playernum, x, y))
		return false;

	if (!P_CheckPosition(players[playernum].mo, x, y))
		return false;

	return true;
}

//
// G_SpawnPlayer
// Spawn a player in a spot appropriate for the gametype --
// or a not-so-appropriate spot, if it initially fails
// due to a lack of starts open or something.
//
void G_SpawnPlayer(INT32 playernum, boolean starpost)
{
	mapthing_t *spawnpoint;

	if (!playeringame[playernum])
		return;

	P_SpawnPlayer(playernum);

	if (starpost) //Don't even bother with looking for a place to spawn.
	{
		P_MovePlayerToStarpost(playernum);
		LUA_HookPlayer(&players[playernum], HOOK(PlayerSpawn)); // Lua hook for player spawning :)
		return;
	}

	// -- CTF --
	// Order: CTF->DM->Coop
	if (gametype == GT_CTF && players[playernum].ctfteam)
	{
		if (!(spawnpoint = G_FindCTFStart(playernum)) // find a CTF start
		&& !(spawnpoint = G_FindMatchStart(playernum))) // find a DM start
			spawnpoint = G_FindRaceStart(playernum); // fallback
	}

	// -- DM/Tag/CTF-spectator/etc --
	// Order: DM->CTF->Coop
	else if (gametype == GT_MATCH || gametype == GT_TEAMMATCH || gametype == GT_CTF
	 || ((gametype == GT_TAG || gametype == GT_HIDEANDSEEK) && !(players[playernum].pflags & PF_TAGIT)))
	{
		if (!(spawnpoint = G_FindMatchStart(playernum)) // find a DM start
		&& !(spawnpoint = G_FindCTFStart(playernum))) // find a CTF start
			spawnpoint = G_FindRaceStart(playernum); // fallback
	}

	// -- Other game modes --
	// Order: Coop->DM->CTF
	else
	{
		if (!(spawnpoint = G_FindRaceStart(playernum)) // find a Race start
		&& !(spawnpoint = G_FindMatchStart(playernum))) // find a DM start
			spawnpoint = G_FindCTFStart(playernum); // fallback
	}

	//No spawns found.  ANYWHERE.
	if (!spawnpoint)
	{
		if (nummapthings)
		{
			if (playernum == consoleplayer
				|| (splitscreen && playernum == displayplayers[1])
				|| (splitscreen > 1 && playernum == displayplayers[2])
				|| (splitscreen > 2 && playernum == displayplayers[3]))
				CONS_Alert(CONS_ERROR, M_GetText("No player spawns found, spawning at the first mapthing!\n"));
			spawnpoint = &mapthings[0];
		}
		else
		{
			if (playernum == consoleplayer
			|| (splitscreen && playernum == displayplayers[1])
			|| (splitscreen > 1 && playernum == displayplayers[2])
			|| (splitscreen > 2 && playernum == displayplayers[3]))
				CONS_Alert(CONS_ERROR, M_GetText("No player spawns found, spawning at the origin!\n"));
			//P_MovePlayerToSpawn handles this fine if the spawnpoint is NULL.
		}
	}
	P_MovePlayerToSpawn(playernum, spawnpoint);

	LUA_HookPlayer(&players[playernum], HOOK(PlayerSpawn)); // Lua hook for player spawning :)
}

mapthing_t *G_FindCTFStart(INT32 playernum)
{
	INT32 i,j;

	if (!numredctfstarts && !numbluectfstarts) //why even bother, eh?
	{
		if (playernum == consoleplayer
			|| (splitscreen && playernum == displayplayers[1])
			|| (splitscreen > 1 && playernum == displayplayers[2])
			|| (splitscreen > 2 && playernum == displayplayers[3]))
			CONS_Alert(CONS_WARNING, M_GetText("No CTF starts in this map!\n"));
		return NULL;
	}

	if ((!players[playernum].ctfteam && numredctfstarts && (!numbluectfstarts || P_RandomChance(FRACUNIT/2))) || players[playernum].ctfteam == 1) //red
	{
		if (!numredctfstarts)
		{
			if (playernum == consoleplayer
				|| (splitscreen && playernum == displayplayers[1])
				|| (splitscreen > 1 && playernum == displayplayers[2])
				|| (splitscreen > 2 && playernum == displayplayers[3]))
				CONS_Alert(CONS_WARNING, M_GetText("No Red Team starts in this map!\n"));
			return NULL;
		}

		for (j = 0; j < 32; j++)
		{
			i = P_RandomKey(numredctfstarts);
			if (G_CheckSpot(playernum, redctfstarts[i]))
				return redctfstarts[i];
		}

		if (playernum == consoleplayer
			|| (splitscreen && playernum == displayplayers[1])
			|| (splitscreen > 1 && playernum == displayplayers[2])
			|| (splitscreen > 2 && playernum == displayplayers[3]))
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Red Team starts!\n"));
		return NULL;
	}
	else if (!players[playernum].ctfteam || players[playernum].ctfteam == 2) //blue
	{
		if (!numbluectfstarts)
		{
			if (playernum == consoleplayer
				|| (splitscreen && playernum == displayplayers[1])
				|| (splitscreen > 1 && playernum == displayplayers[2])
				|| (splitscreen > 2 && playernum == displayplayers[3]))
				CONS_Alert(CONS_WARNING, M_GetText("No Blue Team starts in this map!\n"));
			return NULL;
		}

		for (j = 0; j < 32; j++)
		{
			i = P_RandomKey(numbluectfstarts);
			if (G_CheckSpot(playernum, bluectfstarts[i]))
				return bluectfstarts[i];
		}
		if (playernum == consoleplayer
			|| (splitscreen && playernum == displayplayers[1])
			|| (splitscreen > 1 && playernum == displayplayers[2])
			|| (splitscreen > 2 && playernum == displayplayers[3]))
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Blue Team starts!\n"));
		return NULL;
	}
	//should never be reached but it gets stuff to shut up
	return NULL;
}

mapthing_t *G_FindMatchStart(INT32 playernum)
{
	INT32 i, j;

	if (numdmstarts)
	{
		for (j = 0; j < 64; j++)
		{
			i = P_RandomKey(numdmstarts);
			if (G_CheckSpot(playernum, deathmatchstarts[i]))
				return deathmatchstarts[i];
		}
		if (playernum == consoleplayer
			|| (splitscreen && playernum == displayplayers[1])
			|| (splitscreen > 1 && playernum == displayplayers[2])
			|| (splitscreen > 2 && playernum == displayplayers[3]))
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Deathmatch starts!\n"));
		return NULL;
	}

	if (playernum == consoleplayer
		|| (splitscreen && playernum == displayplayers[1])
		|| (splitscreen > 1 && playernum == displayplayers[2])
		|| (splitscreen > 2 && playernum == displayplayers[3]))
		CONS_Alert(CONS_WARNING, M_GetText("No Deathmatch starts in this map!\n"));
	return NULL;
}

mapthing_t *G_FindRaceStart(INT32 playernum)
{
	if (numcoopstarts)
	{
		UINT8 i;
		UINT8 pos = 0;

		// SRB2Kart: figure out player spawn pos from points
		if (!playeringame[playernum] || players[playernum].spectator)
			return playerstarts[0]; // go to first spot if you're a spectator

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;
			if (i == playernum)
				continue;

			if (players[i].score < players[playernum].score)
			{
				UINT8 j;
				UINT8 num = 0;

				for (j = 0; j < MAXPLAYERS; j++) // I hate similar loops inside loops... :<
				{
					if (!playeringame[j] || players[j].spectator)
						continue;

					if ((j == playernum) || (j == i))
						continue;

					if (players[j].score == players[i].score)
						num++;
				}

				if (num > 1) // found dupes
					pos++;
			}
			else
			{
				if (players[i].score > players[playernum].score || i < playernum)
					pos++;
			}
		}

		if (G_CheckSpot(playernum, playerstarts[pos % numcoopstarts]))
			return playerstarts[pos % numcoopstarts];

		// Your spot isn't available? Find whatever you can get first.
		for (i = 0; i < numcoopstarts; i++)
		{
			if (G_CheckSpot(playernum, playerstarts[i]))
				return playerstarts[i];
		}

		// SRB2Kart: We have solid players, so this behavior is less ideal.
		// Don't bother checking to see if the player 1 start is open.
		// Just spawn there.
		//return playerstarts[0];

		if (playernum == consoleplayer
			|| (splitscreen && playernum == displayplayers[1])
			|| (splitscreen > 1 && playernum == displayplayers[2])
			|| (splitscreen > 2 && playernum == displayplayers[3]))
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Race starts!\n"));
		return NULL;
	}

	if (playernum == consoleplayer
		|| (splitscreen && playernum == displayplayers[1])
		|| (splitscreen > 1 && playernum == displayplayers[2])
		|| (splitscreen > 2 && playernum == displayplayers[3]))
		CONS_Alert(CONS_WARNING, M_GetText("No Race starts in this map!\n"));
	return NULL;
}

// Go back through all the projectiles and remove all references to the old
// player mobj, replacing them with the new one.
void G_ChangePlayerReferences(mobj_t *oldmo, mobj_t *newmo)
{
	thinker_t *th;
	mobj_t *mo2;

	I_Assert((oldmo != NULL) && (newmo != NULL));

	// scan all thinkers
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		if (!(mo2->flags & MF_MISSILE))
			continue;

		if (mo2->target == oldmo)
		{
			P_SetTarget(&mo2->target, newmo);
			mo2->flags2 |= MF2_BEYONDTHEGRAVE; // this mobj belongs to a player who has reborn
		}
	}
}

//
// G_DoReborn
//
void G_DoReborn(INT32 playernum)
{
	player_t *player = &players[playernum];
	boolean starpost = false;

	// Make sure objectplace is OFF when you first start the level!
	OP_ResetObjectplace();

	if (player->bot && playernum != consoleplayer)
	{ // Bots respawn next to their master.
		mobj_t *oldmo = NULL;

		// first dissasociate the corpse
		if (player->mo)
		{
			oldmo = player->mo;
			// Don't leave your carcass stuck 10-billion feet in the ground!
			P_RemoveMobj(player->mo);
		}

		B_RespawnBot(playernum);

		if (oldmo)
			G_ChangePlayerReferences(oldmo, players[playernum].mo);
	}
	else
	{
		// respawn at the start
		mobj_t *oldmo = NULL;

		if (player->spectator)
			;
		else if (player->starpostnum || ((mapheaderinfo[gamemap - 1]->levelflags & LF_SECTIONRACE) && player->laps)) // SRB2kart
			starpost = true;

		// first dissasociate the corpse
		if (player->mo)
		{
			oldmo = player->mo;
			// Don't leave your carcass stuck 10-billion feet in the ground!
			P_RemoveMobj(player->mo);
		}

		G_SpawnPlayer(playernum, starpost);

		if (oldmo)
			G_ChangePlayerReferences(oldmo, players[playernum].mo);

		if (!demo.playback && playernum == consoleplayer)
			kartstats.respawns++;
	}
}

void G_AddPlayer(INT32 playernum)
{
	player_t *p = &players[playernum];

	p->jointime = 0;
	p->playerstate = PST_REBORN;

	demo_extradata[playernum] |= DXD_PLAYSTATE|DXD_COLOR|DXD_NAME|DXD_SKIN; // Set everything
}

void G_ExitLevel(void)
{
	G_ResetAllDeviceRumbles();

	if (gamestate == GS_LEVEL)
	{
		gameaction = ga_completed;
		lastdraw = true;

		// If you want your teams scrambled on map change, start the process now.
		// The teams will scramble at the start of the next round.
		if (cv_scrambleonchange.value && G_GametypeHasTeams())
		{
			if (server)
				CV_SetValue(&cv_teamscramble, cv_scrambleonchange.value);
		}

		if (netgame || multiplayer)
			CON_LogMessage(M_GetText("The round has ended.\n"));

		// Remove CEcho text on round end.
		HU_ClearCEcho();

		// Don't save demos immediately here! Let standings write first
	}
}

// See also the enum GameType in doomstat.h
const char *Gametype_Names[NUMGAMETYPES] =
{
	"Race", // GT_RACE
	"Battle" // GT_MATCH
};

//
// G_GetGametypeByName
//
// Returns the number for the given gametype name string, or -1 if not valid.
//
INT32 G_GetGametypeByName(const char *gametypestr)
{
	INT32 i;

	for (i = 0; i < NUMGAMETYPES; i++)
		if (!stricmp(gametypestr, Gametype_Names[i]))
			return i;

	return -1; // unknown gametype
}

//
// G_SometimesGetDifferentGametype
//
// Oh, yeah, and we sometimes flip encore mode on here too.
//
UINT8 G_SometimesGetDifferentGametype(UINT8 prefgametype)
{
	// Most of the gametype references in this condition are intentionally not prefgametype.
	// This is so a server CAN continue playing a gametype if they like the taste of it.
	// The encore check needs prefgametype so can't use G_RaceGametype...
	boolean encorepossible = (M_SecretUnlocked(SECRET_ENCORE)
		&& (gametype == GT_RACE || prefgametype == GT_RACE));
	boolean encoreactual = false;
	UINT8 encoremodifier = 0;

	if (encorepossible)
	{
		switch (cv_kartvoterulechanges.value)
		{
			case 3: // always
				encoreactual = true;
				break;
			case 2: // frequent
				encoreactual = M_RandomChance(FRACUNIT>>1);
				break;
			case 1: // sometimes
				encoreactual = M_RandomChance(FRACUNIT>>2);
				break;
			default:
				break;
		}

		if (encoreactual != (boolean)cv_kartencore.value)
			encoremodifier = 0x80;
	}

	if (!cv_kartvoterulechanges.value) // never
		return (gametype|encoremodifier);

	if (randmapbuffer[NUMMAPS] > 0 && (cv_kartvoterulechanges.value != 3)) // used to be (encorepossible || rule changes != 3)
	{
		randmapbuffer[NUMMAPS]--;
		return (gametype|encoremodifier);
	}

	switch (cv_kartvoterulechanges.value) // okay, we're having a gametype change! when's the next one, luv?
	{
		case 1: // sometimes
			randmapbuffer[NUMMAPS] = 5; // per "cup"
			break;
		default:
			// fallthrough - happens when clearing buffer, but needs a reasonable countdown if cvar is modified
		case 2: // frequent
			randmapbuffer[NUMMAPS] = 2; // ...every 1/2th-ish cup?
			break;
	}

	// Only this response is prefgametype-based.
	if (prefgametype == GT_MATCH)
	{
		// Intentionally does not use encoremodifier!
		if (cv_kartencore.value)
			return (GT_RACE|0x80);
		return (GT_RACE);
	}

	// This might appear wrong HERE, but the game will display the Encore possibility on the second voting choice instead.
	return (GT_MATCH|encoremodifier);
}

//
// G_GetGametypeColor
//
// Pretty and consistent ^u^
// See also M_GetGametypeColor.
//
UINT8 G_GetGametypeColor(INT16 gt)
{
	if (modeattacking // == ATTACKING_RECORD
	|| gamestate == GS_TIMEATTACK)
		return orangemap[120];
	if (gt == GT_MATCH)
		return redmap[120];
	if (gt == GT_RACE)
		return skymap[120];
	return 247; // FALLBACK
}

/** Get the typeoflevel flag needed to indicate support of a gametype.
  * In single-player, this always returns TOL_SP.
  * \param gametype The gametype for which support is desired.
  * \return The typeoflevel flag to check for that gametype.
  * \author Graue <graue@oceanbase.org>
  */
INT16 G_TOLFlag(INT32 pgametype)
{
	if (!multiplayer)                 return TOL_SP;
	if (pgametype == GT_COOP)         return TOL_RACE; // SRB2kart
	if (pgametype == GT_COMPETITION)  return TOL_COMPETITION;
	if (pgametype == GT_RACE)         return TOL_RACE;
	if (pgametype == GT_MATCH)        return TOL_MATCH;
	if (pgametype == GT_TEAMMATCH)    return TOL_MATCH;
	if (pgametype == GT_TAG)          return TOL_TAG;
	if (pgametype == GT_HIDEANDSEEK)  return TOL_TAG;
	if (pgametype == GT_CTF)          return TOL_CTF;

	CONS_Alert(CONS_ERROR, M_GetText("Unknown gametype! %d\n"), pgametype);
	return INT16_MAX;
}

static INT32 TOLMaps(INT16 tolflags)
{
	INT32 num = 0;
	INT16 i;

	// Find all the maps that are ok and and put them in an array.
	for (i = 0; i < NUMMAPS; i++)
	{
		if (!mapheaderinfo[i])
			continue;
		if (mapheaderinfo[i]->menuflags & LF2_HIDEINMENU) // Don't include Map Hell
			continue;
		if ((mapheaderinfo[i]->typeoflevel & tolflags) == tolflags)
			num++;
	}

	return num;
}

/** Select a random map with the given typeoflevel flags.
  * If no map has those flags, this arbitrarily gives you map 1.
  * \param tolflags The typeoflevel flags to insist on. Other bits may
  *                 be on too, but all of these must be on.
  * \return A random map with those flags, 1-based, or 1 if no map
  *         has those flags.
  * \author Graue <graue@oceanbase.org>
  */
static INT16 *okmaps = NULL;
INT16 G_RandMap(INT16 tolflags, INT16 pprevmap, boolean ignorebuffer, UINT8 maphell, boolean callagainsoon, INT16 *extbuffer)
{
	INT32 numokmaps = 0;
	INT16 ix, bufx;
	UINT16 extbufsize = 0;
	boolean usehellmaps; // Only consider Hell maps in this pick

	if (!okmaps)
		okmaps = Z_Malloc(NUMMAPS * sizeof(INT16), PU_STATIC, NULL);

	if (extbuffer != NULL)
	{
		bufx = 0;
		while (extbuffer[bufx])
		{
			extbufsize++; bufx++;
		}
	}

tryagain:

	usehellmaps = (maphell == 0 ? false : (maphell == 2 || M_RandomChance(FRACUNIT/100))); // 1% chance of Hell

	// Find all the maps that are ok and and put them in an array.
	for (ix = 0; ix < NUMMAPS; ix++)
	{
		boolean isokmap = true;

		if (!mapheaderinfo[ix])
			continue;

		if ((mapheaderinfo[ix]->typeoflevel & tolflags) != tolflags
			|| ix == pprevmap
			|| (!dedicated && M_MapLocked(ix+1))
			|| (usehellmaps != (mapheaderinfo[ix]->menuflags & LF2_HIDEINMENU))) // this is bad
			continue; //isokmap = false;

		if (!ignorebuffer)
		{
			if (extbufsize > 0)
			{
				for (bufx = 0; bufx < extbufsize; bufx++)
				{
					if (extbuffer[bufx] == -1) // Rest of buffer SHOULD be empty
						break;
					if (ix == extbuffer[bufx])
					{
						isokmap = false;
						break;
					}
				}

				if (!isokmap)
					continue;
			}

			for (bufx = 0; bufx < (maphell ? 3 : NUMMAPS); bufx++)
			{
				if (randmapbuffer[bufx] == -1) // Rest of buffer SHOULD be empty
					break;
				if (ix == randmapbuffer[bufx])
				{
					isokmap = false;
					break;
				}
			}

			if (!isokmap)
				continue;
		}

		if (pprevmap == -2) // title demo hack
		{
			lumpnum_t l;
			if ((l = W_CheckNumForName(va("%sS01",G_BuildMapName(ix+1)))) == LUMPERROR)
				continue;
		}

		okmaps[numokmaps++] = ix;
	}

	if (numokmaps == 0)  // If there's no matches... (Goodbye, incredibly silly function chains :V)
	{
		if (!ignorebuffer)
		{
			if (randmapbuffer[3] == -1) // Is the buffer basically empty?
			{
				ignorebuffer = true; // This will probably only help in situations where there's very few maps, but it's folly not to at least try it
				goto tryagain;
			}

			for (bufx = 3; bufx < NUMMAPS; bufx++) // Let's clear all but the three most recent maps...
				randmapbuffer[bufx] = -1;
			goto tryagain;
		}

		if (maphell) // Any wiggle room to loosen our restrictions here?
		{
			maphell--;
			goto tryagain;
		}

		ix = 0; // Sorry, none match. You get MAP01.
		for (bufx = 0; bufx < NUMMAPS+1; bufx++)
			randmapbuffer[bufx] = -1; // if we're having trouble finding a map we should probably clear it
	}
	else
		ix = okmaps[M_RandomKey(numokmaps)];

	if (!callagainsoon)
	{
		Z_Free(okmaps);
		okmaps = NULL;
	}

	return ix;
}

void G_AddMapToBuffer(INT16 map)
{
	INT16 bufx, refreshnum = max(0, TOLMaps(G_TOLFlag(gametype))-3);

	// Add the map to the buffer.
	for (bufx = NUMMAPS-1; bufx > 0; bufx--)
		randmapbuffer[bufx] = randmapbuffer[bufx-1];
	randmapbuffer[0] = map;

	// We're getting pretty full, so lets flush this for future usage.
	if (randmapbuffer[refreshnum] != -1)
	{
		// Clear all but the five most recent maps.
		for (bufx = 5; bufx < NUMMAPS; bufx++) // bufx < refreshnum? Might not handle everything for gametype switches, though.
			randmapbuffer[bufx] = -1;
		//CONS_Printf("Random map buffer has been flushed.\n");
	}
}

//
// G_DoCompleted
//
static void G_DoCompleted(void)
{
	INT32 i, j = 0;

	tokenlist = 0; // Reset the list

	gameaction = ga_nothing;

	if (metalplayback)
		G_StopMetalDemo();
	if (metalrecording)
		G_StopMetalRecording();

	K_StatRound();

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
		{
			// SRB2Kart: exitlevel shouldn't get you the points
			if (!players[i].exiting && !(players[i].pflags & PF_TIMEOVER))
			{
				players[i].pflags |= PF_TIMEOVER;
				if (P_IsLocalPlayer(&players[i]))
					j++;
			}
			G_PlayerFinishLevel(i); // take away cards and stuff
		}

	// play some generic music if there's no win/cool/lose music going on (for exitlevel commands)
	if (G_RaceGametype() && ((multiplayer && demo.playback) || j == splitscreen+1) && (cv_inttime.value > 0))
		S_ChangeMusicInternal("racent", true);

	if (automapactive)
		AM_Stop();

	S_StopSounds();

	prevmap = (INT16)(gamemap-1);

	if (demo.playback) goto demointermission;

	// go to next level
	// nextmap is 0-based, unlike gamemap
	if (nextmapoverride != 0)
		nextmap = (INT16)(nextmapoverride-1);
	else if (mapheaderinfo[gamemap-1]->nextlevel == 1101) // SRB2Kart: !!! WHENEVER WE GET GRAND PRIX, GO TO AWARDS MAP INSTEAD !!!
		nextmap = (INT16)(mapheaderinfo[gamemap] ? gamemap : (spstage_start-1)); // (gamemap-1)+1 == gamemap :V
	else
		nextmap = (INT16)(mapheaderinfo[gamemap-1]->nextlevel-1);

	// Remember last map for when you come out of the special stage.
	lastmap = nextmap;

	// If nextmap is actually going to get used, make sure it points to
	// a map of the proper gametype -- skip levels that don't support
	// the current gametype. (Helps avoid playing boss levels in Race,
	// for instance).
	if (!token && !modeattacking && (nextmap >= 0 && nextmap < NUMMAPS))
	{
		register INT16 cm = nextmap;
		INT16 tolflag = G_TOLFlag(gametype);
		UINT8 visitedmap[(NUMMAPS+7)/8];

		memset(visitedmap, 0, sizeof (visitedmap));

		while (!mapheaderinfo[cm] || !(mapheaderinfo[cm]->typeoflevel & tolflag))
		{
			visitedmap[cm/8] |= (1<<(cm&7));

			if (!mapheaderinfo[cm])
				cm = -1; // guarantee error execution
			else
				cm = (INT16)(mapheaderinfo[cm]->nextlevel-1);

			if (cm >= NUMMAPS || cm < 0) // out of range (either 1100-1102 or error)
			{
				cm = nextmap; //Start the loop again so that the error checking below is executed.

				// Make sure the map actually exists before you try to go to it!
				if ((W_CheckNumForName(G_BuildMapName(cm + 1)) == LUMPERROR))
				{
					//CONS_Alert(CONS_ERROR, M_GetText("Next map given (MAP %d) doesn't exist! Reverting to MAP01.\n"), cm+1);
					CON_LogMessage(va(M_GetText("Next map given (MAP %d) doesn't exist! Reverting to MAP01.\n"), cm+1));
					cm = 0;
					break;
				}
			}

			if (visitedmap[cm/8] & (1<<(cm&7))) // smells familiar
			{
				// We got stuck in a loop, came back to the map we started on
				// without finding one supporting the current gametype.
				// Thus, print a warning, and just use this map anyways.
				//CONS_Alert(CONS_WARNING, M_GetText("Can't find a compatible map after map %d; using map %d anyway\n"), prevmap+1, cm+1);
				CON_LogMessage(va(M_GetText("Can't find a compatible map after map %d; using map %d anyway\n"), prevmap+1, cm+1));
				break;
			}
		}
		nextmap = cm;
	}

	if (nextmap < 0 || (nextmap >= NUMMAPS && nextmap < 1100-1) || nextmap > 1102-1)
		I_Error("Followed map %d to invalid map %d\n", prevmap + 1, nextmap + 1);

	// wrap around in race
	if (nextmap >= 1100-1 && nextmap <= 1102-1 && G_RaceGametype())
		nextmap = (INT16)(spstage_start-1);

	if (gametype == GT_COOP && token)
	{
		token--;

		if (!(emeralds & EMERALD1))
			nextmap = (INT16)(sstage_start - 1); // Special Stage 1
		else if (!(emeralds & EMERALD2))
			nextmap = (INT16)(sstage_start);     // Special Stage 2
		else if (!(emeralds & EMERALD3))
			nextmap = (INT16)(sstage_start + 1); // Special Stage 3
		else if (!(emeralds & EMERALD4))
			nextmap = (INT16)(sstage_start + 2); // Special Stage 4
		else if (!(emeralds & EMERALD5))
			nextmap = (INT16)(sstage_start + 3); // Special Stage 5
		else if (!(emeralds & EMERALD6))
			nextmap = (INT16)(sstage_start + 4); // Special Stage 6
		else if (!(emeralds & EMERALD7))
			nextmap = (INT16)(sstage_start + 5); // Special Stage 7
	}

	automapactive = false;

	if (gametype != GT_COOP)
	{
		if (cv_advancemap.value == 0) // Stay on same map.
			nextmap = prevmap;
		else if (cv_advancemap.value == 2) // Go to random map.
			nextmap = G_RandMap(G_TOLFlag(gametype), prevmap, false, 0, false, NULL);
	}

	// We are committed to this map now.
	// We may as well allocate its header if it doesn't exist
	// (That is, if it's a real map)
	if (nextmap < NUMMAPS && !mapheaderinfo[nextmap])
		P_AllocMapHeader(nextmap);

demointermission:

	if (skipstats && !modeattacking) // Don't skip stats if we're in record attack
		G_AfterIntermission();
	else
	{
		G_SetGamestate(GS_INTERMISSION);
		Y_StartIntermission();
	}
}

void G_AfterIntermission(void)
{
	HU_ClearCEcho();

	if (demo.playback)
	{
		G_StopDemo();

		if (demo.inreplayhut)
			M_ReplayHut(0);
		else
			D_StartTitle();

		return;
	}
	else if (demo.recording && (modeattacking || demo.savemode != DSM_NOTSAVING))
		G_SaveDemo();
	else if (demo.recording)
		G_ResetDemoRecording();

	if (modeattacking) // End the run.
	{
		M_EndModeAttackRun();
		return;
	}

	if (mapheaderinfo[gamemap-1]->cutscenenum) // Start a custom cutscene.
		F_StartCustomCutscene(mapheaderinfo[gamemap-1]->cutscenenum-1, false, false);
	else
	{
		if (nextmap < 1100-1)
			G_NextLevel();
		else
			G_EndGame();
	}
}

//
// G_NextLevel (WorldDone)
//
// init next level or go to the final scene
// called by end of intermission screen (y_inter)
//
void G_NextLevel(void)
{
	if (gamestate != GS_VOTING)
	{
		if ((cv_advancemap.value == 3) && !modeattacking && !skipstats && (multiplayer || netgame))
		{
			UINT8 i;
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (playeringame[i] && !players[i].spectator)
				{
					gameaction = ga_startvote;
					return;
				}
			}
		}

		forceresetplayers = false;
		deferencoremode = (boolean)cv_kartencore.value;
	}

	gameaction = ga_worlddone;
}

static void G_DoWorldDone(void)
{
	if (server)
	{
		// SRB2Kart
		D_MapChange(nextmap+1,
			gametype,
			deferencoremode,
			forceresetplayers,
			0,
			false,
			false);
	}

	gameaction = ga_nothing;
}

//
// G_DoStartVote
//
static void G_DoStartVote(void)
{
	if (server)
		D_SetupVote();
	gameaction = ga_nothing;
}

//
// G_UseContinue
//
void G_UseContinue(void)
{
	if (gamestate == GS_LEVEL && !netgame && !multiplayer)
	{
		gameaction = ga_startcont;
		lastdraw = true;
	}
}

static void G_DoStartContinue(void)
{
	I_Assert(!netgame && !multiplayer);

	legitimateexit = false;
	G_PlayerFinishLevel(consoleplayer); // take away cards and stuff

	F_StartContinue();
	gameaction = ga_nothing;
}

//
// G_Continue
//
// re-init level, used by continue and possibly countdowntimeup
//
void G_Continue(void)
{
	if (!netgame && !multiplayer)
		gameaction = ga_continued;
}

static void G_DoContinued(void)
{
	player_t *pl = &players[consoleplayer];
	I_Assert(!netgame && !multiplayer);
	I_Assert(pl->continues > 0);

	pl->continues--;

	// Reset score
	pl->score = 0;

	// Allow tokens to come back
	tokenlist = 0;
	token = 0;

	// Reset # of lives
	pl->lives = (ultimatemode) ? 1 : 3;

	D_MapChange(gamemap, gametype, false, false, 0, false, false);

	gameaction = ga_nothing;
}

//
// G_EndGame (formerly Y_EndGame)
// Frankly this function fits better in g_game.c than it does in y_inter.c
//
// ...Gee, (why) end the game?
// Because G_AfterIntermission and F_EndCutscene would
// both do this exact same thing *in different ways* otherwise,
// which made it so that you could only unlock Ultimate mode
// if you had a cutscene after the final level and crap like that.
// This function simplifies it so only one place has to be updated
// when something new is added.
void G_EndGame(void)
{
	if (demo.recording && (modeattacking || demo.savemode != DSM_NOTSAVING))
		G_SaveDemo();
	else if (demo.recording)
		G_ResetDemoRecording();

	// Only do evaluation and credits in coop games.
	if (gametype == GT_COOP)
	{
		if (nextmap == 1102-1) // end game with credits
		{
			F_StartCredits();
			return;
		}
		if (nextmap == 1101-1) // end game with evaluation
		{
			F_StartGameEvaluation();
			return;
		}
	}

	// 1100 or competitive multiplayer, so go back to title screen.
	D_StartTitle();
}

//
// G_LoadGameSettings
//
// Sets a tad of default info we need.
void G_LoadGameSettings(void)
{
	// defaults
	spstage_start = 1;
	sstage_start = 50;
	sstage_end = 57; // 8 special stages in vanilla SRB2
	useNightsSS = false; //true;

	// initialize free sfx slots for skin sounds
	S_InitRuntimeSounds();
}

// G_LoadGameData
// Loads the main data file, which stores information such as emblems found, etc.
void G_LoadGameData(void)
{
	size_t length;
	INT32 i, j;
	UINT8 modded = false;
	UINT8 rtemp;
	savebuffer_t save;

	//For records
	tic_t rectime;
	tic_t reclap;

	// Clear things so previously read gamedata doesn't transfer
	// to new gamedata
	G_ClearRecords(); // main and nights records
	M_ClearSecrets(); // emblems, unlocks, maps visited, etc
	K_EraseStats(); // stats

	if (M_CheckParm("-nodata"))
		return; // Don't load.

	// Allow saving of gamedata beyond this point
	gamedataloaded = true;

	if (M_CheckParm("-resetdata"))
		return; // Don't load (essentially, reset).

	length = FIL_ReadFile(va(pandf, srb2home, gamedatafilename), &save.buffer);
	if (!length) // Aw, no game data. Their loss!
		return;

	save.p = save.buffer;

	// Version check
	if (READUINT32(save.p) != 0xFCAFE211)
	{
		const char *gdfolder = "the SRB2Kart folder";
		if (strcmp(srb2home,"."))
			gdfolder = srb2home;

		Z_Free(save.buffer);
		save.p = NULL;
		I_Error("Game data is from another version of SRB2.\nDelete %s(maybe in %s) and try again.", gamedatafilename, gdfolder);
	}

	// well no clue but dont think it would like reading garbage from vanilla files
	K_ReadStats(&save, !savemoddata);

	modded = READUINT8(save.p);

	// Aha! Someone's been screwing with the save file!
	if ((modded && !savemoddata))
		goto datacorrupt;
	else if (modded != true && modded != false)
		goto datacorrupt;

	// TODO put another cipher on these things? meh, I don't care...
	for (i = 0; i < NUMMAPS; i++)
		if ((mapvisited[i] = READUINT8(save.p)) > MV_MAX)
			goto datacorrupt;

	// To save space, use one bit per collected/achieved/unlocked flag
	for (i = 0; i < MAXEMBLEMS;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < MAXEMBLEMS; ++j)
			emblemlocations[j+i].collected = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < MAXEXTRAEMBLEMS;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < MAXEXTRAEMBLEMS; ++j)
			extraemblems[j+i].collected = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < MAXUNLOCKABLES;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < MAXUNLOCKABLES; ++j)
			unlockables[j+i].unlocked = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < MAXCONDITIONSETS;)
	{
		rtemp = READUINT8(save.p);
		for (j = 0; j < 8 && j+i < MAXCONDITIONSETS; ++j)
			conditionSets[j+i].achieved = ((rtemp >> j) & 1);
		i += j;
	}

	timesBeaten = READUINT32(save.p);
	timesBeatenWithEmeralds = READUINT32(save.p);

	// Main records
	for (i = 0; i < NUMMAPS; ++i)
	{
		rectime = (tic_t)READUINT32(save.p);
		reclap  = (tic_t)READUINT32(save.p);

		if (rectime || reclap)
		{
			G_AllocMainRecordData((INT16)i);
			mainrecords[i]->time = rectime;
			mainrecords[i]->lap = reclap;
		}
	}

	// done
	Z_Free(save.buffer);
	save.p = NULL;

	// Silent update unlockables in case they're out of sync with conditions
	M_SilentUpdateUnlockablesAndEmblems();

	return;

	// Landing point for corrupt gamedata
	datacorrupt:
	{
		const char *gdfolder = "the SRB2Kart folder";
		if (strcmp(srb2home,"."))
			gdfolder = srb2home;

		Z_Free(save.buffer);
		save.p = NULL;

		I_Error("Corrupt game data file.\nDelete %s(maybe in %s) and try again.", gamedatafilename, gdfolder);
	}
}

// G_SaveGameData
// Saves the main data file, which stores information such as emblems found, etc.
void G_SaveGameData(boolean force)
{
	size_t length;
	INT32 i, j;
	UINT8 btemp;
	savebuffer_t save;
	(void)force;
	char backupfile[MAX_WADPATH+4];

	if (!gamedataloaded)
		return; // If never loaded (-nodata), don't save

	save.p = save.buffer = (UINT8 *)malloc(GAMEDATASIZE);
	if (!save.p)
	{
		CONS_Alert(CONS_ERROR, M_GetText("No more free memory for saving game data\n"));
		return;
	}

	// Create backup of the save data
	snprintf(backupfile, sizeof(backupfile), "%s.bak", gamedatafilename);
	backupfile[sizeof(backupfile) - 1] = '\0';

	FILE *gamedata = fopen(gamedatafilename, "r");

	if (gamedata != NULL)
	{
		fclose(gamedata);

		if (!FIL_CopyFile(gamedatafilename, backupfile))
		{
			CONS_Alert(CONS_WARNING,"Failed to create a backup of save data. Will not attempt to write to save data\n");
			return;
		}
	}

	// Version test
	WRITEUINT32(save.p, 0xFCAFE211);

	K_WriteStats(&save, !savemoddata);

	btemp = (UINT8)(savemoddata); // what used to be here was profoundly dunderheaded
	WRITEUINT8(save.p, btemp);

	// TODO put another cipher on these things? meh, I don't care...
	for (i = 0; i < NUMMAPS; i++)
		WRITEUINT8(save.p, mapvisited[i]);

	// To save space, use one bit per collected/achieved/unlocked flag
	for (i = 0; i < MAXEMBLEMS;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXEMBLEMS; ++j)
			btemp |= (emblemlocations[j+i].collected << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}
	for (i = 0; i < MAXEXTRAEMBLEMS;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXEXTRAEMBLEMS; ++j)
			btemp |= (extraemblems[j+i].collected << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}
	for (i = 0; i < MAXUNLOCKABLES;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXUNLOCKABLES; ++j)
			btemp |= (unlockables[j+i].unlocked << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}
	for (i = 0; i < MAXCONDITIONSETS;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXCONDITIONSETS; ++j)
			btemp |= (conditionSets[j+i].achieved << j);
		WRITEUINT8(save.p, btemp);
		i += j;
	}

	WRITEUINT32(save.p, timesBeaten);
	WRITEUINT32(save.p, timesBeatenWithEmeralds);

	// Main records
	for (i = 0; i < NUMMAPS; i++)
	{
		if (mainrecords[i])
		{
			WRITEUINT32(save.p, mainrecords[i]->time);
			WRITEUINT32(save.p, mainrecords[i]->lap);
		}
		else
		{
			WRITEUINT32(save.p, 0);
			WRITEUINT32(save.p, 0);
		}
	}

	length = save.p - save.buffer;

	FIL_WriteFile(va(pandf, srb2home, gamedatafilename), save.buffer, length);
	free(save.buffer);
	save.p = save.buffer = NULL;
}

#define VERSIONSIZE 16

#ifdef SAVEGAMES_OTHERVERSIONS
static INT16 startonmapnum = 0;

//
// User wants to load a savegame from a different version?
//
static void M_ForceLoadGameResponse(INT32 ch)
{
	if (ch != 'y' && ch != KEY_ENTER)
	{
		//refused
		Z_Free(save.buffer);
		save.p = save.buffer = NULL;
		startonmapnum = 0;
		M_SetupNextMenu(&SP_LoadDef);
		return;
	}

	// pick up where we left off.
	save.p += VERSIONSIZE;

	if (!P_LoadGame(startonmapnum))
	{
		M_ClearMenus(true); // so ESC backs out to title
		M_StartMessage(M_GetText("Savegame file corrupted\n\nPress ESC\n"), NULL, MM_NOTHING);
		Command_ExitGame_f();
		Z_Free(save.buffer);
		save.p = save.buffer = NULL;
		startonmapnum = 0;

		// no cheating!
		memset(&savedata, 0, sizeof(savedata));
		return;
	}

	// done
	Z_Free(save.buffer);
	save.p = save.buffer = NULL;
	startonmapnum = 0;

	//set cursaveslot to -1 so nothing gets saved.
	cursaveslot = -1;

	displayplayers[0] = consoleplayer;
	multiplayer = false;
	splitscreen = 0;
	SplitScreen_OnChange(); // not needed?

	if (setsizeneeded)
		R_ExecuteSetViewSize();

	M_ClearMenus(true);
	CON_ToggleOff();
}
#endif

//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//
void G_LoadGame(UINT32 slot, INT16 mapoverride)
{
	size_t length;
	char vcheck[VERSIONSIZE];
	char savename[255];
	savebuffer_t save;

	// memset savedata to all 0, fixes calling perfectly valid saves corrupt because of bots
	memset(&savedata, 0, sizeof(savedata));

#ifdef SAVEGAME_OTHERVERSIONS
	//Oh christ.  The force load response needs access to mapoverride too...
	startonmapnum = mapoverride;
#endif

	sprintf(savename, savegamename, slot);

	length = FIL_ReadFile(savename, &save.buffer);
	if (!length)
	{
		CONS_Printf(M_GetText("Couldn't read file %s\n"), savename);
		return;
	}

	save.p = save.buffer;

	memset(vcheck, 0, sizeof (vcheck));
	sprintf(vcheck, "version %d", VERSION);
	if (strcmp((const char *)save.p, (const char *)vcheck))
	{
#ifdef SAVEGAME_OTHERVERSIONS
		M_StartMessage(M_GetText("Save game from different version.\nYou can load this savegame, but\nsaving afterwards will be disabled.\n\nDo you want to continue anyway?\n\n(Press 'Y' to confirm)\n"),
		               M_ForceLoadGameResponse, MM_YESNO);
		//Freeing done by the callback function of the above message
#else
		M_ClearMenus(true); // so ESC backs out to title
		M_StartMessage(M_GetText("Save game from different version\n\nPress ESC\n"), NULL, MM_NOTHING);
		Command_ExitGame_f();
		Z_Free(save.buffer);
		save.p = save.buffer = NULL;

		// no cheating!
		memset(&savedata, 0, sizeof(savedata));
#endif
		return; // bad version
	}
	save.p += VERSIONSIZE;

	if (demo.playback) // reset game engine
		G_StopDemo();

	// dearchive all the modifications
	if (!P_LoadGame(&save, mapoverride))
	{
		M_ClearMenus(true); // so ESC backs out to title
		M_StartMessage(M_GetText("Savegame file corrupted\n\nPress ESC\n"), NULL, MM_NOTHING);
		Command_ExitGame_f();
		Z_Free(save.buffer);
		save.p = save.buffer = NULL;

		// no cheating!
		memset(&savedata, 0, sizeof(savedata));
		return;
	}

	// done
	Z_Free(save.buffer);
	save.p = save.buffer = NULL;

	displayplayers[0] = consoleplayer;
	multiplayer = false;
	splitscreen = 0;
	SplitScreen_OnChange(); // not needed?

	if (setsizeneeded)
		R_ExecuteSetViewSize();

	M_ClearMenus(true);
	CON_ToggleOff();
}

//
// G_SaveGame
// Saves your game.
//
void G_SaveGame(UINT32 savegameslot)
{
	boolean saved;
	char savename[256] = "";
	const char *backup;
	savebuffer_t save;

	sprintf(savename, savegamename, savegameslot);
	backup = va("%s",savename);

	// save during evaluation or credits? game's over, folks!
	if (gamestate == GS_CREDITS || gamestate == GS_EVALUATION)
		gamecomplete = true;

	gameaction = ga_nothing;
	{
		char name[VERSIONSIZE];
		size_t length;

		save.p = save.buffer = (UINT8 *)malloc(SAVEGAMESIZE);
		if (!save.p)
		{
			CONS_Alert(CONS_ERROR, M_GetText("No more free memory for saving game data\n"));
			return;
		}

		memset(name, 0, sizeof (name));
		sprintf(name, "version %d", VERSION);
		WRITEMEM(save.p, name, VERSIONSIZE);

		P_SaveGame(&save);

		length = save.p - save.buffer;
		saved = FIL_WriteFile(backup, save.buffer, length);
		free(save.buffer);
		save.p = save.buffer = NULL;
	}

	gameaction = ga_nothing;

	if (cv_debug && saved)
		CONS_Printf(M_GetText("Game saved.\n"));
	else if (!saved)
		CONS_Alert(CONS_ERROR, M_GetText("Error while writing to %s for save slot %u, base: %s\n"), backup, savegameslot, savegamename);
}

//
// G_DeferedInitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayers[], playeringame[] should be set.
//
void G_DeferedInitNew(boolean pencoremode, const char *mapname, INT32 pickedchar, UINT8 ssplayers, boolean FLS)
{
	INT32 i;
	UINT8 color = 0;
	paused = false;

	if (demo.playback)
		COM_BufAddText("stopdemo\n");

	while (ghosts)
	{
		demoghost *next = ghosts->next;
		Z_Free(ghosts);
		ghosts = next;
	}
	ghosts = NULL;

	for (i = 0; i < NUMMAPS+1; i++)
		randmapbuffer[i] = -1;

	// this leave the actual game if needed
	SV_StartSinglePlayerServer();

	if (savedata.lives > 0)
	{
		color = savedata.skincolor;
		botskin = savedata.botskin;
		botcolor = savedata.botcolor;
		botingame = (botskin != 0);
	}
	else if (splitscreen != ssplayers)
	{
		splitscreen = ssplayers;
		SplitScreen_OnChange();
	}

	if (!color && !modeattacking)
		color = skins[pickedchar].prefcolor;
	SetPlayerSkinByNum(consoleplayer, pickedchar);
	CV_StealthSet(&cv_skin, skins[pickedchar].name);

	if (color)
		CV_StealthSetValue(&cv_playercolor, color);

	if (mapname)
		D_MapChange(M_MapNumber(mapname[3], mapname[4]), gametype, pencoremode, true, 1, false, FLS);
}

//
// This is the map command interpretation something like Command_Map_f
//
// called at: map cmd execution, doloadgame, doplaydemo
void G_InitNew(UINT8 pencoremode, const char *mapname, boolean resetplayer, boolean skipprecutscene)
{
	INT32 i;

	if (paused)
	{
		paused = false;
		S_ResumeAudio();
	}

	prevencoremode = ((gamestate == GS_TITLESCREEN) ? false : encoremode);
	encoremode = pencoremode;

	legitimateexit = false; // SRB2Kart
	comebackshowninfo = false;

	if (!demo.playback && !netgame) // Netgame sets random seed elsewhere, demo playback sets seed just before us!
		P_SetRandSeed(M_RandomizedSeed()); // Use a more "Random" random seed

	//SRB2Kart - Score is literally the only thing you SHOULDN'T reset at all times
	//if (resetplayer)
	{
		// Clear a bunch of variables
		tokenlist = token = sstimer = redscore = bluescore = lastmap = 0;
		racecountdown = exitcountdown = mapreset = 0;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			players[i].playerstate = PST_REBORN;
			players[i].starpostangle = players[i].starpostnum = players[i].starposttime = 0;
			players[i].starpostx = players[i].starposty = players[i].starpostz = 0;
			players[i].lives = 1; // SRB2Kart

			// This should be cleared in P_SpawnPlayer but address sanitizer says "use-after-free"
			// when reloading map sometimes
			players[i].awayviewtics = 0;
			players[i].awayviewmobj = NULL;

			// The latter two should clear by themselves, but just in case
			players[i].pflags &= ~(PF_TAGIT|PF_TAGGED|PF_FULLSTASIS);

			// Clear cheatcodes too, just in case.
			players[i].pflags &= ~(PF_GODMODE|PF_NOCLIP|PF_INVIS);

			players[i].marescore = 0;

			if (resetplayer && !(multiplayer && demo.playback)) // SRB2Kart
			{
				players[i].score = 0;
			}
		}

		// Reset unlockable triggers
		unlocktriggers = 0;

		// clear itemfinder, just in case
		if (!dedicated) // except in dedicated servers, where it is not registered and can actually I_Error debug builds
			CV_StealthSetValue(&cv_itemfinder, 0);
	}

	// internal game map
	// well this check is useless because it is done before (d_netcmd.c::command_map_f)
	// but in case of for demos....
	if (W_CheckNumForName(mapname) == LUMPERROR)
	{
		I_Error("Internal game map '%s' not found\n", mapname);
		Command_ExitGame_f();
		return;
	}

	gamemap = (INT16)M_MapNumber(mapname[3], mapname[4]); // get xx out of MAPxx

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if (!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	maptol = mapheaderinfo[gamemap-1]->typeoflevel;
	globalweather = mapheaderinfo[gamemap-1]->weather;

	// Don't carry over custom music change to another map.
	mapmusic.flags |= MUSIC_RELOADRESET;

	automapactive = false;
	imcontinuing = false;

	if (!skipprecutscene && mapheaderinfo[gamemap-1]->precutscenenum && !modeattacking) // Start a custom cutscene.
		F_StartCustomCutscene(mapheaderinfo[gamemap-1]->precutscenenum-1, true, resetplayer);
	else
	{
		LUA_HookInt(gamemap, HOOK(MapChange));
		G_DoLoadLevel(resetplayer);
	}

	if (netgame)
	{
		char *title = G_BuildMapTitle(gamemap);

		CON_LogMessage(va(M_GetText("Map is now \"%s"), G_BuildMapName(gamemap)));
		if (title)
		{
			CON_LogMessage(va(": %s", title));
			Z_Free(title);
		}
		CON_LogMessage("\"\n");
	}
}

char *G_BuildMapTitle(INT32 mapnum)
{
	char *title = NULL;

	if (mapnum == 0)
		return Z_StrDup("Random");

	if (!mapheaderinfo[mapnum-1])
		P_AllocMapHeader(mapnum-1);

	if (strcmp(mapheaderinfo[mapnum-1]->lvlttl, ""))
	{
		size_t len = 1;
		const char *zonetext = NULL;
		const char *actnum = NULL;

		len += strlen(mapheaderinfo[mapnum-1]->lvlttl);
		if (strlen(mapheaderinfo[mapnum-1]->zonttl) > 0)
		{
			zonetext = M_GetText(mapheaderinfo[mapnum-1]->zonttl);
			len += strlen(zonetext) + 1;	// ' ' + zonetext
		}
		else if (!(mapheaderinfo[mapnum-1]->levelflags & LF_NOZONE))
		{
			zonetext = M_GetText("Zone");
			len += strlen(zonetext) + 1;	// ' ' + zonetext
		}
		if (strlen(mapheaderinfo[mapnum-1]->actnum) > 0)
		{
			actnum = M_GetText(mapheaderinfo[mapnum-1]->actnum);
			len += strlen(actnum) + 1;	// ' ' + actnum
		}

		title = Z_Malloc(len, PU_STATIC, NULL);

		sprintf(title, "%s", mapheaderinfo[mapnum-1]->lvlttl);
		if (zonetext) sprintf(title + strlen(title), " %s", zonetext);
		if (actnum) sprintf(title + strlen(title), " %s", actnum);
	}

	return title;
}

static void measurekeywords(mapsearchfreq_t *fr,
		struct searchdim **dimp, UINT8 *cuntp,
		const char *s, const char *q, boolean wanttable)
{
	char *qp;
	char *sp;
	if (wanttable)
		(*dimp) = Z_Realloc((*dimp), 255 * sizeof (struct searchdim),
				PU_STATIC, NULL);
	for (qp = strtok(va("%s", q), " ");
			qp && fr->total < 255;
			qp = strtok(0, " "))
	{
		if (( sp = strcasestr(s, qp) ))
		{
			if (wanttable)
			{
				(*dimp)[(*cuntp)].pos = sp - s;
				(*dimp)[(*cuntp)].siz = strlen(qp);
			}
			(*cuntp)++;
			fr->total++;
		}
	}
	if (wanttable)
		(*dimp) = Z_Realloc((*dimp), (*cuntp) * sizeof (struct searchdim),
				PU_STATIC, NULL);
}

static void writesimplefreq(mapsearchfreq_t *fr, INT32 *frc,
		INT32 mapnum, UINT8 pos, UINT8 siz)
{
	fr[(*frc)].mapnum = mapnum;
	fr[(*frc)].matchd = ZZ_Alloc(sizeof (struct searchdim));
	fr[(*frc)].matchd[0].pos = pos;
	fr[(*frc)].matchd[0].siz = siz;
	fr[(*frc)].matchc = 1;
	fr[(*frc)].total = 1;
	(*frc)++;
}

INT32 G_FindMap(const char *mapname, char **foundmapnamep,
		mapsearchfreq_t **freqp, INT32 *freqcp)
{
	INT32 newmapnum = 0;
	INT32 mapnum;
	INT32 apromapnum = 0;

	size_t      mapnamelen;
	char   *realmapname = NULL;
	char   *newmapname = NULL;
	char   *apromapname = NULL;
	char   *aprop = NULL;

	mapsearchfreq_t *freq;
	boolean wanttable;
	INT32 freqc;
	UINT8 frequ;

	INT32 i;

	mapnamelen = strlen(mapname);

	/* Count available maps; how ugly. */
	for (i = 0, freqc = 0; i < NUMMAPS; ++i)
	{
		if (mapheaderinfo[i])
			freqc++;
	}

	freq = ZZ_Calloc(freqc * sizeof (mapsearchfreq_t));

	wanttable = !!( freqp );

	freqc = 0;
	for (i = 0, mapnum = 1; i < NUMMAPS; ++i, ++mapnum)
		if (mapheaderinfo[i])
	{
		if (!( realmapname = G_BuildMapTitle(mapnum) ))
			continue;

		aprop = realmapname;

		/* Now that we found a perfect match no need to fucking guess. */
		if (strnicmp(realmapname, mapname, mapnamelen) == 0)
		{
			if (wanttable)
			{
				writesimplefreq(freq, &freqc, mapnum, 0, mapnamelen);
			}
			if (newmapnum == 0)
			{
				newmapnum = mapnum;
				newmapname = realmapname;
				realmapname = 0;
				Z_Free(apromapname);
				if (!wanttable)
					break;
			}
		}
		else if (apromapnum == 0 || wanttable)
		{
			/* LEVEL 1--match keywords verbatim */
			if (( aprop = strcasestr(realmapname, mapname) ))
			{
				if (wanttable)
				{
					writesimplefreq(freq, &freqc,
							mapnum, aprop - realmapname, mapnamelen);
				}
				if (apromapnum == 0)
				{
					apromapnum = mapnum;
					apromapname = realmapname;
					realmapname = 0;
				}
			}
			else/* ...match individual keywords */
			{
				freq[freqc].mapnum = mapnum;
				measurekeywords(&freq[freqc],
						&freq[freqc].matchd, &freq[freqc].matchc,
						realmapname, mapname, wanttable);
				if (freq[freqc].total)
					freqc++;
			}
		}

		Z_Free(realmapname);/* leftover old name */
	}

	if (newmapnum == 0)/* no perfect match--try a substring */
	{
		newmapnum = apromapnum;
		newmapname = apromapname;
	}

	if (newmapnum == 0)/* calculate most queries met! */
	{
		frequ = 0;
		for (i = 0; i < freqc; ++i)
		{
			if (freq[i].total > frequ)
			{
				frequ = freq[i].total;
				newmapnum = freq[i].mapnum;
			}
		}
		if (newmapnum)
		{
			newmapname = G_BuildMapTitle(newmapnum);
		}
	}

	if (freqp)
		(*freqp) = freq;
	else
		Z_Free(freq);

	if (freqcp)
		(*freqcp) = freqc;

	if (foundmapnamep)
		(*foundmapnamep) = newmapname;
	else
		Z_Free(newmapname);

	return newmapnum;
}

void G_FreeMapSearch(mapsearchfreq_t *freq, INT32 freqc)
{
	INT32 i;
	for (i = 0; i < freqc; ++i)
	{
		Z_Free(freq[i].matchd);
	}
	Z_Free(freq);
}

INT32 G_FindMapByNameOrCode(const char *mapname, char **realmapnamep)
{
	boolean usemapcode = false;
	INT32 newmapnum = -1;
	size_t mapnamelen = strlen(mapname);
	char *p;

	if (mapnamelen == 1)
	{
		if (mapname[0] == '*') // current map
			return gamemap;
		else if (mapname[0] == '?' && mapheaderinfo[gamemap-1])
			return G_RandMap(G_TOLFlag(gametype), gamemap-1, false, 0, false, NULL)+1;
		else if (mapname[0] == '+' && mapheaderinfo[gamemap-1]) // next map
		{
			newmapnum = mapheaderinfo[gamemap-1]->nextlevel;
			if (newmapnum < 1 || newmapnum > NUMMAPS)
			{
				CONS_Alert(CONS_ERROR, M_GetText("NextLevel (%d) is not a valid map.\n"), newmapnum);
				return 0;
			}
			else
				return newmapnum;
		}
	}
	else if (mapnamelen == 2)/* maybe two digit code */
	{
		if (( newmapnum = M_MapNumber(mapname[0], mapname[1]) ))
			usemapcode = true;
	}
	else if (mapnamelen == 5 && strnicmp(mapname, "MAP", 3) == 0)
	{
		if (( newmapnum = M_MapNumber(mapname[3], mapname[4]) ))
			usemapcode = true;
	}

	if (!usemapcode)
	{
		/* Now detect map number in base 10, which no one asked for. */
		newmapnum = strtol(mapname, &p, 10);
		if (*p == '\0')/* we got it */
		{
			if (newmapnum < 1 || newmapnum > NUMMAPS)
			{
				CONS_Alert(CONS_ERROR, M_GetText("Invalid map number %d.\n"), newmapnum);
				return 0;
			}
			usemapcode = true;
		}
		else
		{
			newmapnum = G_FindMap(mapname, realmapnamep, NULL, NULL);
		}
	}

	if (usemapcode)
	{
		/* we can't check mapheaderinfo for this hahahaha */
		if (W_CheckNumForName(G_BuildMapName(newmapnum)) == LUMPERROR)
			return 0;

		if (realmapnamep)
			(*realmapnamep) = G_BuildMapTitle(newmapnum);
	}

	return newmapnum;
}

//
// G_SetGamestate
//
// Use this to set the gamestate, please.
//
void G_SetGamestate(gamestate_t newstate)
{
	gamestate = newstate;

	//HACK: reset musiccredits whenever we change gamestate
	// since we allow them to run everywhere now
	S_ResetMusicCredit();

#ifdef HAVE_DISCORDRPC
	DRPC_UpdatePresence();
#endif
}

/* These functions handle the exitgame flag. Before, when the user
   chose to end a game, it happened immediately, which could cause
   crashes if the game was in the middle of something. Now, a flag
   is set, and the game can then be stopped when it's safe to do
   so.
*/

// Used as a callback function.
void G_SetExitGameFlag(void)
{
	exitgame = true;
}

void G_ClearExitGameFlag(void)
{
	exitgame = false;
}

boolean G_GetExitGameFlag(void)
{
	return exitgame;
}

// Same deal with retrying.
void G_SetRetryFlag(void)
{
	retrying = true;
}

void G_ClearRetryFlag(void)
{
	retrying = false;
}

boolean G_GetRetryFlag(void)
{
	return retrying;
}
