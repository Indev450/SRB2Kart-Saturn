// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 2011-2016 by Matthew "Inuyasha" Walsh.
// Copyright (C) 1999-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  m_menu.c
/// \brief XMOD's extremely revamped menu system.

#include "screen.h"
#include "tables.h"
#ifdef __GNUC__
#include <unistd.h>
#endif

#include "m_menu.h"

#include "doomdef.h"
#include "d_main.h"
#include "d_netcmd.h"
#include "d_clisrv.h"
#include "i_net.h"
#include "console.h"
#include "r_fps.h"
#include "r_local.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "g_input.h"
#include "m_argv.h"
#include "m_textinput.h"

// Data.
#include "sounds.h"
#include "s_sound.h"
#include "i_time.h"
#include "i_system.h"
#include "i_threads.h"

// Addfile
#include "filesrch.h"

#include "v_video.h"
#include "i_video.h"
#include "keys.h"
#include "z_zone.h"
#include "w_wad.h"
#include "p_local.h"
#include "p_setup.h"
#include "f_finale.h"

#include "lua_libs.h"

#include "fastcmp.h"

#include "qs22j.h"

#ifdef HWRENDER
#include "hardware/hw_main.h"
#include "hardware/r_opengl/r_opengl.h"
#endif

#include "d_net.h"
#include "mserv.h"
#include "m_misc.h"
#include "m_anigif.h"
#include "byteptr.h"
#include "st_stuff.h"
#include "i_sound.h"
#include "k_kart.h" // SRB2kart
#include "k_hud.h" // SRB2kart
#include "k_stats.h" // SRB2kart
#include "d_player.h" // KITEM_ constants

#include "i_joy.h" // for joystick menu controls

// Condition Sets
#include "m_cond.h"

// And just some randomness for the exits.
#include "m_random.h"

// protocol handling
#include "d_protocol.h"

#include "z_zone.h"
#include "mserv.h"

#if defined(HAVE_SDL)
#include "SDL.h"
#if SDL_VERSION_ATLEAST(2,0,0)
#include "sdl/sdlmain.h" // JOYSTICK_HOTPLUG
#endif
#endif

#ifdef HAVE_DISCORDRPC
//#include "discord_rpc.h"
#include "discord.h"
#endif

#define SKULLXOFF -32
#define LINEHEIGHT 16
#define STRINGHEIGHT 8
#define FONTBHEIGHT 20
#define SMALLLINEHEIGHT 8
#define SLIDER_RANGE 10
#define SLIDER_WIDTH (8*SLIDER_RANGE+6)
#define SERVERS_PER_PAGE 10
#define MAXSTAT 9 //Max number a stat can have

typedef enum
{
	QUITMSG = 0,
	QUITMSG1,
	QUITMSG2,
	QUITMSG3,
	QUITMSG4,
	QUITMSG5,
	QUITMSG6,
	QUITMSG7,

	QUIT2MSG,
	QUIT2MSG1,
	QUIT2MSG2,
	QUIT2MSG3,
	QUIT2MSG4,
	QUIT2MSG5,
	QUIT2MSG6,

	QUIT3MSG,
	QUIT3MSG1,
	QUIT3MSG2,
	QUIT3MSG3,
	QUIT3MSG4,
	QUIT3MSG5,
	QUIT3MSG6,
	NUM_QUITMESSAGES
} text_enum;

#ifdef HAVE_THREADS
I_mutex m_menu_mutex;
#endif

M_waiting_mode_t m_waiting_mode = M_NOT_WAITING;

const char *quitmsg[NUM_QUITMESSAGES] = {};

// Stuff for customizing the player select screen Tails 09-22-2003
description_t description[MAXSKINS] = {};

INT32 mapwads[NUMMAPS] = {};

boolean browselocalskins = false;

boolean menuactive = false;
boolean fromlevelselect = false;

char menu_text_input_buf[MAXSTRINGLENGTH] = {};
static textinput_t menuinput;

static INT32 coolalphatimer = 9;

typedef enum
{
	LLM_CREATESERVER,
	LLM_LEVELSELECT,
	LLM_RECORDATTACK,
	LLM_NIGHTSATTACK
} levellist_mode_t;

levellist_mode_t levellistmode = LLM_CREATESERVER;

static char joystickInfo[8][29];

static UINT32 serverlistpage;
static UINT32 oldserverlistpage;
static float serverlistslidex;
static INT32 serverlistsearched[MAXSERVERLIST] = {0};
static UINT32 serverlistsearchedcount = 0;

static INT16 itemOn = 1; // menu item skull is on, Hack by Tails 09-18-2002
static INT16 skullAnimCounter = 10; // skull animation counter
static boolean interpTimerHackAllow = 0;

static UINT8 setupcontrolplayer;
static INT32 (*setupcontrols)[2];  // pointer to the gamecontrols of the player being edited

// shhh... what am I doing... nooooo!
static INT32 vidm_testingmode = 0;
static INT32 vidm_previousmode;
static INT32 vidm_selected = 0;
static INT32 vidm_nummodes;
static INT32 vidm_column_size;

#define SETUPM_IP_MAXSIZE ((28-1)*8)
static char setupm_ip[64];
static textinput_t setupm_input_ip;


static fixed_t  multi_tics;
static state_t *multi_state;

// this is set before entering the MultiPlayer setup menu,
// for either player 1 or 2
static char        setupm_name[MAXPLAYERNAME+1];
static textinput_t setupm_input;
static player_t   *setupm_player;
static consvar_t  *setupm_cvskin;
static consvar_t  *setupm_cvcolor;
static consvar_t  *setupm_cvname;
static UINT8       setupm_skinxpos;
static INT32       setupm_fakeskin;
static INT32       setupm_fakecolor;
static UINT8 	   setupm_pselect = 1;

//variables used for other skin select menus
static UINT8 setupm_skinypos;
static INT32 setupm_skinselect;
static boolean setupm_skinlockedselect;

static UINT8 setupm_playernum; //brap

//
// PROTOTYPES
//

static void M_StopMessage(INT32 choice);

static void M_HandleServerPage(INT32 choice);
static void M_HandleServerSearch(INT32 choice);
static void M_SearchServerList(void);

// Prototyping is fun, innit?
// ==========================================================================
// NEEDED FUNCTION PROTOTYPES GO HERE
// ==========================================================================

void M_SetWaitingMode(int mode);
int  M_GetWaitingMode(void);

#ifdef HAVE_DISCORDRPC
menu_t MISC_DiscordRequestsDef;
static void M_HandleDiscordRequests(INT32 choice);
static void M_DrawDiscordRequests(void);
#endif

#define lsheadingheight 16

// Sky Room
static void M_Credits(INT32 choice);
static void M_MusicTest(INT32 choice);
static char *M_GetConditionString(condition_t cond);

// Misc. Main Menu
static void M_Options(INT32 choice);
static void M_Multiplayer(INT32 choice);
static void M_CameraMenu(INT32 choice);
static void M_LocalSkinMenu(INT32 choice);
static void M_LocalSkinChange(INT32 choice);
static void M_Manual(INT32 choice);
static void M_SelectableClearMenus(INT32 choice);
static void M_Retry(INT32 choice);
static void M_EndGame(INT32 choice);
static void M_MapChange(INT32 choice);
static void M_ChangeLevel(INT32 choice);
static void M_ConfirmSpectate(INT32 choice);
static void M_ConfirmEnterGame(INT32 choice);
static void M_ConfirmTeamScramble(INT32 choice);
static void M_ConfirmTeamChange(INT32 choice);
static void M_ConfirmSpectateChange(INT32 choice);
static void M_QuitSRB2(INT32 choice);

// Single Player
static void M_TimeAttack(INT32 choice);
static boolean M_QuitTimeAttackMenu(void);
static void M_Statistics(INT32 choice);
static void M_HandleStaffReplay(INT32 choice);
static void M_ReplayTimeAttack(INT32 choice);
static void M_ChooseTimeAttack(INT32 choice);
static void M_ModeAttackEndGame(INT32 choice);
static void M_SetGuestReplay(INT32 choice);

// Multiplayer
static void M_PreStartServerMenu(INT32 choice);
#ifdef MASTERSERVER
static void M_PreStartServerMenuChoice(event_t *ev);
static void M_PreConnectMenu(INT32 choice);
static void M_PreConnectMenuChoice(event_t *ev);
#endif
static void M_StartServerMenu(INT32 choice);
#ifdef MASTERSERVER
static void M_ConnectMenu(INT32 choice);
static void M_ConnectMenuModChecks(INT32 choice);
#endif
static void M_Refresh(INT32 choice);
static void M_Connect(INT32 choice);
static void M_StartOfflineServerMenu(INT32 choice);
static void M_StartServer(INT32 choice);
static void M_SetupMultiPlayer(void);
static void M_SetupMultiPlayer2(void);
static void M_SetupMultiPlayer3(void);
static void M_SetupMultiPlayer4(void);
static void M_SetupMultiHandler(INT32 choice);

// Options
// Split into multiple parts due to size
static void M_VideoModeMenu(INT32 choice);
static void M_Setup1PControlsMenu(void);
static void M_Setup2PControlsMenu(void);
static void M_Setup3PControlsMenu(void);
static void M_Setup4PControlsMenu(void);

static void M_Setup1PJoystickMenu(INT32 choice);
static void M_Setup2PJoystickMenu(INT32 choice);
static void M_Setup3PJoystickMenu(INT32 choice);
static void M_Setup4PJoystickMenu(INT32 choice);

static void M_AssignJoystick(INT32 choice);
static void M_ChangeControl(INT32 choice);
static void M_ResetControls(INT32 choice);

static void M_ScreenshotOptions(INT32 choice);
static void M_EraseData(INT32 choice);

static void M_AddonsInternal();
static void M_Addons(INT32 choice);
static void M_LocalSkins(INT32 choice);
static void M_AddonsOptions(INT32 choice);
#define addonmenusize 9 // number of items actually displayed in the addons menu view, formerly (2*numaddonsshown + 1)
#define numaddonsshown 4 // number of items to each side of the currently selected item, unless at top/bottom ends of directory

static void M_CustomCvarMenu(INT32 choice);
static patch_t *addonsp[NUM_EXT+5];

static void M_DeleteProtocol(void);

// Replay hut
static void M_HandleReplayHutList(INT32 choice);
static void M_HutCheckReplays(size_t maxnum);
static void M_DrawReplayHut(void);
static void M_DrawReplayStartMenu(void);
static boolean M_QuitReplayHut(void);
static void M_HutStartReplay(INT32 choice);

static void M_DrawPlaybackMenu(void);
static void M_PlaybackRewind(INT32 choice);
static void M_PlaybackPause(INT32 choice);
static void M_PlaybackFastForward(INT32 choice);
static void M_PlaybackAdvance(INT32 choice);
static void M_PlaybackSetViews(INT32 choice);
static void M_PlaybackAdjustView(INT32 choice);
static void M_PlaybackToggleFreecam(INT32 choice);
static void M_PlaybackQuit(INT32 choice);

static UINT8 playback_enterheld = 0; // horrid hack to prevent holding the button from being extremely fucked

// Drawing functions
static void M_DrawGenericMenu(void);
static void M_DrawGenericBackgroundMenu(void);
static void M_DrawGenericScrollMenu(void);
static void M_DrawCenteredMenu(void);
static void M_DrawAddons(void);
static void M_DrawSkyRoom(void);
static void M_DrawChecklist(void);
static void M_DrawMusicTest(void);
static void M_DrawPauseMenu(void);
static void M_DrawLevelSelectOnly(boolean leftfade, boolean rightfade);
static void M_DrawServerMenu(void);
static void M_DrawImageDef(void);
static void M_DrawLevelStats(void);
static void M_DrawTimeAttackMenu(void);
static void M_DrawControl(void);
static void M_DrawVideoMenu(void);
static void M_DrawHUDOptions(void);
static void M_DrawVideoMode(void);
static void M_DrawColorMenu(void);
static void M_DrawMonitorToggles(void);
static void M_DrawMPMainMenu(void);
static void M_DrawConnectMenu(void);
static void M_DrawJoystick(void);
static void M_DrawSetupMultiPlayerMenu(void);
static void M_DrawLocalSkinMenu(void);

// Handling functions
static boolean M_CancelConnect(void);
static boolean M_QuitMultiPlayerMenu(void);
static void M_HandleAddons(INT32 choice);
static void M_HandleSoundTest(INT32 choice);
static void M_HandleMusicTest(INT32 choice);
static void M_HandleImageDef(INT32 choice);
static void M_HandleLevelStats(INT32 choice);
static void M_HandleConnectIP(INT32 choice);
static void M_ConnectLastServer(INT32 choice);
static void M_HandleSetupMultiPlayer(INT32 choice);
static void M_HandleVideoMode(INT32 choice);
static void M_ResetCvars(void);
static void M_HandleMonitorToggles(INT32 choice);
static void M_AddonsRefresh(void);

// Consvar onchange functions
static void Newgametype_OnChange(void);
static void Dummymenuplayer_OnChange(void);
static void Dummystaff_OnChange(void);

// Prototypes
static INT32 M_FindFirstMap(INT32 gtype);
static INT32 M_GetFirstLevelInList(void);

// crap to force hud to show when in saturns hud options
boolean forceshowhud = false;

// smol text indicating if game is modified
// so ppl dont wonder where their ra times went and stuff
#define SHOWMODDEDGAME \
	if (savemoddata) \
		V_DrawThinString(0, 0, V_REDMAP|V_SNAPTOTOP|V_SNAPTOLEFT|V_TRANSLUCENT|V_ALLOWLOWERCASE, ("Modified Game"));

// ==========================================================================
// CONSOLE VARIABLES AND THEIR POSSIBLE VALUES GO HERE.
// ==========================================================================

// How much replays are checked per frame when using search
consvar_t cv_replaysearchrate = {"replaysearchrate", "1000", CV_SAVE, CV_Natural, NULL, 0, NULL, NULL, 0, 0, NULL };

consvar_t cv_showfocuslost = {"showfocuslost", "Yes", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL };

static CV_PossibleValue_t map_cons_t[] = {
	{0,"MIN"},
	{NUMMAPS, "MAX"},
	{0, NULL}
};
consvar_t cv_nextmap = {"nextmap", "1", CV_HIDEN|CV_CALL, map_cons_t, Nextmap_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_nextmapaddon = {"nextmapaddon", "1", CV_HIDEN|CV_CALL, NULL, Nextmap_OnChange, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t skins_cons_t[MAXSKINS+1] = {{1, DEFAULTSKIN}};
consvar_t cv_chooseskin = {"chooseskin", DEFAULTSKIN, CV_HIDEN|CV_CALL, skins_cons_t, Nextmap_OnChange, 0, NULL, NULL, 0, 0, NULL};

// This gametype list is integral for many different reasons.
// When you add gametypes here, don't forget to update them in dehacked.c and doomstat.h!
CV_PossibleValue_t gametype_cons_t[NUMGAMETYPES+1];

consvar_t cv_newgametype = {"newgametype", "Race", CV_HIDEN|CV_CALL, gametype_cons_t, Newgametype_OnChange, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_showallmaps = {"showallmaps", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_showmusicfilename = {"showmusicfilename", "No", CV_SAVE, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t serversort_cons_t[] = {
	{0,"Ping"},
	{1,"Modified State"},
	{2,"Most Players"},
	{3,"Least Players"},
	{4,"Max Player Slots"},
	{5,"Gametype"},
	{0,NULL}
};
consvar_t cv_serversort = {"serversort", "Ping", CV_CALL, serversort_cons_t, M_SortServerList, 0, NULL, NULL, 0, 0, NULL};

// autorecord demos for time attack
static consvar_t cv_autorecord = {"autorecord", "Yes", 0, CV_YesNo, NULL, 0, NULL, NULL, 0, 0, NULL};

CV_PossibleValue_t ghost_cons_t[] = {{0, "Hide"}, {1, "Show Character"}, {2, "Show All"}, {0, NULL}};
CV_PossibleValue_t ghost2_cons_t[] = {{0, "Hide"}, {1, "Show"}, {0, NULL}};

consvar_t cv_ghost_besttime  = {"ghost_besttime",  "Show All", CV_SAVE, ghost_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ghost_bestlap   = {"ghost_bestlap",   "Show All", CV_SAVE, ghost_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ghost_last      = {"ghost_last",      "Show All", CV_SAVE, ghost_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ghost_guest     = {"ghost_guest",     "Show", CV_SAVE, ghost2_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_ghost_staff     = {"ghost_staff",     "Show", CV_SAVE, ghost2_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

//Console variables used solely in the menu system.
//todo: add a way to use non-console variables in the menu
//      or make these consvars legitimate like color or skin.
static void Splitplayers_OnChange(void);
CV_PossibleValue_t splitplayers_cons_t[] = {{1, "One"}, {2, "Two"}, {3, "Three"}, {4, "Four"}, {0, NULL}};
consvar_t cv_splitplayers = {"splitplayers", "One", CV_CALL, splitplayers_cons_t, Splitplayers_OnChange, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t dummymenuplayer_cons_t[] = {{0, "NOPE"}, {1, "P1"}, {2, "P2"}, {3, "P3"}, {4, "P4"}, {0, NULL}};
static CV_PossibleValue_t dummyteam_cons_t[] = {{0, "Spectator"}, {1, "Red"}, {2, "Blue"}, {0, NULL}};
static CV_PossibleValue_t dummyspectate_cons_t[] = {{0, "Spectator"}, {1, "Playing"}, {0, NULL}};
static CV_PossibleValue_t dummyscramble_cons_t[] = {{0, "Random"}, {1, "Points"}, {0, NULL}};
static CV_PossibleValue_t ringlimit_cons_t[] = {{0, "MIN"}, {9999, "MAX"}, {0, NULL}};
static CV_PossibleValue_t liveslimit_cons_t[] = {{0, "MIN"}, {99, "MAX"}, {0, NULL}};
static CV_PossibleValue_t dummystaff_cons_t[] = {{0, "MIN"}, {100, "MAX"}, {0, NULL}};

static consvar_t cv_dummymenuplayer = {"dummymenuplayer", "P1", CV_HIDEN|CV_CALL, dummymenuplayer_cons_t, Dummymenuplayer_OnChange, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_dummyteam = {"dummyteam", "Spectator", CV_HIDEN, dummyteam_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_dummyspectate = {"dummyspectate", "Spectator", CV_HIDEN, dummyspectate_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_dummyscramble = {"dummyscramble", "Random", CV_HIDEN, dummyscramble_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_dummyrings = {"dummyrings", "0", CV_HIDEN, ringlimit_cons_t,	NULL, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_dummylives = {"dummylives", "0", CV_HIDEN, liveslimit_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_dummycontinues = {"dummycontinues", "0", CV_HIDEN, liveslimit_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};
static consvar_t cv_dummystaff = {"dummystaff", "0", CV_HIDEN|CV_CALL, dummystaff_cons_t, Dummystaff_OnChange, 0, NULL, NULL, 0, 0, NULL};

// all the menu definitions and onchanges reside in here now
#include "m_menudefs.c"

static INT32 M_ShiftChar(INT32 ch)
{
	if (I_UseNativeKeyboard())
		return ch;

	if (cv_keyboardlayout.value == 3)
	{
		if (ch >= 32 && ch <= 141)
		{
			if (shiftdown)
				ch = shiftxform[ch];
			else if (altdown & 0x2)
				ch = french_altgrxform[ch];
			else
				ch = HU_FallBackFrSpecialLetter(ch);
		}
	}
	else
	{
		if (shiftdown && ch >= 32 && ch <= 127)
			ch = shiftxform[ch];
	}

	return ch;
}

//
// M_GetGametypeColor
//
// Pretty and consistent ^u^
// See also G_GetGametypeColor.
//

static INT32 highlightflags, recommendedflags, warningflags;

inline static void M_GetGametypeColor(void)
{
	INT16 gt;

	warningflags = V_REDMAP;
	recommendedflags = V_GREENMAP;

	if (cons_menuhighlight.value)
	{
		highlightflags = cons_menuhighlight.value;
		if (highlightflags == V_REDMAP)
		{
			warningflags = V_ORANGEMAP;
			return;
		}
		if (highlightflags == V_GREENMAP)
		{
			recommendedflags = V_SKYMAP;
			return;
		}
		return;
	}

	warningflags = V_REDMAP;
	recommendedflags = V_GREENMAP;

	if (modeattacking // == ATTACKING_RECORD
		|| gamestate == GS_TIMEATTACK)
	{
		highlightflags = V_ORANGEMAP;
		return;
	}

	if (currentMenu->drawroutine == M_DrawServerMenu)
		gt = cv_newgametype.value;
	else if (!Playing())
	{
		highlightflags = V_YELLOWMAP;
		return;
	}
	else
		gt = gametype;

	if (gt == GT_MATCH)
	{
		highlightflags = V_REDMAP;
		warningflags = V_ORANGEMAP;
		return;
	}
	if (gt == GT_RACE)
	{
		highlightflags = V_SKYMAP;
		return;
	}

	highlightflags = V_YELLOWMAP; // FALLBACK
}

// excuse me but I'm extremely lazy:
INT32 HU_GetHighlightColor(void)
{
	M_GetGametypeColor();	// update flag colour reguardless of the menu being opened or not.
	return highlightflags;
}

INT16 ccvarposition = 0;

static void M_CustomCvarMenu(INT32 choice)
{
	(void)choice;

	if (ccvarposition)
		M_SetupNextMenu(&OP_CustomCvarMenuDef);
	else
		M_StartMessage(M_GetText("No custom options were found\n"), NULL, MM_NOTHING);
}

// ==========================================================================
// CVAR ONCHANGE EVENTS GO HERE
// ==========================================================================
// (there's only a couple anyway)

// Nextmap.  Used for Time Attack.
void Nextmap_OnChange(void)
{
	char *leveltitle;
	UINT8 active;

	// Update the string in the consvar.
	Z_Free(cv_nextmap.zstring);
	leveltitle = G_BuildMapTitle(cv_nextmap.value);
	cv_nextmap.string = cv_nextmap.zstring = leveltitle ? leveltitle : Z_StrDup(G_BuildMapName(cv_nextmap.value));

	if (currentMenu == &SP_TimeAttackDef)
	{
		// see also p_setup.c's P_LoadRecordGhosts
		const size_t glen = strlen(srb2home)+1+strlen("replay")+1+strlen(timeattackfolder)+1+strlen("MAPXX")+1;
		char *gpath = malloc(glen);
		INT32 i;

		if (!gpath)
			return;

		sprintf(gpath,"%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s", srb2home, timeattackfolder, G_BuildMapName(cv_nextmap.value));

		CV_StealthSetValue(&cv_dummystaff, 0);

		active = false;
		SP_TimeAttackMenu[taguest].status = IT_DISABLED;
		SP_TimeAttackMenu[tareplay].status = IT_DISABLED;

		// Check if file exists, if not, disable REPLAY option
		for (i = 0; i < 4; i++)
		{
			SP_ReplayMenu[i].status = IT_DISABLED;
			SP_GuestReplayMenu[i].status = IT_DISABLED;
		}

		SP_ReplayMenu[4].status = IT_DISABLED;

		SP_GhostMenu[3].status = IT_DISABLED;
		SP_GhostMenu[4].status = IT_DISABLED;

		if (FIL_FileExists(va("%s-%s-time-best.lmp", gpath, cv_chooseskin.string)))
		{
			SP_ReplayMenu[0].status = IT_WHITESTRING|IT_CALL;
			SP_GuestReplayMenu[0].status = IT_WHITESTRING|IT_CALL;
			active |= 3;
		}

		if (FIL_FileExists(va("%s-%s-lap-best.lmp", gpath, cv_chooseskin.string)))
		{
			SP_ReplayMenu[1].status = IT_WHITESTRING|IT_CALL;
			SP_GuestReplayMenu[1].status = IT_WHITESTRING|IT_CALL;
			active |= 3;
		}

		if (FIL_FileExists(va("%s-%s-last.lmp", gpath, cv_chooseskin.string)))
		{
			SP_ReplayMenu[2].status = IT_WHITESTRING|IT_CALL;
			SP_GuestReplayMenu[2].status = IT_WHITESTRING|IT_CALL;
			active |= 3;
		}

		if (FIL_FileExists(va("%s-guest.lmp", gpath)))
		{
			SP_ReplayMenu[3].status = IT_WHITESTRING|IT_CALL;
			SP_GuestReplayMenu[3].status = IT_WHITESTRING|IT_CALL;
			SP_GhostMenu[3].status = IT_STRING|IT_CVAR;
			active |= 3;
		}

		CV_SetValue(&cv_dummystaff, 1);
		if (cv_dummystaff.value)
		{
			SP_ReplayMenu[4].status = IT_WHITESTRING|IT_KEYHANDLER;
			SP_GhostMenu[4].status = IT_STRING|IT_CVAR;
			CV_StealthSetValue(&cv_dummystaff, 1);
			active |= 1;
		}

		if (active)
		{
			if (active & 1)
				SP_TimeAttackMenu[tareplay].status = IT_WHITESTRING|IT_SUBMENU;
			if (active & 2)
				SP_TimeAttackMenu[taguest].status = IT_WHITESTRING|IT_SUBMENU;
		}
		else if (itemOn == tareplay) // Reset lastOn so replay isn't still selected when not available.
		{
			currentMenu->lastOn = itemOn;
			itemOn = tastart;
		}

		if (mapheaderinfo[cv_nextmap.value-1] && mapheaderinfo[cv_nextmap.value-1]->forcecharacter[0] != '\0')
			CV_Set(&cv_chooseskin, mapheaderinfo[cv_nextmap.value-1]->forcecharacter);

		free(gpath);
	}
}

static void Dummymenuplayer_OnChange(void)
{
	if (cv_dummymenuplayer.value < 1)
		CV_StealthSetValue(&cv_dummymenuplayer, splitscreen+1);
	else if (cv_dummymenuplayer.value > splitscreen+1)
		CV_StealthSetValue(&cv_dummymenuplayer, 1);
}

char dummystaffname[22];

static void Dummystaff_OnChange(void)
{
	lumpnum_t l;

	dummystaffname[0] = '\0';

	if ((l = W_CheckNumForName(va("%sS01",G_BuildMapName(cv_nextmap.value)))) == LUMPERROR)
	{
		CV_StealthSetValue(&cv_dummystaff, 0);
		return;
	}
	else
	{
		char *temp = dummystaffname;
		UINT8 numstaff = 1;
		while (numstaff < 99 && (l = W_CheckNumForName(va("%sS%02u",G_BuildMapName(cv_nextmap.value),numstaff+1))) != LUMPERROR)
			numstaff++;

		if (cv_dummystaff.value < 1)
			CV_StealthSetValue(&cv_dummystaff, numstaff);
		else if (cv_dummystaff.value > numstaff)
			CV_StealthSetValue(&cv_dummystaff, 1);

		if ((l = W_CheckNumForName(va("%sS%02u",G_BuildMapName(cv_nextmap.value), cv_dummystaff.value))) == LUMPERROR)
			return; // shouldn't happen but might as well check...

		G_UpdateStaffGhostName(l);

		while (*temp)
			temp++;

		sprintf(temp, " - %d", cv_dummystaff.value);
	}
}

// Newgametype.  Used for gametype changes.
static void Newgametype_OnChange(void)
{
	if (cv_nextmap.value && menuactive)
	{
		if (!mapheaderinfo[cv_nextmap.value-1])
			P_AllocMapHeader((INT16)(cv_nextmap.value-1));

		if ((cv_newgametype.value == GT_RACE && !(mapheaderinfo[cv_nextmap.value-1]->typeoflevel & TOL_RACE)) || // SRB2kart
			((cv_newgametype.value == GT_MATCH || cv_newgametype.value == GT_TEAMMATCH) && !(mapheaderinfo[cv_nextmap.value-1]->typeoflevel & TOL_MATCH)))
		{
			INT32 value = 0;

			switch (cv_newgametype.value)
			{
				case GT_COOP:
					value = TOL_RACE; // SRB2kart
					break;
				case GT_COMPETITION:
					value = TOL_COMPETITION;
					break;
				case GT_RACE:
					value = TOL_RACE;
					break;
				case GT_MATCH:
				case GT_TEAMMATCH:
					value = TOL_MATCH;
					break;
				case GT_TAG:
				case GT_HIDEANDSEEK:
					value = TOL_TAG;
					break;
				case GT_CTF:
					value = TOL_CTF;
					break;
			}

			CV_SetValue(&cv_nextmap, M_FindFirstMap(value));
		}
	}
}

static void Splitplayers_OnChange(void)
{
	if (cv_splitplayers.value < setupm_pselect)
		setupm_pselect = 1;
}

void ShowLocalskinMenu_Onchange(void)
{
	OP_MainMenu[localskin].status = (!cv_showlocalskinmenus.value) ? (IT_DISABLED) : (IT_CALL|IT_STRING);
}

// current menudef
menu_t *currentMenu = &MainDef;

// =========================================================================
// BASIC MENU HANDLING
// =========================================================================

static void M_ChangeCvar(INT32 choice)
{
	consvar_t *cv = (consvar_t *)currentMenu->menuitems[itemOn].itemaction;

	if (choice == -1)
	{
		if (cv == &cv_playercolor)
		{
			INT32 skinno = R_SkinAvailable(cv_chooseskin.string);
			if (skinno != -1)
				CV_SetValue(cv,skins[skinno].prefcolor);
			return;
		}
		CV_Set(cv,cv->defaultvalue);
		return;
	}

	choice = (choice<<1) - 1;

	if (cv->flags & CV_FLOAT)
	{
		char s[20];
		float increment;

		increment = FIXED_TO_FLOAT(cv->value)+(choice)*((currentMenu->menuitems[itemOn].status & IT_CV_BIGFLOAT) ? 0.5f : (1.0f/16.0f));
		sprintf(s,"%ld%s",(long)increment,M_Ftrim(increment));
		CV_Set(cv, s);
	}
	else
	{
		if (((currentMenu->menuitems[itemOn].status & IT_CVARTYPE) == IT_CV_SLIDER)
			||((currentMenu->menuitems[itemOn].status & IT_CVARTYPE) == IT_CV_INVISSLIDER)
			||((currentMenu->menuitems[itemOn].status & IT_CVARTYPE) == IT_CV_NOMOD))
		{
			CV_SetValue(cv,cv->value+choice);
		}
		else
		{
			if (cv == &cv_nettimeout || cv == &cv_jointimeout)
				choice *= (TICRATE/7);
			else if (cv == &cv_maxsend)
				choice *= 512;

			CV_AddValue(cv,choice);
		}
	}
}

static boolean M_ChangeStringCvar(INT32 choice)
{
	consvar_t *cv = (consvar_t *)currentMenu->menuitems[itemOn].itemaction;

	if (M_TextInputHandle(&menuinput, choice))
	{
		S_StartSound(NULL, sfx_menu1); // Tails
		CV_Set(cv, menuinput.buffer);

		return true;
	}

	return false;
}

// resets all cvars on a menu - assumes that all that have itemactions are cvars
static void M_ResetCvars(void)
{
	INT32 i;
	consvar_t *cv;
	for (i = 0; i < currentMenu->numitems; i++)
	{
		if (!(currentMenu->menuitems[i].status & IT_CVAR) || !(cv = (consvar_t *)currentMenu->menuitems[i].itemaction))
			continue;
		CV_SetValue(cv, atoi(cv->defaultvalue));
	}
}

// This is not a particular elegant solution, but it seems to make do for our purposes
static void M_SetTextInput(void)
{
	// dont reset our native input state when console is open
	if (CON_Ready())
		return;

	I_SetTextInput(false);

	if (!menuactive || !currentMenu)
	{
		return;
	}

	// check if the current entry requieres some kind of keyboard input from us
	// except for MM_EVENTHANDLER since thats to be used for the control setup
	const UINT16 status = currentMenu->menuitems[itemOn].status;
	if ((itemOn < currentMenu->numitems)
		&& (((status & IT_CVARTYPE) == IT_CV_STRING) || ((status & IT_TYPE) == IT_KEYHANDLER)
		|| (status == IT_MSGHANDLER && currentMenu->menuitems[itemOn].alphaKey != MM_EVENTHANDLER)))
	{
		I_SetTextInput(true);
	}
}

// If current menu item is IT_CV_STRING, setup menuinput
static void M_CheckStringItem(void)
{
	if (itemOn < currentMenu->numitems && (currentMenu->menuitems[itemOn].status & IT_CVARTYPE) == IT_CV_STRING)
	{
		consvar_t *cv = (consvar_t *)currentMenu->menuitems[itemOn].itemaction;

		// Just in case
		memset(menu_text_input_buf, 0, sizeof menu_text_input_buf);

		// special case: name input, cap it to prevent writing outside the textbox
		// kinda ugly but itll work
		if (cv == setupm_cvname)
			M_TextInputInit(&menuinput, menu_text_input_buf, MAXPLAYERNAME +1);
		else
			M_TextInputInit(&menuinput, menu_text_input_buf, sizeof menu_text_input_buf);

		M_TextInputSetString(&menuinput, cv->string);
	}
}

static void M_NextOpt(void)
{
	INT16 oldItemOn = itemOn; // prevent infinite loop

	do
	{
		if (itemOn + 1 > currentMenu->numitems - 1)
			itemOn = 0;
		else
			itemOn++;
	} while (oldItemOn != itemOn && (currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_SPACE);

	M_CheckStringItem();
	M_SetTextInput();
}

static void M_PrevOpt(void)
{
	INT16 oldItemOn = itemOn; // prevent infinite loop

	do
	{
		if (!itemOn)
			itemOn = currentMenu->numitems - 1;
		else
			itemOn--;
	} while (oldItemOn != itemOn && (currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_SPACE);

	M_CheckStringItem();
	M_SetTextInput();
}

// lock out further input in a tic when important buttons are pressed
// (in other words -- stop bullshit happening by mashing buttons in fades)
static boolean noFurtherInput = false;

static void Command_Manual_f(void)
{
	if (modeattacking)
		return;
	M_StartControlPanel();
	M_Manual(INT32_MAX);
	itemOn = 0;
}

//
// M_Responder
//
boolean M_Responder(event_t *ev)
{
	INT32 ch = -1;
	static tic_t joywaitx = 0, joywaity = 0, joywaitaccel = 0, mousewait = 0;
	static INT32 pjoyx = 0, pjoyy = 0, pjoyaccel = 0;
	static INT32 pmousex = 0, pmousey = 0;
	static INT32 lastx = 0, lasty = 0;
	void (*routine)(INT32 choice); // for some casting problem

	if (dedicated || (demo.playback && demo.title)
	|| gamestate == GS_INTRO || gamestate == GS_CUTSCENE || gamestate == GS_GAMEEND
	|| gamestate == GS_CREDITS || gamestate == GS_EVALUATION)
		return false;

	if (CON_Ready() && gamestate != GS_WAITINGPLAYERS)
		return false;

	if (noFurtherInput)
	{
		// Ignore input after enter/escape/other buttons
		// (but still allow shift keyup so caps doesn't get stuck)
		return false;
	}
	else if (ev->type == ev_keydown)
	{
		ch = ev->data1;

		switch (ch)
		{
			case KEY_MOUSE1:
				//case KEY_JOY1:
				//case KEY_JOY1 + 2:
				ch = KEY_ENTER;
				break;
			/*case KEY_JOY1 + 3: // Brake can function as 'n' for message boxes now.
				ch = 'n';
				break;*/
			case KEY_MOUSE1 + 1:
				//case KEY_JOY1 + 1:
				ch = KEY_BACKSPACE;
				break;
			case KEY_HAT1:
				ch = KEY_UPARROW;
				break;
			case KEY_HAT1 + 1:
				ch = KEY_DOWNARROW;
				break;
			case KEY_HAT1 + 2:
				ch = KEY_LEFTARROW;
				break;
			case KEY_HAT1 + 3:
				ch = KEY_RIGHTARROW;
				break;
		}
	}
	else if (menuactive)
	{
		tic_t thistime = I_GetTime();

		if (ev->type == ev_joystick)
		{
			const INT32 jxdeadzone = ((JOYAXISRANGE-1) * max(cv_xdeadzone[0].value, FRACUNIT/2)) >> FRACBITS;
			const INT32 jydeadzone = ((JOYAXISRANGE-1) * max(cv_ydeadzone[0].value, FRACUNIT/2)) >> FRACBITS;
			INT32 accelaxis = abs(cv_moveaxis[0].value);
			if (ev->data1 == 0)
			{
				if (ev->data3 != INT32_MAX)
				{
					if (Joystick[0].bGamepadStyle || abs(ev->data3) > jydeadzone)
					{
						if (joywaity < thistime
							&& (pjoyy == 0 || (ev->data3 < 0) != (pjoyy < 0))) // no previous direction OR change direction
						{
							ch = (ev->data3 < 0) ? KEY_UPARROW : KEY_DOWNARROW;
							joywaity = thistime + NEWTICRATE/7;
						}
						pjoyy = ev->data3;
					}
					else
						pjoyy = 0;
				}

				if (ev->data2 != INT32_MAX && joywaitx < thistime)
				{
					if (Joystick[0].bGamepadStyle || abs(ev->data2) > jxdeadzone)
					{
						if (joywaitx < thistime
							&& (pjoyx == 0 || (ev->data2 < 0) != (pjoyx < 0))) // no previous direction OR change direction
						{
							ch = (ev->data2 < 0) ? KEY_LEFTARROW : KEY_RIGHTARROW;
							joywaitx = thistime + NEWTICRATE/7;
						}
						pjoyx = ev->data2;
					}
					else
						pjoyx = 0;
				}
			}
			else if (!(accelaxis > JOYAXISSET*2 || accelaxis == 0))
			{
				// The following borrows heavily from Joy1Axis.
				const boolean xmode = (accelaxis%2);
				INT32 retaxis = 0;
				if (!xmode)
					accelaxis--;
				accelaxis /= 2;
				if (ev->data1 == accelaxis)
				{
					const INT32 jacceldeadzone = xmode ? jxdeadzone : jydeadzone;
					retaxis = xmode ? ev->data2 : ev->data3;
					if (retaxis != INT32_MAX)
					{
						if (cv_moveaxis[0].value < 0)
							retaxis = -retaxis;

						if (Joystick[0].bGamepadStyle || retaxis > jacceldeadzone)
						{
							if (joywaitaccel < thistime && retaxis > pjoyaccel) // only on upwards event
							{
								ch = KEY_ENTER;
								joywaitaccel = thistime + NEWTICRATE/3;
							}
							pjoyaccel = retaxis;
						}
						else
							pjoyaccel = 0;
					}
				}
			}
		}
		else if (ev->type == ev_mouse && mousewait < I_GetTime())
		{
			pmousey += ev->data3;
			if (pmousey < lasty-30)
			{
				ch = KEY_DOWNARROW;
				mousewait = I_GetTime() + NEWTICRATE/7;
				pmousey = lasty -= 30;
			}
			else if (pmousey > lasty + 30)
			{
				ch = KEY_UPARROW;
				mousewait = I_GetTime() + NEWTICRATE/7;
				pmousey = lasty += 30;
			}

			pmousex += ev->data2;
			if (pmousex < lastx - 30)
			{
				ch = KEY_LEFTARROW;
				mousewait = I_GetTime() + NEWTICRATE/7;
				pmousex = lastx -= 30;
			}
			else if (pmousex > lastx+30)
			{
				ch = KEY_RIGHTARROW;
				mousewait = I_GetTime() + NEWTICRATE/7;
				pmousex = lastx += 30;
			}
		}
	}

	if (ch == -1)
		return false;
	else if (ch == gamecontrol[0][gc_systemmenu][0] || ch == gamecontrol[0][gc_systemmenu][1]) // allow remappable ESC key
		ch = KEY_ESCAPE;
	else if ((ch == gamecontrol[0][gc_accelerate][0] || ch == gamecontrol[0][gc_accelerate][1])  && ch >= KEY_MOUSE1)
		ch = KEY_ENTER;

	// F-Keys
	if (!menuactive)
	{
		noFurtherInput = true;

		switch (ch)
		{
			case KEY_F1: // Help key
				Command_Manual_f();
				return true;

			case KEY_F2: // Empty
				return true;

			case KEY_F3: // Toggle HUD
				CV_SetValue(&cv_showhud, !cv_showhud.value);
				return true;

			case KEY_F4: // Sound Volume
				if (modeattacking)
					return true;
				M_StartControlPanel();
				M_Options(0);
				currentMenu = &OP_SoundOptionsDef;
				itemOn = 0;
				return true;

			case KEY_F5: // Video Mode
				if (modeattacking)
					return true;
				M_StartControlPanel();
				M_Options(0);
				M_VideoModeMenu(0);
				return true;

			case KEY_F6: // Empty
				return true;

			case KEY_F7: // Options
				if (modeattacking)
					return true;
				M_StartControlPanel();
				M_Options(0);
				M_SetupNextMenu(&OP_MainDef);
				return true;

			// Screenshots on F8 now handled elsewhere
			// Same with Moviemode on F9

			case KEY_F10: // Quit SRB2
				M_QuitSRB2(0);
				return true;

			case KEY_F11: // Fullscreen
				CV_AddValue(&cv_fullscreen, 1);
				return true;

			// Spymode on F12 handled in game logic

			case KEY_ESCAPE: // Pop up menu
				if (chat_on)
				{
					HU_clearChatChars();
					chat_on = false;
				}
				else
					M_StartControlPanel();
				return true;
		}
		noFurtherInput = false; // turns out we didn't care
		return false;
	}

	if ((ch == gamecontrol[0][gc_brake][0] || ch == gamecontrol[0][gc_brake][1]) && ch >= KEY_MOUSE1) // do this here, otherwise brake opens the menu mid-game
		ch = KEY_ESCAPE;

	if (currentMenu == &MISC_ChangeLevelDef ||
		currentMenu == &MP_OfflineServerDef ||
		currentMenu == &MP_ServerDef)
		{
			if (ch == gamecontrol[0][gc_fire][0]
			 || ch == gamecontrol[0][gc_fire][1])
			{
				if (M_SecretUnlocked(SECRET_ENCORE))
					COM_ImmedExecute("add kartencore 1");
				return true;
			}
		}

	routine = currentMenu->menuitems[itemOn].itemaction;

	// Handle menuitems which need a specific key handling
	if (routine && (currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_KEYHANDLER)
	{
		ch = M_ShiftChar(ch);
		routine(ch);
		return true;
	}

	if (currentMenu->menuitems[itemOn].status == IT_MSGHANDLER)
	{
		if (currentMenu->menuitems[itemOn].alphaKey != MM_EVENTHANDLER)
		{
			if (ch == ' ' || ch == 'n' || ch == 'y' || ch == KEY_ESCAPE || ch == KEY_ENTER)
			{
				if (routine)
					routine(ch);
				M_StopMessage(0);
				noFurtherInput = true;
				return true;
			}
			return true;
		}
		else
		{
			// dirty hack: for customising controls, I want only buttons/keys, not moves
			if (ev->type == ev_mouse || ev->type == ev_joystick
				|| ev->type == ev_joystick2 || ev->type == ev_joystick3 || ev->type == ev_joystick4)
				return true;
			if (routine)
			{
				void (*otherroutine)(event_t *sev) = currentMenu->menuitems[itemOn].itemaction;
				otherroutine(ev); //Alam: what a hack
			}
			return true;
		}
	}

	// BP: one of the more big hack i have never made
	if (routine && (currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_CVAR)
	{
		if ((currentMenu->menuitems[itemOn].status & IT_CVARTYPE) == IT_CV_STRING)
		{
			if (M_ChangeStringCvar(ch))
				return true;
			else
				routine = NULL;
		}
		else
			routine = M_ChangeCvar;
	}

	if (currentMenu == &PlaybackMenuDef && !con_destlines)
	{
		playback_last_menu_interaction_leveltime = leveltime;
		// Flip left/right with up/down for the playback menu, since it's a horizontal icon row.

		switch (ch)
		{
			case KEY_LEFTARROW:
				ch = KEY_UPARROW;
				break;
			case KEY_UPARROW:
				ch = KEY_RIGHTARROW;
				break;
			case KEY_RIGHTARROW:
				ch = KEY_DOWNARROW;
				break;
			case KEY_DOWNARROW:
				ch = KEY_LEFTARROW;
				break;

			// arbitrary keyboard shortcuts because fuck you

			case '\'':	// toggle freecam
				M_PlaybackToggleFreecam(0);
				break;

			case ']':	// ffw / advance frame (depends on if paused or not)
				if (paused)
					M_PlaybackAdvance(0);
				else
					M_PlaybackFastForward(0);
				break;

			case '[':	// rewind /backupframe, uses the same function
				M_PlaybackRewind(0);
				break;

			case '\\':	// pause
				M_PlaybackPause(0);
				break;

			// viewpoints, an annoyance (tm)
			case '-':	// viewpoint minus
				M_PlaybackSetViews(-1);	// yeah lol.
				break;

			case '=':	// viewpoint plus
				M_PlaybackSetViews(1);	// yeah lol.
				break;

			// switch viewpoints:
			case '1':	// viewpoint for p1 (also f12)
				// maximum laziness:
				if (!camera[0].freecam)
					G_AdjustView(1, 1, true);
				break;
			case '2':	// viewpoint for p2
				if (!camera[1].freecam)
					G_AdjustView(2, 1, true);
				break;
			case '3':	// viewpoint for p3
				if (!camera[2].freecam)
					G_AdjustView(3, 1, true);
				break;
			case '4':	// viewpoint for p4
				if (!camera[3].freecam)
					G_AdjustView(4, 1, true);
				break;

			default:
				break;
		}
	}

	// Keys usable within menu
	switch (ch)
	{
		case KEY_DOWNARROW:
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1);
			coolalphatimer = 9;
			return true;

		case KEY_UPARROW:
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1);
			coolalphatimer = 9;
			return true;

		case KEY_LEFTARROW:
			if (routine && ((currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_ARROWS
				|| (currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_CVAR))
			{
				if (currentMenu != &OP_SoundOptionsDef || itemOn > 3)
					S_StartSound(NULL, sfx_menu1);
				routine(0);
			}
			return true;

		case KEY_RIGHTARROW:
			if (routine && ((currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_ARROWS
				|| (currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_CVAR))
			{
				if (currentMenu != &OP_SoundOptionsDef || itemOn > 3)
					S_StartSound(NULL, sfx_menu1);
				routine(1);
			}
			return true;

		case KEY_ENTER:
			noFurtherInput = true;
			currentMenu->lastOn = itemOn;

			if (currentMenu == &PlaybackMenuDef)
			{
				boolean held = (boolean)playback_enterheld;
				if (held)
					return true;
				playback_enterheld = 3;
			}

			if (routine)
			{
				S_StartSound(NULL, sfx_menu1);
				switch (currentMenu->menuitems[itemOn].status & IT_TYPE)
				{
					case IT_CVAR:
					case IT_ARROWS:
						routine(1); // right arrow
						break;
					case IT_CALL:
						routine(itemOn);
						break;
					case IT_SUBMENU:
						currentMenu->lastOn = itemOn;
						M_SetupNextMenu((menu_t *)currentMenu->menuitems[itemOn].itemaction);
						break;
				}
			}
			return true;

		case KEY_ESCAPE:
			noFurtherInput = true;
			currentMenu->lastOn = itemOn;
			if (currentMenu->prevMenu)
			{
				//If we entered the game search menu, but didn't enter a game,
				//make sure the game doesn't still think we're in a netgame.
				if (!Playing() && netgame && multiplayer)
				{
					netgame = false;
					multiplayer = false;
				}

				if (currentMenu == &SP_TimeAttackDef) //|| currentMenu == &SP_NightsAttackDef
				{
					// D_StartTitle does its own wipe, since GS_TIMEATTACK is now a complete gamestate.
					menuactive = false;
					D_StartTitle();
				}
				else
					M_SetupNextMenu(currentMenu->prevMenu);
			}
			else
				M_ClearMenus(true);

			return true;

		case KEY_BACKSPACE:
			if ((currentMenu->menuitems[itemOn].status) == IT_CONTROL)
			{
				// detach any keys associated with the game control
				G_ClearControlKeys(setupcontrols, currentMenu->menuitems[itemOn].alphaKey);
				S_StartSound(NULL, sfx_shldls);
				return true;
			}

			if (routine && ((currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_ARROWS
				|| (currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_CVAR))
			{
				consvar_t *cv = (consvar_t *)currentMenu->menuitems[itemOn].itemaction;

				if (cv == &cv_chooseskin
					|| cv == &cv_dummystaff
					|| cv == &cv_nextmap
					|| cv == &cv_newgametype)
					return true;

				if (currentMenu != &OP_SoundOptionsDef || itemOn > 3)
					S_StartSound(NULL, sfx_menu1);
				routine(-1);
				return true;
			}

			return false;
			break;

		default:
			CON_Responder(ev);
			break;
	}

	return true;
}

// special responder for demos
boolean M_DemoResponder(event_t *ev)
{
	INT32 ch = -1;	// cur event data
	boolean eatinput = false;	// :omnom:

	//should be accounted for beforehand but just to be safe...
	if (!demo.playback || demo.title)
		return false;

	if (noFurtherInput)
	{
		// Ignore input after enter/escape/other buttons
		// (but still allow shift keyup so caps doesn't get stuck)
		return false;
	}
	else if (ev->type == ev_keydown && !con_destlines)	// not while the console is on please
	{
		ch = ev->data1;
		// since this is ONLY for demos, there isn't MUCH for us to do.
		// mirrored from m_responder

		switch (ch)
		{
			// arbitrary keyboard shortcuts because fuck you

			case '\'':	// toggle freecam
				M_PlaybackToggleFreecam(0);
				eatinput = true;
				break;

			case ']':	// ffw / advance frame (depends on if paused or not)
				if (paused)
					M_PlaybackAdvance(0);
				else
					M_PlaybackFastForward(0);
				eatinput = true;
				break;

			case '[':	// rewind /backupframe, uses the same function
				M_PlaybackRewind(0);
				break;

			case '\\':	// pause
				M_PlaybackPause(0);
				eatinput = true;
				break;

			// viewpoints, an annoyance (tm)
			case '-':	// viewpoint minus
				M_PlaybackSetViews(-1);	// yeah lol.
				eatinput = true;
				break;

			case '=':	// viewpoint plus
				M_PlaybackSetViews(1);	// yeah lol.
				eatinput = true;
				break;

			// switch viewpoints:
			case '1':	// viewpoint for p1 (also f12)
				// maximum laziness:
				if (!camera[0].freecam)
					G_AdjustView(1, 1, true);
				break;
			case '2':	// viewpoint for p2
				if (!camera[1].freecam)
					G_AdjustView(2, 1, true);
				break;
			case '3':	// viewpoint for p3
				if (!camera[2].freecam)
					G_AdjustView(3, 1, true);
				break;
			case '4':	// viewpoint for p4
				if (!camera[3].freecam)
					G_AdjustView(4, 1, true);
				break;

			default: break;
		}
	}

	return eatinput;
}

// mhhm yes
static boolean ShouldDrawMenuBG(void)
{
	if (WipeInAction) // dont ever draw stuff in wipes
		return false;

	if (currentMenu == &PlaybackMenuDef) // Replay playback has its own background
		return false;

	if (forceshowhud)
		return false;

	// camera options stuff, only do when in level
	if (gamestate == GS_LEVEL &&
	   (currentMenu == &OP_CamOptionsDef || currentMenu == &OP_Player1CamOptionsDef
	 || currentMenu == &OP_Player2CamOptionsDef || currentMenu == &OP_Player3CamOptionsDef
	 || currentMenu == &OP_Player4CamOptionsDef))
		return false;

	return true;
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer(void)
{
	if (currentMenu == &MessageDef)
		menuactive = true;

	forceshowhud = (gamestate == GS_LEVEL && menuactive && (currentMenu == &OP_SaturnHudDef || currentMenu == &OP_HudOffsetDef || currentMenu == &OP_NametagDef || currentMenu == &OP_DriftGaugeDef)); // holy fuick

	if (menuactive)
	{
		// now that's more readable with a faded background (yeah like Quake...)
		if (ShouldDrawMenuBG())
		{
			V_DrawFadeScreen(0xFF00, 16);
		}

		if (currentMenu->drawroutine)
		{
			M_GetGametypeColor();
			currentMenu->drawroutine(); // call current menu Draw routine
		}

		// Draw version down in corner
		// ... but only in the MAIN MENU.  I'm a picky bastard.
		if (currentMenu == &MainDef)
		{
			if (customversionstring[0] != '\0')
			{
				V_DrawThinString(vid.dup, vid.height - 20*vid.dup, V_NOSCALESTART|V_TRANSLUCENT, "Mod version:");
				V_DrawThinString(vid.dup, vid.height - 10*vid.dup, V_NOSCALESTART|V_TRANSLUCENT|V_ALLOWLOWERCASE, customversionstring);
			}
			else
			{
#ifdef DEVELOP // Development -- show revision / branch info
				V_DrawThinString(vid.dup, vid.height - 20*vid.dup, V_NOSCALESTART|V_TRANSLUCENT|V_ALLOWLOWERCASE, compbranch);
				V_DrawThinString(vid.dup, vid.height - 10*vid.dup, V_NOSCALESTART|V_TRANSLUCENT|V_ALLOWLOWERCASE, comprevision);
#else // Regular build
#ifdef SATURN_TESTING // ok not regular build lmao, we dont need to show this stuff in Saturn release builds
				V_DrawThinString(vid.dup, vid.height - 20*vid.dup, V_NOSCALESTART|V_TRANSLUCENT|V_ALLOWLOWERCASE, comprevision);
#endif
				V_DrawThinString(vid.dup, vid.height - 10*vid.dup, V_NOSCALESTART|V_TRANSLUCENT|V_ALLOWLOWERCASE, va("%s", VERSIONSTRING));
#endif
#ifdef HWRENDER
				if (rendermode == render_opengl)
					V_DrawThinString(0, 0, V_GREENMAP|V_SNAPTOTOP|V_SNAPTOLEFT|V_TRANSLUCENT|V_ALLOWLOWERCASE, ("Opengl"));
				else
#endif
				if (rendermode == render_soft)
					V_DrawThinString(0, 0, V_REDMAP|V_SNAPTOTOP|V_SNAPTOLEFT|V_TRANSLUCENT|V_ALLOWLOWERCASE, ("Software"));
			}
		}
	}

	// focus lost notification goes on top of everything, even the former everything
	if (window_notinfocus && cv_showfocuslost.value)
	{
		M_DrawTextBox((BASEVIDWIDTH/2) - (60), (BASEVIDHEIGHT/2) - (16), 13, 2);
		if (gamestate == GS_LEVEL && (P_AutoPause() || paused))
			V_DrawCenteredString(BASEVIDWIDTH/2, (BASEVIDHEIGHT/2) - (4), highlightflags, "Game Paused");
		else
			V_DrawCenteredString(BASEVIDWIDTH/2, (BASEVIDHEIGHT/2) - (4), highlightflags, "Focus Lost");
	}

	interpTimerHackAllow = false;
}

//
// M_StartControlPanel
//
void M_StartControlPanel(void)
{
	// intro might call this repeatedly
	if (menuactive)
	{
		CON_ToggleOff(); // move away console
		return;
	}

	menuactive = true;
	G_ResetControls();

	if (demo.playback)
	{
		currentMenu = &PlaybackMenuDef;
		playback_last_menu_interaction_leveltime = leveltime;
	}
	else if (!Playing())
	{
		currentMenu = &MainDef;
		itemOn = singleplr;
	}
	else if (modeattacking)
	{
		currentMenu = &MAPauseDef;
		itemOn = mapause_continue;
	}
	else if (!(netgame || multiplayer)) // Single Player
	{
		if (gamestate != GS_LEVEL) // intermission, so gray out stuff.
		{
			SPauseMenu[spause_retry].status = IT_GRAYEDOUT;
		}
		else
		{
			SPauseMenu[spause_retry].status = (IT_STRING | IT_CALL);
		}

		currentMenu = &SPauseDef;
		itemOn = spause_continue;
	}
	else // multiplayer
	{
		MPauseMenu[mpause_switchmap].status = IT_DISABLED;
		MPauseMenu[mpause_addons].status = IT_DISABLED;
		MPauseMenu[mpause_scramble].status = IT_DISABLED;
		MPauseMenu[mpause_psetupsplit].status = IT_DISABLED;
		MPauseMenu[mpause_psetupsplit2].status = IT_DISABLED;
		MPauseMenu[mpause_psetupsplit3].status = IT_DISABLED;
		MPauseMenu[mpause_psetupsplit4].status = IT_DISABLED;
		MPauseMenu[mpause_spectate].status = IT_DISABLED;
		MPauseMenu[mpause_entergame].status = IT_DISABLED;
		MPauseMenu[mpause_canceljoin].status = IT_DISABLED;
		MPauseMenu[mpause_switchteam].status = IT_DISABLED;
		MPauseMenu[mpause_switchspectate].status = IT_DISABLED;
		MPauseMenu[mpause_psetup].status = IT_DISABLED;
		MISC_ChangeTeamMenu[0].status = IT_DISABLED;
		MISC_ChangeSpectateMenu[0].status = IT_DISABLED;

		MPauseMenu[mpause_addlocalskins].status = IT_STRING | IT_CALL;
		MPauseMenu[mpause_localskin].status = IT_STRING | IT_CALL;

		// Reset these in case splitscreen messes things up
		MPauseMenu[mpause_addons].alphaKey = 8;

		if (IsPlayerAdmin(consoleplayer))
			MPauseMenu[mpause_addlocalskins].alphaKey = 16;
		else
			MPauseMenu[mpause_addlocalskins].alphaKey = 24;

		MPauseMenu[mpause_scramble].alphaKey = 8;
		MPauseMenu[mpause_switchmap].alphaKey = 24;

		MPauseMenu[mpause_switchteam].alphaKey = 48;
		MPauseMenu[mpause_switchspectate].alphaKey = 48;
		MPauseMenu[mpause_localskin].alphaKey = 64;
		MPauseMenu[mpause_options].alphaKey = 72;
		MPauseMenu[mpause_title].alphaKey = 88;
		MPauseMenu[mpause_quit].alphaKey = 96;

		Dummymenuplayer_OnChange();

		if (server || IsPlayerAdmin(consoleplayer))
		{
			MPauseMenu[mpause_switchmap].status = IT_STRING | IT_CALL;
			MPauseMenu[mpause_addons].status = IT_STRING | IT_CALL;

			if (G_GametypeHasTeams())
				MPauseMenu[mpause_scramble].status = IT_STRING | IT_SUBMENU;
		}

		if (server || (!cv_showlocalskinmenus.value))
		{
			MPauseMenu[mpause_addlocalskins].status = IT_DISABLED;
			MPauseMenu[mpause_localskin].status = IT_DISABLED;

			MPauseMenu[mpause_options].alphaKey = 64;
			MPauseMenu[mpause_title].alphaKey = 80;
			MPauseMenu[mpause_quit].alphaKey = 88;
		}


		if (splitscreen)
		{
			MPauseMenu[mpause_psetupsplit].status = MPauseMenu[mpause_psetupsplit2].status = IT_STRING | IT_CALL;
			MISC_ChangeTeamMenu[0].status = MISC_ChangeSpectateMenu[0].status = IT_STRING|IT_CVAR;

			if (netgame)
			{
				if (G_GametypeHasTeams())
				{
					MPauseMenu[mpause_switchteam].status = IT_STRING | IT_SUBMENU;
					MPauseMenu[mpause_switchteam].alphaKey += ((splitscreen+1) * 8);
					MPauseMenu[mpause_localskin].alphaKey += 8;
					MPauseMenu[mpause_options].alphaKey += 8;
					MPauseMenu[mpause_title].alphaKey += 8;
					MPauseMenu[mpause_quit].alphaKey += 8;
				}
				else if (G_GametypeHasSpectators())
				{
					MPauseMenu[mpause_switchspectate].status = IT_STRING | IT_SUBMENU;
					MPauseMenu[mpause_switchspectate].alphaKey += ((splitscreen+1) * 8);
					MPauseMenu[mpause_localskin].alphaKey += 8;
					MPauseMenu[mpause_options].alphaKey += 8;
					MPauseMenu[mpause_title].alphaKey += 8;
					MPauseMenu[mpause_quit].alphaKey += 8;
				}
			}

			if (splitscreen > 1)
			{
				MPauseMenu[mpause_psetupsplit3].status = IT_STRING | IT_CALL;

				MPauseMenu[mpause_localskin].alphaKey += 8;
				MPauseMenu[mpause_options].alphaKey += 8;
				MPauseMenu[mpause_title].alphaKey += 8;
				MPauseMenu[mpause_quit].alphaKey += 8;

				if (splitscreen > 2)
				{
					MPauseMenu[mpause_psetupsplit4].status = IT_STRING | IT_CALL;
					MPauseMenu[mpause_localskin].alphaKey += 8;
					MPauseMenu[mpause_options].alphaKey += 8;
					MPauseMenu[mpause_title].alphaKey += 8;
					MPauseMenu[mpause_quit].alphaKey += 8;
				}
			}
		}
		else
		{
			MPauseMenu[mpause_psetup].status = IT_STRING | IT_CALL;

			if (G_GametypeHasTeams())
				MPauseMenu[mpause_switchteam].status = IT_STRING | IT_SUBMENU;
			else if (G_GametypeHasSpectators())
			{
				if (!players[consoleplayer].spectator)
					MPauseMenu[mpause_spectate].status = IT_STRING | IT_CALL;
				else if (players[consoleplayer].pflags & PF_WANTSTOJOIN)
					MPauseMenu[mpause_canceljoin].status = IT_STRING | IT_CALL;
				else
					MPauseMenu[mpause_entergame].status = IT_STRING | IT_CALL;
			}
			else // in this odd case, we still want something to be on the menu even if it's useless
				MPauseMenu[mpause_spectate].status = IT_GRAYEDOUT;
		}

#ifdef HAVE_DISCORDRPC
		{
			UINT8 i;

			for (i = 0; i < mpause_discordrequests; i++)
				MPauseMenu[i].alphaKey -= 8;

			MPauseMenu[mpause_discordrequests].alphaKey = MPauseMenu[i].alphaKey;

			M_RefreshPauseMenu();
		}
#endif
		currentMenu = &MPauseDef;
		itemOn = mpause_continue;
	}

	CON_ToggleOff(); // move away console
}

void M_EndModeAttackRun(void)
{
	M_ModeAttackEndGame(0);
}

//
// M_ClearMenus
//
void M_ClearMenus(boolean callexitmenufunc)
{
	M_SetTextInput();

	if (!menuactive)
		return;

	if (currentMenu->quitroutine && callexitmenufunc && !currentMenu->quitroutine())
		return; // we can't quit this menu (also used to set parameter from the menu)

	// Save the config file. I'm sick of crashing the game later and losing all my changes!
	COM_BufAddText(va("saveconfig \"%s\" -silent\n", configfile));

	if (currentMenu == &MessageDef) // Oh sod off!
		currentMenu = &MainDef; // Not like it matters
	menuactive = false;
}

//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
	INT16 i;

	if (currentMenu->quitroutine)
	{
		// If you're going from a menu to itself, why are you running the quitroutine? You're not quitting it! -SH
		if (currentMenu != menudef && !currentMenu->quitroutine())
			return; // we can't quit this menu (also used to set parameter from the menu)
	}
	currentMenu = menudef;
	itemOn = currentMenu->lastOn;

	// in case of...
	if (itemOn >= currentMenu->numitems)
		itemOn = currentMenu->numitems - 1;

	// the curent item can be disabled,
	// this code go up until an enabled item found
	if ((currentMenu->menuitems[itemOn].status & IT_TYPE) == IT_SPACE)
	{
		for (i = 0; i < currentMenu->numitems; i++)
		{
			if ((currentMenu->menuitems[i].status & IT_TYPE) != IT_SPACE)
			{
				itemOn = i;
				break;
			}
		}
	}

	M_CheckStringItem();
	M_SetTextInput();
}

//
// M_Ticker
//
void M_Ticker(void)
{
	// reset input trigger
	noFurtherInput = false;

	if (dedicated)
		return;

	if (currentMenu->tickroutine)
		currentMenu->tickroutine();

	if (menuactive)
		I_HandleControllerHatRepeat();

	if (--skullAnimCounter <= 0)
		skullAnimCounter = 8;

	if (currentMenu == &PlaybackMenuDef)
	{
		if (playback_enterheld > 0)
			playback_enterheld--;
	}
	else
		playback_enterheld = 0;

	if (demo.inreplayhut)
		M_HutCheckReplays(cv_replaysearchrate.value);

	interpTimerHackAllow = true;

	//added : 30-01-98 : test mode for five seconds
	if (vidm_testingmode > 0)
	{
		// restore the previous video mode
		if (--vidm_testingmode == 0)
			setmodeneeded = vidm_previousmode + 1;
	}

#if defined (MASTERSERVER) && defined (HAVE_THREADS)
	I_lock_mutex(&ms_ServerList_mutex);
	{
		if (ms_ServerList)
		{
			CL_QueryServerList(ms_ServerList);
			free(ms_ServerList);
			ms_ServerList = NULL;
		}
	}
	I_unlock_mutex(ms_ServerList_mutex);
#endif
}

//
// M_Init
//
void M_Init(void)
{
	UINT8 i;

	COM_AddCommand("manual", Command_Manual_f);

	CV_RegisterVar(&cv_nextmap);
	CV_RegisterVar(&cv_newgametype);
	CV_RegisterVar(&cv_chooseskin);
	CV_RegisterVar(&cv_autorecord);

	if (dedicated)
		return;

	// Menu hacks
	CV_RegisterVar(&cv_dummymenuplayer);
	CV_RegisterVar(&cv_dummyteam);
	CV_RegisterVar(&cv_dummyspectate);
	CV_RegisterVar(&cv_dummyscramble);
	CV_RegisterVar(&cv_dummyrings);
	CV_RegisterVar(&cv_dummylives);
	CV_RegisterVar(&cv_dummycontinues);
	CV_RegisterVar(&cv_dummystaff);

	quitmsg[QUITMSG] = M_GetText("Eggman's tied explosives\nto your girlfriend, and\nwill activate them if\nyou press the 'Y' key!\nPress 'N' to save her!\n\n(Press 'Y' to quit)");
	quitmsg[QUITMSG1] = M_GetText("What would Tails say if\nhe saw you quitting the game?\n\n(Press 'Y' to quit)");
	quitmsg[QUITMSG2] = M_GetText("Hey!\nWhere do ya think you're goin'?\n\n(Press 'Y' to quit)");
	quitmsg[QUITMSG3] = M_GetText("Forget your studies!\nPlay some more!\n\n(Press 'Y' to quit)");
	quitmsg[QUITMSG4] = M_GetText("You're trying to say you\nlike Sonic R better than\nthis, aren't you?\n\n(Press 'Y' to quit)");
	quitmsg[QUITMSG5] = M_GetText("Don't leave yet -- there's a\nsuper emerald around that corner!\n\n(Press 'Y' to quit)");
	quitmsg[QUITMSG6] = M_GetText("You'd rather work than play?\n\n(Press 'Y' to quit)");
	quitmsg[QUITMSG7] = M_GetText("Go ahead and leave. See if I care...\n*sniffle*\n\n(Press 'Y' to quit)");

	quitmsg[QUIT2MSG] = M_GetText("If you leave now,\nEggman will take over the world!\n\n(Press 'Y' to quit)");
	quitmsg[QUIT2MSG1] = M_GetText("On your mark,\nget set,\nhit the 'N' key!\n\n(Press 'Y' to quit)");
	quitmsg[QUIT2MSG2] = M_GetText("Aw c'mon, just\na few more laps!\n\n(Press 'Y' to quit)");
	quitmsg[QUIT2MSG3] = M_GetText("Did you get all those Chaos Emeralds?\n\n(Press 'Y' to quit)");
	quitmsg[QUIT2MSG4] = M_GetText("If you leave, I'll use\nmy Jawz on you!\n\n(Press 'Y' to quit)");
	quitmsg[QUIT2MSG5] = M_GetText("Don't go!\nYou might find the hidden\nlevels!\n\n(Press 'Y' to quit)");
	quitmsg[QUIT2MSG6] = M_GetText("Hit the 'N' key, Sonic!\nThe 'N' key!\n\n(Press 'Y' to quit)");

	quitmsg[QUIT3MSG] = M_GetText("Are you really going to give up?\nWe certainly would never give you up.\n\n(Press 'Y' to quit)");
	quitmsg[QUIT3MSG1] = M_GetText("Come on, just ONE more netgame!\n\n(Press 'Y' to quit)");
	quitmsg[QUIT3MSG2] = M_GetText("Press 'N' to unlock\nthe Golden Kart!\n\n(Press 'Y' to quit)");
	quitmsg[QUIT3MSG3] = M_GetText("Couldn't handle\nthe banana meta?\n\n(Press 'Y' to quit)");
	quitmsg[QUIT3MSG4] = M_GetText("Every time you press 'Y', an\nSRB2Kart Developer cries...\n\n(Press 'Y' to quit)");
	quitmsg[QUIT3MSG5] = M_GetText("You'll be back to play soon, though...\n...right?\n\n(Press 'Y' to quit)");
	quitmsg[QUIT3MSG6] = M_GetText("Aww, is Eggman's Nightclub too\ndifficult for you?\n\n(Press 'Y' to quit)");

	// Setup PlayerMenu table
	for (i = 0; i < MAXSKINS; i++)
	{
		PlayerMenu[i].status = (i == 0 ? IT_CALL : IT_DISABLED);
		PlayerMenu[i].patch = PlayerMenu[i].text = NULL;
		PlayerMenu[i].itemaction = M_ChoosePlayer;
		PlayerMenu[i].alphaKey = 0;
	}

#ifdef HWRENDER
	// Permanently hide some options based on render mode
	if (rendermode == render_soft)
	{
		OP_VideoOptionsMenu[op_video_ogl].status = IT_DISABLED;

		OP_ExpOptionsMenu[op_exp_glscrtx].status = IT_DISABLED;
#ifdef USE_FBO_OGL
		OP_ExpOptionsMenu[op_exp_fbo].status = IT_DISABLED;
#endif
		OP_ExpOptionsMenu[op_exp_paldepth].status = IT_DISABLED;
	}
	else if (rendermode == render_opengl)
	{
#ifdef USE_FBO_OGL
		if (!supportFBO)
			OP_ExpOptionsMenu[op_exp_fbo].status = IT_GRAYEDOUT;
#endif
		if (!gl_shadersavailable)
		{
			OP_OpenGLOptionsMenu[op_gl_shader].status = IT_GRAYEDOUT;
			OP_OpenGLOptionsMenu[op_gl_lightdither].status = IT_GRAYEDOUT;
			OP_OpenGLOptionsMenu[op_gl_palrender].status = IT_GRAYEDOUT;

			OP_ExpOptionsMenu[op_exp_paldepth].status = IT_GRAYEDOUT;
		}
	}
#endif

	if (!xtra_speedo && !kartz_speedo && !achi_speedo && !dial_speedo) // why bother?
		OP_SaturnHudMenu[sh_speedometer].status = IT_GRAYEDOUT;

	if (!clr_hud) // uhguauhauguuhee
	{
		OP_SaturnHudMenu[sh_colorhud].status = IT_GRAYEDOUT;
		OP_SaturnHudMenu[sh_coloritem].status = IT_GRAYEDOUT;
		OP_SaturnHudMenu[sh_colorhud_customcolor].status = IT_GRAYEDOUT;
	}

	if (!nametaggfx)
		OP_NametagMenu[nt_ntchar].status = IT_GRAYEDOUT;

	if (!minidoticon && !minilighticon)
		OP_SaturnHudMenu[sh_minidot].status = IT_GRAYEDOUT;

	CV_RegisterVar(&cv_serversort);

	//todo put this somewhere better...
	CV_RegisterVar(&cv_allcaps);

	memset(menu_text_input_buf, 0, sizeof menu_text_input_buf);
	M_TextInputInit(&menuinput, menu_text_input_buf, sizeof menu_text_input_buf);
}

void M_InitCharacterTables(void)
{
	UINT8 i;

	// Setup PlayerMenu table
	for (i = 0; i < MAXSKINS; i++)
	{
		PlayerMenu[i].status = (i < 4 ? IT_CALL : IT_DISABLED);
		PlayerMenu[i].patch = PlayerMenu[i].text = NULL;
		PlayerMenu[i].itemaction = M_ChoosePlayer;
		PlayerMenu[i].alphaKey = 0;
	}

	// Setup description table
	for (i = 0; i < MAXSKINS; i++)
	{
		if (i == 0)
		{
			strcpy(description[i].notes, "\x82Sonic\x80 is the fastest of the three, but also the hardest to control. Beginners beware, but experts will find Sonic very powerful.\n\n\x82""Ability:\x80 Speed Thok\nDouble jump to zoom forward with a huge burst of speed.\n\n\x82Tip:\x80 Simply letting go of forward does not slow down in SRB2. To slow down, hold the opposite direction.");
			strcpy(description[i].picname, "");
			strcpy(description[i].skinname, "sonic");
		}
		else if (i == 1)
		{
			strcpy(description[i].notes, "\x82Tails\x80 is the most mobile of the three, but has the slowest speed. Because of his mobility, he's well-\nsuited to beginners.\n\n\x82""Ability:\x80 Fly\nDouble jump to start flying for a limited time. Repetitively hit the jump button to ascend.\n\n\x82Tip:\x80 To quickly descend while flying, hit the spin button.");
			strcpy(description[i].picname, "");
			strcpy(description[i].skinname, "tails");
		}
		else if (i == 2)
		{
			strcpy(description[i].notes, "\x82Knuckles\x80 is well-\nrounded and can destroy breakable walls simply by touching them, but he can't jump as high as the other two.\n\n\x82""Ability:\x80 Glide & Climb\nDouble jump to glide in the air as long as jump is held. Glide into a wall to climb it.\n\n\x82Tip:\x80 Press spin while climbing to jump off the wall; press jump instead to jump off\nand face away from\nthe wall.");
			strcpy(description[i].picname, "");
			strcpy(description[i].skinname, "knuckles");
		}
		else if (i == 3)
		{
			strcpy(description[i].notes, "\x82Sonic & Tails\x80 team up to take on Dr. Eggman!\nControl Sonic while Tails desperately struggles to keep up.\n\nPlayer 2 can control Tails directly by setting the controls in the options menu.\nTails's directional controls are relative to Player 1's camera.\n\nTails can pick up Sonic while flying and carry him around.");
			strcpy(description[i].picname, "CHRS&T");
			strcpy(description[i].skinname, "sonic&tails");
		}
		else
		{
			strcpy(description[i].notes, "???");
			strcpy(description[i].picname, "");
			strcpy(description[i].skinname, "");
		}
	}
}

// ==========================================================================
// SPECIAL MENU OPTION DRAW ROUTINES GO HERE
// ==========================================================================

// Converts a string into question marks.
// Used for the secrets menu, to hide yet-to-be-unlocked stuff.
static const char *M_CreateSecretMenuOption(const char *str)
{
	static char qbuf[32];
	int i;

	for (i = 0; i < 31; ++i)
	{
		if (!str[i])
		{
			qbuf[i] = '\0';
			return qbuf;
		}
		else if (str[i] != ' ')
			qbuf[i] = '?';
		else
			qbuf[i] = ' ';
	}

	qbuf[31] = '\0';
	return qbuf;
}

static void M_DrawThermo(INT32 x, INT32 y, consvar_t *cv)
{
	INT32 xx = x, i;
	lumpnum_t leftlump, rightlump, centerlump[2], cursorlump;
	patch_t *p;

	leftlump      = W_GetNumForName("M_THERML");
	rightlump     = W_GetNumForName("M_THERMR");
	centerlump[0] = W_GetNumForName("M_THERMM");
	centerlump[1] = W_GetNumForName("M_THERMM");
	cursorlump    = W_GetNumForName("M_THERMO");

	V_DrawScaledPatch(xx, y, 0, p = (patch_t *)W_CachePatchNum(leftlump, PU_PATCH));
	xx += p->width - p->leftoffset;
	for (i = 0; i < 16; i++)
	{
		V_DrawScaledPatch(xx, y, V_WRAPX, (patch_t *)W_CachePatchNum(centerlump[i & 1], PU_PATCH));
		xx += 8;
	}
	V_DrawScaledPatch(xx, y, 0, (patch_t *)W_CachePatchNum(rightlump, PU_PATCH));

	xx = (cv->value - cv->PossibleValue[0].value) * (15*8) /
		(cv->PossibleValue[1].value - cv->PossibleValue[0].value);

	V_DrawScaledPatch((x + 8) + xx, y, 0, (patch_t *)W_CachePatchNum(cursorlump, PU_PATCH));
}

//  A smaller 'Thermo', with range given as percents (0-100)
static void M_DrawSlider(INT32 x, INT32 y, const consvar_t *cv, boolean ontop)
{
	INT32 i;
	INT32 range;
	patch_t *p;

	x = BASEVIDWIDTH - x - SLIDER_WIDTH;

	p =  (patch_t *)W_CachePatchName("M_SLIDEL", PU_PATCH);
	V_DrawScaledPatch(x - 8, y, 0, p);

	p =  (patch_t *)W_CachePatchName("M_SLIDEM", PU_PATCH);
	for (i = 0; i < SLIDER_RANGE; i++)
		V_DrawScaledPatch (x+i*8, y, 0,p);

	p = (patch_t *)W_CachePatchName("M_SLIDER", PU_PATCH);
	V_DrawScaledPatch(x+i*8, y, 0, p);

	// draw the slider cursor
	p = (patch_t *)W_CachePatchName("M_SLIDEC", PU_PATCH);

	for (i = 0; cv->PossibleValue[i+1].strvalue; i++);

	if (cv->flags & CV_FLOAT)
		range = (INT32)(atof(cv->defaultvalue)*FRACUNIT);
	else
		range = atoi(cv->defaultvalue);

	if (range != cv->value)
	{
		range = ((range - cv->PossibleValue[0].value) * 100 /
		 (cv->PossibleValue[i].value - cv->PossibleValue[0].value));

		if (range < 0)
			range = 0;
		else if (range > 100)
			range = 100;

		V_DrawMappedPatch(x - 4 + (((SLIDER_RANGE)*8 + 4)*range)/100, y, 0, p, yellowmap);
	}

	range = ((cv->value - cv->PossibleValue[0].value) * 100 /
	 (cv->PossibleValue[i].value - cv->PossibleValue[0].value));

	if (range < 0)
		range = 0;
	else if (range > 100)
		range = 100;

	V_DrawMappedPatch(x - 4 + (((SLIDER_RANGE)*8 + 4)*range)/100, y, 0, p, yellowmap);

	if (ontop)
	{
		V_DrawCharacter(x - 16 - (skullAnimCounter/5), y,
			'\x1C' | highlightflags, false);
		V_DrawCharacter(x+(SLIDER_RANGE*8) + 8 + (skullAnimCounter/5), y,
			'\x1D' | highlightflags, false);
		V_DrawCenteredString(x + ((SLIDER_RANGE*8) + 8)/2, y, V_30TRANS,
			(cv->flags & CV_FLOAT) ? va("%.2f", FIXED_TO_FLOAT(cv->value)) : va("%d", cv->value));
	}
}

//
//  Draw a textbox, like Quake does, because sometimes it's difficult
//  to read the text with all the stuff in the background...
//
void M_DrawTextBox(INT32 x, INT32 y, INT32 width, INT32 boxlines)
{
	// Solid color textbox.
	V_DrawFill(x+5, y+5, width*8+6, boxlines*8+6, 239);
}

void M_DrawTextBoxFlags(INT32 x, INT32 y, INT32 width, INT32 boxlines, INT32 flags)
{
	// Solid color textbox.
	V_DrawFill(x+5, y+5, width*8+6, boxlines*8+6, 239|flags);
}

// horizontally centered text
static void M_CentreText(INT32 y, const char *string)
{
	INT32 x;
	//added : 02-02-98 : centre on 320, because V_DrawString centers on vid.width...
	x = (BASEVIDWIDTH - V_StringWidth(string, V_OLDSPACING))>>1;
	V_DrawString(x,y,V_OLDSPACING|MENUCAPS,string);
}

//
// M_DrawMapEmblems
//
// used by pause & statistics to draw a row of emblems for a map
//
static void M_DrawMapEmblems(INT32 mapnum, INT32 x, INT32 y)
{
	UINT8 lasttype = UINT8_MAX, curtype;
	emblem_t *emblem = M_GetLevelEmblems(mapnum);

	while (emblem)
	{
		switch (emblem->type)
		{
			case ET_TIME: //case ET_SCORE: case ET_RINGS:
				curtype = 1; break;
			default:
				curtype = 0; break;
		}

		// Shift over if emblem is of a different discipline
		if (lasttype != UINT8_MAX && lasttype != curtype)
			x -= 4;
		lasttype = curtype;

		if (emblem->collected)
			V_DrawSmallMappedPatch(x, y, 0, (patch_t *)W_CachePatchName(M_GetEmblemPatch(emblem), PU_PATCH),
			                       R_GetTranslationColormap(TC_DEFAULT, M_GetEmblemColor(emblem), GTC_MENUCACHE));
		else
			V_DrawSmallScaledPatch(x, y, 0, (patch_t *)W_CachePatchName("NEEDIT", PU_PATCH));

		emblem = M_GetLevelEmblems(-1);
		x -= 8;
	}
}

static void M_DrawMenuTitle(void)
{
	if (currentMenu->menutitlepic)
	{
		patch_t *p = (patch_t *)W_CachePatchName(currentMenu->menutitlepic, PU_PATCH);

		if (p->height > 24) // title is larger than normal
		{
			INT32 xtitle = (BASEVIDWIDTH - (p->width/2))/2;
			INT32 ytitle = (30 - (p->height/2))/2;

			if (xtitle < 0)
				xtitle = 0;
			if (ytitle < 0)
				ytitle = 0;

			V_DrawSmallScaledPatch(xtitle, ytitle, 0, p);
		}
		else
		{
			INT32 xtitle = (BASEVIDWIDTH - p->width)/2;
			INT32 ytitle = (30 - p->height)/2;

			if (xtitle < 0)
				xtitle = 0;
			if (ytitle < 0)
				ytitle = 0;

			V_DrawScaledPatch(xtitle, ytitle, 0, p);
		}
	}
}

// TODO: This is fucking terrible.
static void M_DrawSplitText(INT32 x, INT32 y, INT32 option, const char* str, INT32 alpha)
{
	char* icopy = strdup(str);
	char** clines = NULL;
	INT16 num_lines = 0;

	if (icopy == NULL)
		return;

	char* tok = strtok(icopy, "\n");

	while (tok != NULL)
	{
		char* line = strdup(tok);

		if (line == NULL)
		{
			goto cleanup;
		}

		char **tmp = realloc(clines, (num_lines + 1) * sizeof(char *));

		if (tmp == NULL)
		{
			free(line);
			goto cleanup;
		}

		clines = tmp;
		clines[num_lines] = line;
		num_lines++;

		tok = strtok(NULL, "\n");
	}

	INT16 yoffset;
	yoffset = (((5*10 - num_lines*10)));

	// Draw BG first,,,
	for (int i = 0; i < num_lines; i++)
	{
		V_DrawFill(0, (y + yoffset - 6)+5, vid.width, 11, 239|V_SNAPTOBOTTOM|V_SNAPTOLEFT);
		yoffset += 11;
	}

	yoffset = (((5*10 - num_lines*10)));

	// THEN the text
	for (int i = 0; i < num_lines; i++)
	{
        V_DrawCenteredThinString(x, y + yoffset, option, clines[i]);
		V_DrawCenteredThinString(x, y + yoffset, option|V_YELLOWMAP|((9 - alpha) << V_ALPHASHIFT), clines[i]);
		yoffset += 10;
    }

cleanup:
	if (clines)
	{
		// Remember to free the memory for each line when you're done with it.
		for (int i = 0; i < num_lines; i++)
			free(clines[i]);
		free(clines);
	}
	free(icopy);
}

static void M_DoToolTips(menu_t* menu)
{
	if (!menu->tooltips || itemOn == -1 || !menu->tooltips[itemOn])
		return;

	M_DrawSplitText(BASEVIDWIDTH / 2, BASEVIDHEIGHT-50, V_ALLOWLOWERCASE|V_SNAPTOBOTTOM, menu->tooltips[itemOn], coolalphatimer);

	if ((coolalphatimer > 0) && interpTimerHackAllow)
		coolalphatimer--;
}

static void M_DrawGenericMenu(void)
{
	INT32 x, y, w, i, cursory = 0;

	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y;

	// draw title (or big pic)
	M_DrawMenuTitle();

	for (i = 0; i < currentMenu->numitems; i++)
	{
		if (i == itemOn)
			cursory = y;
		switch (currentMenu->menuitems[i].status & IT_DISPLAY)
		{
			case IT_PATCH:
				if (currentMenu->menuitems[i].patch && currentMenu->menuitems[i].patch[0])
				{
					if (currentMenu->menuitems[i].status & IT_CENTER)
					{
						patch_t *p;
						p = (patch_t *)W_CachePatchName(currentMenu->menuitems[i].patch, PU_PATCH);
						V_DrawScaledPatch((BASEVIDWIDTH - p->width)/2, y, 0, p);
					}
					else
					{
						V_DrawScaledPatch(x, y, 0,
							(patch_t *)W_CachePatchName(currentMenu->menuitems[i].patch, PU_PATCH));
					}
				}
				/* FALLTHRU */
			case IT_NOTHING:
			case IT_DYBIGSPACE:
				y = currentMenu->y+currentMenu->menuitems[i].alphaKey;
				break;
			case IT_BIGSLIDER:
				M_DrawThermo(x, y, (consvar_t *)currentMenu->menuitems[i].itemaction);
				y += LINEHEIGHT;
				break;
			case IT_STRING:
			case IT_WHITESTRING:
				if (currentMenu->menuitems[i].alphaKey)
					y = currentMenu->y+currentMenu->menuitems[i].alphaKey;
				if (i == itemOn)
					cursory = y;

				if ((currentMenu->menuitems[i].status & IT_DISPLAY)==IT_STRING)
					V_DrawString(x, y, MENUCAPS, currentMenu->menuitems[i].text);
				else
					V_DrawString(x, y, MENUCAPS|highlightflags, currentMenu->menuitems[i].text);

				// Cvar specific handling
				switch (currentMenu->menuitems[i].status & IT_TYPE)
					case IT_CVAR:
					{
						consvar_t *cv = (consvar_t *)currentMenu->menuitems[i].itemaction;
						switch (currentMenu->menuitems[i].status & IT_CVARTYPE)
						{
							case IT_CV_SLIDER:
								M_DrawSlider(x, y, cv, (i == itemOn));
							case IT_CV_NOPRINT: // color use this
							case IT_CV_INVISSLIDER: // monitor toggles use this
								break;
							case IT_CV_STRING:
								M_DrawTextBox(x, y + 4, MAXSTRINGLENGTH, 1);

								if (itemOn == i)
									M_DrawTextInput(x + 8, y + 12, &menuinput, 0);
								else
									V_DrawString(x + 8, y + 12, V_ALLOWLOWERCASE, cv->string);

								y += 16;
								break;
							default:
								w = V_StringWidth(cv->string, 0);
								V_DrawString(BASEVIDWIDTH - x - w, y,
									((cv->flags & CV_CHEAT) && !CV_IsSetToDefault(cv) ? warningflags : highlightflags)|MENUCAPS, cv->string);
								if (i == itemOn)
								{
									V_DrawCharacter(BASEVIDWIDTH - x - 10 - w - (skullAnimCounter/5), y,
											'\x1C' | highlightflags, false); // left arrow
									V_DrawCharacter(BASEVIDWIDTH - x + 2 + (skullAnimCounter/5), y,
											'\x1D' | highlightflags, false); // right arrow
								}
								break;
						}
						break;
					}
					y += STRINGHEIGHT;
					break;
			case IT_STRING2:
				V_DrawString(((BASEVIDWIDTH - V_StringWidth(currentMenu->menuitems[i].text, 0))>>1), y, MENUCAPS, currentMenu->menuitems[i].text);
				/* FALLTHRU */
			case IT_DYLITLSPACE:
				y += SMALLLINEHEIGHT;
				break;
			case IT_GRAYPATCH:
				if (currentMenu->menuitems[i].patch && currentMenu->menuitems[i].patch[0])
					V_DrawMappedPatch(x, y, 0,
						(patch_t *)W_CachePatchName(currentMenu->menuitems[i].patch, PU_PATCH), graymap);
				y += LINEHEIGHT;
				break;
			case IT_TRANSTEXT:
				if (currentMenu->menuitems[i].alphaKey)
					y = currentMenu->y+currentMenu->menuitems[i].alphaKey;
				/* FALLTHRU */
			case IT_TRANSTEXT2:
				V_DrawString(x, y, V_TRANSLUCENT|MENUCAPS, currentMenu->menuitems[i].text);
				y += SMALLLINEHEIGHT;
				break;
			case IT_QUESTIONMARKS:
				if (currentMenu->menuitems[i].alphaKey)
					y = currentMenu->y+currentMenu->menuitems[i].alphaKey;

				V_DrawString(x, y, V_TRANSLUCENT|V_OLDSPACING|MENUCAPS, M_CreateSecretMenuOption(currentMenu->menuitems[i].text));
				y += SMALLLINEHEIGHT;
				break;
			case IT_HEADERTEXT: // draws 16 pixels to the left, in yellow text
				if (currentMenu->menuitems[i].alphaKey)
					y = currentMenu->y+currentMenu->menuitems[i].alphaKey;

				V_DrawString(x-16, y, highlightflags|MENUCAPS, currentMenu->menuitems[i].text);
				y += SMALLLINEHEIGHT;
				break;
		}
	}

	// DRAW THE SKULL CURSOR
	if (((currentMenu->menuitems[itemOn].status & IT_DISPLAY) == IT_PATCH)
		|| ((currentMenu->menuitems[itemOn].status & IT_DISPLAY) == IT_NOTHING))
	{
		V_DrawScaledPatch(currentMenu->x + SKULLXOFF, cursory - 5, 0,
			(patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
	}
	else
	{
		V_DrawScaledPatch(currentMenu->x - 24, cursory, 0,
			(patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
		V_DrawString(currentMenu->x, cursory, MENUCAPS|highlightflags, currentMenu->menuitems[itemOn].text);
	}

	// dumb hack
	// tooltips
	M_DoToolTips(currentMenu);
}

static void M_DrawGenericBackgroundMenu(void)
{
	V_DrawPatchFill(srb2back);
	M_DrawGenericMenu();
}

#define scrollareaheight 72

// note that alphakey is multiplied by 2 for scrolling menus to allow greater usage in UINT8 range.
static void M_DrawGenericScrollMenu(void)
{
	INT32 x, y, i, max, bottom, tempcentery, cursory = 0;

	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y;

	if (currentMenu->menuitems[currentMenu->numitems-1].alphaKey < scrollareaheight)
		tempcentery = y; // Not tall enough to scroll, but this thinker is used in case it becomes so
	else if ((currentMenu->menuitems[itemOn].alphaKey*2 - currentMenu->menuitems[0].alphaKey*2) <= scrollareaheight)
		tempcentery = y - currentMenu->menuitems[0].alphaKey*2;
	else if ((currentMenu->menuitems[currentMenu->numitems-1].alphaKey*2 - currentMenu->menuitems[itemOn].alphaKey*2) <= scrollareaheight)
		tempcentery = y - currentMenu->menuitems[currentMenu->numitems-1].alphaKey*2 + 2*scrollareaheight;
	else
		tempcentery = y - currentMenu->menuitems[itemOn].alphaKey*2 + scrollareaheight;

	for (i = 0; i < currentMenu->numitems; i++)
	{
		if (currentMenu->menuitems[i].status != IT_DISABLED && currentMenu->menuitems[i].alphaKey*2 + tempcentery >= y)
			break;
	}

	for (bottom = currentMenu->numitems; bottom > 0; bottom--)
	{
		if (currentMenu->menuitems[bottom-1].status != IT_DISABLED)
			break;
	}

	for (max = bottom; max > 0; max--)
	{
		if (currentMenu->menuitems[max-1].status != IT_DISABLED && currentMenu->menuitems[max-1].alphaKey*2 + tempcentery <= (y + 2*scrollareaheight))
			break;
	}

	if (i)
		V_DrawString(x - 20, y - (skullAnimCounter/5), highlightflags, "\x1A"); // up arrow
	if (max != bottom)
		V_DrawString(x - 20, y + 2*scrollareaheight + (skullAnimCounter/5), highlightflags, "\x1B"); // down arrow

	// draw title (or big pic)
	M_DrawMenuTitle();

	for (; i < max; i++)
	{
		y = currentMenu->menuitems[i].alphaKey*2 + tempcentery;
		if (i == itemOn)
			cursory = y;
		switch (currentMenu->menuitems[i].status & IT_DISPLAY)
		{
			case IT_PATCH:
				// unsupported
				break;
			case IT_NOTHING:
			case IT_DYBIGSPACE:
				break;
			case IT_STRING:
			case IT_WHITESTRING:
				if (i != itemOn && (currentMenu->menuitems[i].status & IT_DISPLAY)==IT_STRING)
					V_DrawString(x, y, MENUCAPS, currentMenu->menuitems[i].text);
				else
					V_DrawString(x, y, MENUCAPS|highlightflags, currentMenu->menuitems[i].text);

				// Cvar specific handling
				switch (currentMenu->menuitems[i].status & IT_TYPE)
					case IT_CVAR:
					{
						consvar_t *cv = (consvar_t *)currentMenu->menuitems[i].itemaction;
						switch (currentMenu->menuitems[i].status & IT_CVARTYPE)
						{
							case IT_CV_SLIDER:
								M_DrawSlider(x, y, cv, (i == itemOn));
							case IT_CV_NOPRINT: // color use this
							case IT_CV_INVISSLIDER: // monitor toggles use this
								break;
							case IT_CV_STRING:
								if (y + 12 > (y + 2*scrollareaheight))
									break;
								M_DrawTextBox(x, y + 4, MAXSTRINGLENGTH, 1);

								if (itemOn == i)
									M_DrawTextInput(x + 8, y + 12, &menuinput, 0);
								else
									V_DrawString(x + 8, y + 12, MENUCAPS, cv->string);

								break;
							default:
								V_DrawRightAlignedString(BASEVIDWIDTH - x, y,
									((cv->flags & CV_CHEAT) && !CV_IsSetToDefault(cv) ? V_REDMAP : highlightflags)|MENUCAPS, cv->string);
								if (i == itemOn)
								{
									V_DrawCharacter(BASEVIDWIDTH - x - 10 - V_StringWidth(cv->string, 0) - (skullAnimCounter/5), y,
											'\x1C' | highlightflags, false);
									V_DrawCharacter(BASEVIDWIDTH - x + 2 + (skullAnimCounter/5), y,
											'\x1D' | highlightflags, false);
								}
								break;
						}
						break;
					}
					break;
			case IT_TRANSTEXT:
				V_DrawString(x, y, V_TRANSLUCENT|MENUCAPS, currentMenu->menuitems[i].text);
				break;
			case IT_QUESTIONMARKS:
				V_DrawString(x, y, MENUCAPS|V_TRANSLUCENT|V_OLDSPACING, M_CreateSecretMenuOption(currentMenu->menuitems[i].text));
				break;
			case IT_HEADERTEXT:
				V_DrawString(x-16, y, highlightflags|MENUCAPS, currentMenu->menuitems[i].text);
				break;
		}
	}

	// DRAW THE SKULL CURSOR
	V_DrawScaledPatch(x - 24, cursory, 0, (patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));

	// dumb hack
	// tooltips
	M_DoToolTips(currentMenu);
}

static void M_DrawPauseMenu(void)
{
#ifdef HAVE_DISCORDRPC
	// kind of hackily baked in here
	if (currentMenu == &MPauseDef && discordRequestList != NULL)
	{
		const tic_t freq = TICRATE/2;

		if ((leveltime % freq) >= freq/2)
		{
			V_DrawFixedPatch(204 * FRACUNIT,
				(currentMenu->y + MPauseMenu[mpause_discordrequests].alphaKey - 1) * FRACUNIT,
				FRACUNIT,
				0,
				(patch_t *)W_CachePatchName("K_REQUE2", PU_PATCH),
				NULL
			);
		}
	}
#endif

	musicdef_t *def;
	if (cv_pausesongcredits.value && (def = S_FindMusicCredit(S_MusicName())) != NULL)
	{
		V_DrawThinString(2, 2, V_SNAPTOTOP|V_SNAPTOLEFT|V_ALLOWLOWERCASE, va("\x1F"" %s", def->source));
	}

	M_DrawGenericMenu();
}

static void M_DrawCenteredMenu(void)
{
	INT32 x, y, i, cursory = 0;

	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y;

	// draw title (or big pic)
	M_DrawMenuTitle();

	for (i = 0; i < currentMenu->numitems; i++)
	{
		if (i == itemOn)
			cursory = y;
		switch (currentMenu->menuitems[i].status & IT_DISPLAY)
		{
			case IT_PATCH:
				if (currentMenu->menuitems[i].patch && currentMenu->menuitems[i].patch[0])
				{
					patch_t *p = (patch_t *)W_CachePatchName(currentMenu->menuitems[i].patch, PU_PATCH);

					if (currentMenu->menuitems[i].status & IT_CENTER)
					{
						V_DrawScaledPatch((BASEVIDWIDTH - p->width)/2, y, 0, p);
					}
					else
					{
						V_DrawScaledPatch(x, y, 0, p);
					}
				}
				/* FALLTHRU */
			case IT_NOTHING:
			case IT_DYBIGSPACE:
				y += LINEHEIGHT;
				break;
			case IT_BIGSLIDER:
				M_DrawThermo(x, y, (consvar_t *)currentMenu->menuitems[i].itemaction);
				y += LINEHEIGHT;
				break;
			case IT_STRING:
			case IT_WHITESTRING:
				if (currentMenu->menuitems[i].alphaKey)
					y = currentMenu->y+currentMenu->menuitems[i].alphaKey;
				if (i == itemOn)
					cursory = y;

				if ((currentMenu->menuitems[i].status & IT_DISPLAY)==IT_STRING)
					V_DrawCenteredString(x, y, MENUCAPS, currentMenu->menuitems[i].text);
				else
					V_DrawCenteredString(x, y, highlightflags|MENUCAPS, currentMenu->menuitems[i].text);

				// Cvar specific handling
				switch(currentMenu->menuitems[i].status & IT_TYPE)
					case IT_CVAR:
					{
						consvar_t *cv = (consvar_t *)currentMenu->menuitems[i].itemaction;
						switch(currentMenu->menuitems[i].status & IT_CVARTYPE)
						{
							case IT_CV_SLIDER:
								M_DrawSlider(x, y, cv, (i == itemOn));
							case IT_CV_NOPRINT: // color use this
								break;
							case IT_CV_STRING:
								M_DrawTextBox(x, y + 4, MAXSTRINGLENGTH, 1);

								if (itemOn == i)
									M_DrawTextInput(x + 8, y + 12, &menuinput, 0);
								else
									V_DrawString(x + 8, y + 12, V_ALLOWLOWERCASE, cv->string);

								y += 16;
								break;
							default:
								V_DrawString(BASEVIDWIDTH - x - V_StringWidth(cv->string, 0), y,
									((cv->flags & CV_CHEAT) && !CV_IsSetToDefault(cv) ? warningflags : highlightflags)|MENUCAPS, cv->string);
								break;
						}
						break;
					}
					y += STRINGHEIGHT;
					break;
			case IT_STRING2:
				V_DrawCenteredString(x, y, MENUCAPS, currentMenu->menuitems[i].text);
				/* FALLTHRU */
			case IT_DYLITLSPACE:
				y += SMALLLINEHEIGHT;
				break;
			case IT_QUESTIONMARKS:
				if (currentMenu->menuitems[i].alphaKey)
					y = currentMenu->y+currentMenu->menuitems[i].alphaKey;

				V_DrawCenteredString(x, y, V_TRANSLUCENT|V_OLDSPACING|MENUCAPS, M_CreateSecretMenuOption(currentMenu->menuitems[i].text));
				y += SMALLLINEHEIGHT;
				break;
			case IT_GRAYPATCH:
				if (currentMenu->menuitems[i].patch && currentMenu->menuitems[i].patch[0])
					V_DrawMappedPatch(x, y, 0,
						(patch_t *)W_CachePatchName(currentMenu->menuitems[i].patch, PU_PATCH), graymap);
				y += LINEHEIGHT;
				break;
		}
	}

	// DRAW THE SKULL CURSOR
	if (((currentMenu->menuitems[itemOn].status & IT_DISPLAY) == IT_PATCH)
		|| ((currentMenu->menuitems[itemOn].status & IT_DISPLAY) == IT_NOTHING))
	{
		V_DrawScaledPatch(x + SKULLXOFF, cursory - 5, 0,
			(patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
	}
	else
	{
		V_DrawScaledPatch(x - V_StringWidth(currentMenu->menuitems[itemOn].text, 0)/2 - 24, cursory, 0,
			(patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
		V_DrawCenteredString(x, cursory, highlightflags|MENUCAPS, currentMenu->menuitems[itemOn].text);
	}
}

//
// M_StringHeight
//
// Find string height from hu_font chars
//
static inline size_t M_StringHeight(const char *string)
{
	size_t h = 8, i;
	const size_t strlength = strlen(string);

	for (i = 0; i < strlength; i++)
		if (string[i] == '\n')
			h += 8;

	return h;
}

// ==========================================================================
// Extraneous menu patching functions
// ==========================================================================

//
// M_PatchSkinNameTable
//
// Like M_PatchLevelNameTable, but for cv_chooseskin
//
static void M_PatchSkinNameTable(void)
{
	INT32 j;

	memset(skins_cons_t, 0, sizeof (skins_cons_t));

	for (j = 0; j < MAXSKINS; j++)
	{
		if (skins[j].name[0] != '\0')
		{
			skins_cons_t[j].strvalue = skins[j].name;
			skins_cons_t[j].value = j+1;
		}
		else
		{
			skins_cons_t[j].strvalue = NULL;
			skins_cons_t[j].value = 0;
			break;
		}
	}

	j = R_SkinAvailable(cv_skin.string);
	if (j == -1)
		j = 0;

	CV_SetValue(&cv_chooseskin, j+1); // This causes crash sometimes?!

	return;
}

// Call before showing any level-select menus
static void M_PrepareLevelSelect(void)
{
	if (levellistmode != LLM_CREATESERVER)
		CV_SetValue(&cv_nextmap, M_GetFirstLevelInList());
	else
		Newgametype_OnChange(); // Make sure to start on an appropriate map if wads have been added
}

//
// M_CanShowLevelInList
//
// Determines whether to show a given map in the various level-select lists.
// Set gt = -1 to ignore gametype.
//
boolean M_CanShowLevelInList(INT32 mapnum, INT32 gt)
{
	// invalid mapnum
	if (mapnum < -1 || mapnum >= NUMMAPS)
		return false;

	// Random map!
	if (mapnum == -1)
		return (gamestate != GS_TIMEATTACK && !modeattacking);

	// Does the map exist?
	if (!mapheaderinfo[mapnum])
		return false;

	// Does the map have a name?
	if (!mapheaderinfo[mapnum]->lvlttl[0])
		return false;

	switch (levellistmode)
	{
		case LLM_CREATESERVER:
			// Should the map be hidden? <-- well imma wanna toggle it, its just annoying being unable to select hell maps in mapselect

			if (mapheaderinfo[mapnum]->menuflags & LF2_HIDEINMENU && mapnum+1 != gamemap)
			{
				if (cv_showallmaps.value &&
					((gt == GT_RACE && (mapheaderinfo[mapnum]->typeoflevel & TOL_RACE)) ||                         // race map hell
					((gt == GT_MATCH || gt == GT_TEAMMATCH) && (mapheaderinfo[mapnum]->typeoflevel & TOL_MATCH)))) // battle map hell
					return true;
				else
					return false;
			}

			// same goes here, just show every map if i want to
			if (M_MapLocked(mapnum+1)) // not unlocked
				return cv_showallmaps.value;

			if ((gt == GT_MATCH || gt == GT_TEAMMATCH) && (mapheaderinfo[mapnum]->typeoflevel & TOL_MATCH))
				return true;

			if (gt == GT_RACE && (mapheaderinfo[mapnum]->typeoflevel & TOL_RACE))
				return true;

			return false;

		case LLM_RECORDATTACK:
			if (!(mapheaderinfo[mapnum]->typeoflevel & TOL_RACE))
				return false;

			if (M_MapLocked(mapnum+1))
				return false; // not unlocked

			if (M_SecretUnlocked(SECRET_HELLATTACK))
				return true; // now you're in hell

			if (mapheaderinfo[mapnum]->menuflags & LF2_HIDEINMENU)
				return false; // map hell

			return true;
		default:
			return false;
	}

	// Hmm? Couldn't decide?
	return false;
}

static INT32 M_CountLevelsToShowInList(void)
{
	INT32 mapnum, count = 0;

	for (mapnum = 0; mapnum < NUMMAPS; mapnum++)
		if (M_CanShowLevelInList(mapnum, -1))
			count++;

	return count;
}

static INT32 M_GetFirstLevelInList(void)
{
	INT32 mapnum;

	for (mapnum = 0; mapnum < NUMMAPS; mapnum++)
		if (M_CanShowLevelInList(mapnum, -1))
			return mapnum + 1;

	return 1;
}

// ==================================================
// MESSAGE BOX (aka: a hacked, cobbled together menu)
// ==================================================
static void M_DrawMessageMenu(void);

// Because this is just a hack-ish 'menu', I'm not putting this with the others
static menuitem_t MessageMenu[] =
{
	// TO HACK
	{0,NULL, NULL, NULL,0}
};

menu_t MessageDef =
{
	NULL,               // title
	1,                  // # of menu items
	NULL,               // previous menu       (TO HACK)
	MessageMenu,        // menuitem_t ->
	M_DrawMessageMenu,  // drawing routine ->
	NULL,               // ticker routine
	0, 0,               // x, y                (TO HACK)
	0,                  // lastOn, flags       (TO HACK)
	NULL,
	NULL,               // tooltips lel
};

void M_StartMessage(const char *string, void *routine,
	menumessagetype_t itemtype)
{
	size_t max = 0, start = 0, i, strlines;
	static char *message = NULL;
	Z_Free(message);
	message = Z_StrDup(string);
	DEBFILE(message);

	// Rudementary word wrapping.
	// Simple and effective. Does not handle nonuniform letter sizes, colors, etc. but who cares.
	strlines = 0;
	for (i = 0; message[i]; i++)
	{
		if (message[i] == ' ')
		{
			start = i;
			max += 4;
		}
		else if (message[i] == '\n')
		{
			strlines = i;
			start = 0;
			max = 0;
			continue;
		}
		else
			max += 8;

		// Start trying to wrap if presumed length exceeds the screen width.
		if (max >= BASEVIDWIDTH && start > 0)
		{
			message[start] = '\n';
			max -= (start-strlines)*8;
			strlines = start;
			start = 0;
		}
	}

	start = 0;
	max = 0;

	M_StartControlPanel(); // can't put menuactive to true

	if (currentMenu == &MessageDef) // Prevent recursion
		MessageDef.prevMenu = ((demo.playback) ? &PlaybackMenuDef : &MainDef);
	else
		MessageDef.prevMenu = currentMenu;

	MessageDef.menuitems[0].text     = message;
	MessageDef.menuitems[0].alphaKey = (UINT8)itemtype;
	if (!routine && itemtype != MM_NOTHING) itemtype = MM_NOTHING;
	switch (itemtype)
	{
		case MM_NOTHING:
			MessageDef.menuitems[0].status     = IT_MSGHANDLER;
			MessageDef.menuitems[0].itemaction = M_StopMessage;
			break;
		case MM_YESNO:
			MessageDef.menuitems[0].status     = IT_MSGHANDLER;
			MessageDef.menuitems[0].itemaction = routine;
			break;
		case MM_EVENTHANDLER:
			MessageDef.menuitems[0].status     = IT_MSGHANDLER;
			MessageDef.menuitems[0].itemaction = routine;
			break;
	}
	//added : 06-02-98: now draw a textbox around the message
	// compute lenght max and the numbers of lines
	for (strlines = 0; *(message+start); strlines++)
	{
		for (i = 0;i < strlen(message+start);i++)
		{
			if (*(message+start+i) == '\n')
			{
				if (i > max)
					max = i;
				start += i;
				i = (size_t)-1; //added : 07-02-98 : damned!
				start++;
				break;
			}
		}

		if (i == strlen(message+start))
		{
			start += i;
			if (i > max)
				max = i;
		}
	}

	MessageDef.x = (INT16)((BASEVIDWIDTH  - 8*max-16)/2);
	MessageDef.y = (INT16)((BASEVIDHEIGHT - M_StringHeight(message))/2);

	MessageDef.lastOn = (INT16)((strlines<<8)+max);

	//M_SetupNextMenu();
	currentMenu = &MessageDef;
	itemOn = 0;
}

#define MAXMSGLINELEN 256

static void M_DrawMessageMenu(void)
{
	INT32 y = currentMenu->y;
	size_t i, start = 0;
	INT16 max;
	char string[MAXMSGLINELEN];
	INT32 mlines;
	const char *msg = currentMenu->menuitems[0].text;

	mlines = currentMenu->lastOn>>8;
	max = (INT16)((UINT8)(currentMenu->lastOn & 0xFF)*8);

	// hack: draw RA background in RA menus
	if (gamestate == GS_TIMEATTACK)
		V_DrawPatchFill(srb2back);

	M_DrawTextBox(currentMenu->x, y - 8, (max+7)>>3, mlines);

	while (*(msg+start))
	{
		size_t len = strlen(msg+start);

		for (i = 0; i < len; i++)
		{
			if (*(msg+start+i) == '\n')
			{
				memset(string, 0, MAXMSGLINELEN);
				if (i >= MAXMSGLINELEN)
				{
					CONS_Printf("M_DrawMessageMenu: too long segment in %s\n", msg);
					return;
				}
				else
				{
					strncpy(string,msg+start, i);
					string[i] = '\0';
					start += i;
					i = (size_t)-1; //added : 07-02-98 : damned!
					start++;
				}
				break;
			}
		}

		if (i == strlen(msg+start))
		{
			if (i >= MAXMSGLINELEN)
			{
				CONS_Printf("M_DrawMessageMenu: too long segment in %s\n", msg);
				return;
			}
			else
			{
				strcpy(string, msg + start);
				start += i;
			}
		}

		V_DrawString((BASEVIDWIDTH - V_StringWidth(string, 0))/2,y,V_ALLOWLOWERCASE,string);
		y += 8; //hu_font[0]->height;
	}
}

// default message handler
static void M_StopMessage(INT32 choice)
{
	(void)choice;
	if (menuactive)
		M_SetupNextMenu(MessageDef.prevMenu);
}

// =========
// IMAGEDEFS
// =========

// Draw an Image Def.  Aka, Help images.
// Defines what image is used in (menuitem_t)->text.
// You can even put multiple images in one menu!
static void M_DrawImageDef(void)
{
	patch_t *patch = (patch_t *)W_CachePatchName(currentMenu->menuitems[itemOn].text, PU_PATCH);

	if (patch->width <= BASEVIDWIDTH)
		V_DrawScaledPatch(0,0,0,patch);
	else
		V_DrawSmallScaledPatch(0,0,0,patch);

	if (currentMenu->menuitems[itemOn].alphaKey)
	{
		V_DrawString(2,BASEVIDHEIGHT-10, V_YELLOWMAP, va("%d", (itemOn<<1)-1)); // intentionally not highlightflags, unlike below
		V_DrawRightAlignedString(BASEVIDWIDTH-2,BASEVIDHEIGHT-10, V_YELLOWMAP, va("%d", itemOn<<1)); // ditto
	}
	else
	{
		INT32 x = BASEVIDWIDTH>>1, y = (BASEVIDHEIGHT>>1) - 4;
		x += (itemOn ? 1 : -1)*((BASEVIDWIDTH>>2) + 10);
		V_DrawCenteredString(x, y-10, highlightflags|MENUCAPS, "Use arrow keys");
		V_DrawCharacter(x - 10 - (skullAnimCounter/5), y,
			'\x1C' | highlightflags, false); // left arrow
		V_DrawCharacter(x + 2 + (skullAnimCounter/5), y,
			'\x1D' | highlightflags, false); // right arrow
		V_DrawCenteredString(x, y+10, highlightflags|MENUCAPS, "to leaf through");
	}
}

// Handles the ImageDefs.  Just a specialized function that
// uses left and right movement.
static void M_HandleImageDef(INT32 choice)
{
	boolean exitmenu = false;

	switch (choice)
	{
		case KEY_RIGHTARROW:
			if (itemOn >= (INT16)(currentMenu->numitems-1))
				break;
			S_StartSound(NULL, sfx_menu1);
			itemOn++;
			break;

		case KEY_LEFTARROW:
			if (!itemOn)
				break;

			S_StartSound(NULL, sfx_menu1);
			itemOn--;
			break;

		case KEY_ESCAPE:
		case KEY_ENTER:
			exitmenu = true;
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// ======================
// MISC MAIN MENU OPTIONS
// ======================

static void M_AddonsOptions(INT32 choice)
{
	(void)choice;
	Addons_option_Onchange();

	M_SetupNextMenu(&OP_AddonsOptionsDef);
}

#define LOCATIONSTRING1 "Visit \x83mb.srb2.org/addons\x80 to get addons!"
#define LOCATIONSTRING2 "Visit \x88mb.srb2.org/addons\x80 to get addons!"

static void M_AddonsInternal(void)
{
	const char *pathname = ".";

	switch (cv_addons_option.value)
	{
		case 0:
			pathname = usehome ? srb2home : srb2path;
			break;
		case 1:
			pathname = srb2home;
			break;
		case 2:
			pathname = srb2path;
			break;
		case 3:
			if (*cv_addons_folder.string != '\0')
				pathname = cv_addons_folder.string;
			break;
	}

	strlcpy(menupath, pathname, MAXFILEPATH);
	menupathindex[(menudepthleft = menudepth-1)] = strlen(menupath) + 1;

	if (menupath[menupathindex[menudepthleft]-2] != PATHSEP[0])
	{
		menupath[menupathindex[menudepthleft]-1] = PATHSEP[0];
		menupath[menupathindex[menudepthleft]] = 0;
	}
	else
		--menupathindex[menudepthleft];

	if (!preparefilemenu(false, false))
	{
		M_StartMessage(va("No files/folders found.\n\n%s\n\n(Press a key)\n", (recommendedflags == V_SKYMAP ? LOCATIONSTRING2 : LOCATIONSTRING1)),NULL,MM_NOTHING);
		return;
	}
	else
		dir_on[menudepthleft] = 0;

	if (addonsp[0]) // never going to have some provided but not all, saves individually checking
	{
		size_t i;
		for (i = 0; i < NUM_EXT+5; i++)
			W_UnlockCachedPatch(addonsp[i]);
	}

	addonsp[EXT_FOLDER]    = (patch_t *)W_CachePatchName("M_FFLDR", PU_PATCH);
	addonsp[EXT_UP]        = (patch_t *)W_CachePatchName("M_FBACK", PU_PATCH);
	addonsp[EXT_NORESULTS] = (patch_t *)W_CachePatchName("M_FNOPE", PU_PATCH);
	addonsp[EXT_TXT]       = (patch_t *)W_CachePatchName("M_FTXT", PU_PATCH);
	addonsp[EXT_CFG]       = (patch_t *)W_CachePatchName("M_FCFG", PU_PATCH);
	addonsp[EXT_WAD]       = (patch_t *)W_CachePatchName("M_FWAD", PU_PATCH);
#ifdef USE_KART
	addonsp[EXT_KART]      = (patch_t *)W_CachePatchName("M_FKART", PU_PATCH);
#endif
	addonsp[EXT_PK3]   = (patch_t *)W_CachePatchName("M_FPK3", PU_PATCH);
	addonsp[EXT_SOC]   = (patch_t *)W_CachePatchName("M_FSOC", PU_PATCH);
	addonsp[EXT_LUA]   = (patch_t *)W_CachePatchName("M_FLUA", PU_PATCH);
	addonsp[NUM_EXT]   = (patch_t *)W_CachePatchName("M_FUNKN", PU_PATCH);
	addonsp[NUM_EXT+1] = (patch_t *)W_CachePatchName("M_FSEL", PU_PATCH);
	addonsp[NUM_EXT+2] = (patch_t *)W_CachePatchName("M_FLOAD", PU_PATCH);
	addonsp[NUM_EXT+3] = (patch_t *)W_CachePatchName("M_FSRCH", PU_PATCH);
	addonsp[NUM_EXT+4] = (patch_t *)W_CachePatchName("M_FSAVE", PU_PATCH);

	MISC_AddonsDef.prevMenu = currentMenu;
	M_SetupNextMenu(&MISC_AddonsDef);
}

static void M_Addons(INT32 choice)
{
	(void)choice;
	browselocalskins = false;
	M_AddonsInternal();
}

static void M_LocalSkins(INT32 choice)
{
	(void)choice;
	browselocalskins = true;
	M_AddonsInternal();
}

#define width 4
#define vpadding 27
#define h (BASEVIDHEIGHT-(2*vpadding))
#define NUMCOLOURS 8 // when toast's coding it's british english hacker fucker
static void M_DrawTemperature(INT32 x, fixed_t t)
{
	INT32 y;

	// bounds check
	if (t > FRACUNIT)
		t = FRACUNIT;

	// scale
	if (t > 1)
		t = (FixedMul(h<<FRACBITS, t)>>FRACBITS);

	// border
	V_DrawFill(x - 1, vpadding, 1, h, 120);
	V_DrawFill(x + width, vpadding, 1, h, 120);
	V_DrawFill(x - 1, vpadding-1, width+2, 1, 120);
	V_DrawFill(x - 1, vpadding+h, width+2, 1, 120);

	// bar itself
	y = h;
	if (t)
		for (t = h - t; y > 0; y--)
		{
			UINT8 colours[NUMCOLOURS] = {135, 133, 92, 77, 114, 178, 161, 162};
			UINT8 c;
			if (y <= t) break;
			if (y+vpadding >= BASEVIDHEIGHT/2)
				c = 185;
			else
				c = colours[(NUMCOLOURS*(y-1))/(h/2)];
			V_DrawFill(x, y-1 + vpadding, width, 1, c);
		}

	// fill the rest of the backing
	if (y)
		V_DrawFill(x, vpadding, width, y, 30);
}
#undef width
#undef vpadding
#undef h
#undef NUMCOLOURS

static char *M_AddonsHeaderPath(void)
{
	UINT32 len;
	static char header[MAXFILEPATH];

	strlcpy(header, va("%s folder%s", cv_addons_option.string, menupath+menupathindex[menudepth-1]-1), MAXFILEPATH);
	len = strlen(header);
	if (len > 34)
	{
		len = len-34;
		header[len] = header[len+1] = header[len+2] = '.';
	}
	else
		len = 0;

	return header+len;
}

#define UNEXIST S_StartSound(NULL, sfx_s26d);\
		M_SetupNextMenu(MISC_AddonsDef.prevMenu);\
		M_StartMessage(va("\x82%s\x80\nThis folder no longer exists!\nAborting to main menu.\n\n(Press a key)\n", M_AddonsHeaderPath()),NULL,MM_NOTHING)

#define CLEARNAME Z_Free(refreshdirname);\
					refreshdirname = NULL

static boolean prevmajormods = false;

static void M_AddonsClearName(INT32 choice)
{
	if (!majormods || prevmajormods)
	{
		CLEARNAME;
	}
	M_StopMessage(choice);
}

// Handles messages for addon errors.
static void M_AddonsRefresh(void)
{
	if ((refreshdirmenu & REFRESHDIR_NORMAL) && !preparefilemenu(true, false))
	{
		UNEXIST;
		if (refreshdirname)
		{
			CLEARNAME;
		}
		return;
	}

	if (!majormods && prevmajormods)
		prevmajormods = false;

	if ((refreshdirmenu & REFRESHDIR_ADDFILE) || (majormods && !prevmajormods))
	{
		char *message = NULL;

		if (refreshdirmenu & REFRESHDIR_NOTLOADED)
		{
			S_StartSound(NULL, sfx_s26d);
			if (refreshdirmenu & REFRESHDIR_MAX)
			{
				if (refreshdirname)
					message = va("%c%s\x80\nMaximum number of addons reached.\nA file could not be loaded.\nIf you wish to play with this addon, restart the game to clear existing ones.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), refreshdirname);
				else
					message = va("%c\x80 Maximum number of addons reached.\nA file could not be loaded.\nIf you wish to play with this addon, restart the game to clear existing ones.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)));
			}
			else
			{
				if (refreshdirname)
					message = va("%c%s\x80\nA file was not loaded.\nCheck the console log for more information.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), refreshdirname);
				else
					message = va("%c\x80 A file was not loaded.\nCheck the console log for more information.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)));
			}
		}
		else if (refreshdirmenu & (REFRESHDIR_WARNING | REFRESHDIR_ERROR))
		{
			S_StartSound(NULL, sfx_s224);
			if (refreshdirname)
				message = va("%c%s\x80\nA file was loaded with %s.\nCheck the console log for more information.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), refreshdirname, ((refreshdirmenu & REFRESHDIR_ERROR) ? "errors" : "warnings"));
			else
				message = va("%c\x80 A file was loaded with %s.\nCheck the console log for more information.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), ((refreshdirmenu & REFRESHDIR_ERROR) ? "errors" : "warnings"));
		}
		else if (majormods && !prevmajormods)
		{
			S_StartSound(NULL, sfx_s221);
			if (refreshdirname)
				message = va("%c%s\x80\nYou've loaded a gameplay-modifying addon.\n\nRecord Attack data will be saved to a seperate save file.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), refreshdirname);
			else
				message = va("%c\x80 You've loaded a gameplay-modifying addon.\n\nRecord Attack data will be saved to a seperate save file.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)));
			prevmajormods = majormods;
		}

		if (message)
		{
			M_StartMessage(message, M_AddonsClearName, MM_EVENTHANDLER);
			return;
		}

		S_StartSound(NULL, sfx_s221);
		CLEARNAME;
	}

	return;
}

static tic_t addons_scrolltic = 0; // maybe not the best place but e

static void M_DrawAddons(void)
{
	INT32 x, y;
	size_t i, m;
	size_t t, b; // top and bottom item #s to draw in directory
	const UINT8 *flashcol = NULL;
	UINT8 hilicol;

	if (renderisnewtic) addons_scrolltic++;

	if (Playing())
	{
		if (browselocalskins)
			V_DrawCenteredString(BASEVIDWIDTH/2, 5, V_ALLOWLOWERCASE, "Load \x83local skins\x80 from addons!");
		else
			V_DrawCenteredString(BASEVIDWIDTH/2, 5, warningflags, "Adding files mid-game may cause problems.");
	}
	else
		V_DrawCenteredString(BASEVIDWIDTH/2, 5, 0, (recommendedflags == V_SKYMAP ? LOCATIONSTRING2 : LOCATIONSTRING1));

	if (numwadfiles <= mainwads+1)
		y = 0;
	else if (numwadfiles >= MAX_WADFILES)
		y = FRACUNIT;
	else
	{
		y = FixedDiv(((ssize_t)(numwadfiles) - (ssize_t)(mainwads+1))<<FRACBITS, ((ssize_t)MAX_WADFILES - (ssize_t)(mainwads+1))<<FRACBITS);
		if (y > FRACUNIT) // happens because of how we're shrinkin' it a little
			y = FRACUNIT;
	}

	M_DrawTemperature(BASEVIDWIDTH - 19 - 5, y);

	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y + 1;

	hilicol = V_GetStringColormap(highlightflags)[120];

#define boxwidth (MAXSTRINGLENGTH*8+6)

	// draw the file path and the top white + black lines of the box
	V_DrawString(x-21, (y - 16) + (lsheadingheight - 12), highlightflags|V_ALLOWLOWERCASE, M_AddonsHeaderPath());
	V_DrawFill(x-21, (y - 16) + (lsheadingheight - 3), boxwidth, 1, hilicol);
	V_DrawFill(x-21, (y - 16) + (lsheadingheight - 2), boxwidth, 1, 30);

	m = (BASEVIDHEIGHT - currentMenu->y + 2) - (y - 1);
	V_DrawFill(x-21, y - 1, boxwidth, m, 239);

	// The directory is too small for a scrollbar, so just draw a tall white line
	if (sizedirmenu <= addonmenusize)
	{
		t = 0; // first item
		b = sizedirmenu - 1; // last item
		i = 0; // "scrollbar" at "top" position
	}
	else
	{
		size_t q = m;
		m = (addonmenusize * m)/sizedirmenu; // height of scroll bar

		if (dir_on[menudepthleft] <= numaddonsshown) // all the way up
		{
			t = 0; // first item
			b = addonmenusize - 1; //9th item
			i = 0; // scrollbar at top position
		}
		else if (dir_on[menudepthleft] >= sizedirmenu - (numaddonsshown + 1)) // all the way down
		{
			t = sizedirmenu - addonmenusize; // # 9th last
			b = sizedirmenu - 1; // last item
			i = q-m; // scrollbar at bottom position
		}
		else // somewhere in the middle
		{
			t = dir_on[menudepthleft] - numaddonsshown; // 4 items above
			b = dir_on[menudepthleft] + numaddonsshown; // 4 items below
			i = (t * (q-m))/(sizedirmenu - addonmenusize); // calculate position of scrollbar
		}
	}

	// draw the scrollbar!
	V_DrawFill((x-21) + boxwidth-1, (y - 1) + i, 1, m, hilicol);

#undef boxwidth

	// draw up arrow that bobs up and down
	if (t != 0)
		V_DrawString(19, y+4 - (skullAnimCounter/5), highlightflags, "\x1A");

	// make the selection box flash yellow
	if (skullAnimCounter < 4)
		flashcol = V_GetStringColormap(highlightflags);

	// draw icons and item names
	for (i = t; i <= b; i++)
	{
		UINT32 flags = V_ALLOWLOWERCASE;

#define charsonside 14
#define MAXADDONNAME (charsonside*2 + 3)
	char scrollbuf[MAXADDONNAME+1] = {0};

		if (y > BASEVIDHEIGHT)
			break;

		if (dirmenu[i])
#define type (UINT8)(dirmenu[i][DIR_TYPE])
		{
			if (type & EXT_LOADED)
			{
				flags |= V_TRANSLUCENT;
				V_DrawSmallScaledPatch(x-(16+4), y, V_TRANSLUCENT, addonsp[(type & ~EXT_LOADED)]);
				V_DrawSmallScaledPatch(x-(16+4), y, 0, addonsp[NUM_EXT+2]);
			}
			else
				V_DrawSmallScaledPatch(x-(16+4), y, 0, addonsp[(type & ~EXT_LOADED)]);

			// draw selection box for the item currently selected
			if (i == dir_on[menudepthleft])
			{
				V_DrawFixedPatch((x-(16+4))<<FRACBITS, (y)<<FRACBITS, FRACUNIT/2, 0, addonsp[NUM_EXT+1], flashcol);
				flags = V_ALLOWLOWERCASE|highlightflags;
			}

			// draw name of the item, use ... if too long

			if (dirmenu[i][DIR_LEN] > MAXADDONNAME)
			{
				if ((size_t)i == dir_on[menudepthleft])
				{
					M_ScrollString(dirmenu[i]+DIR_STRING, dirmenu[i][DIR_LEN]-1, scrollbuf, MAXADDONNAME, addons_scrolltic);
				}
				else
					strncpy(scrollbuf, va("%.*s...%s", charsonside, dirmenu[i]+DIR_STRING, dirmenu[i]+DIR_STRING+dirmenu[i][DIR_LEN]-(charsonside+1)), MAXADDONNAME);

				V_DrawString(x, y+4, flags, scrollbuf);
			}
#undef charsonside
#undef MAXADDONNAME
			else
				V_DrawString(x, y+4, flags, dirmenu[i]+DIR_STRING);
		}
#undef type
		y += 16;
	}

	// draw down arrow that bobs down and up
	if (b != sizedirmenu)
		V_DrawString(19, y-12 + (skullAnimCounter/5), highlightflags, "\x1B");

	// draw search box
	y = BASEVIDHEIGHT - currentMenu->y + 1;

	M_DrawTextBox(x - (21 + 5), y, MAXSTRINGLENGTH, 1);

	if (menusearch.length)
		M_DrawTextInput(x - 18, y + 8, &menusearch, 0);
	else
		V_DrawString(x - 18, y + 8, V_ALLOWLOWERCASE|V_TRANSLUCENT, "Type to search...");

	// draw search icon
	x -= (21 + 5 + 16);
	V_DrawSmallScaledPatch(x, y + 4, (menusearch.length ? 0 : V_TRANSLUCENT), addonsp[NUM_EXT+3]);

	// draw save icon
	x = BASEVIDWIDTH - x - 16;
	V_DrawSmallScaledPatch(x, y + 4, ((!majormods) ? 0 : V_TRANSLUCENT), addonsp[NUM_EXT+4]);

	if (modifiedgame)
		V_DrawSmallScaledPatch(x, y + 4, 0, addonsp[NUM_EXT+2]);

	// no space on alot of resolutions
	//m = numwadfiles-(mainwads+2+1);
	//V_DrawCenteredString(BASEVIDWIDTH/2, y+24, (majormods ? highlightflags : V_TRANSLUCENT), va("%d ADD-ON%s LOADED", (int)m, (m == 1) ? "" : "S")); //+2 for music, sounds, +1 for main.kart

	V_DrawThinString(0, BASEVIDHEIGHT-10, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_TRANSLUCENT|V_ALLOWLOWERCASE, ("END Key - Add addon to autoload"));
}

static void M_AddonExec(INT32 ch)
{
	if (ch != 'y' && ch != KEY_ENTER)
		return;

	S_StartSound(NULL, sfx_zoom);
	COM_BufAddText(va("exec \"%s%s\"", menupath, dirmenu[dir_on[menudepthleft]]+DIR_STRING));
}

// static void M_AddonAutoLoad(INT32 ch);
// exports mods to a file, which helps in autoloading that file
//
static void M_AddonAutoLoad(INT32 ch)
{
	// initalize these variables //
	const char *path;
	FILE *autoloadconfigfile;

	// check our controls //
	if (ch != 'y' && ch != KEY_ENTER && ch != KEY_END)
	{
		S_StartSound(NULL, sfx_s26d);
		return;
	}

	// first, find the file //
	path = va("%s"PATHSEP"%s", srb2home, AUTOLOADCONFIGFILENAME);
	autoloadconfigfile = fopen(path, "a");

	// then, execute the addon and store it in our autoload.cfg //
	switch (dirmenu[dir_on[menudepthleft]][DIR_TYPE])
	{
	    case EXT_FOLDER:
	        M_StartMessage(va("%c%s\x80\nAutoloading folders is not supported as of yet. \n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),NULL,MM_NOTHING);
            break;
		case EXT_TXT:
		case EXT_CFG:
			CONS_Printf("Added the \x82%s\x80 console script to the autoload configuration list.\n", dirmenu[dir_on[menudepthleft]]+DIR_STRING);
			fprintf(autoloadconfigfile, "%s\n", dirmenu[dir_on[menudepthleft]]+DIR_STRING);

			S_StartSound(NULL, sfx_s221);
			break;
		case EXT_LUA:
		case EXT_SOC:
		case EXT_WAD:
		case EXT_KART:
		case EXT_PK3:
		default:
			if (!(refreshdirmenu & REFRESHDIR_MAX))
			{
				CONS_Printf("Added \x82%s\x80 to the autoload configuration list.\n", dirmenu[dir_on[menudepthleft]]+DIR_STRING);
				fprintf(autoloadconfigfile, "%s\n", dirmenu[dir_on[menudepthleft]]+DIR_STRING);

				S_StartSound(NULL, sfx_s221);
			}
			else
			{
				M_StartMessage(va("%c%s\x80\nToo many add-ons are loaded! \nYou need to restart the game to autoload more add-ons and folders. \nYou can still autoload console scripts though. \n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),NULL,MM_NOTHING);
				S_StartSound(NULL, sfx_s26d);
			}
			break;
	}

	// lastly, do some last things and close the autoload config file //
	fclose(autoloadconfigfile);
}

// i hate myself
static boolean DumbStartsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

static void M_HandleAddons(INT32 choice)
{
	boolean exitmenu = false; // exit to previous menu

	if (M_TextInputHandle(&menusearch, choice))
	{
		S_StartSound(NULL, sfx_menu1);

		char *tempname = NULL;
		if (dirmenu && dirmenu[dir_on[menudepthleft]])
			tempname = Z_StrDup(dirmenu[dir_on[menudepthleft]]+DIR_STRING); // don't need to I_Error if can't make - not important, just QoL

		searchfilemenu(tempname);
	}

	switch (choice)
	{
		case KEY_DOWNARROW:
			if (dir_on[menudepthleft] < sizedirmenu-1)
				dir_on[menudepthleft]++;
			addons_scrolltic = 0;
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_UPARROW:
			if (dir_on[menudepthleft])
				dir_on[menudepthleft]--;
			addons_scrolltic = 0;
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_PGDN:
			{
				UINT8 i;
				for (i = numaddonsshown; i && (dir_on[menudepthleft] < sizedirmenu-1); i--)
					dir_on[menudepthleft]++;
			}
			addons_scrolltic = 0;
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_PGUP:
			{
				UINT8 i;
				for (i = numaddonsshown; i && (dir_on[menudepthleft]); i--)
					dir_on[menudepthleft]--;
			}
			addons_scrolltic = 0;
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_ENTER:
			{
				boolean refresh = true;
				if (!dirmenu[dir_on[menudepthleft]])
					S_StartSound(NULL, sfx_s26d);
				else
				{
					switch (dirmenu[dir_on[menudepthleft]][DIR_TYPE])
					{
						case EXT_FOLDER:
							strcpy(&menupath[menupathindex[menudepthleft]],dirmenu[dir_on[menudepthleft]]+DIR_STRING);
							if (menudepthleft)
							{
								menupathindex[--menudepthleft] = strlen(menupath);
								menupath[menupathindex[menudepthleft]] = 0;

								if (!preparefilemenu(false, false))
								{
									S_StartSound(NULL, sfx_s224);
									M_StartMessage(va("%c%s\x80\nThis folder is empty.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), M_AddonsHeaderPath()),NULL,MM_NOTHING);
									menupath[menupathindex[++menudepthleft]] = 0;

									if (!preparefilemenu(true, false))
									{
										UNEXIST;
										return;
									}
								}
								else
								{
									S_StartSound(NULL, sfx_menu1);
									dir_on[menudepthleft] = 1;
								}
								refresh = false;
							}
							else
							{
								S_StartSound(NULL, sfx_s26d);
								M_StartMessage(va("%c%s\x80\nThis folder is too deep to navigate to!\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), M_AddonsHeaderPath()),NULL,MM_NOTHING);
								menupath[menupathindex[menudepthleft]] = 0;
							}
							break;
						case EXT_UP:
							S_StartSound(NULL, sfx_menu1);
							menupath[menupathindex[++menudepthleft]] = 0;
							if (!preparefilemenu(false, false))
							{
								UNEXIST;
								return;
							}
							break;
						case EXT_TXT:
							M_StartMessage(va("%c%s\x80\nThis file may not be a console script.\nAttempt to run anyways? \n\n(Press 'Y' to confirm)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),M_AddonExec,MM_YESNO);
							break;
						case EXT_CFG:
							M_AddonExec(KEY_ENTER);
							break;
						// else intentional fallthrough
						case EXT_LUA:
						case EXT_SOC:
						case EXT_WAD:
#ifdef USE_KART
						case EXT_KART:
#endif
						case EXT_PK3:
							if (browselocalskins)
							{
								if (DumbStartsWith("KC_", dirmenu[dir_on[menudepthleft]]+DIR_STRING) || DumbStartsWith("kc_", dirmenu[dir_on[menudepthleft]]+DIR_STRING)) {
									M_StartMessage(va("%c%s\x80\nYou are loading a local skin.\nLocal skins will not be usable\nafter going back from\nthe title screen.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),NULL,MM_NOTHING);
									COM_BufAddText(va("addfilelocal \"%s%s\"", menupath, dirmenu[dir_on[menudepthleft]]+DIR_STRING));
								}
								else
									S_StartSound(NULL, sfx_s26d);
							}
							else
							{
								COM_BufAddText(va("addfile \"%s%s\"", menupath, dirmenu[dir_on[menudepthleft]]+DIR_STRING));
							}
							break;
						default:
							S_StartSound(NULL, sfx_s26d);
					}
				}
				if (refresh)
					refreshdirmenu |= REFRESHDIR_NORMAL;
			}
			break;

		case KEY_END:
			{
				boolean refresh = true;
				if (!dirmenu[dir_on[menudepthleft]])
					S_StartSound(NULL, sfx_s26d);
				else
				{
					switch (dirmenu[dir_on[menudepthleft]][DIR_TYPE])
					{
						case EXT_FOLDER:
							M_StartMessage(va("%c%s%s\x80\nAutoloading a folder is not yet suppported. \n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), M_AddonsHeaderPath(), dirmenu[dir_on[menudepthleft]]+DIR_STRING),NULL,MM_NOTHING);
							S_StartSound(NULL, sfx_s26d);
							break;
						case EXT_UP:
							S_StartSound(NULL, sfx_s224);
							M_StartMessage(va("%c%s%s\x80\nNice try. \n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), M_AddonsHeaderPath(), dirmenu[dir_on[menudepthleft]]+DIR_STRING),NULL,MM_NOTHING);
							break;
						case EXT_TXT:
						case EXT_CFG:
							if (fastcmp(dirmenu[dir_on[menudepthleft]]+DIR_STRING, CONFIGFILENAME)
								|| fastcmp(dirmenu[dir_on[menudepthleft]]+DIR_STRING, AUTOLOADCONFIGFILENAME)
								|| fastcmp(dirmenu[dir_on[menudepthleft]]+DIR_STRING, "kartserv.cfg")
								|| fastcmp(dirmenu[dir_on[menudepthleft]]+DIR_STRING, "kartexec.cfg"))
							{
								M_StartMessage(va("%c%s\x80\nYou can't autoload this builds' base console scripts, silly!\n They're already autoloaded on startup! \n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),NULL,MM_NOTHING);
								S_StartSound(NULL, sfx_s26d);
							}
							else
								M_StartMessage(va("%c%s\x80\nYou're trying to autoload a console script. \nIgnore my warning anyways? \n\n(Press 'Y' to confirm)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),M_AddonAutoLoad,MM_YESNO);
							break;
						case EXT_LUA:
						case EXT_SOC:
						case EXT_WAD:
						case EXT_KART:
						case EXT_PK3:
							M_StartMessage(va("%c%s\x80\nYou are trying to mark an addon to autoload\nat startup. This will skip modifiedgame checks. \n\n(Press 'Y' to confirm)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),M_AddonAutoLoad,MM_YESNO);
							break;
						default:
							M_StartMessage(va("%c%s\x80\nIt may be dangerous to autoload this file. \nBut you're the boss, and I'm just hand-written code.\n Proceed? \n\n(Press 'Y' to confirm)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), dirmenu[dir_on[menudepthleft]]+DIR_STRING),M_AddonAutoLoad,MM_YESNO);
							break;
					}
				}
				if (refresh)
					refreshdirmenu |= REFRESHDIR_NORMAL;
			}
			break;

		case KEY_ESCAPE:
			exitmenu = true;
			addons_scrolltic = 0;
			break;

		default:
			break;
	}

	if (exitmenu)
	{
		closefilemenu(true);

		// Secret menu!
		//MainMenu[secrets].status = (M_AnySecretUnlocked()) ? (IT_STRING | IT_CALL) : (IT_DISABLED);

		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// ---- REPLAY HUT -----
menudemo_t *demolist = NULL; // Replays that that have been checked to match with query

// Locked behind Lock_search_state
menudemo_t *demolist_all = NULL; // All replays
size_t demolist_all_size = 0;
boolean replaynamesloaded = false;

#ifdef HAVE_THREADS
I_mutex replayquerymutex;

// I_In_Exiting_Signal_Handler is an evil hack
// to avoid infinite SIGABRT recursion in the signal handler
// due to poisoned locks or mach-o kernel not supporting locks in signals
// or something like that. idk
#  define Lock_search_state()    if (!I_In_Exiting_Signal_Handler()) { I_lock_mutex(&replayquerymutex); }
#  define Unlock_search_state()  if (!I_In_Exiting_Signal_Handler()) { I_unlock_mutex(replayquerymutex); }
#else /*HAVE_THREADS*/
#  define Lock_search_state()
#  define Unlock_search_state()
#endif /*HAVE_THREADS*/


#define MAXREPLAYQUERY 37
static char replayqueryinput_buffer[MAXREPLAYQUERY+1]; // The input typed
static textinput_t replayqueryinput;

static size_t replayqueryfound = 0; // Number of checked replay entries
static size_t replayquerycheck = 0; // Index of next replay entry to check in demolist_all

#define DF_ENCORE       0x40
static INT16 replayScrollTitle = 0;
static SINT8 replayScrollDelay = TICRATE, replayScrollDir = 1;

static void ReplayNamesLoadThread(void* userdata)
{
	Lock_search_state();

	size_t demolist_all_size_local = demolist_all_size;
	menudemo_t *demolist_all_local = (menudemo_t*)malloc(sizeof(menudemo_t)*demolist_all_size_local);
	memcpy(demolist_all_local, demolist_all, sizeof(menudemo_t)*demolist_all_size_local);
	char *replaydirpath = (char*)userdata;

	Unlock_search_state();

	for (size_t i = 0; i < demolist_all_size_local; ++i)
	{
		if (demolist_all_local[i].type != MD_SUBDIR)
		{
			G_LoadDemoTitle(&demolist_all_local[i]);

			// this is fucking horrid, but i wanna be able to search for dates
			/*if (demolist_all_local[i].date[0] != '\0')
			{
				snprintf(demolist_all_local[i].title, sizeof(demolist_all_local[i].title), "%s %s", demolist_all_local[i].title, demolist_all_local[i].date);
			}*/
		}
	}

	Lock_search_state();

	if (fastcmp(menupath, replaydirpath) && demolist_all)
	{
		memcpy(demolist_all, demolist_all_local, sizeof(menudemo_t)*demolist_all_size);
		replaynamesloaded = true;
	}

	Unlock_search_state();

	// Was allocated before thread start, we need to free it
	free(replaydirpath);
	free(demolist_all_local);
}

static void LoadReplayNames(void)
{
	char *replaydirpath = strdup(menupath);

	Lock_search_state();
	replaynamesloaded = false;
	Unlock_search_state();

#ifdef HAVE_THREADS
	I_spawn_thread("replay-names-load", ReplayNamesLoadThread, replaydirpath);
#else
	ReplayNamesLoadThread(replaydirpath);
#endif
}

static void ResetReplayQuery(void)
{
	memset(replayqueryinput_buffer, 0, MAXREPLAYQUERY);
	M_TextInputInit(&replayqueryinput, replayqueryinput_buffer, MAXREPLAYQUERY);

	replayqueryfound = 0;
	replayquerycheck = 0;
}

static void AddCheckedReplay(void)
{
	memcpy(&demolist[replayqueryfound], &demolist_all[replayquerycheck], sizeof(menudemo_t));
	++replayqueryfound;
	++replayquerycheck;
}

// Check up to maxnum replays if they match with query
static void M_HutCheckReplays(size_t maxnum)
{
	// Stripped of color codes
	char demo_title[sizeof(demolist_all[0].title)];

	if (!replaynamesloaded)
		return;

	// Already checked everything
	if (replayquerycheck == sizedirmenu)
		return;

	// If we don't have any query, just copy all demos and consider them checked
	if (replayqueryinput.length == 0)
	{
		// Mark as done
		replayqueryfound = replayquerycheck = sizedirmenu;

		memcpy(demolist, demolist_all, sizeof(menudemo_t) * sizedirmenu);

		return;
	}

	while (maxnum-- && replayquerycheck < sizedirmenu)
	{
		switch (demolist_all[replayquerycheck].type)
		{
			case MD_SUBDIR:
				AddCheckedReplay(); // Just add subdirs
				// Will also be decremented on next iteration. Basically, we just don't want to
				// spend our number of tries on subdirs, since we don't check anything for them
				++maxnum;
				break;

			case MD_NOTLOADED:
			case MD_OUTDATED:
			case MD_LOADED:
				StripColors(demo_title, demolist_all[replayquerycheck].title, sizeof(demo_title));

				if (demolist_all[replayquerycheck].title[0] && strcasestr(demo_title, replayqueryinput.buffer) != NULL)
					AddCheckedReplay(); // It matches, add it!
				else
					replayquerycheck++; // Doesn't match, moving on...

				break;

			// Don't show invalid replays
			case MD_INVALID:
				replayquerycheck++;
		}
	}
}

static int ReplayListSortComparator(const void *entry1, const void *entry2)
{
	const menudemo_t *demo1 = (const menudemo_t*)entry1;
	const menudemo_t *demo2 = (const menudemo_t*)entry2;

	char filepath1[sizeof(demo1->filepath)];
	char filepath2[sizeof(demo2->filepath)];

	// First check for directories, they always should be at the top
	if (demo1->type == MD_SUBDIR && demo2->type != MD_SUBDIR)
		return -1;
	else if (demo2->type == MD_SUBDIR && demo1->type != MD_SUBDIR)
		return 1;
	else if (demo1->type == MD_SUBDIR && demo2->type == MD_SUBDIR)
		return 0;

	memcpy(filepath1, demo1->filepath, sizeof(filepath1));
	memcpy(filepath2, demo2->filepath, sizeof(filepath2));

	nameonly(filepath1);
	nameonly(filepath2);

	// Comparing in opposite order to move new replays to the top
	return strncmp(filepath2, filepath1, sizeof(filepath1));
}

static void PrepReplayList(boolean reset)
{
	size_t i;

	replayquerycheck = replayqueryfound = 0;

	Z_Free(demolist);
	demolist = Z_Calloc(sizeof(menudemo_t) * sizedirmenu, PU_STATIC, NULL);

	// If directory didn't change, keep demolist_all
	if (!reset)
		return;

	Lock_search_state();

	Z_Free(demolist_all);
	demolist_all = Z_Calloc(sizeof(menudemo_t) * sizedirmenu, PU_STATIC, NULL);
	demolist_all_size = sizedirmenu;

	for (i = 0; i < sizedirmenu; i++)
	{
		if (dirmenu[i][DIR_TYPE] == EXT_UP)
		{
			demolist_all[i].type = MD_SUBDIR;
			sprintf(demolist_all[i].title, "UP");
		}
		else if (dirmenu[i][DIR_TYPE] == EXT_FOLDER)
		{
			demolist_all[i].type = MD_SUBDIR;
			strncpy(demolist_all[i].title, dirmenu[i] + DIR_STRING, 64);
		}
		else
		{
			demolist_all[i].type = MD_NOTLOADED;
			snprintf(demolist_all[i].filepath, sizeof(demolist_all[i].filepath),
					 // 255 = UINT8 limit. dirmenu entries are restricted to this length (see DIR_LEN).
					 "%s%.255s", menupath, dirmenu[i] + DIR_STRING);
			sprintf(demolist_all[i].title, ".....");
		}
	}

	qs22j(demolist_all, sizedirmenu, sizeof(menudemo_t), ReplayListSortComparator);

	Unlock_search_state();

	LoadReplayNames();
}

void M_ReplayHut(INT32 choice)
{
	(void)choice;

	if (!demo.inreplayhut)
	{
		snprintf(menupath, 1024, "%s"PATHSEP"replay"PATHSEP"online"PATHSEP, srb2home);
		menupathindex[(menudepthleft = menudepth-1)] = strlen(menupath);
		ResetReplayQuery();
	}

	if (!preparefilemenu(false, true))
	{
		M_StartMessage("No replays found.\n\n(Press a key)\n", NULL, MM_NOTHING);
		return;
	}
	else if (!demo.inreplayhut)
		dir_on[menudepthleft] = 0;

	demo.inreplayhut = true;

	replayScrollTitle = 0; replayScrollDelay = TICRATE; replayScrollDir = 1;

	PrepReplayList(true);

	menuactive = true;
	M_SetupNextMenu(&MISC_ReplayHutDef);
	G_SetGamestate(GS_TIMEATTACK);

	demo.rewinding = false;
	CL_ClearRewinds();

	S_ChangeMusicInternal("replst", true);
}

static boolean M_HandleReplayHutQuery(INT32 choice)
{
	// Yea gonna copy buffer and check if it changed, thats better than checking for specific keys i think
	char tmp[MAXREPLAYQUERY+1];
	memcpy(tmp, replayqueryinput_buffer, MAXREPLAYQUERY+1);

	if (M_TextInputHandle(&replayqueryinput, choice))
	{
		S_StartSound(NULL, sfx_menu1);

		// Restart search only if we actually modified input and not just moved in it
		if (memcmp(tmp, replayqueryinput_buffer, MAXREPLAYQUERY+1))
		{
			preparefilemenu(false, true);
			dir_on[menudepthleft] = 0;
			PrepReplayList(false);
		}

		return true;
	}

	return false;
}

static void M_HandleReplayHutList(INT32 choice)
{
	if (M_HandleReplayHutQuery(choice))
		return;

	size_t scrollamt = 1;

	switch (choice)
	{
	case KEY_PGUP:
		scrollamt = 8;
		/* FALLTHRU */
	case KEY_UPARROW:
		if (!replaynamesloaded)
			return;

		if (dir_on[menudepthleft])
			dir_on[menudepthleft] -= min(dir_on[menudepthleft], scrollamt);
		else
			return;
			//M_PrevOpt();

		S_StartSound(NULL, sfx_menu1);
		replayScrollTitle = 0; replayScrollDelay = TICRATE; replayScrollDir = 1;
		break;

	case KEY_PGDN:
		scrollamt = 8;
		/* FALLTHRU */
	case KEY_DOWNARROW:
		if (!replaynamesloaded)
			return;

		if (dir_on[menudepthleft] < replayqueryfound-1)
			dir_on[menudepthleft] = min(replayqueryfound-1, dir_on[menudepthleft] + scrollamt);
		else
			return;
			//itemOn = 0; // Not M_NextOpt because that would take us to the extra dummy item

		S_StartSound(NULL, sfx_menu1);
		replayScrollTitle = 0; replayScrollDelay = TICRATE; replayScrollDir = 1;
		break;

	case KEY_ESCAPE:
		M_QuitReplayHut();
		break;

	case KEY_ENTER:
		if (!replaynamesloaded)
			return;

		if (replayqueryfound == 0)
			return;

		switch (dirmenu[dir_on[menudepthleft]][DIR_TYPE])
		{
			case EXT_FOLDER:
				strcpy(&menupath[menupathindex[menudepthleft]],dirmenu[dir_on[menudepthleft]]+DIR_STRING);
				if (menudepthleft)
				{
					menupathindex[--menudepthleft] = strlen(menupath);
					menupath[menupathindex[menudepthleft]] = 0;

					if (!preparefilemenu(false, true))
					{
						S_StartSound(NULL, sfx_s224);
						M_StartMessage(va("%c%s\x80\nThis folder is empty.\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), M_AddonsHeaderPath()),NULL,MM_NOTHING);
						menupath[menupathindex[++menudepthleft]] = 0;

						if (!preparefilemenu(true, true))
						{
							M_QuitReplayHut();
							return;
						}
					}
					else
					{
						S_StartSound(NULL, sfx_menu1);
						dir_on[menudepthleft] = 1;
						ResetReplayQuery();
						PrepReplayList(true);
					}
				}
				else
				{
					S_StartSound(NULL, sfx_s26d);
					M_StartMessage(va("%c%s\x80\nThis folder is too deep to navigate to!\n\n(Press a key)\n", ('\x80' + (highlightflags>>V_CHARCOLORSHIFT)), M_AddonsHeaderPath()),NULL,MM_NOTHING);
					menupath[menupathindex[menudepthleft]] = 0;
				}
				break;
			case EXT_UP:
				S_StartSound(NULL, sfx_menu1);
				menupath[menupathindex[++menudepthleft]] = 0;
				if (!preparefilemenu(false, true))
				{
					M_QuitReplayHut();
					return;
				}
				PrepReplayList(true);
				break;
			default:
				// We can't just use M_SetupNextMenu because that'll run ReplayDef's quitroutine and boot us back to the title screen!
				currentMenu->lastOn = itemOn;
				currentMenu = &MISC_ReplayStartDef;

				replayScrollTitle = 0; replayScrollDelay = TICRATE; replayScrollDir = 1;

				switch (demolist[dir_on[menudepthleft]].addonstatus)
				{
				case DFILE_ERROR_CANNOTLOAD:
					// Only show "Watch Replay Without Addons"
					MISC_ReplayStartMenu[0].status = IT_DISABLED;
					MISC_ReplayStartMenu[1].status = IT_CALL|IT_STRING;
					//MISC_ReplayStartMenu[1].alphaKey = 0;
					MISC_ReplayStartMenu[2].status = IT_DISABLED;
					itemOn = 1;
					break;

				case DFILE_ERROR_NOTLOADED:
				case DFILE_ERROR_INCOMPLETEOUTOFORDER:
					// Show "Load Addons and Watch Replay" and "Watch Replay Without Addons"
					MISC_ReplayStartMenu[0].status = IT_CALL|IT_STRING;
					MISC_ReplayStartMenu[1].status = IT_CALL|IT_STRING;
					//MISC_ReplayStartMenu[1].alphaKey = 10;
					MISC_ReplayStartMenu[2].status = IT_DISABLED;
					itemOn = 0;
					break;

				case DFILE_ERROR_EXTRAFILES:
				case DFILE_ERROR_OUTOFORDER:
				default:
					// Show "Watch Replay"
					MISC_ReplayStartMenu[0].status = IT_DISABLED;
					MISC_ReplayStartMenu[1].status = IT_DISABLED;
					MISC_ReplayStartMenu[2].status = IT_CALL|IT_STRING;
					//MISC_ReplayStartMenu[2].alphaKey = 0;
					itemOn = 2;
					break;
				}
		}

		break;
	}
}

static void DrawReplayHutReplayInfo(void)
{
	lumpnum_t lumpnum;
	patch_t *patch;
	UINT8 *colormap;
	INT32 x, y, w, h;

	const menudemo_t *replaydemolist = &demolist[dir_on[menudepthleft]];

	switch (replaydemolist->type)
	{
		case MD_NOTLOADED:
			V_DrawCenteredString(160, 40, V_SNAPTOTOP|MENUCAPS, "Loading replay information...");
			break;

		case MD_INVALID:
			V_DrawCenteredString(160, 40, V_SNAPTOTOP|warningflags|MENUCAPS, "This replay cannot be played.");
			break;

		case MD_SUBDIR:
			break; // Can't think of anything to draw here right now

		case MD_OUTDATED:
			V_DrawThinString(17, 64, V_SNAPTOTOP|V_ALLOWLOWERCASE|V_TRANSLUCENT|highlightflags, va("Recorded on an outdated version. %s", replaydemolist->version));
			/* FALLTHRU */
		default:
			// Draw level stuff
			x = 15; y = 15;

			//  A 160x100 image of the level as entry MAPxxP
			//CONS_Printf("%d %s\n", replaydemolist->map, G_BuildMapName(replaydemolist->map));
			lumpnum = W_CheckNumForName(va("%sP", G_BuildMapName(replaydemolist->map)));
			if (lumpnum != LUMPERROR)
				patch = (patch_t *)W_CachePatchNum(lumpnum, PU_PATCH);
			else
				patch = (patch_t *)W_CachePatchName("M_NOLVL", PU_PATCH);

			if (!(replaydemolist->kartspeed & DF_ENCORE))
				V_DrawSmallScaledPatch(x, y, V_SNAPTOTOP, patch);
			else
			{
				w = patch->width;
				h = patch->height;
				V_DrawSmallScaledPatch(x+(w>>1), y, V_SNAPTOTOP|V_FLIP, patch);

				{
					static angle_t rubyfloattime = 0;
					const fixed_t rubyheight = FINESINE(rubyfloattime>>ANGLETOFINESHIFT);
					V_DrawFixedPatch((x+(w>>2))<<FRACBITS, ((y+(h>>2))<<FRACBITS) - (rubyheight<<1), FRACUNIT, V_SNAPTOTOP, (patch_t *)W_CachePatchName("RUBYICON", PU_PATCH), NULL);
					rubyfloattime += FixedMul(ANGLE_MAX/NEWTICRATE, renderdeltatics);
				}
			}

			x += 85;

			if (mapheaderinfo[replaydemolist->map-1])
			{
				char *title = G_BuildMapTitle(replaydemolist->map);
				if (title)
				{
					V_DrawString(x, y, V_SNAPTOTOP|MENUCAPS, title);
					Z_Free(title);
				}
			}
			else
				V_DrawString(x, y, V_SNAPTOTOP|V_ALLOWLOWERCASE|V_TRANSLUCENT, "Level is not loaded.");

			INT32 datew = 0;

			if (replaydemolist->date[0] != '\0')
			{
				datew = V_StringWidth(replaydemolist->date, 0);
				V_DrawThinString(x, y+9, V_SNAPTOTOP|V_ALLOWLOWERCASE, va("%s", replaydemolist->date));
			}

			if (replaydemolist->numlaps)
				V_DrawThinString(x+datew, y+9, V_SNAPTOTOP|V_ALLOWLOWERCASE, va("(%d laps)", replaydemolist->numlaps));

			V_DrawString(x, y+20, V_SNAPTOTOP|V_ALLOWLOWERCASE, replaydemolist->gametype == GT_RACE ?
				va("Race (%s speed)", kartspeed_cons_t[replaydemolist->kartspeed & ~DF_ENCORE].strvalue) :
				"Battle Mode");

			if (!replaydemolist->standings[0].ranking)
			{
				// No standings were loaded!
				V_DrawString(x, y+39, V_SNAPTOTOP|V_ALLOWLOWERCASE|V_TRANSLUCENT, "No standings available.");
				break;
			}

			V_DrawThinString(x, y+29, V_SNAPTOTOP|highlightflags|MENUCAPS, "Winner");
			V_DrawString(x+38, y+30, V_SNAPTOTOP|V_ALLOWLOWERCASE, replaydemolist->standings[0].name);

			if (replaydemolist->gametype == GT_RACE)
			{
				V_DrawThinString(x, y+39, V_SNAPTOTOP|highlightflags|MENUCAPS, "Time");
			}
			else
			{
				V_DrawThinString(x, y+39, V_SNAPTOTOP|highlightflags|MENUCAPS, "Score");
			}

			const UINT32 timeorscore = replaydemolist->standings[0].timeorscore;

			if (timeorscore == (UINT32_MAX-1))
			{
				V_DrawThinString(x+32, y+40-1, V_SNAPTOTOP|MENUCAPS, "No Contest");
			}
			else if (replaydemolist->gametype == GT_RACE)
			{
				V_DrawRightAlignedString(x+84, y+40, V_SNAPTOTOP, va("%d'%02d\"%02d",
												G_TicsToMinutes(timeorscore, true),
												G_TicsToSeconds(timeorscore),
												G_TicsToCentiseconds(timeorscore)
				));
			}
			else
			{
				V_DrawString(x+32, y+40, V_SNAPTOTOP, va("%d", timeorscore));
			}

			// Character face!
			if (replaydemolist->standings[0].skin < numskins && W_CheckNumForName(skins[replaydemolist->standings[0].skin].facewant) != LUMPERROR)
			{
				patch = facewantprefix[replaydemolist->standings[0].skin];
				colormap = R_GetTranslationColormap(
					replaydemolist->standings[0].skin,
					replaydemolist->standings[0].color,
					GTC_MENUCACHE);
			}
			else
			{
				patch = (patch_t *)W_CachePatchName("M_NOWANT", PU_PATCH);
				colormap = R_GetTranslationColormap(
					TC_RAINBOW,
					replaydemolist->standings[0].color,
					GTC_MENUCACHE);
			}

			V_DrawMappedPatch(BASEVIDWIDTH-15 - patch->width, y+20, V_SNAPTOTOP, patch, colormap);

			break;
	}
}

static void M_DrawReplayHut(void)
{
	INT32 x, y, cursory = 0;
	INT32 i;
	INT16 replaylistitem = currentMenu->numitems-2;
	boolean processed_one_this_frame = false;
	const INT32 scaledviewheight = (vid.height/vid.dup);

	static UINT16 replayhutmenuy = 0;

	V_DrawPatchFill(srb2back);

	if (cv_vhseffect.value)
		V_DrawVhsEffect(false);

	// Draw menu choices
	x = currentMenu->x;
	y = currentMenu->y;

	if (itemOn > replaylistitem)
	{
		itemOn = replaylistitem;
		dir_on[menudepthleft] = replayqueryfound-1;
		replayScrollTitle = 0; replayScrollDelay = TICRATE; replayScrollDir = 1;
	}
	else if (itemOn < replaylistitem)
	{
		dir_on[menudepthleft] = 0;
		replayScrollTitle = 0; replayScrollDelay = TICRATE; replayScrollDir = 1;
	}

	if (itemOn == replaylistitem)
	{
		INT32 maxy;
		// Scroll menu items if needed
		cursory = y + currentMenu->menuitems[replaylistitem].alphaKey + dir_on[menudepthleft]*10;
		maxy = y + currentMenu->menuitems[replaylistitem].alphaKey + replayqueryfound*10;

		if (cursory > maxy - 20)
			cursory = maxy - 20;

		if (cursory - replayhutmenuy > scaledviewheight-50)
			replayhutmenuy += (cursory-scaledviewheight-replayhutmenuy + 51)/2;
		else if (cursory - replayhutmenuy < 110)
			replayhutmenuy += (max(0, cursory-110)-replayhutmenuy - 1)/2;
	}
	else
		replayhutmenuy /= 2;

	y -= replayhutmenuy;

	// Draw static menu items
	for (i = 0; i < replaylistitem; i++)
	{
		INT32 localy = y + currentMenu->menuitems[i].alphaKey;

		if (localy < 65)
			continue;

		if (i == itemOn)
			cursory = localy;

		if ((currentMenu->menuitems[i].status & IT_DISPLAY) == IT_STRING)
			V_DrawString(x, localy, V_SNAPTOTOP|V_SNAPTOLEFT, currentMenu->menuitems[i].text);
		else
			V_DrawString(x, localy, V_SNAPTOTOP|V_SNAPTOLEFT|highlightflags, currentMenu->menuitems[i].text);
	}

	y += currentMenu->menuitems[replaylistitem].alphaKey;

	if (!replaynamesloaded)
	{
		const char *msg = "Loading replays, please wait...";
		V_DrawCenteredString(160, 100, V_ALLOWLOWERCASE, msg);
	}

	for (i = 0; i < (INT32)replayqueryfound; i++)
	{
		INT32 localy = y+i*10;
		INT32 localx = x;

		if (localy < 65)
			continue;
		if (localy >= scaledviewheight - 24)
			break;

		if (demolist[i].type == MD_NOTLOADED && !processed_one_this_frame)
		{
			processed_one_this_frame = true;
			G_LoadDemoInfo(&demolist[i]);
		}

		if (demolist[i].type == MD_SUBDIR)
		{
			localx += 8;
			V_DrawScaledPatch(x - 4, localy, V_SNAPTOTOP|V_SNAPTOLEFT, (patch_t *)W_CachePatchName(dirmenu[i][DIR_TYPE] == EXT_UP ? "M_RBACK" : "M_RFLDR", PU_PATCH));
		}

		if (itemOn == replaylistitem && i == (INT16)dir_on[menudepthleft])
		{
			cursory = localy;

			if (!interpTimerHackAllow)
				;
			else if (replayScrollDelay)
				replayScrollDelay--;
			else if (replayScrollDir > 0)
			{
				if (replayScrollTitle < (V_StringWidth(demolist[i].title, 0) - (vid.scaledwidth - (x<<1)))<<1)
					replayScrollTitle++;
				else
				{
					replayScrollDelay = TICRATE;
					replayScrollDir = -1;
				}
			}
			else
			{
				if (replayScrollTitle > 0)
					replayScrollTitle--;
				else
				{
					replayScrollDelay = TICRATE;
					replayScrollDir = 1;
				}
			}

			V_DrawString(localx - (replayScrollTitle>>1), localy, V_SNAPTOTOP|V_SNAPTOLEFT|highlightflags|V_ALLOWLOWERCASE, demolist[i].title);
		}
		else
			V_DrawString(localx, localy, V_SNAPTOTOP|V_SNAPTOLEFT|V_ALLOWLOWERCASE, demolist[i].title);
	}

	// Draw scrollbar
	y = replayqueryfound*10 + currentMenu->menuitems[replaylistitem].alphaKey + 30;
	if (y > scaledviewheight-80)
	{
		V_DrawFill(BASEVIDWIDTH-4, 75, 4, scaledviewheight-80, V_SNAPTOTOP|V_SNAPTORIGHT|239);
		V_DrawFill(BASEVIDWIDTH-3, 76 + (scaledviewheight-80) * replayhutmenuy / y, 2, max((((scaledviewheight-80) * (scaledviewheight-80))-1) / y - 1, 1), V_SNAPTOTOP|V_SNAPTORIGHT|229);
	}

	// Draw the cursor
	V_DrawScaledPatch(currentMenu->x - 24, cursory, V_SNAPTOTOP|V_SNAPTOLEFT, (patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
	V_DrawString(currentMenu->x, cursory, V_SNAPTOTOP|V_SNAPTOLEFT|highlightflags, currentMenu->menuitems[itemOn].text);

	// Now draw some replay info!
	V_DrawFill(10, 10, 300, 60, V_SNAPTOTOP|239);

	if (itemOn == replaylistitem)
	{
		DrawReplayHutReplayInfo();
	}

	x = 4;

	// Draw search query
	M_DrawTextBoxFlags(x, 200 - 22, MAXREPLAYQUERY, 1, V_SNAPTOBOTTOM);

	if (replayqueryinput.length)
		M_DrawTextInput(x + 8, 200 - 14, &replayqueryinput, V_SNAPTOBOTTOM);
	else
		V_DrawString(x + 8, 200 - 14, V_ALLOWLOWERCASE|V_SNAPTOBOTTOM, "\x86Type to search...");

	if (replaynamesloaded && replayquerycheck < sizedirmenu)
		V_DrawString(x + 8, 200 - 30, V_ALLOWLOWERCASE|V_SNAPTOBOTTOM, va("Searching %u/%u...", (unsigned)replayquerycheck, (unsigned)sizedirmenu));
}

static void M_DrawReplayStartMenu(void)
{
	const char *warning = "";
	UINT8 i;

	M_DrawGenericBackgroundMenu();

	const menudemo_t *replaydemolist = &demolist[dir_on[menudepthleft]];

#define STARTY 62-(replayScrollTitle>>1)
	// Draw rankings beyond first
	for (i = 1; i < MAXPLAYERS && replaydemolist->standings[i].ranking; i++)
	{
		patch_t *patch;
		UINT8 *colormap;

		V_DrawRightAlignedString(BASEVIDWIDTH-100, STARTY + i*20, V_SNAPTOTOP|highlightflags, va("%2d", replaydemolist->standings[i].ranking));
		V_DrawThinString(BASEVIDWIDTH-96, STARTY + i*20, V_SNAPTOTOP|V_ALLOWLOWERCASE, replaydemolist->standings[i].name);

		const UINT32 timeorscore = replaydemolist->standings[i].timeorscore;

		if (timeorscore == UINT32_MAX-1)
			V_DrawThinString(BASEVIDWIDTH-92, STARTY + i*20 + 9, V_SNAPTOTOP, "NO CONTEST");
		else if (replaydemolist->gametype == GT_RACE)
			V_DrawRightAlignedString(BASEVIDWIDTH-40, STARTY + i*20 + 9, V_SNAPTOTOP, va("%d'%02d\"%02d",
											G_TicsToMinutes(timeorscore, true),
											G_TicsToSeconds(timeorscore),
											G_TicsToCentiseconds(timeorscore)
			));
		else
			V_DrawString(BASEVIDWIDTH-92, STARTY + i*20 + 9, V_SNAPTOTOP, va("%d", timeorscore));

		// Character face!
		if (replaydemolist->standings[i].skin < numskins && W_CheckNumForName(skins[replaydemolist->standings[i].skin].facerank) != LUMPERROR)
		{
			patch = facerankprefix[replaydemolist->standings[i].skin];
			colormap = R_GetTranslationColormap(
				replaydemolist->standings[i].skin,
				replaydemolist->standings[i].color,
				GTC_MENUCACHE);
		}
		else
		{
			patch = (patch_t *)W_CachePatchName("M_NORANK", PU_PATCH);
			colormap = R_GetTranslationColormap(
				TC_RAINBOW,
				replaydemolist->standings[i].color,
				GTC_MENUCACHE);
		}

		V_DrawMappedPatch(BASEVIDWIDTH-5 - patch->width, STARTY + i*20, V_SNAPTOTOP, patch, colormap);
	}
#undef STARTY

	// Handle scrolling rankings
	if (!interpTimerHackAllow)
		;
	else if (replayScrollDelay)
		replayScrollDelay--;
	else if (replayScrollDir > 0)
	{
		if (replayScrollTitle < (i*20 - (vid.height/vid.dup) + 100)<<1)
			replayScrollTitle++;
		else
		{
			replayScrollDelay = TICRATE;
			replayScrollDir = -1;
		}
	}
	else
	{
		if (replayScrollTitle > 0)
			replayScrollTitle--;
		else
		{
			replayScrollDelay = TICRATE;
			replayScrollDir = 1;
		}
	}

	V_DrawFill(10, 10, 300, 60, V_SNAPTOTOP|239);
	DrawReplayHutReplayInfo();

	V_DrawString(10, 72, V_SNAPTOTOP|highlightflags|V_ALLOWLOWERCASE, replaydemolist->title);

	// Draw a warning prompt if needed
	switch (replaydemolist->addonstatus)
	{
		case DFILE_ERROR_CANNOTLOAD:
			warning = "Some addons in this replay cannot be loaded.\nYou can watch anyway, but desyncs may occur.";
			break;

		case DFILE_ERROR_NOTLOADED:
		case DFILE_ERROR_INCOMPLETEOUTOFORDER:
			warning = "Loading addons will mark your game as modified, and Record Attack may be unavailable.\nYou can watch without loading addons, but desyncs may occur.";
			break;

		case DFILE_ERROR_EXTRAFILES:
			warning = "You have addons loaded that were not present in this replay.\nYou can watch anyway, but desyncs may occur.";
			break;

		case DFILE_ERROR_OUTOFORDER:
			warning = "You have this replay's addons loaded, but they are out of order.\nYou can watch anyway, but desyncs may occur.";
			break;

		default:
			return;
	}

	if (warning)
		V_DrawSmallString(4, BASEVIDHEIGHT-14, V_SNAPTOBOTTOM|V_SNAPTOLEFT|V_ALLOWLOWERCASE, warning);
}

void M_ResetDemoList(void)
{
	Lock_search_state();
	replaynamesloaded = false;

	Z_Free(demolist_all);
	demolist_all = NULL;

	Z_Free(demolist);
	demolist = NULL;

	demo.inreplayhut = false;

	Unlock_search_state();
}

static boolean M_QuitReplayHut(void)
{
	// D_StartTitle does its own wipe, since GS_TIMEATTACK is now a complete gamestate.
	menuactive = false;
	D_StartTitle();
	M_ResetDemoList();

	return true;
}

// same as M_QuitReplayHut, but calls M_StopMessage
void M_ReturnToTitleFromError(void)
{
	M_StopMessage(0);
	// D_StartTitle does its own wipe, since GS_TIMEATTACK is now a complete gamestate.
	menuactive = false;
	D_StartTitle();
	M_ResetDemoList();
}

static void M_HutStartReplay(INT32 choice)
{
	(void)choice;

	M_ClearMenus(false);
	demo.loadfiles = (itemOn == 0);
	demo.ignorefiles = (itemOn != 0);

	G_DoPlayDemo(demolist[dir_on[menudepthleft]].filepath);
}

void M_SetPlaybackMenuPointer(void)
{
	itemOn = playback_pause;
}

static void M_DrawPlaybackMenu(void)
{
	INT16 i;
	patch_t *icon;
	UINT8 *activemap = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_GOLD, GTC_MENUCACHE);
	UINT32 transmap = max(0, (INT32)(leveltime - playback_last_menu_interaction_leveltime - 4*TICRATE)) / 5;
	transmap = min(8, transmap) << V_ALPHASHIFT;

	if (leveltime - playback_last_menu_interaction_leveltime >= 6*TICRATE)
		playback_last_menu_interaction_leveltime = leveltime - 6*TICRATE;

	// Toggle items
	if (paused && !demo.rewinding)
	{
		PlaybackMenu[playback_pause].status = PlaybackMenu[playback_fastforward].status = PlaybackMenu[playback_rewind].status = IT_DISABLED;
		PlaybackMenu[playback_resume].status = PlaybackMenu[playback_advanceframe].status = PlaybackMenu[playback_backframe].status = IT_CALL|IT_STRING;

		if (itemOn >= playback_rewind && itemOn <= playback_fastforward)
			itemOn += playback_backframe - playback_rewind;
	}
	else
	{
		PlaybackMenu[playback_pause].status = PlaybackMenu[playback_fastforward].status = PlaybackMenu[playback_rewind].status = IT_CALL|IT_STRING;
		PlaybackMenu[playback_resume].status = PlaybackMenu[playback_advanceframe].status = PlaybackMenu[playback_backframe].status = IT_DISABLED;

		if (itemOn >= playback_backframe && itemOn <= playback_advanceframe)
			itemOn -= playback_backframe - playback_rewind;
	}

	if (modeattacking)
	{
		for (i = playback_viewcount; i <= playback_view4; i++)
			PlaybackMenu[i].status = IT_DISABLED;
		PlaybackMenu[playback_freecamera].alphaKey = 72;
		PlaybackMenu[playback_quit].alphaKey = 88;

		currentMenu->x = BASEVIDWIDTH/2 - 52;
	}
	else
	{
		PlaybackMenu[playback_viewcount].status = IT_ARROWS|IT_STRING;

		for (i = 0; i <= splitscreen; i++)
			PlaybackMenu[playback_view1+i].status = IT_ARROWS|IT_STRING;
		for (i = splitscreen+1; i < 4; i++)
			PlaybackMenu[playback_view1+i].status = IT_DISABLED;

		PlaybackMenu[playback_freecamera].alphaKey = 156;
		PlaybackMenu[playback_quit].alphaKey = 172;
		currentMenu->x = BASEVIDWIDTH/2 - 88;
	}

	for (i = 0; i < currentMenu->numitems; i++)
	{
		UINT8 *inactivemap = NULL;

		if (i >= playback_view1 && i <= playback_view4)
		{
			if (modeattacking) continue;

			if (splitscreen >= i - playback_view1)
			{
				INT32 ply = displayplayers[i - playback_view1];

				icon = facerankprefix[players[ply].skin];
				if (i != itemOn)
					inactivemap = R_GetTranslationColormap(players[ply].skin, players[ply].skincolor, GTC_MENUCACHE);
			}
			else if (currentMenu->menuitems[i].patch && W_CheckNumForName(currentMenu->menuitems[i].patch) != LUMPERROR)
				icon = (patch_t *)W_CachePatchName(currentMenu->menuitems[i].patch, PU_PATCH);
			else
				icon = (patch_t *)W_CachePatchName("PLAYRANK", PU_PATCH); // temp
		}
		else if (currentMenu->menuitems[i].status == IT_DISABLED)
			continue;
		else if (currentMenu->menuitems[i].patch && W_CheckNumForName(currentMenu->menuitems[i].patch) != LUMPERROR)
			icon = (patch_t *)W_CachePatchName(currentMenu->menuitems[i].patch, PU_PATCH);
		else
			icon = (patch_t *)W_CachePatchName("PLAYRANK", PU_PATCH); // temp

		if ((i == playback_fastforward && cv_playbackspeed.value > 1) || (i == playback_rewind && demo.rewinding))
			V_DrawMappedPatch(currentMenu->x + currentMenu->menuitems[i].alphaKey, currentMenu->y, transmap|V_SNAPTOTOP, icon, R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_JAWZ, GTC_MENUCACHE));
		else
			V_DrawMappedPatch(currentMenu->x + currentMenu->menuitems[i].alphaKey, currentMenu->y, transmap|V_SNAPTOTOP, icon, (i == itemOn) ? activemap : inactivemap);

		if (i == itemOn)
		{
			V_DrawCharacter(currentMenu->x + currentMenu->menuitems[i].alphaKey + 4, currentMenu->y + 14,
				'\x1A' | transmap|V_SNAPTOTOP|highlightflags, false);

			V_DrawCenteredString(BASEVIDWIDTH/2, currentMenu->y + 18, transmap|V_SNAPTOTOP|V_ALLOWLOWERCASE, currentMenu->menuitems[i].text);

			if ((currentMenu->menuitems[i].status & IT_TYPE) == IT_ARROWS)
			{
				char *str;

				if (!(i == playback_viewcount && splitscreen == 3))
					V_DrawCharacter(BASEVIDWIDTH/2 - 4, currentMenu->y + 28 - (skullAnimCounter/5),
						'\x1A' | transmap|V_SNAPTOTOP|highlightflags, false); // up arrow

				if (!(i == playback_viewcount && splitscreen == 0))
					V_DrawCharacter(BASEVIDWIDTH/2 - 4, currentMenu->y + 48 + (skullAnimCounter/5),
						'\x1B' | transmap|V_SNAPTOTOP|highlightflags, false); // down arrow

				switch (i)
				{
				case playback_viewcount:
					str = va("%d", splitscreen+1);
					break;

				case playback_view1:
				case playback_view2:
				case playback_view3:
				case playback_view4:
					str = player_names[displayplayers[i - playback_view1]]; // 0 to 3
					break;

				default: // shouldn't ever be reached but whatever
					continue;
				}

				V_DrawCenteredString(BASEVIDWIDTH/2, currentMenu->y + 38, transmap|V_SNAPTOTOP|V_ALLOWLOWERCASE|highlightflags, str);
			}
		}
	}
}

static void M_PlaybackRewind(INT32 choice)
{
	static tic_t lastconfirmtime;

	(void)choice;

	if (!demo.rewinding)
	{
		if (paused)
		{
			G_ConfirmRewind(leveltime-1);
			paused = true;
			S_PauseAudio();
		}
		else
		{
			demo.rewinding = true;
			paused = true;
		}
	}
	else if (lastconfirmtime + TICRATE/2 < I_GetTime())
	{
		lastconfirmtime = I_GetTime();
		G_ConfirmRewind(leveltime);
	}

	CV_SetValue(&cv_playbackspeed, 1);
}

static void M_PlaybackPause(INT32 choice)
{
	(void)choice;

	paused = !paused;

	if (demo.rewinding)
	{
		G_ConfirmRewind(leveltime);
		paused = true;
		S_PauseAudio();
	}
	else if (paused)
		S_PauseAudio();
	else
		S_ResumeAudio();

	CV_SetValue(&cv_playbackspeed, 1);
}

static void M_PlaybackFastForward(INT32 choice)
{
	(void)choice;

	if (demo.rewinding)
	{
		G_ConfirmRewind(leveltime);
		paused = false;
		S_ResumeAudio();
	}
	CV_SetValue(&cv_playbackspeed, cv_playbackspeed.value == 1 ? 4 : 1);
}

static void M_PlaybackAdvance(INT32 choice)
{
	(void)choice;

	paused = false;
	TryRunTics(1);
	paused = true;
}


static void M_PlaybackSetViews(INT32 choice)
{
	if (camera[0].freecam || camera[1].freecam || camera[2].freecam || camera[3].freecam)
		return;	// not here.

	if (choice > 0)
	{
		if (splitscreen < 3)
			G_AdjustView(splitscreen + 2, 0, true);
	}
	else if (splitscreen)
	{
		splitscreen--;
		R_ExecuteSetViewSize();
	}
}

static void M_PlaybackAdjustView(INT32 choice)
{
	G_AdjustView(itemOn - playback_viewcount, (choice > 0) ? 1 : -1, true);
}

// this one's rather tricky
static void M_PlaybackToggleFreecam(INT32 choice)
{
	(void)choice;
	M_ClearMenus(true);

	// remove splitscreen:
	splitscreen = 0;
	R_ExecuteSetViewSize();

	UINT8 i;
	for (i = 0; i <= splitscreen; ++i)
	{
		P_ToggleDemoCamera(i);
	}
}

static void M_PlaybackQuit(INT32 choice)
{
	(void)choice;
	G_StopDemo();

	if (demo.inreplayhut)
		M_ReplayHut(choice);
	else if (modeattacking)
	{
		M_EndModeAttackRun();
		S_ChangeMusicInternal("racent", true);
	}
	else
		D_StartTitle();
}

static void M_ChangeLevel(INT32 choice)
{
	char mapname[6];
	(void)choice;

	strlcpy(mapname, G_BuildMapName(cv_nextmap.value), sizeof (mapname));
	strlwr(mapname);
	mapname[5] = '\0';

	M_ClearMenus(true);
	COM_BufAddText(va("map %s -gametype \"%s\"\n", mapname, cv_newgametype.string));
}

static void M_ConfirmSpectate(INT32 choice)
{
	(void)choice;
	// We allow switching to spectator even if team changing is not allowed
	M_ClearMenus(true);
	COM_ImmedExecute("changeteam spectator");
}

static void M_ConfirmEnterGame(INT32 choice)
{
	(void)choice;
	if (!cv_allowteamchange.value)
	{
		M_StartMessage(M_GetText("The server is not allowing\nteam changes at this time.\nPress a key.\n"), NULL, MM_NOTHING);
		return;
	}
	M_ClearMenus(true);
	COM_ImmedExecute("changeteam playing");
}

static void M_ConfirmTeamScramble(INT32 choice)
{
	(void)choice;
	M_ClearMenus(true);

	COM_ImmedExecute(va("teamscramble %d", cv_dummyscramble.value+1));
}

static void M_ConfirmTeamChange(INT32 choice)
{
	(void)choice;

	if (cv_dummymenuplayer.value > splitscreen+1)
		return;

	if (!cv_allowteamchange.value && cv_dummyteam.value)
	{
		M_StartMessage(M_GetText("The server is not allowing\nteam changes at this time.\nPress a key.\n"), NULL, MM_NOTHING);
		return;
	}

	M_ClearMenus(true);

	switch (cv_dummymenuplayer.value)
	{
		case 1:
		default:
			COM_ImmedExecute(va("changeteam %s", cv_dummyteam.string));
			break;
		case 2:
			COM_ImmedExecute(va("changeteam2 %s", cv_dummyteam.string));
			break;
		case 3:
			COM_ImmedExecute(va("changeteam3 %s", cv_dummyteam.string));
			break;
		case 4:
			COM_ImmedExecute(va("changeteam4 %s", cv_dummyteam.string));
			break;
	}
}

static void M_ConfirmSpectateChange(INT32 choice)
{
	(void)choice;

	if (cv_dummymenuplayer.value > splitscreen+1)
		return;

	if (!cv_allowteamchange.value && cv_dummyspectate.value)
	{
		M_StartMessage(M_GetText("The server is not allowing\nteam changes at this time.\nPress a key.\n"), NULL, MM_NOTHING);
		return;
	}

	M_ClearMenus(true);

	switch (cv_dummymenuplayer.value)
	{
		case 1:
		default:
			COM_ImmedExecute(va("changeteam %s", cv_dummyspectate.string));
			break;
		case 2:
			COM_ImmedExecute(va("changeteam2 %s", cv_dummyspectate.string));
			break;
		case 3:
			COM_ImmedExecute(va("changeteam3 %s", cv_dummyspectate.string));
			break;
		case 4:
			COM_ImmedExecute(va("changeteam4 %s", cv_dummyspectate.string));
			break;
	}
}

static void M_Options(INT32 choice)
{
	(void)choice;

	// if the player is not admin or server, disable gameplay & server options
	OP_MainMenu[gameopt].status = OP_MainMenu[serveropt].status = (Playing() && !(server || IsPlayerAdmin(consoleplayer))) ? (IT_GRAYEDOUT) : (IT_STRING|IT_SUBMENU);

	OP_MainMenu[credits].status = (Playing()) ? (IT_GRAYEDOUT) : (IT_STRING|IT_CALL); // Play credits

	OP_MainMenu[localskin].status = (!cv_showlocalskinmenus.value) ? (IT_DISABLED) : (IT_CALL|IT_STRING);

#ifdef HAVE_DISCORDRPC
	OP_DataOptionsMenu[4].status = (Playing()) ? (IT_GRAYEDOUT) : (IT_STRING|IT_SUBMENU); // Erase data
#else
	OP_DataOptionsMenu[3].status = (Playing()) ? (IT_GRAYEDOUT) : (IT_STRING|IT_SUBMENU); // Erase data
#endif

	OP_GameOptionsMenu[op_game_encore].status = (M_SecretUnlocked(SECRET_ENCORE)) ? (IT_CVAR|IT_STRING) : IT_SECRET; // cv_kartencore

	OP_MainDef.prevMenu = currentMenu;
	M_SetupNextMenu(&OP_MainDef);
}

static void M_Manual(INT32 choice)
{
	(void)choice;

	MISC_HelpDef.prevMenu = (choice == INT32_MAX ? NULL : currentMenu);
	M_SetupNextMenu(&MISC_HelpDef);
}

static void M_RetryResponse(INT32 ch)
{
	if (ch != 'y' && ch != KEY_ENTER)
		return;

	if (netgame || multiplayer) // Should never happen!
		return;

	M_ClearMenus(true);
	G_SetRetryFlag();
}

static void M_Retry(INT32 choice)
{
	(void)choice;
	M_StartMessage(M_GetText("Start this race over?\n\n(Press 'Y' to confirm)\n"),M_RetryResponse,MM_YESNO);
}

static void M_SelectableClearMenus(INT32 choice)
{
	(void)choice;
	M_ClearMenus(true);
}

void M_RefreshPauseMenu(void)
{
#ifdef HAVE_DISCORDRPC
	if (discordRequestList != NULL)
	{
		MPauseMenu[mpause_discordrequests].status = IT_STRING | IT_SUBMENU;
	}
	else
	{
		MPauseMenu[mpause_discordrequests].status = IT_GRAYEDOUT;
	}
#endif
}

boolean firstDismissedRulesThisBoot = true;

void M_PopupMasterServerRules(void)
{
#ifdef MASTERSERVER
	if (cv_advertise.value && ((serverrunning && netgame) || currentMenu == &MP_ServerDef) && firstDismissedRulesThisBoot)
	{
		char *rules = GetMasterServerRules();

		if (rules)
		{
			firstDismissedRulesThisBoot = false;
			M_StartMessage(va("%s\n(press any key)", rules), NULL, MM_NOTHING);
			free(rules);
		}
	}
#endif
}

#define CCVHEIGHT 5
#define CCVHEIGHTHEADER 1
#define CCVHEIGHTHEADERAFTER 6

UINT16 ccvaralphakey = 4;
INT16 ccvarlaststheader = 0;

INT32 CVARSETUP = 0;

void M_SlotCvarIntoModMenu(consvar_t* cvar, const char* category, const char* name)
{
	if (ccvarposition == INT16_MAX)
		return;

	if (ccvarposition >= MAXMENUCCVARS - 2)
	{
		CONS_Printf("failed to register cvar into custom settings menu as menu reached limit\n");
		ccvarposition = INT16_MAX;
		return;
	}

	if (!CVARSETUP)
	{
		CONS_Printf("custom settings menu initiation\n");
		for (CVARSETUP = 0; CVARSETUP < MAXMENUCCVARS; ++CVARSETUP)
			OP_CustomCvarMenu[CVARSETUP] = (menuitem_t){IT_DISABLED, NULL, "", NULL, INT16_MAX};
	}

	if (category && ((ccvarposition == 0 && category[0] != '\0') || !fasticmp(category, OP_CustomCvarMenu[ccvarlaststheader].text)))
	{
		ccvarlaststheader = ccvarposition;
		ccvaralphakey += CCVHEIGHTHEADER;

		OP_CustomCvarMenu[ccvarposition] = (menuitem_t){IT_HEADER, NULL, Z_StrDup(category), NULL, ccvaralphakey};
		ccvaralphakey += CCVHEIGHTHEADERAFTER;

		++ccvarposition;
	}

	if (cvar->flags & CV_NETVAR)
		OP_CustomCvarMenu[ccvarposition] = (menuitem_t){ IT_STRING | IT_CVAR , NULL, Z_StrDup(va("\x85 %s", name)), cvar, ccvaralphakey };
	else
		OP_CustomCvarMenu[ccvarposition] = (menuitem_t){ IT_STRING | IT_CVAR, NULL, Z_StrDup(name), cvar, ccvaralphakey };

	ccvaralphakey += CCVHEIGHT;
	++ccvarposition;
}

// ========
// SKY ROOM
// ========

UINT8 skyRoomMenuTranslations[MAXUNLOCKABLES] = {};

static char *M_GetConditionString(condition_t cond)
{
	char *title = NULL;
	char *response = NULL;

	switch(cond.type)
	{
		case UC_PLAYTIME:
			return va("Play for %i:%02i:%02i",
				G_TicsToHours(cond.requirement),
				G_TicsToMinutes(cond.requirement, false),
				G_TicsToSeconds(cond.requirement));
		case UC_MATCHESPLAYED:
			return va("Play %d matches", cond.requirement);
		case UC_GAMECLEAR:
			if (cond.requirement > 1)
				return va("Beat game %d times", cond.requirement);
			else
				return va("Beat the game");
		case UC_ALLEMERALDS:
			if (cond.requirement > 1)
				return va("Beat game w/ all emeralds %d times", cond.requirement);
			else
				return va("Beat game w/ all emeralds");
		case UC_OVERALLTIME:
			return va("Get overall time of %i:%02i:%02i",
				G_TicsToHours(cond.requirement),
				G_TicsToMinutes(cond.requirement, false),
				G_TicsToSeconds(cond.requirement));
		case UC_MAPVISITED:
		{
			title = G_BuildMapTitle(cond.requirement-1);
			if (title)
			{
				response = va("Visit %s", title);
				Z_Free(title);
			}
			return response;
		}
		case UC_MAPBEATEN:
		{
			title = G_BuildMapTitle(cond.requirement-1);
			if (title)
			{
				response = va("Beat %s", title);
				Z_Free(title);
			}
			return response;
		}
		case UC_MAPALLEMERALDS:
		{
			title = G_BuildMapTitle(cond.requirement-1);
			if (title)
			{
				response = va("Beat %s w/ all emeralds", title);
				Z_Free(title);
			}
			return response;
		}
		case UC_MAPTIME:
		{
			title = G_BuildMapTitle(cond.extrainfo1-1);
			if (title)
			{
				response = va("Beat %s in %i:%02i.%02i", title,
					G_TicsToMinutes(cond.requirement, true),
					G_TicsToSeconds(cond.requirement),
					G_TicsToCentiseconds(cond.requirement));
				Z_Free(title);
			}
			return response;
		}
		case UC_TOTALEMBLEMS:
			return va("Get %d medals", cond.requirement);
		case UC_EXTRAEMBLEM:
			return va("Get \"%s\" medal", extraemblems[cond.requirement-1].name);
		default:
			return NULL;
	}
}

#define NUMCHECKLIST 23
static void M_DrawChecklist(void)
{
	UINT32 i, line = 0, c;
	INT32 lastid;
	boolean secret = false;

	for (i = 0; i < MAXUNLOCKABLES; i++)
	{
		const char *secretname;

		secret = (!M_Achieved(unlockables[i].showconditionset - 1) && !unlockables[i].unlocked);

		if (unlockables[i].name[0] == 0 || unlockables[i].nochecklist
		|| !unlockables[i].conditionset || unlockables[i].conditionset > MAXCONDITIONSETS
		|| (unlockables[i].type == SECRET_HELLATTACK && secret)) // TODO: turn this into an unlockable setting instead of tying it to Hell Attack
			continue;

		++line;
		secretname = M_CreateSecretMenuOption(unlockables[i].name);

		V_DrawString(8, (line*8), V_RETURN8|MENUCAPS|(unlockables[i].unlocked ? recommendedflags : warningflags), (secret ? secretname : unlockables[i].name));

		if (conditionSets[unlockables[i].conditionset - 1].numconditions)
		{
			lastid = -1;

			for (c = 0; c < conditionSets[unlockables[i].conditionset - 1].numconditions; c++)
			{
				condition_t cond = conditionSets[unlockables[i].conditionset - 1].condition[c];
				UINT8 achieved = M_CheckCondition(&cond);
				char *str = M_GetConditionString(cond);
				const char *secretstr = M_CreateSecretMenuOption(str);

				if (!str)
					continue;

				++line;

				if (lastid == -1 || cond.id != (UINT32)lastid)
				{
					V_DrawString(16, (line*8), V_MONOSPACE|V_ALLOWLOWERCASE|(achieved ? highlightflags : 0), "*");
					V_DrawString(32, (line*8), V_MONOSPACE|V_ALLOWLOWERCASE|(achieved ? highlightflags : 0), (secret ? secretstr : str));
				}
				else
				{
					V_DrawString(32, (line*8), V_MONOSPACE|V_ALLOWLOWERCASE|(achieved ? highlightflags : 0), (secret ? "?" : "&"));
					V_DrawString(48, (line*8), V_MONOSPACE|V_ALLOWLOWERCASE|(achieved ? highlightflags : 0), (secret ? secretstr : str));
				}

				lastid = cond.id;
			}
		}

		++line;

		if (line >= NUMCHECKLIST)
			break;
	}
}
#undef NUMCHECKLIST

static void M_DrawSkyRoom(void)
{
	INT32 i, y = 0;
	INT32 lengthstring = 0;

	M_DrawGenericMenu();

	if (currentMenu == &OP_SoundOptionsDef)
	{
		V_DrawRightAlignedString(BASEVIDWIDTH - currentMenu->x,
			currentMenu->y+currentMenu->menuitems[0].alphaKey,
			(sound_disabled ? warningflags : highlightflags),
			(sound_disabled ? "OFF" : "ON"));

		V_DrawRightAlignedString(BASEVIDWIDTH - currentMenu->x,
			currentMenu->y+currentMenu->menuitems[2].alphaKey,
			(music_disabled ? warningflags : highlightflags),
			(music_disabled ? "OFF" : "ON"));

		if (itemOn == 0)
			lengthstring = 8*(sound_disabled ? 3 : 2);
		else if (itemOn == 2)
			lengthstring = 8*(music_disabled ? 3 : 2);
	}

	for (i = 0; i < currentMenu->numitems; ++i)
	{
		if (currentMenu->menuitems[i].itemaction == M_HandleSoundTest)
		{
			y = currentMenu->menuitems[i].alphaKey;
			break;
		}
	}

	if (y)
	{
		y += currentMenu->y;

		V_DrawRightAlignedString(BASEVIDWIDTH - currentMenu->x, y, highlightflags, cv_soundtest.string);
		if (cv_soundtest.value)
			V_DrawRightAlignedString(BASEVIDWIDTH - currentMenu->x, y + 8, highlightflags, S_sfx[cv_soundtest.value].name);

		if (i == itemOn)
			lengthstring = V_StringWidth(cv_soundtest.string, 0);
	}

	if (lengthstring)
	{
		V_DrawCharacter(BASEVIDWIDTH - currentMenu->x - 10 - lengthstring - (skullAnimCounter/5), currentMenu->y+currentMenu->menuitems[itemOn].alphaKey,
			'\x1C' | highlightflags, false); // left arrow
		V_DrawCharacter(BASEVIDWIDTH - currentMenu->x + 2 + (skullAnimCounter/5), currentMenu->y+currentMenu->menuitems[itemOn].alphaKey,
			'\x1D' | highlightflags, false); // right arrow
	}
}

static void M_HandleSoundTest(INT32 choice)
{
	boolean exitmenu = false; // exit to previous menu

	switch (choice)
	{
		case KEY_DOWNARROW:
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_UPARROW:
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_BACKSPACE:
		case KEY_ESCAPE:
			exitmenu = true;
			break;

		case KEY_RIGHTARROW:
			CV_AddValue(&cv_soundtest, 1);
			break;
		case KEY_LEFTARROW:
			CV_AddValue(&cv_soundtest, -1);
			break;
		case KEY_ENTER:
			S_StopSounds();
			S_StartSound(NULL, cv_soundtest.value);
			break;

		default:
			break;
	}
	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

static musicdef_t *curplaying = NULL;
static INT32 st_sel = 0;
static tic_t st_musictime = 0;

static void M_MusicTest(INT32 choice)
{
	(void)choice;

	if (!nummusicdefs)
	{
		M_StartMessage(M_GetText("No selectable tracks found.\n"),NULL,MM_NOTHING);
		return;
	}

	curplaying = NULL;

	st_sel = 0;

	M_SetupNextMenu(&SR_MusicTestDef);
}

static void M_DrawMusicTest(void)
{
	INT32 x, y, i;

	y = (BASEVIDWIDTH-vid.scaledwidth)/2;

	V_DrawFill(y-1, 20, vid.scaledwidth+1, 24, 239);

	{
		static fixed_t st_scroll = -FRACUNIT;
		const char* titl;

		x = 16;
		V_DrawString(x, 10, 0, "NOW PLAYING:");
		if (curplaying)
		{
			if (curplaying->title[0] && curplaying->alttitle[0])
				titl = va("%s - %s - ", curplaying->title, curplaying->alttitle);
			else if (curplaying->title[0])
				titl = va("%s - ", curplaying->title);
			else
				titl = va("%s - ", curplaying->source);
		}
		else
			titl = "NONE - ";

		i = V_LevelNameWidth(titl);

		st_scroll += renderdeltatics;

		while (st_scroll >= (i << FRACBITS))
			st_scroll -= i << FRACBITS;

		x -= st_scroll >> FRACBITS;

		while (x < BASEVIDWIDTH-y)
			x += i;
		while (x > y)
		{
			x -= i;
			V_DrawLevelTitle(x, 24, 0, titl);
		}

		if (curplaying && curplaying->authors[0])
			V_DrawRightAlignedThinString(BASEVIDWIDTH-16, 46, V_ALLOWLOWERCASE, curplaying->authors);

		if (curplaying)
		{
			if (!curplaying->usage[0])
				V_DrawString(vid.dup, vid.height - 10*vid.dup, V_NOSCALESTART|V_ALLOWLOWERCASE, va("%.6s", curplaying->name));
			else
				V_DrawSmallString(vid.dup, vid.height - 5*vid.dup, V_NOSCALESTART|V_ALLOWLOWERCASE, va("%.6s - %.255s\n", curplaying->name, curplaying->usage));

			if (cv_showmusicfilename.value)
				V_DrawSmallString(0, 0, V_SNAPTOTOP|V_SNAPTOLEFT|V_ALLOWLOWERCASE, curplaying->filename);
		}
	}

	V_DrawFill(20, 60, 280, 128, 239);

	{
		INT32 t, b, q, m = 128;

		if (nummusicdefs <= 8)
		{
			t = 0;
			b = nummusicdefs - 1;
			i = 0;
		}
		else
		{
			q = m;
			m = (5*m)/nummusicdefs;
			if (st_sel < 3)
			{
				t = 0;
				b = 7;
				i = 0;
			}
			else if (st_sel >= nummusicdefs-4)
			{
				t = nummusicdefs - 8;
				b = nummusicdefs - 1;
				i = q-m;
			}
			else
			{
				t = st_sel - 3;
				b = st_sel + 4;
				i = (t * (q-m))/(nummusicdefs - 8);
			}
		}

		V_DrawFill(20+280-1, 60 + i, 1, m, 0);

		if (t != 0)
			V_DrawString(20+280+4, 60+4 - (skullAnimCounter/5), V_YELLOWMAP, "\x1A");

		if (b != nummusicdefs - 1)
			V_DrawString(20+280+4, 60+128-12 + (skullAnimCounter/5), V_YELLOWMAP, "\x1B");

		x = 24;
		y = 64;

		if (renderisnewtic) st_musictime++;

		while (t <= b)
		{
			if (t == st_sel)
				V_DrawFill(20, y-4, 280-1, 16, 237);

			{
				const musicdef_t *def = S_GetMusicCredit(t);
				const size_t MAXLENGTH = 34;
				const char *songname = def->title[0] ? def->title : def->source;

				size_t namelength = strlen(songname);

				char buf[MAXLENGTH+1];

				if (t == st_sel && namelength > MAXLENGTH)
					M_ScrollString(songname, namelength, buf, MAXLENGTH, st_musictime);
				else
					strlcpy(buf, songname, MAXLENGTH);

				V_DrawString(x, y, (t == st_sel ? V_YELLOWMAP : 0)|V_ALLOWLOWERCASE|V_MONOSPACE, buf);
				if (curplaying == def)
				{
					V_DrawFill(20+280-9, y-4, 8, 16, 230);
				}
			}
			t++;
			y += 16;
		}
	}
}

static void M_HandleMusicTest(INT32 choice)
{
	boolean exitmenu = false; // exit to previous menu

	switch (choice)
	{
		case KEY_DOWNARROW:
			if (st_sel++ >= nummusicdefs-1)
				st_sel = 0;
			{
				S_StartSound(NULL, sfx_menu1);
			}
			st_musictime = 0;
			break;
		case KEY_UPARROW:
			if (!st_sel--)
				st_sel = nummusicdefs-1;
			{
				S_StartSound(NULL, sfx_menu1);
			}
			st_musictime = 0;
			break;
		case KEY_PGDN:
			if (st_sel < nummusicdefs-1)
			{
				st_sel += 3;
				if (st_sel >= nummusicdefs-1)
					st_sel = nummusicdefs-1;
				S_StartSound(NULL, sfx_menu1);
			}
			st_musictime = 0;
			break;
		case KEY_PGUP:
			if (st_sel)
			{
				st_sel -= 3;
				if (st_sel < 0)
					st_sel = 0;
				S_StartSound(NULL, sfx_menu1);
			}
			st_musictime = 0;
			break;
		case KEY_BACKSPACE:
			if (curplaying)
			{
				S_StopSounds();
				S_StopMusic();
				curplaying = NULL;
				S_StartSound(NULL, sfx_skid);
			}
			break;
		case KEY_ESCAPE:
			exitmenu = true;
			st_musictime = 0;
			break;

		case KEY_RIGHTARROW:
		case KEY_LEFTARROW:
		case KEY_ENTER:
			S_StopSounds();
			S_StopMusic();
			curplaying = S_GetMusicCredit(st_sel);
			S_ChangeMusicInternal(curplaying->name, true);
			break;

		default:
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// ==================
// NEW GAME FUNCTIONS
// ==================

static void M_Credits(INT32 choice)
{
	(void)choice;
	cursaveslot = -2;
	M_ClearMenus(true);
	F_StartCredits();
}

// ===============
// STATISTICS MENU
// ===============

static void M_DrawStatsMaps(void);
static void M_DrawStatsPlaytime(void);
static void M_DrawStatsExtra(void); // dunno how to name this one

static INT32 statsLocation;
static INT32 statsMax;
static INT16 statsMapList[NUMMAPS+1];
static UINT8 statsCurrentPage = 0;

typedef struct statpage_s {
	const char *title;
	void (*drawer)(void);
} statpage_t;

static statpage_t statsPages[] = {
	{ "Play Time Statistics", M_DrawStatsPlaytime, },
	{ "Level Statistics", M_DrawStatsMaps, },
	{ "Extra Statistics", M_DrawStatsExtra, },
};

#define LENSTATSPAGES (sizeof(statsPages)/sizeof(statsPages[0]))
#define NUMSTATSPAGES (kartstats.vanilla ? 2 : LENSTATSPAGES)

static void M_Statistics(INT32 choice)
{
	INT16 i, j = 0;

	(void)choice;

	memset(statsMapList, 0, sizeof(statsMapList));

	for (i = 0; i < NUMMAPS; i++)
	{
		if (!mapheaderinfo[i] || mapheaderinfo[i]->lvlttl[0] == '\0')
			continue;

		if (!(mapheaderinfo[i]->typeoflevel & TOL_RACE) // TOL_SP
			|| (mapheaderinfo[i]->menuflags & (LF2_HIDEINSTATS|LF2_HIDEINMENU)))
			continue;

		if (M_MapLocked(i+1)) // !mapvisited[i]
			continue;

		statsMapList[j++] = i;
	}
	statsMapList[j] = -1;
	statsMax = j - 11 + numextraemblems;
	statsLocation = 0;
	statsCurrentPage = 0;

	if (statsMax < 0)
		statsMax = 0;

	M_SetupNextMenu(&SP_LevelStatsDef);
}

static void M_DrawStatsMaps(void)
{
	char beststr[40];
	tic_t besttime = 0;
	INT32 mapsunfinished = 0;

	int location = statsLocation;
	INT32 y = 62, i = -1, j;
	INT16 mnum;
	extraemblem_t *exemblem;
	boolean dotopname = true, dobottomarrow = (location < statsMax);

	SHOWMODDEDGAME

	for (j = 0; j < NUMMAPS; j++)
	{
		if (!mapheaderinfo[j] || !(mapheaderinfo[j]->menuflags & LF2_RECORDATTACK))
			continue;

		if (!mainrecords[j] || mainrecords[j]->time <= 0)
		{
			mapsunfinished++;
			continue;
		}

		besttime += mainrecords[j]->time;
	}

	V_DrawString(20, 42, highlightflags|MENUCAPS, "Combined time records:");

	sprintf(beststr, "%i:%02i:%02i.%02i", G_TicsToHours(besttime), G_TicsToMinutes(besttime, false), G_TicsToSeconds(besttime), G_TicsToCentiseconds(besttime));
	V_DrawRightAlignedString(BASEVIDWIDTH-16, 42, (mapsunfinished ? warningflags : 0), beststr);

	if (mapsunfinished)
		V_DrawRightAlignedString(BASEVIDWIDTH-16, 50, warningflags|MENUCAPS, va("(%d unfinished)", mapsunfinished));
	else
		V_DrawRightAlignedString(BASEVIDWIDTH-16, 50, recommendedflags|MENUCAPS, "(complete)");

	V_DrawString(32, 50, MENUCAPS, va("x %d/%d", M_CountEmblems(), numemblems+numextraemblems));
	V_DrawSmallScaledPatch(20, 50, 0, (patch_t *)W_CachePatchName("GOTITA", PU_PATCH));

	if (location)
		V_DrawCharacter(10, y-(skullAnimCounter/5),
			'\x1A' | highlightflags, false); // up arrow

	while (statsMapList[++i] != -1)
	{
		if (location)
		{
			--location;
			continue;
		}
		else if (dotopname)
		{
			V_DrawString(20,  y, highlightflags|MENUCAPS, "Level name");
			V_DrawString(256, y, highlightflags|MENUCAPS, "Medals");
			y += 8;
			dotopname = false;
		}

		mnum = statsMapList[i];
		M_DrawMapEmblems(mnum+1, 295, y);

		if (mapheaderinfo[mnum]->levelflags & LF_NOZONE)
			V_DrawString(20, y, MENUCAPS, va("%s %s",
				mapheaderinfo[mnum]->lvlttl,
				mapheaderinfo[mnum]->actnum));
		else
			V_DrawString(20, y, MENUCAPS, va("%s %s %s",
				mapheaderinfo[mnum]->lvlttl,
				(mapheaderinfo[mnum]->zonttl[0] ? mapheaderinfo[mnum]->zonttl : "Zone"),
				mapheaderinfo[mnum]->actnum));

		y += 8;

		if (y >= BASEVIDHEIGHT-8)
			goto bottomarrow;
	}
	if (dotopname && !location)
	{
		V_DrawString(20,  y, highlightflags|MENUCAPS, "Level name");
		V_DrawString(256, y, highlightflags|MENUCAPS, "Medals");
		y += 8;
	}
	else if (location)
		--location;

	// Extra Emblems
	for (i = -2; i < numextraemblems; ++i)
	{
		if (i == -1)
		{
			V_DrawString(20, y, highlightflags|MENUCAPS, "Extra medals");
			if (location)
			{
				y += 8;
				location++;
			}
		}
		if (location)
		{
			--location;
			continue;
		}

		if (i >= 0)
		{
			exemblem = &extraemblems[i];

			if (exemblem->collected)
				V_DrawSmallMappedPatch(295, y, 0, (patch_t *)W_CachePatchName(M_GetExtraEmblemPatch(exemblem), PU_PATCH),
				                       R_GetTranslationColormap(TC_DEFAULT, M_GetExtraEmblemColor(exemblem), GTC_MENUCACHE));
			else
				V_DrawSmallScaledPatch(295, y, 0, (patch_t *)W_CachePatchName("NEEDIT", PU_PATCH));

			V_DrawString(20, y, MENUCAPS, va("%s", exemblem->description));
		}

		y += 8;

		if (y >= BASEVIDHEIGHT-8)
			goto bottomarrow;
	}
bottomarrow:
	if (dobottomarrow)
		V_DrawCharacter(10, y-8 + (skullAnimCounter/5),
			'\x1B' | highlightflags, false); // down arrow
}

#define DRAWTIMESTAT(y, title, field) { \
		char timebuf[80]; \
		V_DrawString(20, (y), highlightflags|MENUCAPS, title); \
		tic_t timeval = kartstats.field; \
		snprintf(timebuf, 80, "%02i:%02i:%02i", G_TicsToHours(timeval), G_TicsToMinutes(timeval, false), G_TicsToSeconds(timeval)); \
		V_DrawRightAlignedString(BASEVIDWIDTH-16, (y), MENUCAPS, timebuf); \
	}

#define DRAWAMOUNTSTAT(y, title, field) { \
		V_DrawString(20, (y), highlightflags|MENUCAPS, title); \
		unsigned amountval = kartstats.field; \
		V_DrawRightAlignedString(BASEVIDWIDTH-16, (y), MENUCAPS, va("%u", amountval)); \
	}

static void M_DrawStatsPlaytime(void)
{
	V_DrawString(20, 42, highlightflags|MENUCAPS, "Total Play Time:");
	V_DrawCenteredString(BASEVIDWIDTH/2, 52, MENUCAPS, va("%i hours, %i minutes, %i seconds",
	                         G_TicsToHours(kartstats.totalplaytime),
	                         G_TicsToMinutes(kartstats.totalplaytime, false),
	                         G_TicsToSeconds(kartstats.totalplaytime)));
	V_DrawString(20, 62, highlightflags|MENUCAPS, "Total Matches:");
	V_DrawRightAlignedString(BASEVIDWIDTH-16, 62, MENUCAPS, va("%i played", kartstats.matchesplayed));

	// Nothing else to draw
	if (kartstats.vanilla)
		return;

	SHOWMODDEDGAME

	DRAWTIMESTAT(82, "RA Play Time:", raplaytime);
	DRAWTIMESTAT(92, "Online Play Time:", onlineplaytime);
	DRAWTIMESTAT(102, "Race Play Time:", raceplaytime);
	DRAWTIMESTAT(112, "Battle Play Time:", battleplaytime);
}

// Note: only available with non-vanilla stats loaded, so it doesn't check for that
static void M_DrawStatsExtra(void)
{
	SHOWMODDEDGAME

	DRAWTIMESTAT(42, "Time being SPB target:", spbtargettime);
	DRAWTIMESTAT(52, "Time spent in spinout:", spinouttime);

	DRAWAMOUNTSTAT(72, "Total wins:", totalwins);
	DRAWAMOUNTSTAT(82, "Total podium (2nd/3rd place):", totalpodium);

	DRAWAMOUNTSTAT(102, "Hits landed:", hits);
	DRAWAMOUNTSTAT(112, "Self-hits landed:", selfhits);

	DRAWAMOUNTSTAT(132, "Sinks landed:", sinks);
	DRAWAMOUNTSTAT(142, "Times hit by sink:", sinked);

	DRAWAMOUNTSTAT(162, "Total respawns:", respawns);
}

#undef DRAWAMOUNTSTAT
#undef DRAWTIMESTAT

static void M_DrawLevelStats(void)
{
	M_DrawMenuTitle();

	V_DrawCenteredString(BASEVIDWIDTH/2, 28, highlightflags|MENUCAPS, statsPages[statsCurrentPage].title);

	INT32 w = V_StringWidth(statsPages[statsCurrentPage].title, highlightflags|MENUCAPS);
	V_DrawCharacter(BASEVIDWIDTH/2 - w/2 - 10 - (skullAnimCounter/5), 28,
			'\x1C' | highlightflags, false); // left arrow
	V_DrawCharacter(BASEVIDWIDTH/2 + w/2 + 2 + (skullAnimCounter/5), 28,
			'\x1D' | highlightflags, false); // right arrow

	statsPages[statsCurrentPage].drawer();
}

// Handle statistics.
static void M_HandleLevelStats(INT32 choice)
{
	boolean exitmenu = false; // exit to previous menu

	switch (choice)
	{
		case KEY_DOWNARROW:
			if (statsCurrentPage != 1) // Must be on level stats page
				break;
			S_StartSound(NULL, sfx_menu1);
			if (statsLocation < statsMax)
				++statsLocation;
			break;

		case KEY_UPARROW:
			if (statsCurrentPage != 1) // Must be on level stats page
				break;
			S_StartSound(NULL, sfx_menu1);
			if (statsLocation)
				--statsLocation;
			break;

		case KEY_RIGHTARROW:
			S_StartSound(NULL, sfx_menu1);
			statsCurrentPage++;
			if (statsCurrentPage >= NUMSTATSPAGES)
				statsCurrentPage = 0;
			break;

		case KEY_LEFTARROW:
			S_StartSound(NULL, sfx_menu1);
			if (statsCurrentPage == 0)
				statsCurrentPage = NUMSTATSPAGES-1;
			else
				--statsCurrentPage;
			break;

		case KEY_PGDN:
			if (statsCurrentPage != 1) // Must be on level stats page
				break;
			S_StartSound(NULL, sfx_menu1);
			statsLocation += (statsLocation+13 >= statsMax) ? statsMax-statsLocation : 13;
			break;

		case KEY_PGUP:
			if (statsCurrentPage != 1) // Must be on level stats page
				break;
			S_StartSound(NULL, sfx_menu1);
			statsLocation -= (statsLocation < 13) ? statsLocation : 13;
			break;

		case KEY_ESCAPE:
			exitmenu = true;
			break;
	}
	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// ===========
// MODE ATTACK
// ===========

// Drawing function for Time Attack
void M_DrawTimeAttackMenu(void)
{
	INT32 i, x, y, cursory = 0;
	UINT16 dispstatus;

	//S_ChangeMusicInternal("racent", true); // Eww, but needed for when user hits escape during demo playback

	V_DrawPatchFill(srb2back);

	M_DrawMenuTitle();
	if (currentMenu == &SP_TimeAttackDef)
		M_DrawLevelSelectOnly(true, false);

	// draw menu (everything else goes on top of it)
	// Sadly we can't just use generic mode menus because we need some extra hacks
	x = currentMenu->x;
	y = currentMenu->y;

	SHOWMODDEDGAME

	const INT32 skinnum = max(cv_chooseskin.value-1, 0); // dont think its needed but better safe than sorry!
	const skin_t *skin = &skins[skinnum];

	// Character face!
	if (W_CheckNumForName(skin->facewant) != LUMPERROR)
	{
		const INT32 charx = (BASEVIDWIDTH-x - facewantprefix[skinnum]->width);

		UINT8 *colormap = NULL;

		colormap = R_GetTranslationColormap(skinnum, cv_playercolor.value, GTC_MENUCACHE);
		V_DrawMappedPatch(charx, y, 0, facewantprefix[skinnum], colormap);

		// draw stats
		// speed
		colormap = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_BLUEBERRY, GTC_CACHE);
		V_DrawFixedPatch((charx-6) << FRACBITS, (y-5) << FRACBITS, 3*FRACUNIT/2, 0, kp_facenum[min(9, max(1, skin->kartspeed))], colormap);
		// weight
		colormap = R_GetTranslationColormap(TC_RAINBOW, SKINCOLOR_BURGUNDY, GTC_CACHE);
		V_DrawFixedPatch((charx+25) << FRACBITS, (y+25) << FRACBITS, 3*FRACUNIT/2, 0, kp_facenum[min(9, max(1, skin->kartweight))], colormap);
		// idk if kp_facenum are the best numbers for this case? but works with some scaling lul
	}

	for (i = 0; i < currentMenu->numitems; ++i)
	{
		dispstatus = (currentMenu->menuitems[i].status & IT_DISPLAY);
		if (dispstatus != IT_STRING && dispstatus != IT_WHITESTRING)
			continue;

		y = currentMenu->y+currentMenu->menuitems[i].alphaKey;
		if (i == itemOn)
			cursory = y;

		V_DrawString(x, y, ((dispstatus == IT_WHITESTRING) ? highlightflags : 0)|MENUCAPS, currentMenu->menuitems[i].text);

		// Cvar specific handling
		if ((currentMenu->menuitems[i].status & IT_TYPE) == IT_CVAR)
		{
			consvar_t *cv = (consvar_t *)currentMenu->menuitems[i].itemaction;
			if (currentMenu->menuitems[i].status & IT_CV_STRING)
			{
				M_DrawTextBox(x + 32, y - 8, MAXPLAYERNAME, 1);

				if (itemOn != i)
					V_DrawString(x + 40, y, V_ALLOWLOWERCASE, cv->string);
				else
					M_DrawTextInput(x + 40, y, &menuinput, 0);
			}
			else
			{
				const char *str = ((cv == &cv_chooseskin) ? skin->realname : cv->string);
				INT32 soffset = 40, strw = V_StringWidth(str, 0);

				// hack to keep the menu from overlapping the level icon
				if (currentMenu != &SP_TimeAttackDef || cv == &cv_nextmap)
					soffset = 0;

				// Should see nothing but strings
				V_DrawString(BASEVIDWIDTH - x - soffset - strw, y, highlightflags|MENUCAPS, str);

				if (i == itemOn)
				{
					V_DrawCharacter(BASEVIDWIDTH - x - soffset - 10 - strw - (skullAnimCounter/5), y,
						'\x1C' | highlightflags, false); // left arrow
					V_DrawCharacter(BASEVIDWIDTH - x - soffset + 2 + (skullAnimCounter/5), y,
						'\x1D' | highlightflags, false); // right arrow
				}
			}
		}
		else if ((currentMenu->menuitems[i].status & IT_TYPE) == IT_KEYHANDLER && cv_dummystaff.value) // bad hacky assumption: IT_KEYHANDLER is assumed to be staff ghost selector
		{
			INT32 strw = V_StringWidth(dummystaffname, V_ALLOWLOWERCASE);
			V_DrawString(BASEVIDWIDTH - x - strw, y, highlightflags|V_ALLOWLOWERCASE, dummystaffname);
			if (i == itemOn)
			{
				V_DrawCharacter(BASEVIDWIDTH - x - 10 - strw - (skullAnimCounter/5), y,
					'\x1C' | highlightflags, false); // left arrow
				V_DrawCharacter(BASEVIDWIDTH - x + 2 + (skullAnimCounter/5), y,
					'\x1D' | highlightflags, false); // right arrow
			}
		}
	}

	x = currentMenu->x;
	y = currentMenu->y;

	// DRAW THE SKULL CURSOR
	V_DrawScaledPatch(x - 24, cursory, 0, (patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
	V_DrawString(x, cursory, highlightflags|MENUCAPS, currentMenu->menuitems[itemOn].text);

	// Level record list
	if (cv_nextmap.value)
	{
		tic_t lap = 0, time = 0;

		const recorddata_t *currecord = mainrecords[cv_nextmap.value-1];

		if (currecord)
		{
			lap = currecord->lap;
			time = currecord->time;
		}

		V_DrawFill((BASEVIDWIDTH - vid.scaledwidth)>>1, 78, vid.scaledwidth, 36, 239);

		V_DrawRightAlignedString(149, 80, highlightflags|MENUCAPS, "Best Lap:");
		K_drawKartTimestamp(lap, 19, 86, 0, 2);

		V_DrawRightAlignedString(292, 80, highlightflags|MENUCAPS, "Best Time:");
		K_drawKartTimestamp(time, 162, 86, cv_nextmap.value, 1);
	}

	// ALWAYS DRAW player name, level name, skin and color even when not on this menu!
	if (currentMenu != &SP_TimeAttackDef)
	{
		consvar_t *ncv;

		for (i = 0; i < 4; ++i)
		{
			y = currentMenu->y+SP_TimeAttackMenu[i].alphaKey;
			V_DrawString(x, y, V_TRANSLUCENT|MENUCAPS, SP_TimeAttackMenu[i].text);
			ncv = (consvar_t *)SP_TimeAttackMenu[i].itemaction;

			if (SP_TimeAttackMenu[i].status & IT_CV_STRING)
			{
				M_DrawTextBox(x + 32, y - 8, MAXPLAYERNAME, 1);
				V_DrawString(x + 40, y, V_TRANSLUCENT|V_ALLOWLOWERCASE, ncv->string);
			}
			else
			{
				const char *str = ((ncv == &cv_chooseskin) ? skin->realname : ncv->string);
				INT32 soffset = 40, strw = V_StringWidth(str, 0);

				// hack to keep the menu from overlapping the level icon
				if (ncv == &cv_nextmap)
					soffset = 0;

				// Should see nothing but strings
				V_DrawString(BASEVIDWIDTH - x - soffset - strw, y, highlightflags|V_TRANSLUCENT|MENUCAPS, str);
			}
		}
	}
}

// Going to Time Attack menu...
static void M_TimeAttack(INT32 choice)
{
	(void)choice;

	memset(skins_cons_t, 0, sizeof (skins_cons_t));

	levellistmode = LLM_RECORDATTACK; // Don't be dependent on cv_newgametype

	if (M_CountLevelsToShowInList() == 0)
	{
		M_StartMessage(M_GetText("No record-attackable levels found.\n"), NULL, MM_NOTHING);
		return;
	}

	M_PatchSkinNameTable();

	M_PrepareLevelSelect();
	M_SetupNextMenu(&SP_TimeAttackDef);

	G_SetGamestate(GS_TIMEATTACK);

	if (cv_nextmap.value)
		Nextmap_OnChange();
	else
		CV_AddValue(&cv_nextmap, 1);

	itemOn = tastart; // "Start" is selected.

	S_ChangeMusicInternal("racent", true);
}

static boolean M_QuitTimeAttackMenu(void)
{
	// you know what? always putting these in the buffer won't hurt anything.
	COM_BufAddText(va("skin \"%s\"\n", cv_chooseskin.string));
	return true;
}

// Player has selected the "START" from the time attack screen
static void M_ChooseTimeAttack(INT32 choice)
{
	const char *mapname = G_BuildMapName(cv_nextmap.value);
	char nameofdemo[256];
	(void)choice;
	emeralds = 0;
	M_ClearMenus(true);
	modeattacking = ATTACKING_RECORD;

	I_mkdir(va("%s"PATHSEP"replay", srb2home), 0755);
	I_mkdir(va("%s"PATHSEP"replay"PATHSEP"%s", srb2home, timeattackfolder), 0755);

	snprintf(nameofdemo, sizeof nameofdemo, "replay"PATHSEP"%s"PATHSEP"%s-%s-last", timeattackfolder, mapname, cv_chooseskin.string);

	if (!cv_autorecord.value)
		remove(va("%s"PATHSEP"%s.lmp", srb2home, nameofdemo));
	else
		G_RecordDemo(nameofdemo);

	G_DeferedInitNew(false, mapname, (UINT8)(cv_chooseskin.value-1), 0, false);
}

static void M_HandleStaffReplay(INT32 choice)
{
	boolean exitmenu = false; // exit to previous menu
	lumpnum_t l = W_CheckNumForName(va("%sS%02u",G_BuildMapName(cv_nextmap.value),cv_dummystaff.value));

	switch (choice)
	{
		case KEY_DOWNARROW:
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_UPARROW:
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_BACKSPACE:
		case KEY_ESCAPE:
			exitmenu = true;
			break;
		case KEY_RIGHTARROW:
			CV_AddValue(&cv_dummystaff, 1);
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_LEFTARROW:
			CV_AddValue(&cv_dummystaff, -1);
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_ENTER:
			if (l == LUMPERROR)
				break;
			M_ClearMenus(true);
			modeattacking = ATTACKING_RECORD;
			demo.loadfiles = false; demo.ignorefiles = true; // Just assume that record attack replays have the files needed
			G_DoPlayDemo(va("%sS%02u",G_BuildMapName(cv_nextmap.value),cv_dummystaff.value));
			break;
		default:
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// Player has selected the "REPLAY" from the time attack screen
static void M_ReplayTimeAttack(INT32 choice)
{
	const char *which;
	M_ClearMenus(true);
	modeattacking = ATTACKING_RECORD; // set modeattacking before G_DoPlayDemo so the map loader knows
	demo.loadfiles = false; demo.ignorefiles = true; // Just assume that record attack replays have the files needed

	if (currentMenu == &SP_ReplayDef)
	{
		switch(choice) {
		default:
		case 0: // best time
			which = "time-best";
			break;
		case 1: // best lap
			which = "lap-best";
			break;
		case 2: // last
			which = "last";
			break;
		case 3: // guest
			// srb2/replay/main/map01-guest.lmp
			G_DoPlayDemo(va("%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-guest.lmp", srb2home, timeattackfolder, G_BuildMapName(cv_nextmap.value)));
			return;
		}
		// srb2/replay/main/map01-sonic-time-best.lmp
		G_DoPlayDemo(va("%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-%s-%s.lmp", srb2home, timeattackfolder, G_BuildMapName(cv_nextmap.value), cv_chooseskin.string, which));
	}
}

static void M_EraseGuest(INT32 choice)
{
	const char *rguest = va("%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-guest.lmp", srb2home, timeattackfolder, G_BuildMapName(cv_nextmap.value));
	(void)choice;
	if (FIL_FileExists(rguest))
		remove(rguest);

	M_SetupNextMenu(&SP_TimeAttackDef);
	Nextmap_OnChange();
	M_StartMessage(M_GetText("Guest replay data erased.\n"),NULL,MM_NOTHING);
}

static void M_OverwriteGuest(const char *which)
{
	char *rguest;
	UINT8 *buf;
	size_t len;
	len = FIL_ReadFile(va("%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-%s-%s.lmp", srb2home, timeattackfolder, G_BuildMapName(cv_nextmap.value), cv_chooseskin.string, which), &buf);

	if (!len)
		return;

	rguest = Z_StrDup(va("%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-guest.lmp", srb2home, timeattackfolder, G_BuildMapName(cv_nextmap.value)));

	if (FIL_FileExists(rguest))
	{
		M_StopMessage(0);
		remove(rguest);
	}

	FIL_WriteFile(rguest, buf, len);
	Z_Free(rguest);

	M_SetupNextMenu(&SP_TimeAttackDef);
	Nextmap_OnChange();
	M_StartMessage(M_GetText("Guest replay data saved.\n"),NULL,MM_NOTHING);
}

static void M_OverwriteGuest_Time(INT32 choice)
{
	(void)choice;
	M_OverwriteGuest("time-best");
}

static void M_OverwriteGuest_Lap(INT32 choice)
{
	(void)choice;
	M_OverwriteGuest("lap-best");
}

static void M_OverwriteGuest_Last(INT32 choice)
{
	(void)choice;
	M_OverwriteGuest("last");
}

static void M_SetGuestReplay(INT32 choice)
{
	void (*which)(INT32);
	switch(choice)
	{
	case 0: // best time
		which = M_OverwriteGuest_Time;
		break;
	case 1: // best lap
		which = M_OverwriteGuest_Lap;
		break;
	case 2: // last
		which = M_OverwriteGuest_Last;
		break;
	case 3: // guest
	default:
		M_StartMessage(M_GetText("Are you sure you want to\ndelete the guest replay data?\n\n(Press 'Y' to confirm)\n"),M_EraseGuest,MM_YESNO);
		return;
	}
	if (FIL_FileExists(va("%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s-guest.lmp", srb2home, timeattackfolder, G_BuildMapName(cv_nextmap.value))))
		M_StartMessage(M_GetText("Are you sure you want to\noverwrite the guest replay data?\n\n(Press 'Y' to confirm)\n"),which,MM_YESNO);
	else
		which(0);
}

void M_ModeAttackRetry(INT32 choice)
{
	(void)choice;
	G_CheckDemoStatus(); // Cancel recording
	if (modeattacking == ATTACKING_RECORD)
		M_ChooseTimeAttack(0);
}

static void M_ModeAttackEndGame(INT32 choice)
{
	(void)choice;
	G_CheckDemoStatus(); // Cancel recording

	if (gamestate == GS_LEVEL || gamestate == GS_INTERMISSION || gamestate == GS_VOTING)
		Command_ExitGame_f();

	M_StartControlPanel();
	currentMenu = &SP_TimeAttackDef;
	itemOn = currentMenu->lastOn;
	G_SetGamestate(GS_TIMEATTACK);
	modeattacking = ATTACKING_NONE;
	S_ChangeMusicInternal("racent", true);
	// Update replay availability.
	Nextmap_OnChange();
}

// ========
// END GAME
// ========

static void M_ExitGameResponse(INT32 ch)
{
	if (ch != 'y' && ch != KEY_ENTER)
		return;

	//Command_ExitGame_f();
	G_SetExitGameFlag();
	M_ClearMenus(true);
}

static void M_EndGame(INT32 choice)
{
	(void)choice;

	if (demo.playback || !Playing())
		return;

	M_StartMessage(M_GetText("Are you sure you want to end the game?\n\n(Press 'Y' to confirm)\n"), M_ExitGameResponse, MM_YESNO);
}

//===========================================================================
// Connect Menu
//===========================================================================

void M_SetWaitingMode(int mode)
{
#ifdef HAVE_THREADS
	I_lock_mutex(&m_menu_mutex);
#endif
	m_waiting_mode = mode;
#ifdef HAVE_THREADS
	I_unlock_mutex(m_menu_mutex);
#endif
}

int M_GetWaitingMode(void)
{
	int mode;

#ifdef HAVE_THREADS
	I_lock_mutex(&m_menu_mutex);
#endif
	mode = m_waiting_mode;
#ifdef HAVE_THREADS
	I_unlock_mutex(m_menu_mutex);
#endif

	return mode;
}

#ifdef MASTERSERVER
#ifdef HAVE_THREADS
static void Spawn_masterserver_thread(const char *name, void (*thread)(int*))
{
	int *id = malloc(sizeof *id);

	I_lock_mutex(&ms_QueryId_mutex);
	*id = ms_QueryId;
	I_unlock_mutex(ms_QueryId_mutex);

	I_spawn_thread(name, (I_thread_fn)thread, id);
}

static int Same_instance(int id)
{
	int okay;

	I_lock_mutex(&ms_QueryId_mutex);
	okay = ( id == ms_QueryId );
	I_unlock_mutex(ms_QueryId_mutex);

	return okay;
}
#endif/*HAVE_THREADS*/

static void Fetch_servers_thread(int *id)
{
	msg_server_t * server_list;

	(void)id;

	M_SetWaitingMode(M_WAITING_SERVERS);

#ifdef HAVE_THREADS
	server_list = GetShortServersList(*id);
#else
	server_list = GetShortServersList(0);
#endif

	if (server_list)
	{
#ifdef HAVE_THREADS
		if (Same_instance(*id))
#endif
		{
			M_SetWaitingMode(M_NOT_WAITING);

#ifdef HAVE_THREADS
			I_lock_mutex(&ms_ServerList_mutex);
			ms_ServerList = server_list;
			I_unlock_mutex(ms_ServerList_mutex);
#else
			CL_QueryServerList(server_list);
			free(server_list);
#endif
		}
#ifdef HAVE_THREADS
		else
		{
			free(server_list);
		}
#endif
	}

#ifdef HAVE_THREADS
	free(id);
#endif
}
#endif/*MASTERSERVER*/

#define SERVERHEADERHEIGHT 48
#define SERVERLINEHEIGHT 12

#define S_LINEY(n) currentMenu->y + SERVERHEADERHEIGHT + (n * SERVERLINEHEIGHT)

static UINT32 localservercount;

static void M_SearchServerList(void)
{
	char servername[MAXSERVERNAME+1] = {0};
	serverlistsearchedcount = 0;

#ifdef HAVE_THREADS
	I_lock_mutex(&ms_ServerList_mutex);
#endif

	for (UINT32 i = 0; i < serverlistcount; ++i)
	{
		StripColors(servername, serverlist[i].info.servername, MAXSERVERNAME);
		if (menuinput.length == 0 || strcasestr(servername, menuinput.buffer) != NULL)
			serverlistsearched[serverlistsearchedcount++] = i;
	}

#ifdef HAVE_THREADS
	I_unlock_mutex(ms_ServerList_mutex);
#endif

	if (menuinput.length > 0)
		serverlistpage = 0;
}

static void M_HandleServerSearch(INT32 choice)
{
	boolean exitmenu = false; // exit to previous menu

	switch (choice)
	{
		case KEY_DOWNARROW:
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_UPARROW:
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_ESCAPE:
			exitmenu = true;
			break;

		default:
			if (M_TextInputHandle(&menuinput, choice))
			{
				S_StartSound(NULL, sfx_menu1);
				M_SearchServerList();
			}
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

static void M_HandleServerPage(INT32 choice)
{
	boolean exitmenu = false; // exit to previous menu

	switch (choice)
	{
		case KEY_DOWNARROW:
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_UPARROW:
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1);
			break;
		case KEY_BACKSPACE:
		case KEY_ESCAPE:
			exitmenu = true;
			break;

		case KEY_ENTER:
		case KEY_RIGHTARROW:
			S_StartSound(NULL, sfx_menu1);
			if ((serverlistpage + 1) * SERVERS_PER_PAGE < serverlistsearchedcount)
			{
				oldserverlistpage = serverlistpage++;
				serverlistslidex = BASEVIDWIDTH;
			}
			break;
		case KEY_LEFTARROW:
			S_StartSound(NULL, sfx_menu1);
			if (serverlistpage > 0)
			{
				oldserverlistpage = serverlistpage--;
				serverlistslidex = -(BASEVIDWIDTH);
			}
			break;

		default:
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

static void M_Connect(INT32 choice)
{
	// do not call menuexitfunc
	M_ClearMenus(false);
	COM_BufAddText(va("connect node %d\n", serverlist[serverlistsearched[choice-FIRSTSERVERLINE + serverlistpage * SERVERS_PER_PAGE]].node));
}

static void M_ResetServerList(void)
{
	serverlistpage = 0;
	oldserverlistpage = 0;

	serverlistslidex = 0.0f;
	memset(serverlistsearched, 0, sizeof(serverlistsearched));
	serverlistsearchedcount = 0;
}

static void M_Refresh(INT32 choice)
{
	(void)choice;

	// first page of servers
	M_ResetServerList();

	CL_UpdateServerList();

#ifdef MASTERSERVER
#ifdef HAVE_THREADS
	Spawn_masterserver_thread("fetch-servers", Fetch_servers_thread);
#else/*HAVE_THREADS*/
	Fetch_servers_thread(NULL);
#endif/*HAVE_THREADS*/
#endif/*MASTERSERVER*/
}

#ifdef MASTERSERVER
static void M_DrawServerCountAndHorizontalBar(void)
{
	const char *text;
	INT32 radius;
	INT32 center = BASEVIDWIDTH/2;

	switch (M_GetWaitingMode())
	{
		case M_WAITING_VERSION:
			text = "Checking for updates";
			break;

		case M_WAITING_SERVERS:
			text = "Loading server list";
			break;

		default:
			if (serverlistcount > 0)
			{
				text = va("%d servers found", serverlistcount);
			}
			else
			{
				text = "No servers found";
			}
	}

	radius = V_StringWidth(text, 0) / 2;

	V_DrawCenteredString(center, currentMenu->y+40, 0, text);

	// Horizontal line!
	V_DrawFill(1, currentMenu->y+44, center - radius - 2, 1, 0);
	V_DrawFill(center + radius + 2, currentMenu->y+44, BASEVIDWIDTH - 1, 1, 0);
}
#endif

static void M_DrawServerLines(INT32 x, INT32 page)
{
	UINT16 i;
	const char *gt = "Unknown";
	const char *spd = "";

	for (i = 0; i < min(serverlistsearchedcount - page * SERVERS_PER_PAGE, SERVERS_PER_PAGE); i++)
	{
		INT32 slindex = serverlistsearched[i + page * SERVERS_PER_PAGE];
		UINT32 globalflags = ((serverlist[slindex].info.numberofplayer >= serverlist[slindex].info.maxplayer) ? V_TRANSLUCENT : 0)
			|((itemOn == FIRSTSERVERLINE+i) ? highlightflags : 0)|V_ALLOWLOWERCASE;

		V_DrawString(x, S_LINEY(i), globalflags, serverlist[slindex].info.servername);

		// Don't use color flags intentionally, the global yellow color will auto override the text color code
		if (serverlist[slindex].info.modifiedgame)
			V_DrawSmallString(x+202, S_LINEY(i)+8, globalflags, "\x85" "Mod");
		if (serverlist[slindex].info.cheatsenabled)
			V_DrawSmallString(x+222, S_LINEY(i)+8, globalflags, "\x83" "Cheats");

		V_DrawSmallString(x, S_LINEY(i)+8, globalflags,
								va("Ping: %u", (UINT32)LONG(serverlist[slindex].info.time)));

		gt = "Unknown";
		if (serverlist[slindex].info.gametype < NUMGAMETYPES)
			gt = Gametype_Names[serverlist[slindex].info.gametype];

		V_DrawSmallString(x+46,S_LINEY(i)+8, globalflags,
		                         va("Players: %02d/%02d", serverlist[slindex].info.numberofplayer, serverlist[slindex].info.maxplayer));

		V_DrawSmallString(x+112, S_LINEY(i)+8, globalflags, gt);

		// display game speed for race gametypes
		if (serverlist[slindex].info.gametype == GT_RACE)
		{
			spd = kartspeed_cons_t[serverlist[slindex].info.kartvars & SV_SPEEDMASK].strvalue;

			V_DrawSmallString(x+132, S_LINEY(i)+8, globalflags, va("(%s Speed)", spd));
		}

		MP_ConnectMenu[i+FIRSTSERVERLINE].status = IT_STRING | IT_CALL;
	}
}

static void M_DrawConnectMenu(void)
{
	UINT16 i;
	INT32 numPages = (serverlistsearchedcount+(SERVERS_PER_PAGE-1))/SERVERS_PER_PAGE;
	INT32 mservflags = V_ALLOWLOWERCASE;

	for (i = FIRSTSERVERLINE; i < min(localservercount, SERVERS_PER_PAGE)+FIRSTSERVERLINE; i++)
		MP_ConnectMenu[i].status = IT_STRING | IT_SPACE;

	if (!numPages)
		numPages = 1;

	// Page num
	V_DrawRightAlignedString(BASEVIDWIDTH - currentMenu->x, currentMenu->y + MP_ConnectMenu[mp_connect_page].alphaKey,
	                         highlightflags, va("%u of %d", serverlistpage+1, numPages));

	// Did you change the Server Browser address? Have a little reminder.
#ifdef MASTERSERVER
	if (CV_IsSetToDefault(&cv_masterserver))
		mservflags = mservflags|highlightflags|V_30TRANS;
	else
		mservflags = mservflags|warningflags;

	V_DrawRightAlignedSmallString(BASEVIDWIDTH - currentMenu->x, currentMenu->y+3 + MP_ConnectMenu[mp_connect_refresh].alphaKey,
	                         mservflags, va("MS: %s", cv_masterserver.string));

	M_DrawServerCountAndHorizontalBar();
#endif

	// When switching pages, slide the old page and the
	// new page across the screen
	if (oldserverlistpage != serverlistpage)
	{
		const float ease = serverlistslidex / 2.f;
		const INT32 offx = serverlistslidex > 0 ? BASEVIDWIDTH : -(BASEVIDWIDTH);
		const INT32 x = (FLOAT_TO_FIXED(serverlistslidex) + ease * R_GetTimeFrac(RTF_MENU)) / FRACUNIT;

		M_DrawServerLines(currentMenu->x + x - offx, oldserverlistpage);
		M_DrawServerLines(currentMenu->x + x, serverlistpage);

		if (interpTimerHackAllow)
		{
			serverlistslidex -= ease;

			if ((INT32)serverlistslidex == 0)
				oldserverlistpage = serverlistpage;
		}
	}
	else
	{
		M_DrawServerLines(currentMenu->x, serverlistpage);
	}

	INT32 input_y = currentMenu->menuitems[mp_connect_search].alphaKey;

	V_DrawFill(currentMenu->x, currentMenu->y+input_y, MAXSTRINGLENGTH*8+6, 8+6, 239);

	const INT32 xoff = 3, yoff = 3;

	if (itemOn != mp_connect_search)
		V_DrawString(currentMenu->x+xoff, currentMenu->y+yoff+input_y, V_ALLOWLOWERCASE, menuinput.buffer);
	else
		M_DrawTextInput(currentMenu->x+xoff, currentMenu->y+yoff+input_y, &menuinput, 0);

	localservercount = serverlistcount;

	M_DrawGenericMenu();
}

static boolean M_CancelConnect(void)
{
	D_CloseConnection();
	return true;
}

// Ascending order, not descending.
// The casts are safe as long as the caller doesn't do anything stupid.
#define SERVER_LIST_ENTRY_COMPARATOR(key) \
static int ServerListEntryComparator_##key(const void *entry1, const void *entry2) \
{ \
	const serverelem_t *sa = (const serverelem_t*)entry1, *sb = (const serverelem_t*)entry2; \
	if (sa->info.key != sb->info.key) \
		return sa->info.key - sb->info.key; \
	return strcmp(sa->info.servername, sb->info.servername); \
}

// This does descending instead of ascending.
#define SERVER_LIST_ENTRY_COMPARATOR_REVERSE(key) \
static int ServerListEntryComparator_##key##_reverse(const void *entry1, const void *entry2) \
{ \
	const serverelem_t *sa = (const serverelem_t*)entry1, *sb = (const serverelem_t*)entry2; \
	if (sb->info.key != sa->info.key) \
		return sb->info.key - sa->info.key; \
	return strcmp(sb->info.servername, sa->info.servername); \
}

SERVER_LIST_ENTRY_COMPARATOR(time)
SERVER_LIST_ENTRY_COMPARATOR(numberofplayer)
SERVER_LIST_ENTRY_COMPARATOR_REVERSE(numberofplayer)
SERVER_LIST_ENTRY_COMPARATOR_REVERSE(maxplayer)
SERVER_LIST_ENTRY_COMPARATOR(gametype)

// Special one for modified state.
static int ServerListEntryComparator_modified(const void *entry1, const void *entry2)
{
	const serverelem_t *sa = (const serverelem_t*)entry1, *sb = (const serverelem_t*)entry2;

	// Modified acts as 2 points, cheats act as one point.
	int modstate_a = (sa->info.cheatsenabled ? 1 : 0) | (sa->info.modifiedgame ? 2 : 0);
	int modstate_b = (sb->info.cheatsenabled ? 1 : 0) | (sb->info.modifiedgame ? 2 : 0);

	if (modstate_a != modstate_b)
		return modstate_a - modstate_b;

	// Default to strcmp.
	return strcmp(sa->info.servername, sb->info.servername);
}

void M_SortServerList(void)
{
	switch (cv_serversort.value)
	{
	case 0:		// Ping.
		qs22j(serverlist, serverlistcount, sizeof(serverelem_t), ServerListEntryComparator_time);
		break;
	case 1:		// Modified state.
		qs22j(serverlist, serverlistcount, sizeof(serverelem_t), ServerListEntryComparator_modified);
		break;
	case 2:		// Most players.
		qs22j(serverlist, serverlistcount, sizeof(serverelem_t), ServerListEntryComparator_numberofplayer_reverse);
		break;
	case 3:		// Least players.
		qs22j(serverlist, serverlistcount, sizeof(serverelem_t), ServerListEntryComparator_numberofplayer);
		break;
	case 4:		// Max players.
		qs22j(serverlist, serverlistcount, sizeof(serverelem_t), ServerListEntryComparator_maxplayer_reverse);
		break;
	case 5:		// Gametype.
		qs22j(serverlist, serverlistcount, sizeof(serverelem_t), ServerListEntryComparator_gametype);
		break;
	}

	M_SearchServerList();
}

#ifdef UPDATE_ALERT
#ifdef MASTERSERVER
static void M_CheckMODVersion(int id)
{
	char updatestring[500];
	const char *updatecheck = GetMODVersion(id);

	if (updatecheck)
	{
		sprintf(updatestring, UPDATE_ALERT_STRING, VERSIONSTRING, updatecheck);
#ifdef HAVE_THREADS
		I_lock_mutex(&m_menu_mutex);
#endif
		M_StartMessage(updatestring, NULL, MM_NOTHING);
#ifdef HAVE_THREADS
		I_unlock_mutex(m_menu_mutex);
#endif
	}
}
#endif //MASTERSERVER
#endif/*UPDATE_ALERT*/

#if defined (UPDATE_ALERT) && defined (HAVE_THREADS)
#ifdef MASTERSERVER
static void Check_new_version_thread(int *id)
{
	M_SetWaitingMode(M_WAITING_VERSION);

	M_CheckMODVersion(*id);

	if (Same_instance(*id))
	{
		Fetch_servers_thread(id);
	}
	else
	{
		free(id);
	}
}
#endif
#endif/*defined (UPDATE_ALERT) && defined (HAVE_THREADS)*/

#ifdef MASTERSERVER
static void M_ConnectMenu(INT32 choice)
{
	(void)choice;
	// modified game check: no longer handled
	// we don't request a restart unless the filelist differs

	// first page of servers
	M_ResetServerList();

	CL_UpdateServerList();

	// Reset
	M_TextInputSetString(&menuinput, "");

	M_SetupNextMenu(&MP_ConnectDef);
	itemOn = 0;

#if defined (MASTERSERVER) && defined (HAVE_THREADS)
	I_lock_mutex(&ms_QueryId_mutex);
	{
		ms_QueryId++;
	}
	I_unlock_mutex(ms_QueryId_mutex);

	I_lock_mutex(&ms_ServerList_mutex);
	{
		if (ms_ServerList)
		{
			free(ms_ServerList);
			ms_ServerList = NULL;
		}
	}
	I_unlock_mutex(ms_ServerList_mutex);

#ifdef UPDATE_ALERT
	Spawn_masterserver_thread("check-new-version", Check_new_version_thread);
#else/*UPDATE_ALERT*/
	Spawn_masterserver_thread("fetch-servers", Fetch_servers_thread);
#endif/*UPDATE_ALERT*/
#else/*defined (MASTERSERVER) && defined (HAVE_THREADS)*/
#ifdef UPDATE_ALERT
	M_CheckMODVersion(0);
#endif/*UPDATE_ALERT*/
	M_Refresh(0);
#endif/*defined (MASTERSERVER) && defined (HAVE_THREADS)*/
}

static void M_ConnectMenuModChecks(INT32 choice)
{
	(void)choice;
	// okay never mind we want to COMMUNICATE to the player pre-emptively instead of letting them try and then get confused when it doesn't work

	if (modifiedgame)
	{
		M_StartMessage("You have addons loaded.\nYou won't be able to join netgames!\n\nTo play online, restart the game\nand don't load any addons.\nSRB2Kart will automatically add\neverything you need when you join.\n\n(Press a key)\n", M_ConnectMenu, MM_EVENTHANDLER);
		return;
	}

	M_ConnectMenu(-1);
}
#endif

boolean firstDismissedNagThisBoot = true;
#ifdef MASTERSERVER

enum {
	CONNECT_MENU,
	STARTSERVER_MENU,
} connect_error_continue = CONNECT_MENU;

static void M_HandleMasterServerResetChoice(event_t *ev)
{
	INT32 choice = -1;

	choice = ev->data1;

	if (ev->type == ev_keydown)
	{
		if (choice == ' ' || choice == 'y' || choice == KEY_ENTER || choice == gamecontrol[0][gc_accelerate][0] || choice == gamecontrol[0][gc_accelerate][1])
		{
			CV_Set(&cv_masterserver, cv_masterserver.defaultvalue);
			CV_Set(&cv_masterserver_nagattempts, cv_masterserver_nagattempts.defaultvalue);
			S_StartSound(NULL, sfx_s221);
		}
		else
		{
			if (firstDismissedNagThisBoot)
			{
				if (cv_masterserver_nagattempts.value > 0)
				{
					CV_SetValue(&cv_masterserver_nagattempts, cv_masterserver_nagattempts.value - 1);
				}
				firstDismissedNagThisBoot = false;
			}
		}
	}
}

void M_PopupMasterServerConnectError(void)
{
	if (!CV_IsSetToDefault(&cv_masterserver) && cv_masterserver_nagattempts.value > 0)
	{
		M_StartMessage(M_GetText("There was a problem connecting to\ncustom Master Server\n\nYou've changed the Server Browser address.\nUnless you're from the future, this probably isn't what you want.\n\n\x83Press Accel\x80 to fix this and continue.\n"), connect_error_continue == STARTSERVER_MENU ? M_PreStartServerMenuChoice : M_PreConnectMenuChoice, MM_EVENTHANDLER);
	}
	else
	{
		M_StartMessage(M_GetText("There was a problem connecting to\nthe Master Server\n\nCheck the console for details.\n"), NULL, MM_NOTHING);
	}
}
#endif

static void M_PreStartServerMenu(INT32 choice)
{
	(void)choice;
#ifdef MASTERSERVER
	connect_error_continue = STARTSERVER_MENU;
#endif
	M_StartServerMenu(-1);

}
#ifdef MASTERSERVER
static void M_PreConnectMenu(INT32 choice)
{
	(void)choice;
	connect_error_continue = CONNECT_MENU;
	M_ConnectMenuModChecks(-1);
}

static void M_PreStartServerMenuChoice(event_t *ev)
{
	M_HandleMasterServerResetChoice(ev);
	M_StartServerMenu(-1);
}

static void M_PreConnectMenuChoice(event_t *ev)
{
	M_HandleMasterServerResetChoice(ev);
	M_ConnectMenuModChecks(-1);
}
#endif

//===========================================================================
// Start Server Menu
//===========================================================================

//
// FindFirstMap
//
// Finds the first map of a particular gametype (or returns the current map)
// Defaults to 1 if nothing found.
//
static INT32 M_FindFirstMap(INT32 gtype)
{
	INT32 i;

	if (mapheaderinfo[gamemap-1] && (mapheaderinfo[gamemap-1]->typeoflevel & gtype))
		return gamemap;

	for (i = 0; i < NUMMAPS; i++)
	{
		if (!mapheaderinfo[i])
			continue;
		if (!(mapheaderinfo[i]->typeoflevel & gtype))
			continue;
		return i + 1;
	}

	return 1;
}

static void M_StartServer(INT32 choice)
{
	UINT8 ssplayers = cv_splitplayers.value-1;

	(void)choice;

	if (currentMenu == &MP_OfflineServerDef)
		netgame = false;
	else
		netgame = true;

	multiplayer = true;

	strncpy(connectedservername, cv_servername.string, MAXSERVERNAME);

	// Still need to reset devmode
	cv_debug = 0;

	if (demo.playback)
		G_StopDemo();

	if (!cv_nextmap.value)
		CV_SetValue(&cv_nextmap, G_RandMap(G_TOLFlag(cv_newgametype.value), -1, false, 0, false, NULL)+1);

	if (cv_maxplayers.value < ssplayers+1)
		CV_SetValue(&cv_maxplayers, ssplayers+1);

	if (splitscreen != ssplayers)
	{
		splitscreen = ssplayers;
		SplitScreen_OnChange();
	}

	if (currentMenu == &MP_OfflineServerDef) // offline server
	{
		paused = false;
		SV_StartSinglePlayerServer();
		multiplayer = true; // yeah, SV_StartSinglePlayerServer clobbers this...
		D_MapChange(cv_nextmap.value, cv_newgametype.value, (boolean)cv_kartencore.value, 1, 1, false, false);
	}
	else
	{
		D_MapChange(cv_nextmap.value, cv_newgametype.value, (boolean)cv_kartencore.value, 1, 1, false, false);
		COM_BufAddText("dummyconsvar 1\n");
	}

	M_ClearMenus(true);
}

static void M_DrawLevelSelectOnly(boolean leftfade, boolean rightfade)
{
	lumpnum_t lumpnum;
	patch_t *PictureOfLevel;
	INT32 x, y, w, i, oldval, trans, dupadjust = (vid.scaledwidth - BASEVIDWIDTH)>>1;

	// so it doesent show in record attack menu
	if (levellistmode != LLM_RECORDATTACK && M_SecretUnlocked(SECRET_ENCORE)) // gotta have it unlocked first ofc
	{
		char encoretoggle[32] = {0};
		const char *item1 = gamecontrol[0][gc_fire][0] != 0 ? G_KeynumToString(gamecontrol[0][gc_fire][0]) : NULL;
		const char *item2 = gamecontrol[0][gc_fire][1] != 0 ? G_KeynumToString(gamecontrol[0][gc_fire][1]) : NULL;

		if (item1 != NULL && item2 != NULL)
			snprintf(encoretoggle, 32, "%s/%s - Toggle Encore", item1, item2);
		else
			snprintf(encoretoggle, 32, "%s - Toggle Encore", item1 != NULL ? item1 : item2 != NULL ? item2 : "Item");

		V_DrawThinString(1, BASEVIDHEIGHT-8-1, V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_TRANSLUCENT|V_ALLOWLOWERCASE, encoretoggle);
	}

	//  A 160x100 image of the level as entry MAPxxP
	if (cv_nextmap.value)
	{
		lumpnum = W_CheckNumForName(va("%sP", G_BuildMapName(cv_nextmap.value)));
		if (lumpnum != LUMPERROR)
			PictureOfLevel = (patch_t *)W_CachePatchNum(lumpnum, PU_PATCH);
		else
			PictureOfLevel = (patch_t *)W_CachePatchName("BLANKLVL", PU_PATCH);
	}
	else
		PictureOfLevel = (patch_t *)W_CachePatchName("RANDOMLV", PU_PATCH);

	w = PictureOfLevel->width/2;
	i = PictureOfLevel->height/2;
	x = BASEVIDWIDTH/2 - w/2;
	y = currentMenu->y + 130 + 8 - i;

	if (currentMenu->menuitems[itemOn].itemaction == &cv_nextmap && skullAnimCounter < 4)
		trans = 120;
	else
		trans = G_GetGametypeColor(cv_newgametype.value);

	V_DrawFill(x-1, y-1, w+2, i+2, trans); // variable reuse...

	if (cv_nextmap.value && cv_showtrackaddon.value)
	{
		static tic_t namescroll = 0;
		static INT32 namescrollmap = 0;
		char namescrollbuf[64]= {0};

		if (cv_nextmap.value != namescrollmap)
		{
			namescrollmap = cv_nextmap.value;
			namescroll = 0;
		}

		if (renderisnewtic)
			namescroll++;

		char *addonname = wadfiles[mapwads[cv_nextmap.value-1]]->filename;
		INT32 len;
		INT32 charlimit = min((size_t)(21 + (dupadjust/5)), sizeof(namescrollbuf)-1);

		nameonly(addonname);
		len = strlen(addonname);

		if (len > charlimit)
			M_ScrollString(addonname, len, namescrollbuf, charlimit, namescroll);
		else
			strncpy(namescrollbuf, addonname, sizeof(namescrollbuf) - 1);

		V_DrawThinString(x+w+5, y+i-8, V_TRANSLUCENT|MENUCAPS, namescrollbuf); // variable reuse...
	}

	if (!cv_kartencore.value || gamestate == GS_TIMEATTACK || cv_newgametype.value != GT_RACE)
		V_DrawSmallScaledPatch(x, y, 0, PictureOfLevel);
	else
	{
		/*UINT8 *mappingforencore = NULL;
		if ((lumpnum = W_CheckNumForName(va("%sE", mapname))) != LUMPERROR)
			mappingforencore = (patch_t *)W_CachePatchNum(lumpnum, PU_PATCH);*/

		V_DrawFixedPatch((x+w)<<FRACBITS, (y)<<FRACBITS, FRACUNIT/2, V_FLIP, PictureOfLevel, NULL);

		{
			static angle_t rubyfloattime = 0;
			const fixed_t rubyheight = FINESINE(rubyfloattime>>ANGLETOFINESHIFT);
			V_DrawFixedPatch((x+w/2)<<FRACBITS, ((y+i/2)<<FRACBITS) - (rubyheight<<1), FRACUNIT, 0, (patch_t *)W_CachePatchName("RUBYICON", PU_PATCH), NULL);
			rubyfloattime += FixedMul(ANGLE_MAX/NEWTICRATE, renderdeltatics);
		}
	}

	y += i/4;
	i = cv_nextmap.value - 1;
	trans = (leftfade ? V_TRANSLUCENT : 0);

#define horizspac 2
	do
	{
		oldval = i;
		do
		{
			i--;
			if (i == -2)
				i = NUMMAPS-1;

			if (i == oldval)
				return;

			if (i >= 0 && !mapheaderinfo[i])
				continue; // Don't allocate the header.  That just makes memory usage skyrocket.

		} while (!M_CanShowLevelInList(i, cv_newgametype.value));

		//  A 160x100 image of the level as entry MAPxxP
		if (i+1)
		{
			lumpnum = W_CheckNumForName(va("%sP", G_BuildMapName(i+1)));
			if (lumpnum != LUMPERROR)
				PictureOfLevel = (patch_t *)W_CachePatchNum(lumpnum, PU_PATCH);
			else
				PictureOfLevel = (patch_t *)W_CachePatchName("BLANKLVL", PU_PATCH);
		}
		else
			PictureOfLevel = (patch_t *)W_CachePatchName("RANDOMLV", PU_PATCH);

		x -= horizspac + w/2;

		V_DrawTinyScaledPatch(x, y, trans, PictureOfLevel);
	} while (x > horizspac-dupadjust);

	x = (BASEVIDWIDTH + w)/2 + horizspac;
	i = cv_nextmap.value - 1;
	trans = (rightfade ? V_TRANSLUCENT : 0);

	while (x < BASEVIDWIDTH+dupadjust-horizspac)
	{
		oldval = i;
		do
		{
			i++;
			if (i == NUMMAPS)
				i = -1;

			if (i == oldval)
				return;

			if (i >= 0 && !mapheaderinfo[i])
				continue; // Don't allocate the header.  That just makes memory usage skyrocket.

		} while (!M_CanShowLevelInList(i, cv_newgametype.value));

		//  A 160x100 image of the level as entry MAPxxP
		if (i+1)
		{
			lumpnum = W_CheckNumForName(va("%sP", G_BuildMapName(i+1)));
			if (lumpnum != LUMPERROR)
				PictureOfLevel = (patch_t *)W_CachePatchNum(lumpnum, PU_PATCH);
			else
				PictureOfLevel = (patch_t *)W_CachePatchName("BLANKLVL", PU_PATCH);
		}
		else
			PictureOfLevel = (patch_t *)W_CachePatchName("RANDOMLV", PU_PATCH);

		V_DrawTinyScaledPatch(x, y, trans, PictureOfLevel);

		x += horizspac + w/2;
	}
#undef horizspac
}

static void M_DrawServerMenu(void)
{
	M_DrawLevelSelectOnly(false, false);
#ifdef MASTERSERVER
	if (currentMenu == &MP_ServerDef && cv_advertise.value) // Remind players where they're hosting.
	{
		int mservflags = V_ALLOWLOWERCASE;
		if (CV_IsSetToDefault(&cv_masterserver))
			mservflags = mservflags|highlightflags|V_30TRANS;
		else
			mservflags = mservflags|warningflags;
		V_DrawCenteredThinString(BASEVIDWIDTH/2, BASEVIDHEIGHT-12, mservflags, va("Master Server: %s", cv_masterserver.string));
	}
#endif
	M_DrawGenericMenu();

}

static void M_MapChange(INT32 choice)
{
	(void)choice;

	levellistmode = LLM_CREATESERVER;

	CV_SetValue(&cv_newgametype, gametype);
	CV_SetValue(&cv_nextmap, gamemap);

	M_PrepareLevelSelect();
	M_SetupNextMenu(&MISC_ChangeLevelDef);
}

static void M_StartOfflineServerMenu(INT32 choice)
{
	(void)choice;
	levellistmode = LLM_CREATESERVER;
	M_PrepareLevelSelect();
	M_SetupNextMenu(&MP_OfflineServerDef);
}

static void M_StartServerMenu(INT32 choice)
{
	(void)choice;
	levellistmode = LLM_CREATESERVER;
	M_PrepareLevelSelect();
	M_SetupNextMenu(&MP_ServerDef);
#ifdef MASTERSERVER
	Get_rules();
#endif
	M_PopupMasterServerRules();
}

// ==============
// CONNECT VIA IP
// ==============

void M_Multiplayer(INT32 choice)
{
	(void)choice;
	memset(setupm_ip, 0, sizeof(setupm_ip));
	M_TextInputInit(&setupm_input_ip, setupm_ip, sizeof(setupm_ip));
	M_SetupNextMenu(&MP_MainDef);
}

// Draw the funky Connect IP menu. Tails 11-19-2002
// So much work for such a little thing!
static void M_DrawMPMainMenu(void)
{
	INT32 x = currentMenu->x;
	INT32 y = currentMenu->y;

	// use generic drawer for cursor, items and title
	M_DrawGenericMenu();

#if MAXPLAYERS != 16
Update the maxplayers label...
#endif
	V_DrawRightAlignedString(BASEVIDWIDTH-x, y+MP_MainMenu[4].alphaKey,
		((itemOn == 4) ? highlightflags : 0)|MENUCAPS, "(2-16 Players)");

	V_DrawRightAlignedString(BASEVIDWIDTH-x, y+MP_MainMenu[5].alphaKey,
		((itemOn == 5) ? highlightflags : 0)|MENUCAPS,
		"(2-4 players)"
		);

	y += MP_MainMenu[9].alphaKey;

	V_DrawFill(x+5, y+4+5, /*16*8 + 6,*/ BASEVIDWIDTH - 2*(x+5), 8+6, 239);

	// draw name string
	if (itemOn != 9)
		V_DrawString(x+8,y+12, V_ALLOWLOWERCASE, setupm_ip);
	else
		M_DrawTextInputScroll(x+8, y+12, &setupm_input_ip, 0, SETUPM_IP_MAXSIZE);

	// character bar, ripped off the color bar :V
	{
#define iconwidth 32
#define spacingwidth 32
#define incrwidth (iconwidth + spacingwidth)
		UINT8 i = 0, pskin, pcol;
		// player arrangement width, but there's also a chance i'm a furry, shhhhhh
		const INT32 paw = iconwidth + 3*incrwidth;
		INT32 trans = 0;
		UINT8 *colmap;
		x = BASEVIDWIDTH/2 - paw/2;
		y = currentMenu->y + 32;

		while (++i <= 4)
		{
			switch (i)
			{
				default:
					pskin = R_SkinAvailable(cv_skin.string);
					pcol = cv_playercolor.value;
					break;
				case 2:
					pskin = R_SkinAvailable(cv_skin2.string);
					pcol = cv_playercolor2.value;
					break;
				case 3:
					pskin = R_SkinAvailable(cv_skin3.string);
					pcol = cv_playercolor3.value;
					break;
				case 4:
					pskin = R_SkinAvailable(cv_skin4.string);
					pcol = cv_playercolor4.value;
					break;
			}

			if (pskin >= MAXSKINS)
				pskin = 0;

			if (!trans && i > cv_splitplayers.value)
				trans = V_TRANSLUCENT;

			colmap = R_GetTranslationColormap(pskin, pcol, GTC_MENUCACHE);

			V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, trans, facewantprefix[pskin], colmap);

			if (itemOn == 2 && i == setupm_pselect)
			{
				static fixed_t cursorframe = 0;

				cursorframe += renderdeltatics / 4;
				for (; cursorframe > 7 * FRACUNIT; cursorframe -= 7 * FRACUNIT) {}

				V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, 0, (patch_t *)W_CachePatchName(va("K_BHILI%d", (cursorframe >> FRACBITS) + 1), PU_PATCH), NULL);
			}

			x += incrwidth;
		}
#undef incrwidth
#undef spacingwidth
#undef iconwidth
	}
}

static void M_SetupMultiHandler(INT32 choice)
{
	boolean exitmenu = false;  // exit to previous menu and send name change

	switch (choice)
	{
		case KEY_LEFTARROW:
			if (cv_splitplayers.value > 1)
			{
				if (--setupm_pselect < 1)
					setupm_pselect = cv_splitplayers.value;
				S_StartSound(NULL, sfx_menu1); // Tails
			}
			break;

		case KEY_RIGHTARROW:
			if (cv_splitplayers.value > 1)
			{
				if (++setupm_pselect > cv_splitplayers.value)
					setupm_pselect = 1;
				S_StartSound(NULL, sfx_menu1); // Tails
			}
			break;

		case KEY_DOWNARROW:
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1); // Tails
			break;

		case KEY_UPARROW:
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1); // Tails
			break;

		case KEY_ENTER:
		{
			S_StartSound(NULL, sfx_menu1); // Tails
			currentMenu->lastOn = itemOn;
			switch (setupm_pselect)
			{
				case 2:
					M_SetupMultiPlayer2();
					return;
				case 3:
					M_SetupMultiPlayer3();
					return;
				case 4:
					M_SetupMultiPlayer4();
					return;
				default:
					M_SetupMultiPlayer();
					return;
			}

			break;
		}

		case KEY_ESCAPE:
			exitmenu = true;
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu (currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// Tails 11-19-2002
static void M_ConnectIP(INT32 choice)
{
	(void)choice;

	if (*setupm_ip == 0)
	{
		M_StartMessage("You must specify an IP address.\n", NULL, MM_NOTHING);
		return;
	}

	M_ClearMenus(true);

	COM_BufAddText(va("connect \"%s\"\n", setupm_ip));

	// A little "please wait" message.
	M_DrawTextBox(56, BASEVIDHEIGHT/2-12, 24, 2);
	V_DrawCenteredString(BASEVIDWIDTH/2, BASEVIDHEIGHT/2, 0, "Connecting to server...");
	I_OsPolling();
	if (rendermode == render_soft)
		I_FinishUpdate(); // page flip or blit buffer
}

//Join Last server
static void M_ConnectLastServer(INT32 choice)
{
	(void)choice;

	if (!*cv_lastserver.string)
	{
		M_StartMessage("You haven't previously joined a server.\n", NULL, MM_NOTHING);
		return;
	}

	M_ClearMenus(true);
	COM_BufAddText(va("connect \"%s\"\n", cv_lastserver.string));
}

// Tails 11-19-2002
static void M_HandleConnectIP(INT32 choice)
{
	boolean exitmenu = false;  // exit to previous menu and send name change

	switch (choice)
	{
		case KEY_DOWNARROW:
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1); // Tails
			break;

		case KEY_UPARROW:
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1); // Tails
			break;

		case KEY_ENTER:
			S_StartSound(NULL, sfx_menu1); // Tails
			currentMenu->lastOn = itemOn;
			M_ConnectIP(1);
			break;

		case KEY_ESCAPE:
			exitmenu = true;
			break;

		case KEY_LEFTARROW:
		case KEY_RIGHTARROW:
		case KEY_BACKSPACE:
		case KEY_DEL:
			if (M_TextInputHandle(&setupm_input_ip, choice))
				S_StartSound(NULL, sfx_menu1); // Tails
			break;

		default:
			// Rudimentary number and period enforcing - also allows letters so hostnames can be used instead
			// ctrl-v allows to bypass that anyway, so we just remove that for now to allow stuff like shift+insert
			/*if ((choice >= '-' && choice <= ':') || (choice >= 'A' && choice <= 'Z') || (choice >= 'a' && choice <= 'z')
				|| (choice >= 199 && choice <= 211 && choice != 202 && choice != 206))*/ //numpad too!
			{
				if (M_TextInputHandle(&setupm_input_ip, choice))
					S_StartSound(NULL, sfx_menu1); // Tails
			}
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// ========================
// MULTIPLAYER PLAYER SETUP
// ========================
// Tails 03-02-2002

#define SELECTEDSTATSCOUNT skinstatscount[setupm_skinxpos][setupm_skinypos]
#define LASTSELECTEDSTAT skinstats[setupm_skinxpos][setupm_skinypos][skinstatscount[setupm_skinxpos][setupm_skinypos]]

#define SKINGRIDWIDTH 8
#define SKINGRIDHEIGHT 6

#define SKINGRIDNEWWIDTH 8
#define SKINGRIDNEWHEIGHT 9

static const char *sortNames[] = {
	"Name",
	"Internal name",
	"Speed",
	"Weight",
	"Preferred color",
	"ID"
};

static void M_DrawSetupMultiPlayerMenu(void)
{
	INT32 mx, my, st, flags = 0;
	INT32 tw = 0;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	patch_t *statbg  = (patch_t *)W_CachePatchName("K_STATBG", PU_PATCH);
	patch_t *statlr  = (patch_t *)W_CachePatchName("K_STATLR", PU_PATCH);
	patch_t *statud  = (patch_t *)W_CachePatchName("K_STATUD", PU_PATCH);
	patch_t *statdot = (patch_t *)W_CachePatchName("K_SDOT0" , PU_PATCH);
	patch_t *patch;
	UINT8 frame;
	UINT8 speed;
	UINT8 weight;
	UINT8 i;
	UINT8 s, w;
	const UINT8 *flashcol = V_GetStringColormap(highlightflags);
	INT32 skinnum = 0;
	INT32 statx, staty;
	UINT32 speenframe;
	INT32 sltw, actw, hetw;
	UINT8 skintodisplay;
	INT32 nameboxaddy = 0;
	int statoffset = 0;
	int gridyoffset = 0;

	mx = MP_PlayerSetupDef.x;
	my = MP_PlayerSetupDef.y;

	statx = (BASEVIDWIDTH - mx - 118);
	staty = (my+62);

	// use generic drawer for cursor, items and title
	M_DrawGenericMenu();

	// Offsets
	switch (cv_skinselectmenu.value)
	{
		case SKINMENUTYPE_EXTENDED:
			nameboxaddy = 6;
			break;
		case SKINMENUTYPE_GRID:
			nameboxaddy = 6;
			break;
		default:
			nameboxaddy = 0;
			break;
	}

	M_DrawTextBox(mx + 32, my - 8 + nameboxaddy, MAXPLAYERNAME, 1);

	if (itemOn != 0)
		V_DrawString(mx + 40, my + nameboxaddy, V_ALLOWLOWERCASE, setupm_input.buffer);
	else
		M_DrawTextInput(mx + 40, my + nameboxaddy, &setupm_input, 0);

	// draw skin string
	st = V_StringWidth(skins[setupm_fakeskin].realname, 0);
	switch (cv_skinselectmenu.value)
	{
		case SKINMENUTYPE_EXTENDED:
#define GETSELECTEDSKINNAME (itemOn == 1 && setupm_skinselect < numskins ? skins[skinsorted[setupm_skinselect]].realname : skins[setupm_fakeskin].realname)
#define GETSELECTEDSPEED (itemOn == 1 && setupm_skinselect < numskins ? skins[skinsorted[setupm_skinselect]].kartspeed : skins[setupm_fakeskin].kartspeed)
#define GETSELECTEDWEIGHT (itemOn == 1 && setupm_skinselect < numskins ? skins[skinsorted[setupm_skinselect]].kartweight : skins[setupm_fakeskin].kartweight)

			tw = V_StringWidth("Character", 0);//V_StringWidth(GETSELECTEDSKINNAME, 0);
			st = V_StringWidth(GETSELECTEDSKINNAME, 0);

			V_DrawString((mx+(tw/2)) - (st/2), my + 37,
				((MP_PlayerSetupMenu[2].status & IT_TYPE) == IT_SPACE ? V_TRANSLUCENT : 0) | highlightflags | V_ALLOWLOWERCASE,
				GETSELECTEDSKINNAME);
			if (statdp == true)
				statoffset = 50;
			else
				statoffset = 113;

			V_DrawString(statx - statoffset, staty - 10, V_6WIDTHSPACE, va("\x84%dS \x87%dW", GETSELECTEDSPEED, GETSELECTEDWEIGHT));
#undef GETSELECTEDSKINNAME
#undef GETSELECTEDSPEED
#undef GETSELECTEDWEIGHT
			break;
		case SKINMENUTYPE_GRID:
#define GETSELECTEDSKINNAME (itemOn == 1 && setupm_skinselect < numskins ? skins[skinsorted[setupm_skinselect]].realname : skins[setupm_fakeskin].realname)
			tw = V_StringWidth("Character", 0);//V_StringWidth(GETSELECTEDSKINNAME, 0);
			st = V_StringWidth(GETSELECTEDSKINNAME, 0);
			V_DrawString((mx+(tw/2)) - (st/2), my + 37,
				((MP_PlayerSetupMenu[2].status & IT_TYPE) == IT_SPACE ? V_TRANSLUCENT : 0) | highlightflags | V_ALLOWLOWERCASE,
				GETSELECTEDSKINNAME);
#undef GETSELECTEDSKINNAME
			break;
		case SKINMENUTYPE_2D:

			skintodisplay = setupm_fakeskin;
			if (setupm_skinlockedselect) //show the skin we are trying to select
				skintodisplay = skinstats[setupm_skinxpos][setupm_skinypos][setupm_skinselect];
			else if (skinstatscount[setupm_skinxpos][setupm_skinypos] && itemOn == 1)
				skintodisplay = skinstats[setupm_skinxpos][setupm_skinypos][(I_GetTime()/TICRATE)%SELECTEDSTATSCOUNT];

			tw = V_StringWidth("Character", 0);
			st = V_StringWidth(skins[skintodisplay].realname, 0);
			V_DrawString((mx+(tw/2)) - (st/2), my + 37,
				((MP_PlayerSetupMenu[2].status & IT_TYPE) == IT_SPACE ? V_TRANSLUCENT : 0) | highlightflags | V_ALLOWLOWERCASE,
				skins[skintodisplay].realname);
			// the menu is now 2d, no need for scroll arrows...
			if (itemOn == 1 && setupm_skinlockedselect)
			{
				//V_DrawFill(mx + 43 - (72 / 2), my + 65, 72, 84, 239);
				V_DrawCharacter(mx + 43 - (72 / 2) - 8 - (skullAnimCounter / 5), my + 65 + (84 / 2),
								'\x1C' | highlightflags, false); // left arrow
				V_DrawCharacter(mx + 43 - (72 / 2) + 72 + (skullAnimCounter / 5), my + 65 + (84 / 2),
								'\x1D' | highlightflags, false); // right arrow
			}
			break;
		default:
			V_DrawString(BASEVIDWIDTH - mx - st, my + 16,
						((MP_PlayerSetupMenu[2].status & IT_TYPE) == IT_SPACE ? V_TRANSLUCENT : 0)|highlightflags|V_ALLOWLOWERCASE,
						skins[setupm_fakeskin].realname);
			if (itemOn == 1)
			{
				V_DrawCharacter(BASEVIDWIDTH - mx - 10 - st - (skullAnimCounter/5), my + 16,
								'\x1C' | highlightflags, false); // left arrow
				V_DrawCharacter(BASEVIDWIDTH - mx + 2 + (skullAnimCounter/5), my + 16,
								'\x1D' | highlightflags, false); // right arrow
			}
			break;
	}

	// draw the name of the color you have chosen
	// Just so people don't go thinking that "Default" is Green.
	st = V_StringWidth(KartColor_Names[setupm_fakecolor], 0);

	switch (cv_skinselectmenu.value)
	{
		case SKINMENUTYPE_EXTENDED:
			V_DrawString(mx, my + 164, highlightflags | V_ALLOWLOWERCASE, KartColor_Names[setupm_fakecolor]); // SRB2kart
			if (itemOn == 2)
			{
				V_DrawCharacter(mx - 10/* - st*/ - (skullAnimCounter/5), my + 164,
					'\x1C' | highlightflags, false); // left arrow
				V_DrawCharacter(mx + 2 + st + (skullAnimCounter/5), my + 164,
					'\x1D' | highlightflags, false); // right arrow
			}
			break;
		case SKINMENUTYPE_GRID:
		case SKINMENUTYPE_2D:
			V_DrawString(mx, my + 152, highlightflags | V_ALLOWLOWERCASE, KartColor_Names[setupm_fakecolor]); // SRB2kart
			if (itemOn == 2)
			{
				V_DrawCharacter(mx - 10/* - st*/ - (skullAnimCounter/5), my + 152,
					'\x1C' | highlightflags, false); // left arrow
				V_DrawCharacter(mx + 2 + st + (skullAnimCounter/5), my + 152,
					'\x1D' | highlightflags, false); // right arrow
			}
			break;
		default:
			V_DrawString(BASEVIDWIDTH - mx - st, my + 152, highlightflags|V_ALLOWLOWERCASE, KartColor_Names[setupm_fakecolor]);	// SRB2kart
			if (itemOn == 2)
			{
				V_DrawCharacter(BASEVIDWIDTH - mx - 10 - st - (skullAnimCounter/5), my + 152,
					'\x1C' | highlightflags, false); // left arrow
				V_DrawCharacter(BASEVIDWIDTH - mx + 2 + (skullAnimCounter/5), my + 152,
					'\x1D' | highlightflags, false); // right arrow
			}
			break;
	}

#define GRIDSTATOFFSET 0

	switch (cv_skinselectmenu.value)
	{
		case SKINMENUTYPE_EXTENDED:
			// SRB2Kart: draw the stat backer
			//This is where stats and shit would go.
			// gonna put the sorttype here as well
			V_DrawSmallString(statx-3, staty-37, V_6WIDTHSPACE|highlightflags, "Sort:");
			V_DrawSmallString(statx+17, staty-37, V_6WIDTHSPACE|highlightflags, sortNames[cv_skinselectgridsort.value]);
			if (itemOn == 1)
				V_DrawSmallString(statx+101, staty-37, V_6WIDTHSPACE|highlightflags, "BS: change");

#define GETSELECTEDSPEED (itemOn == 1 && setupm_skinselect < numskins ? skins[skinsorted[setupm_skinselect]].kartspeed : skins[setupm_fakeskin].kartspeed)
#define GETSELECTEDWEIGHT (itemOn == 1 && setupm_skinselect < numskins ? skins[skinsorted[setupm_skinselect]].kartweight : skins[setupm_fakeskin].kartweight)

			if (statdp == true)
			{
				//Background
				V_DrawScaledPatch(statx - 50, staty + 4, 0, (patch_t *)W_CachePatchName("K_STATNB", PU_PATCH));

				//Speed
				for (i = 0; i < GETSELECTEDSPEED; i++) // draw the stat bars
				{
					if (i == 0)
						V_DrawScaledPatch(statx - 45, staty + 63, 0, (patch_t *)W_CachePatchName("K_STATN1", PU_PATCH));
					else if (i == GETSELECTEDSPEED -1 )
						V_DrawScaledPatch(statx - 45, staty + 63 -(5 *i), 0, (patch_t *)W_CachePatchName("K_STATN3", PU_PATCH));
					else
						V_DrawScaledPatch(statx - 45, staty + 63 -(5 *i), 0, (patch_t *)W_CachePatchName("K_STATN2", PU_PATCH));
				}

				//Weight
				for (i = 0; i < GETSELECTEDWEIGHT; i++) // draw the stat bars
				{

					if (i == 0)
						V_DrawScaledPatch(statx - 30, staty + 63, 0, (patch_t *)W_CachePatchName("K_STATN4", PU_PATCH));
					else if (i == GETSELECTEDWEIGHT -1)
						V_DrawScaledPatch(statx - 30, staty + 63 -(5 *i), 0, (patch_t *)W_CachePatchName("K_STATN6", PU_PATCH));
					else
						V_DrawScaledPatch(statx - 30, staty + 63 -(5 *i), 0, (patch_t *)W_CachePatchName("K_STATN5", PU_PATCH));
				}
			}

#undef GETSELECTEDSPEED
#undef GETSELECTEDWEIGHT

			break;
		case SKINMENUTYPE_GRID:
			// SRB2Kart: draw the stat backer
			// labels
			V_DrawSmallString(statx+12+GRIDSTATOFFSET, staty+67, V_6WIDTHSPACE|highlightflags, "Acceleration");
			V_DrawSmallString(statx+76+GRIDSTATOFFSET, staty+67, V_6WIDTHSPACE|highlightflags, "Max Speed");
			V_DrawSmallString(statx+14+GRIDSTATOFFSET, staty+75, V_6WIDTHSPACE|highlightflags, "Handling");
			V_DrawSmallString(statx+21+GRIDSTATOFFSET, staty+108, V_6WIDTHSPACE|highlightflags, "Weight");
			// label arrows
			V_DrawFixedPatch(((statx+61+GRIDSTATOFFSET)<<FRACBITS) + (FRACUNIT>>1), (staty+67)<<FRACBITS, FRACUNIT>>1, 0, statlr, flashcol);
			V_DrawFixedPatch((statx+40+GRIDSTATOFFSET)<<FRACBITS, (staty+80)<<FRACBITS, FRACUNIT>>1, 0, statud, flashcol);
			// bg
			V_DrawFixedPatch(((statx+48+GRIDSTATOFFSET)<<FRACBITS)+(FRACUNIT>>1), (staty+73)<<FRACBITS, FRACUNIT>>1, 0, statbg, NULL);

			for (i = 0; i < numskins; i++) // draw the stat dots
			{
				if (i != setupm_fakeskin && R_SkinAvailable(skins[i].name) != -1)
				{
					speed = skins[i].kartspeed;
					weight = skins[i].kartweight;
					V_DrawFixedPatch((((statx+46+GRIDSTATOFFSET) + (speed*4))<<FRACBITS) + (FRACUNIT>>1), (((staty+71) + (weight*4))<<FRACBITS), FRACUNIT>>1, 0, statdot, NULL);
				}
			}

			// gonna put the sorttype here as well
			V_DrawSmallString(statx+85, staty-57, V_6WIDTHSPACE|highlightflags, "Sort:");
			V_DrawSmallString(statx+89, staty-52, V_6WIDTHSPACE|highlightflags, sortNames[cv_skinselectgridsort.value]);
			if (itemOn == 1)
				V_DrawSmallString(statx+85, staty-47, V_6WIDTHSPACE|highlightflags, "Backspace: change");

			break;
		case SKINMENUTYPE_2D:
#define SKINXSHIFT 55
			statx = ((BASEVIDWIDTH / 2) - (18 * 4)) - 8 + SKINXSHIFT;
			staty = ((BASEVIDHEIGHT / 2) - (18 * 4)) - 8;
			sltw = V_ThinStringWidth("Accel", V_6WIDTHSPACE);
			actw = V_ThinStringWidth("Turn", V_6WIDTHSPACE);
			hetw = V_ThinStringWidth("Heavy", V_6WIDTHSPACE);

#define DRAWSLOW(x, y) V_DrawThinString((x), (y), V_6WIDTHSPACE | highlightflags, "Accel")
#define DRAWFAST(x, y) V_DrawThinString((x), (y), V_6WIDTHSPACE | highlightflags, "Speed")
#define DRAWACCEL(x, y) V_DrawThinString((x), (y), V_6WIDTHSPACE | highlightflags, "Turn")
#define DRAWHEAVY(x, y) V_DrawThinString((x), (y), V_6WIDTHSPACE | highlightflags, "Heavy")
#define TEXTVERTSHIFT 10

			DRAWSLOW(statx - sltw - 2, staty);
			DRAWSLOW(statx - sltw - 2, staty - TEXTVERTSHIFT+ (9 * 18) - 11);
			DRAWFAST(statx + (9 * 18), staty);
			DRAWFAST(statx + (9 * 18), staty - TEXTVERTSHIFT+ (9 * 18) - 11);
			DRAWACCEL(statx - actw - 2, staty + TEXTVERTSHIFT);
			DRAWACCEL(statx + (9 * 18), staty + TEXTVERTSHIFT);
			DRAWHEAVY(statx - hetw - 2, staty + (9 * 18) - 11);
			DRAWHEAVY(statx + (9 * 18), staty + (9 * 18) - 11);

#undef DRAWSLOW
#undef DRAWFAST
#undef DRAWACCEL
#undef DRAWHEAVY
#undef TEXTVERTSHIFT
			break;
		default:
			// SRB2Kart: draw the stat backer
			// labels
			V_DrawThinString(statx+16, staty, V_6WIDTHSPACE|highlightflags, "Acceleration");
			V_DrawThinString(statx+91, staty, V_6WIDTHSPACE|highlightflags, "Max Speed");
			V_DrawThinString(statx, staty+12, V_6WIDTHSPACE|highlightflags, "Handling");
			V_DrawThinString(statx+7, staty+77, V_6WIDTHSPACE|highlightflags, "Weight");
			// label arrows
			V_DrawFixedPatch((statx+64)<<FRACBITS, staty<<FRACBITS, FRACUNIT, 0, statlr, flashcol);
			V_DrawFixedPatch((statx+24)<<FRACBITS, (staty+22)<<FRACBITS, FRACUNIT, 0, statud, flashcol);
			// bg
			V_DrawFixedPatch((statx+34)<<FRACBITS, (staty+10)<<FRACBITS, FRACUNIT, 0, statbg, NULL);

			for (i = 0; i < numskins; i++) // draw the stat dots
			{
				if (i != setupm_fakeskin && R_SkinAvailable(skins[i].name) != -1)
				{
					speed = skins[i].kartspeed;
					weight = skins[i].kartweight;
					V_DrawFixedPatch(((BASEVIDWIDTH - mx - 80) + ((speed-1)*8))<<FRACBITS, ((my+76) + ((weight-1)*8))<<FRACBITS, FRACUNIT, 0, statdot, NULL);
				}
			}
			break;
	}

	switch (cv_skinselectmenu.value)
	{
			//Skin grid stuff
			case SKINMENUTYPE_EXTENDED:
			gridyoffset = 10;
			for (s = 0; s < SKINGRIDNEWWIDTH*SKINGRIDNEWHEIGHT; s++)
			{
				INT32 x = ((s % SKINGRIDNEWWIDTH) * 18) + ((BASEVIDWIDTH / 2) - (18 * SKINGRIDNEWWIDTH) - 8) + 100 + SKINXSHIFT; //BASEVIDWIDTH / 2 - ((icons + 1) * 24) - 4;
				INT32 y = ((s / SKINGRIDNEWWIDTH) * 18) + ((BASEVIDHEIGHT / 2) - (18 * (SKINGRIDNEWWIDTH/2)) + gridyoffset); //BASEVIDWIDTH / 2 - ((icons + 1) * 24) - 4;
				INT32 calcs = s + (setupm_skinypos * SKINGRIDNEWWIDTH);
				INT32 skinn;
				patch_t *face;
				UINT8 *cmap;

				if (calcs < numskins)
					skinn = skinsorted[calcs];
				else if (s % SKINGRIDNEWWIDTH == 0)
					break; //really conveniant place to break out here
				else
				{
					V_DrawFill(x, y, 16, 16, 239);
					continue;
				}

				face = facerankprefix[skinn];
				cmap = R_GetTranslationColormap(skinn, skins[skinn].prefcolor, GTC_MENUCACHE);

				V_DrawFixedPatch(x << FRACBITS, y << FRACBITS, FRACUNIT, 0, face, cmap);
			}

			if (itemOn == 1) //has to be on skin select part
			{
				patch_t *cursor;
				INT32 curx = (((setupm_skinselect % SKINGRIDNEWWIDTH) * 18) + ((BASEVIDWIDTH / 2) - (18 * SKINGRIDNEWWIDTH/2)) + SKINXSHIFT) + 20;
				INT32 cury = (((setupm_skinselect / SKINGRIDNEWWIDTH) - setupm_skinypos) * 18) + ((BASEVIDHEIGHT / 2) - (18 * (SKINGRIDNEWWIDTH/2))+ gridyoffset);

				UINT8 cursorframe = (I_GetTime() / 4) % 7;
				cursor = (patch_t *)W_CachePatchName(va("K_CHILI%d", cursorframe + 1), PU_PATCH);
				V_DrawFixedPatch((curx << FRACBITS) - (FRACUNIT), (cury << FRACBITS) - (FRACUNIT), FRACUNIT+(FRACUNIT>>3), 0, cursor, NULL);
			}

			break;
		case SKINMENUTYPE_GRID:
			for (s = 0; s < SKINGRIDWIDTH*SKINGRIDHEIGHT; s++)
			{
				INT32 x = ((s % SKINGRIDWIDTH) * 18) + ((BASEVIDWIDTH / 2) - (18 * SKINGRIDWIDTH) - 8) + 100 + SKINXSHIFT; //BASEVIDWIDTH / 2 - ((icons + 1) * 24) - 4;
				INT32 y = ((s / SKINGRIDWIDTH) * 18) + ((BASEVIDHEIGHT / 2) - (18 * (SKINGRIDWIDTH/2))); //BASEVIDWIDTH / 2 - ((icons + 1) * 24) - 4;
				INT32 calcs = s + (setupm_skinypos * SKINGRIDWIDTH);
				INT16 skinn;
				patch_t *face;
				UINT8 *cmap;

				if (calcs < numskins)
					skinn = skinsorted[calcs];
				else if (s % SKINGRIDWIDTH == 0)
					break; //really conveniant place to break out here
				else
				{
					V_DrawFill(x, y, 16, 16, 239);
					continue;
				}

				face = facerankprefix[skinn];
				cmap = R_GetTranslationColormap(skinn, skins[skinn].prefcolor, GTC_MENUCACHE);

				V_DrawFixedPatch(x << FRACBITS, y << FRACBITS, FRACUNIT, 0, face, cmap);
			}

			if (itemOn == 1) //has to be on skin select part
			{
				patch_t *cursor;
				INT32 curx = (((setupm_skinselect % SKINGRIDWIDTH) * 18) + ((BASEVIDWIDTH / 2) - (18 * SKINGRIDWIDTH/2)) + SKINXSHIFT) + 20;
				INT32 cury = (((setupm_skinselect / SKINGRIDWIDTH) - setupm_skinypos) * 18) + ((BASEVIDHEIGHT / 2) - (18 * (SKINGRIDWIDTH/2)));

				if (setupm_skinselect < numskins)
				{
					UINT8 *cmap = R_GetTranslationColormap(setupm_skinselect, setupm_fakecolor, GTC_MENUCACHE);
					cursor = facewantprefix[skinsorted[setupm_skinselect]];
					V_DrawFixedPatch(((curx-8) << FRACBITS), ((cury-8) << FRACBITS), FRACUNIT, 0, cursor, cmap);
				}
				else
				{
					UINT8 cursorframe = (I_GetTime() / 4) % 7;
					cursor = (patch_t *)W_CachePatchName(va("K_CHILI%d", cursorframe + 1), PU_PATCH);
					V_DrawFixedPatch((curx << FRACBITS) - (FRACUNIT), (cury << FRACBITS) - (FRACUNIT), FRACUNIT+(FRACUNIT>>3), 0, cursor, NULL);
				}
			}

			{ // stat dot
				INT32 selectedskin = (itemOn == 1 && setupm_skinselect < numskins ? skinsorted[setupm_skinselect] : setupm_fakeskin);
				speed = skins[selectedskin].kartspeed;
				weight = skins[selectedskin].kartweight;
				statdot = (patch_t *)W_CachePatchName("K_SDOT1", PU_PATCH);
				if (skullAnimCounter < 4) // SRB2Kart: we draw this dot later so that it's not covered if there's multiple skins with the same stats
					V_DrawFixedPatch((((statx+46+GRIDSTATOFFSET) + (speed*4))<<FRACBITS) + (FRACUNIT>>1), (((staty+71) + (weight*4))<<FRACBITS), FRACUNIT>>1, 0, statdot, flashcol);
				else
					V_DrawFixedPatch((((statx+46+GRIDSTATOFFSET) + (speed*4))<<FRACBITS) + (FRACUNIT>>1), (((staty+71) + (weight*4))<<FRACBITS), FRACUNIT>>1, 0, statdot, NULL);

				statdot = (patch_t *)W_CachePatchName("K_SDOT2", PU_PATCH); // coloured center
				if (setupm_fakecolor)
					V_DrawFixedPatch((((statx+46+GRIDSTATOFFSET) + (speed*4))<<FRACBITS) + (FRACUNIT>>1), (((staty+71) + (weight*4))<<FRACBITS), FRACUNIT>>1, 0, statdot, R_GetTranslationColormap(0, setupm_fakecolor, GTC_MENUCACHE));
			}
			break;
		case SKINMENUTYPE_2D:
			//better select screen
			for (s = 0; s < MAXSTAT; s++)
			{
				for (w = 0; w < MAXSTAT; w++)
				{
					INT32 x = ((s * 18) + ((BASEVIDWIDTH / 2) - (18 * 4)) - 8 + SKINXSHIFT); //BASEVIDWIDTH / 2 - ((icons + 1) * 24) - 4;
					INT32 y = ((w * 18) + ((BASEVIDHEIGHT / 2) - (18 * 4)) - 8); //BASEVIDWIDTH / 2 - ((icons + 1) * 24) - 4;
					INT32 skinn;
					patch_t *face;
					UINT8 *cmap;

					if (!skinstatscount[s][w])
					{
						V_DrawFill(x, y, 16, 16, 239);
						continue;
					}

					skinn = skinstats[s][w][(I_GetTime() / TICRATE) % skinstatscount[s][w]];
					face = facerankprefix[skinn];
					cmap = R_GetTranslationColormap(skinn, skins[skinn].prefcolor, GTC_MENUCACHE);

					V_DrawFixedPatch(x << FRACBITS, y << FRACBITS, FRACUNIT, 0, face, cmap);
				}
			}

			if (itemOn == 1) //has to be on skin select part
			{
				patch_t *cursor;
				INT32 curx = ((setupm_skinxpos * 18) + ((BASEVIDWIDTH / 2) - (18 * 4)) - 8 + SKINXSHIFT);
				INT32 cury = ((setupm_skinypos * 18) + ((BASEVIDHEIGHT / 2) - (18 * 4)) - 8);

				if (skinstatscount[setupm_skinxpos][setupm_skinypos])
				{
					UINT8 *cmap = R_GetTranslationColormap(setupm_skinselect, setupm_fakecolor, GTC_MENUCACHE);
					INT32 skinn = skinstats[setupm_skinxpos][setupm_skinypos][(I_GetTime() / TICRATE) % skinstatscount[setupm_skinxpos][setupm_skinypos]];

					cursor = facewantprefix[skinn];
					V_DrawFixedPatch(((curx-8) << FRACBITS), ((cury-8) << FRACBITS), FRACUNIT, 0, cursor, cmap);
				}
				else
				{
					UINT8 cursorframe = (I_GetTime() / 4) % 7;
					cursor = (patch_t *)W_CachePatchName(va("K_CHILI%d", cursorframe + 1), PU_PATCH);
					V_DrawFixedPatch((curx << FRACBITS) - (FRACUNIT), (cury << FRACBITS) - (FRACUNIT), FRACUNIT+(FRACUNIT>>3), 0, cursor, NULL);
				}
			}
			break;
#undef GRIDSTATOFFSET
#undef SKINXSHIFT
		default:
			speed = skins[setupm_fakeskin].kartspeed;
			weight = skins[setupm_fakeskin].kartweight;

			statdot = (patch_t *)W_CachePatchName("K_SDOT1", PU_PATCH);
			if (skullAnimCounter < 4) // SRB2Kart: we draw this dot later so that it's not covered if there's multiple skins with the same stats
				V_DrawFixedPatch(((BASEVIDWIDTH - mx - 80) + ((speed-1)*8))<<FRACBITS, ((my+76) + ((weight-1)*8))<<FRACBITS, FRACUNIT, 0, statdot, flashcol);
			else
				V_DrawFixedPatch(((BASEVIDWIDTH - mx - 80) + ((speed-1)*8))<<FRACBITS, ((my+76) + ((weight-1)*8))<<FRACBITS, FRACUNIT, 0, statdot, NULL);

			statdot = (patch_t *)W_CachePatchName("K_SDOT2", PU_PATCH); // coloured center
			if (setupm_fakecolor)
				V_DrawFixedPatch(((BASEVIDWIDTH - mx - 80) + ((speed-1)*8))<<FRACBITS, ((my+76) + ((weight-1)*8))<<FRACBITS, FRACUNIT, 0, statdot, R_GetTranslationColormap(0, setupm_fakecolor, GTC_MENUCACHE));
			break;
	}

	// 2.2 color bar backported with permission
#define charw 72
#define indexwidth 8
	{
		INT32 colwidth = ((BASEVIDWIDTH-(2*mx))-charw)/(2*indexwidth);

		if (cv_skinselectmenu.value == SKINMENUTYPE_EXTENDED)
			colwidth = colwidth - 10;

		INT32 j = -colwidth;
		INT16 col = setupm_fakecolor - colwidth;
		INT32 x = mx;
		INT32 cw = indexwidth;
		UINT8 ch;

		while (col < 1)
			col += MAXSKINCOLORS-1;
		while (j <= colwidth)
		{
			if (!(j++))
				cw = charw;
			else
				cw = indexwidth;
			for (ch = 0; ch < 16; ch++)
				V_DrawFill(x, my+(cv_skinselectmenu.value?176:162)+ch, cw, 1, colortranslations[col][ch]);
			if (++col >= MAXSKINCOLORS)
				col -= MAXSKINCOLORS-1;
			x += cw;
		}
	}
#undef indexwidth

	// character bar, ripped off the color bar :V
	if (setupm_fakecolor && !cv_skinselectmenu.value) // inverse should never happen
#define iconwidth 32
	{
		const INT32 icons = 4;
		INT32 k = -icons;
		INT16 col = setupm_fakeskin - icons;
		INT32 x = BASEVIDWIDTH/2 - ((icons+1)*24) - 4;
		fixed_t scale = FRACUNIT/2;
		INT32 offx = 8, offy = 8;
		patch_t *cursor;
		static fixed_t cursorframe = 0;
		patch_t *face;
		UINT8 *colmap;

		cursorframe += renderdeltatics / 4;
		for (; cursorframe > 7 * FRACUNIT; cursorframe -= 7 * FRACUNIT) {}

		cursor = (patch_t *)W_CachePatchName(va("K_BHILI%d", (cursorframe >> FRACBITS) + 1), PU_PATCH);

		if (col < 0)
			col += numskins;
		while (k <= icons)
		{
			if (!(k++))
			{
				scale = FRACUNIT;
				face = facewantprefix[col];
				offx = 12;
				offy = 0;
			}
			else
			{
				scale = FRACUNIT/2;
				face = facerankprefix[col];
				offx = 8;
				offy = 8;
			}
			colmap =  R_GetTranslationColormap(col, setupm_fakecolor, GTC_MENUCACHE);
			V_DrawFixedPatch((x+offx)<<FRACBITS, (my+28+offy)<<FRACBITS, FRACUNIT, 0, face, colmap);
			if (scale == FRACUNIT) // bit of a hack
				V_DrawFixedPatch((x+offx)<<FRACBITS, (my+28+offy)<<FRACBITS, FRACUNIT, 0, cursor, colmap);
			if (++col >= numskins)
				col -= numskins;
			x += FixedMul(iconwidth<<FRACBITS, 3*scale/2)/FRACUNIT;
		}
	}
#undef iconwidth

	// anim the player in the box
	multi_tics -= renderdeltatics;

	while (multi_tics <= 0)
	{
		st = cv_skinselectspin.value == SKINSELECTSPIN_PAIN ? S_KART_PAIN : multi_state->nextstate;

		if (st != S_NULL)
			multi_state = &states[st];

		if (multi_state->tics <= -1)
			multi_tics += 15*FRACUNIT;
		else
			multi_tics += multi_state->tics * FRACUNIT;
	}

	// skin 0 is default player sprite
	switch (cv_skinselectmenu.value)
	{
		case SKINMENUTYPE_2D:
			skintodisplay = (UINT8)setupm_fakeskin;
			if (setupm_skinlockedselect) // show the skin we are trying to select
				skintodisplay = skinstats[setupm_skinxpos][setupm_skinypos][setupm_skinselect];
			else if (skinstatscount[setupm_skinxpos][setupm_skinypos] && itemOn == 1)
				skintodisplay = skinstats[setupm_skinxpos][setupm_skinypos][(I_GetTime()/TICRATE)%SELECTEDSTATSCOUNT];
			break;
		case SKINMENUTYPE_EXTENDED:
		case SKINMENUTYPE_GRID:
			skintodisplay = ((itemOn == 1 && (setupm_skinselect < numskins)) ? skinsorted[setupm_skinselect] : (UINT8)setupm_fakeskin);
			break;
		default:
			skintodisplay = (UINT8)setupm_fakeskin;
			break;
	}

	if (skintodisplay >= MAXSKINS)
		skintodisplay = 0;

	skinnum = R_SkinAvailable(skins[skintodisplay].name);

	if (skinnum < 0 || skinnum >= MAXSKINS)
		skinnum = 0;

	sprdef = &skins[skinnum].spritedef;

	if (!sprdef->numframes) // No frames ??
		return; // Can't render!

	frame = multi_state->frame & FF_FRAMEMASK;
	if (frame >= sprdef->numframes) // Walking animation missing
		frame = 0; // Try to use standing frame

	// draw box around guy
	V_DrawFill(mx + 36 - (charw/2), my+65, charw, 84, 239);
#undef charw

	// draw player sprite
	if (setupm_fakecolor) // inverse should never happen
	{
		UINT8 *colormap = R_GetTranslationColormap(skintodisplay, setupm_fakecolor, GTC_MENUCACHE);
#ifdef HWRENDER
		md2_t *md2 = &md2_playermodels[skinnum];
#endif
		mx += 36;
		my += 131;
#ifdef HWRENDER
		// if we have 3d models enabled and a model exists
		// try to show it instead of the sprite
		if (rendermode == render_opengl && cv_glmdls.value
		&& !md2->error && !md2->notfound)
		{
			const angle_t ROTATE_PER_TIC = (UINT64)ANGLE_45 * cv_skinselectspin.value / TICRATE;
			angle_t angle = I_GetTime()*ROTATE_PER_TIC + FixedMul(cv_uncappedhud.value ? renderdeltatics : FRACUNIT, ROTATE_PER_TIC);
			HWR_Draw2DModel(md2, mx, my, skinnum, (skincolors_t)setupm_fakecolor, colormap, 8*FRACUNIT/3, frame, angle);
		}
		else
#endif
		{
			sprframe = &sprdef->spriteframes[frame];

			// minenice's speen css, it's a piece of shit but hey
			speenframe = (I_GetTime()*cv_skinselectspin.value/TICRATE + 1)%8;

			// this is a very shitty solution for checking if a sprite needs flipping
			// but it works
			if ((speenframe > 4) && (sprframe->lumppat[speenframe] == sprframe->lumppat[8-speenframe]))
			{
				flags = V_FLIP; // This sprite is left/right flipped!
			}

			patch = (patch_t *)W_CachePatchNum(sprframe->lumppat[speenframe], PU_PATCH);

			if (skins[skintodisplay].flags & SF_HIRES)
			{
				V_DrawFixedPatch(mx<<FRACBITS,
								 my<<FRACBITS,
								 skins[skintodisplay].highresscale,
								 flags, patch, colormap);
			}
			else
			{
				V_DrawMappedPatch(mx, my, flags, patch, colormap);
			}
		}
	}
}

// Handle 1P/2P MP Setup
static void M_HandleSetupMultiPlayer(INT32 choice)
{
	boolean exitmenu = false;  // exit to previous menu and send name change
	const boolean gridselect = (cv_skinselectmenu.value == SKINMENUTYPE_GRID || cv_skinselectmenu.value == SKINMENUTYPE_EXTENDED); // menus with "grids"

	if ((choice == gamecontrol[0][gc_fire][0] || choice == gamecontrol[0][gc_fire][1]) && (itemOn == 2 || (gridselect && itemOn == 1 && choice != KEY_ENTER)))
		choice = KEY_BACKSPACE; // Hack to allow resetting prefcolor on controllers

#define BREAKWHENLOCKED {\
	if (setupm_skinlockedselect) \
		break; }
#define ROUNDSKINSUPTO8 (numskins % SKINGRIDWIDTH ? ((numskins / SKINGRIDWIDTH) * SKINGRIDWIDTH) + SKINGRIDWIDTH : numskins)
	switch (choice)
	{
		case KEY_DOWNARROW:
			if (cv_skinselectmenu.value == SKINMENUTYPE_2D)
			{
				BREAKWHENLOCKED
				if (itemOn == 1 && setupm_skinypos < MAXSTAT-1) //player skin
					setupm_skinypos++;
				else if (itemOn == 0)
				{
					setupm_skinypos = 0;
					M_NextOpt();
				}
				else
					M_NextOpt();
				S_StartSound(NULL, sfx_menu1); // Tails
				break;
			}
			else if (gridselect) //grid skin select menu
			{
				if (itemOn == 1) //if we are on the skin select menu
				{
					if (setupm_skinselect < ROUNDSKINSUPTO8 - SKINGRIDWIDTH) //if we arent at the bottom of the menu
					{
						setupm_skinselect += SKINGRIDWIDTH;

						if (cv_skinselectmenu.value == SKINMENUTYPE_GRID)
						{
							if (setupm_skinselect >= ((setupm_skinypos-1)+SKINGRIDHEIGHT)*8 && setupm_skinypos < (ROUNDSKINSUPTO8/8)-SKINGRIDHEIGHT)
								setupm_skinypos++;
						}
						else if (cv_skinselectmenu.value == SKINMENUTYPE_EXTENDED)
						{
							if (setupm_skinselect >= ((setupm_skinypos-1)+SKINGRIDHEIGHT)*8+24 && setupm_skinypos < (ROUNDSKINSUPTO8/8)-SKINGRIDHEIGHT+24)
								setupm_skinypos++;
						}
					}
					else
					{
						M_NextOpt();
					}
					S_StartSound(NULL, sfx_menu1);
				}
				else if (itemOn == 0)
				{
					setupm_skinselect = 0;
					setupm_skinypos = 0;
					M_NextOpt();
				}
				else
					M_NextOpt();
				S_StartSound(NULL, sfx_menu1); // Tails
				break;
			}
			M_NextOpt();
			S_StartSound(NULL, sfx_menu1); // Tails
			break;

		case KEY_UPARROW:
			if (cv_skinselectmenu.value == SKINMENUTYPE_2D)
			{
				BREAKWHENLOCKED
				if (itemOn == 1 && setupm_skinypos > 0)
					setupm_skinypos--;
				else if (itemOn == 2)
				{
					setupm_skinypos = MAXSTAT - 1;
					M_PrevOpt();
				}
				else
					M_PrevOpt();
				S_StartSound(NULL, sfx_menu1); // Tails
				break;
			}
			else if (gridselect)
			{
				if (itemOn == 1)
				{
					if (setupm_skinselect >= SKINGRIDWIDTH) //if we arent at the top of the menu
					{
						setupm_skinselect -= SKINGRIDWIDTH;

						if (setupm_skinselect < ((setupm_skinypos+1)*SKINGRIDWIDTH) && setupm_skinypos > 0)
							setupm_skinypos--;
					}
					else
					{
						M_PrevOpt();
					}
					S_StartSound(NULL, sfx_menu1);
				}
				else if (itemOn == 2)
				{
					setupm_skinselect = numskins - 1;
					if (cv_skinselectmenu.value == SKINMENUTYPE_GRID)
					{
						setupm_skinypos = (((numskins / SKINGRIDWIDTH) - (SKINGRIDHEIGHT-1)) > 0 ? ((numskins / SKINGRIDWIDTH) - (SKINGRIDHEIGHT-1)) : 0);
						M_PrevOpt();
					}
					else if (cv_skinselectmenu.value == SKINMENUTYPE_EXTENDED)
					{
						setupm_skinypos = (((numskins / SKINGRIDWIDTH) - (SKINGRIDHEIGHT-1)-2) > 0 ? ((numskins / SKINGRIDWIDTH) - (SKINGRIDHEIGHT-1)-2) : 0);
						M_PrevOpt();
					}

				}
				else
					M_PrevOpt();
				S_StartSound(NULL, sfx_menu1); // Tails
				break;
			}
			M_PrevOpt();
			S_StartSound(NULL, sfx_menu1); // Tails
			break;

		case KEY_LEFTARROW:
			if (itemOn == 0)
			{
				M_TextInputHandle(&setupm_input, choice);
				S_StartSound(NULL, sfx_menu1); // Tails
			}
			else if (cv_skinselectmenu.value == SKINMENUTYPE_2D && itemOn == 1)
			{
				if (setupm_skinlockedselect)
				{
					if (setupm_skinselect > 0)
						setupm_skinselect--;
					else
						setupm_skinselect = SELECTEDSTATSCOUNT - 1;
					S_StartSound(NULL, sfx_menu1);
				}
				else       //player skin
				{
					S_StartSound(NULL, sfx_menu1); // Tails

					if (setupm_skinxpos > 0)
						setupm_skinxpos--;
					else
						setupm_skinxpos = MAXSTAT-1;
				}
				break;
			}
			else if (gridselect && itemOn == 1)
			{
				if (setupm_skinselect > 0)
				{
					setupm_skinselect--;

					if (setupm_skinselect < ((setupm_skinypos+1)*SKINGRIDWIDTH) && setupm_skinypos > 0)
						setupm_skinypos--;
				}
				else
				{
					INT32 roundedskins = ROUNDSKINSUPTO8;
					setupm_skinselect = roundedskins-1;

					if (cv_skinselectmenu.value == SKINMENUTYPE_GRID)
						setupm_skinypos = (((roundedskins/8) - SKINGRIDHEIGHT) > 0 ? (roundedskins/8) - SKINGRIDHEIGHT : 0);
					else if (cv_skinselectmenu.value == SKINMENUTYPE_EXTENDED)
						setupm_skinypos = (((roundedskins/8) - SKINGRIDHEIGHT-2) > 0 ? (roundedskins/8) - SKINGRIDHEIGHT-2 : 0);
				}
				S_StartSound(NULL, sfx_menu1);
				break;
			}
			if (itemOn == 1)       //player skin
			{
				S_StartSound(NULL, sfx_menu1); // Tails
				setupm_fakeskin--;
			}
			else if (itemOn == 2) // player color
			{
				S_StartSound(NULL, sfx_menu1); // Tails
				setupm_fakecolor--;
				G_SetPlayerGamepadIndicatorColor(setupm_playernum, (UINT8)setupm_fakecolor);
			}
			break;

		case KEY_RIGHTARROW:
			if (itemOn == 0)
			{
				M_TextInputHandle(&setupm_input, choice);
				S_StartSound(NULL, sfx_menu1); // Tails
			}
			else if (cv_skinselectmenu.value == SKINMENUTYPE_2D && itemOn == 1)
			{
				if (setupm_skinlockedselect)
				{
					if (setupm_skinselect < SELECTEDSTATSCOUNT-1)
						setupm_skinselect++;
					else
						setupm_skinselect = 0;
					S_StartSound(NULL, sfx_menu1);
				}
				else       //player skin
				{
					S_StartSound(NULL, sfx_menu1); // Tails
					if (setupm_skinxpos < MAXSTAT - 1)
						setupm_skinxpos++;
					else
						setupm_skinxpos = 0;
				}
				break;
			}
			else if (gridselect && itemOn == 1)
			{
				if (setupm_skinselect < ROUNDSKINSUPTO8 - 1)
				{
					setupm_skinselect++;

					if (cv_skinselectmenu.value == SKINMENUTYPE_GRID)
					{
						if (setupm_skinselect >= ((setupm_skinypos-1)+SKINGRIDHEIGHT)*8 && setupm_skinypos < (ROUNDSKINSUPTO8/8)-SKINGRIDHEIGHT)
							setupm_skinypos++;
					}
					else if (cv_skinselectmenu.value == SKINMENUTYPE_EXTENDED)
					{
						if (setupm_skinselect >= ((setupm_skinypos-1)+SKINGRIDHEIGHT)*8+24 && setupm_skinypos < (ROUNDSKINSUPTO8/8)-SKINGRIDHEIGHT+24)
							setupm_skinypos++;
					}
				}
				else
				{
					setupm_skinselect = 0;
					setupm_skinypos = 0;
				}
				S_StartSound(NULL, sfx_menu1);
				break;
			}
			if (itemOn == 1)       //player skin
			{
				S_StartSound(NULL, sfx_menu1); // Tails
				setupm_fakeskin++;
			}
			else if (itemOn == 2) // player color
			{
				S_StartSound(NULL, sfx_menu1); // Tails
				setupm_fakecolor++;
				G_SetPlayerGamepadIndicatorColor(setupm_playernum, (UINT8)setupm_fakecolor);
			}
			break;

		case KEY_ESCAPE:
			if (setupm_skinlockedselect)
			{
				setupm_skinlockedselect = false;
				break;
			}
			exitmenu = true;
			break;

		case KEY_BACKSPACE:
			if (cv_skinselectmenu.value)
				BREAKWHENLOCKED
			if (itemOn == 0)
			{
				M_TextInputHandle(&setupm_input, choice);
				S_StartSound(NULL, sfx_menu1); // Tails
			}
			else if (gridselect && itemOn == 1)
			{
				// change sort for select menu (damn now i have to add another cvar...)
				// now we have the cvar
				// time to :shitsfree:
				CV_StealthSetValue(&cv_skinselectgridsort, (cv_skinselectgridsort.value+1)%MAXSKINMENUSORTS);
				sortSkinGrid();
				S_StartSound(NULL, sfx_menu1);
			}
			else if (itemOn == 2)
			{
				UINT8 col = skins[setupm_fakeskin].prefcolor;
				if (setupm_fakecolor != col)
				{
					S_StartSound(NULL, sfx_menu1); // Tails
					setupm_fakecolor = col;
					G_SetPlayerGamepadIndicatorColor(setupm_playernum, (UINT8)setupm_fakecolor);
				}
			}
			break;

		case KEY_DEL:
			if (cv_skinselectmenu.value)
				BREAKWHENLOCKED
			if (itemOn == 0)
			{
				M_TextInputHandle(&setupm_input, choice);
				S_StartSound(NULL, sfx_menu1); // Tails
			}
			break;

		case KEY_ENTER:
			if (cv_skinselectmenu.value == SKINMENUTYPE_2D)
			{
				if (setupm_skinlockedselect)
				{
					setupm_fakeskin = skinstats[setupm_skinxpos][setupm_skinypos][setupm_skinselect];
					setupm_skinlockedselect = false;
					S_StartSound(NULL, sfx_s221);
					break;
				}
				if (itemOn == 1 && SELECTEDSTATSCOUNT == 1)
				{
					setupm_fakeskin = skinstats[setupm_skinxpos][setupm_skinypos][0];
					S_StartSound(NULL, sfx_s221);
				}
				else if (itemOn == 1 && SELECTEDSTATSCOUNT > 1)
				{
					setupm_skinlockedselect = true;
					setupm_skinselect = 0;
					S_StartSound(NULL, sfx_menu1);
				}
			}
			else if (gridselect && itemOn == 1 && setupm_skinselect < numskins)
			{
				setupm_fakeskin = skinsorted[setupm_skinselect];
				S_StartSound(NULL, sfx_s221);
			}
			break;

		default:
			if (itemOn == 0)
			{
				if (M_TextInputHandle(&setupm_input, choice))
					S_StartSound(NULL, sfx_menu1); // Tails
			}
			break;
		}
#undef BREAKWHENLOCKED

		// check skin
		if (setupm_fakeskin < 0)
			setupm_fakeskin = numskins-1;
		if (setupm_fakeskin > numskins-1)
			setupm_fakeskin = 0;

		// check color
		if (setupm_fakecolor < 1)
			setupm_fakecolor = MAXSKINCOLORS-1;
		if (setupm_fakecolor > MAXSKINCOLORS-1)
			setupm_fakecolor = 1;

		if (exitmenu)
		{
			if (currentMenu->prevMenu)
				M_SetupNextMenu (currentMenu->prevMenu);
			else
				M_ClearMenus(true);
		}
}

#define SKINSELECTMENUEDIT \
{\
switch (cv_skinselectmenu.value)\
{\
case SKINMENUTYPE_2D:\
	MP_PlayerSetupMenu[0].alphaKey = 0;\
	MP_PlayerSetupMenu[1].alphaKey = 25;\
	MP_PlayerSetupMenu[2].alphaKey = 164;\
	MP_PlayerSetupDef.y = 6;\
	break;\
case SKINMENUTYPE_EXTENDED:\
	MP_PlayerSetupMenu[0].alphaKey = 6;\
	MP_PlayerSetupMenu[1].alphaKey = 25;\
	MP_PlayerSetupMenu[2].alphaKey = 152;\
	MP_PlayerSetupDef.y = 6;\
	break;\
case SKINMENUTYPE_GRID:\
	MP_PlayerSetupMenu[0].alphaKey = 6;\
	MP_PlayerSetupMenu[1].alphaKey = 25;\
	MP_PlayerSetupMenu[2].alphaKey = 164;\
	MP_PlayerSetupDef.y = 6;\
	break;\
default:\
	MP_PlayerSetupMenu[0].alphaKey = 0;\
	MP_PlayerSetupMenu[1].alphaKey = 16;\
	MP_PlayerSetupMenu[2].alphaKey = 152;\
	MP_PlayerSetupDef.y = 14;\
	break;\
}\
}

// start the multiplayer setup menu

static void M_DoSetupMultiPlayer(UINT8 pnum)
{
	multi_state = cv_skinselectspin.value == SKINSELECTSPIN_PAIN ? &states[S_KART_PAIN] : &states[mobjinfo[MT_PLAYER].seestate];
	multi_tics = multi_state->tics*FRACUNIT;

	switch (pnum)
	{
		case 1:
			setupm_player  = &players[displayplayers[pnum]];
			setupm_cvskin  = &cv_skin2;
			setupm_cvcolor = &cv_playercolor2;
			setupm_cvname  = &cv_playername2;
			break;
		case 2:
			setupm_player  = &players[displayplayers[pnum]];
			setupm_cvskin  = &cv_skin3;
			setupm_cvcolor = &cv_playercolor3;
			setupm_cvname  = &cv_playername3;
			break;
		case 3:
			setupm_player  = &players[displayplayers[pnum]];
			setupm_cvskin  = &cv_skin4;
			setupm_cvcolor = &cv_playercolor4;
			setupm_cvname  = &cv_playername4;
			break;
		case 0:
			setupm_player  = &players[consoleplayer];
			setupm_cvskin  = &cv_skin;
			setupm_cvcolor = &cv_playercolor;
			setupm_cvname  = &cv_playername;
			break;
	}

	M_TextInputInit(&setupm_input, setupm_name, sizeof(setupm_name));
	M_TextInputSetString(&setupm_input, setupm_cvname->string);

	setupm_skinxpos = 4;
	setupm_skinypos = 0;
	setupm_skinlockedselect = false;

	setupm_playernum = pnum;

	// For whatever reason this doesn't work right if you just use ->value
	setupm_fakeskin = R_SkinAvailable(setupm_cvskin->string);
	if (setupm_fakeskin == -1)
		setupm_fakeskin = 0;
	setupm_fakecolor = setupm_cvcolor->value;

	// disable skin changes if we can't actually change skins
	if (splitscreen && pnum > 0)
	{
		if (splitscreen && !CanChangeSkin(displayplayers[pnum]))
			MP_PlayerSetupMenu[2].status = (IT_GRAYEDOUT);
		else
			MP_PlayerSetupMenu[2].status = (IT_KEYHANDLER | IT_STRING);
	}
	else
	{
		if (!CanChangeSkin(consoleplayer))
			MP_PlayerSetupMenu[2].status = (IT_GRAYEDOUT);
		else
			MP_PlayerSetupMenu[2].status = (IT_KEYHANDLER|IT_STRING);
	}

	//change the y offsets of the menu depending on cvar settings
	SKINSELECTMENUEDIT

	sortSkinGrid();

	MP_PlayerSetupDef.prevMenu = currentMenu;
	M_SetupNextMenu(&MP_PlayerSetupDef);
}
#undef SKINSELECTMENUEDIT

static void M_SetupMultiPlayer(void)
{
	M_DoSetupMultiPlayer(0);
}

static void M_SetupMultiPlayer2(void)
{
	M_DoSetupMultiPlayer(1);
}

static void M_SetupMultiPlayer3(void)
{
	M_DoSetupMultiPlayer(2);
}

static void M_SetupMultiPlayer4(void)
{
	M_DoSetupMultiPlayer(3);
}

static boolean M_QuitMultiPlayerMenu(void)
{
	size_t l;

	// send name if changed
	if (!fastcmp(setupm_name, setupm_cvname->string))
	{
		// remove trailing whitespaces
		for (l = strlen(setupm_name)-1;
		    (signed)l >= 0 && setupm_name[l] ==' '; l--)
			setupm_name[l] =0;
		COM_BufAddText(va("%s \"%s\"\n",setupm_cvname->name,setupm_name));
	}

	// you know what? always putting these in the buffer won't hurt anything.
	COM_BufAddText(va("%s \"%s\"\n",setupm_cvskin->name,skins[setupm_fakeskin].name));
	COM_BufAddText(va("%s %d\n",setupm_cvcolor->name,setupm_fakecolor));

	return true;
}


// =================
// DATA OPTIONS MENU
// =================
static UINT8 erasecontext = 0;

static void M_EraseDataResponse(INT32 ch)
{
	if (ch != 'y' && ch != KEY_ENTER)
		return;

	S_StartSound(NULL, sfx_itrole); // bweh heh heh

	// Delete the data
	if (erasecontext == 2)
	{
		// SRB2Kart: This actually needs to be done FIRST, so that you don't immediately regain playtime/matches secrets
		K_EraseStats();
		F_StartIntro();
	}

	if (erasecontext != 1)
		G_ClearRecords();

	if (erasecontext != 0)
		M_ClearSecrets();

	M_ClearMenus(true);
}

static void M_EraseData(INT32 choice)
{
	const char *eschoice, *esstr = M_GetText("Are you sure you want to erase\n%s?\n\n(Press 'Y' to confirm)\n");

	erasecontext = (UINT8)choice;

	if (choice == 0)
		eschoice = M_GetText("Record Attack data");
	else if (choice == 1)
		eschoice = M_GetText("Secrets data");
	else
		eschoice = M_GetText("ALL game data");

	M_StartMessage(va(esstr, eschoice),M_EraseDataResponse,MM_YESNO);
}

static void M_ScreenshotOptions(INT32 choice)
{
	(void)choice;
	Screenshot_option_Onchange();
	Moviemode_mode_Onchange();

	M_SetupNextMenu(&OP_ScreenshotOptionsDef);
}

static void M_DeleteProtocol(void)
{
	M_StartMessage("Are you sure you want to disable and delete protocol registers?\n"
			"Protocols can be registered again with the Register Protocol option in this menu\n\n"
			"(Press 'Y' to confirm)\n",
			D_DeleteProtocol, MM_YESNO);
}

// =============
// JOYSTICK MENU
// =============

// Start the controls menu, setting it up for either the console player,
// or the secondary splitscreen player

static void M_DrawJoystick(void)
{
	INT32 i, compareval4, compareval3, compareval2, compareval;

	M_DrawGenericMenu();

	for (i = 0; i < 8; i++)
	{
		M_DrawTextBox(OP_JoystickSetDef.x-8, OP_JoystickSetDef.y+LINEHEIGHT*i-12, 28, 1);
		//M_DrawSaveLoadBorder(OP_JoystickSetDef.x, OP_JoystickSetDef.y+LINEHEIGHT*i);

#ifdef JOYSTICK_HOTPLUG
		compareval4 = atoi(cv_usejoystick[3].string);
		if (compareval4 <= numcontrollers)
			compareval4 = cv_usejoystick[3].value;

		compareval3 = atoi(cv_usejoystick[2].string);
		if (compareval3 <= numcontrollers)
			compareval3 = cv_usejoystick[2].value;

		compareval2 = atoi(cv_usejoystick[1].string);
		if (compareval2 <= numcontrollers)
			compareval2 = cv_usejoystick[1].value;

		compareval = atoi(cv_usejoystick[0].string);
		if (compareval <= numcontrollers)
			compareval = cv_usejoystick[0].value;
#else
		compareval4 = cv_usejoystick[3].value;
		compareval3 = cv_usejoystick[2].value;
		compareval2 = cv_usejoystick[1].value;
		compareval = cv_usejoystick[0].value;
#endif

		if    ((setupcontrolplayer == 4 && (i == compareval4))
			|| (setupcontrolplayer == 3 && (i == compareval3))
			|| (setupcontrolplayer == 2 && (i == compareval2))
			|| (setupcontrolplayer == 1 && (i == compareval)))
			V_DrawString(OP_JoystickSetDef.x, OP_JoystickSetDef.y+LINEHEIGHT*i-4,V_GREENMAP,joystickInfo[i]);
		else
			V_DrawString(OP_JoystickSetDef.x, OP_JoystickSetDef.y+LINEHEIGHT*i-4,0,joystickInfo[i]);
	}
}

void M_SetupJoystickMenu(INT32 choice)
{
	INT32 i = 0;
	const char *joyNA = "Unavailable";
	(void)choice;

	strcpy(joystickInfo[i], "None");

	for (i = 1; i < 8; i++)
	{
		const char *joyname = I_GetJoyName(i);

		if (i <= numcontrollers && joyname != NULL)
			strncpy(joystickInfo[i], joyname, 28);
		else
			strcpy(joystickInfo[i], joyNA);

#ifdef JOYSTICK_HOTPLUG
		// We use cv_usejoystick.string as the USER-SET var
		// and cv_usejoystick.value as the INTERNAL var
		//
		// In practice, if cv_usejoystick.string == 0, this overrides
		// cv_usejoystick.value and always disables
		//
		// Update cv_usejoystick.string here so that the user can
		// properly change this value.
		if (i == cv_usejoystick[0].value)
			CV_SetValue(&cv_usejoystick[0], i);
		if (i == cv_usejoystick[1].value)
			CV_SetValue(&cv_usejoystick[1], i);
		if (i == cv_usejoystick[2].value)
			CV_SetValue(&cv_usejoystick[2], i);
		if (i == cv_usejoystick[3].value)
			CV_SetValue(&cv_usejoystick[3], i);
#endif
	}

	M_SetupNextMenu(&OP_JoystickSetDef);
}

static void M_Setup1PJoystickMenu(INT32 choice)
{
	setupcontrolplayer = 1;
	OP_JoystickSetDef.prevMenu = &OP_Joystick1Def;
	M_SetupJoystickMenu(choice);
}

static void M_Setup2PJoystickMenu(INT32 choice)
{
	setupcontrolplayer = 2;
	OP_JoystickSetDef.prevMenu = &OP_Joystick2Def;
	M_SetupJoystickMenu(choice);
}

static void M_Setup3PJoystickMenu(INT32 choice)
{
	setupcontrolplayer = 3;
	OP_JoystickSetDef.prevMenu = &OP_Joystick3Def;
	M_SetupJoystickMenu(choice);
}

static void M_Setup4PJoystickMenu(INT32 choice)
{
	setupcontrolplayer = 4;
	OP_JoystickSetDef.prevMenu = &OP_Joystick4Def;
	M_SetupJoystickMenu(choice);
}

#ifdef JOYSTICK_HOTPLUG
static void M_DoAssignJoystick(UINT8 pnum, INT32 choice)
{
	INT32 oldchoice, oldstringchoice;
	const int joynum = atoi(cv_usejoystick[pnum].string);

	oldchoice = oldstringchoice = joynum > numcontrollers ? joynum : cv_usejoystick[pnum].value;
	CV_SetValue(&cv_usejoystick[pnum], choice);

	// Just in case last-minute changes were made to cv_usejoystick.value,
	// update the string too
	// But don't do this if we're intentionally setting higher than numjoys
	if (choice <= numcontrollers)
	{
		CV_SetValue(&cv_usejoystick[pnum], cv_usejoystick[pnum].value);

		if (oldchoice > numcontrollers)  /* reset this so the comparison is valid*/
			oldchoice = cv_usejoystick[pnum].value;

		if (oldchoice != choice)
		{
			if (choice && oldstringchoice > numcontrollers) // if we did not select "None", we likely selected a used device
				CV_SetValue(&cv_usejoystick[pnum], (oldstringchoice > numcontrollers ? oldstringchoice : oldchoice));

			if (oldstringchoice ==
				(joynum > numcontrollers ? joynum : cv_usejoystick[pnum].value))
				M_StartMessage("This joystick is used by another\n"
				"player. Reset the joystick\n"
				"for that player first.\n\n"
				"(Press a key)\n", NULL, MM_NOTHING);
		}
	}
}
#endif

static void M_AssignJoystick(INT32 choice)
{
#ifdef JOYSTICK_HOTPLUG
	switch (setupcontrolplayer)
	{
		case 4:
			M_DoAssignJoystick(3, choice);
			break;
		case 3:
			M_DoAssignJoystick(2, choice);
			break;
		case 2:
			M_DoAssignJoystick(1, choice);
			break;
		case 1:
			M_DoAssignJoystick(0, choice);
			break;
	}
#else
	switch (setupcontrolplayer)
	{
		case 4:
			CV_SetValue(&cv_usejoystick[3], choice);
			break;
		case 3:
			CV_SetValue(&cv_usejoystick[2], choice);
			break;
		case 2:
			CV_SetValue(&cv_usejoystick[1], choice);
			break;
		case 1:
			CV_SetValue(&cv_usejoystick[0], choice);
			break;
	}
#endif
}

// =============
// CONTROLS MENU
// =============

static void M_SetupControlsMenu(UINT8 pnum)
{
	setupcontrolplayer = pnum+1;
	setupcontrols = gamecontrol[pnum];        // was called from main Options (for console player, then)
	currentMenu->lastOn = itemOn;

	// Set proper gamepad options
	switch (pnum)
	{
		case 1:
			OP_AllControlsMenu[0].itemaction = &OP_Joystick2Def;
			break;
		case 2:
			OP_AllControlsMenu[0].itemaction = &OP_Joystick3Def;
			break;
		case 3:
			OP_AllControlsMenu[0].itemaction = &OP_Joystick4Def;
			break;
		case 0:
			OP_AllControlsMenu[0].itemaction = &OP_Joystick1Def;
			break;
	}

	OP_AllControlsMenu[4].itemaction = &cv_litesteer[pnum];
	OP_AllControlsMenu[5].itemaction = &cv_turnsmooth[pnum];

	if (pnum > 0)
	{
		// Hide P1-only controls
		OP_AllControlsMenu[19].status = IT_GRAYEDOUT2; // Chat
		OP_AllControlsMenu[10].status = IT_GRAYEDOUT2; // Rankings
		OP_AllControlsMenu[21].status = IT_GRAYEDOUT2; // Pause
		OP_AllControlsMenu[22].status = IT_GRAYEDOUT2; // Screenshot
		OP_AllControlsMenu[23].status = IT_GRAYEDOUT2; // GIF
		OP_AllControlsMenu[24].status = IT_GRAYEDOUT2; // System Menu
		OP_AllControlsMenu[25].status = IT_GRAYEDOUT2; // Console
		OP_AllControlsMenu[41].status = IT_GRAYEDOUT2; // Director
	}
	else
	{
		// Unhide P1-only controls
		OP_AllControlsMenu[19].status = IT_CONTROL; // Chat
		OP_AllControlsMenu[10].status = IT_CONTROL; // Rankings
		OP_AllControlsMenu[21].status = IT_CONTROL; // Pause
		OP_AllControlsMenu[22].status = IT_CONTROL; // Screenshot
		OP_AllControlsMenu[23].status = IT_CONTROL; // GIF
		OP_AllControlsMenu[24].status = IT_CONTROL; // System Menu
		OP_AllControlsMenu[25].status = IT_CONTROL; // Console
		OP_AllControlsMenu[41].status = IT_CONTROL; // Director
	}

	M_SetupNextMenu(&OP_AllControlsDef);
}

static void M_Setup1PControlsMenu(void)
{
	M_SetupControlsMenu(0);
}

static void M_Setup2PControlsMenu(void)
{
	M_SetupControlsMenu(1);
}

static void M_Setup3PControlsMenu(void)
{
	M_SetupControlsMenu(2);
}

static void M_Setup4PControlsMenu(void)
{
	M_SetupControlsMenu(3);
}

#define controlheight 18

// Draws the Customise Controls menu
static void M_DrawControl(void)
{
	char tmp[50];
	INT32 x, y, i, max, cursory = 0, iter;
	INT32 keys[2];

	x = currentMenu->x;
	y = currentMenu->y;

	iter = (controlheight/2);
	for (i = itemOn; ((iter || currentMenu->menuitems[i].status == IT_GRAYEDOUT2) && i > 0); i--)
	{
		if (currentMenu->menuitems[i].status != IT_GRAYEDOUT2)
			iter--;
	}
	if (currentMenu->menuitems[i].status == IT_GRAYEDOUT2)
		i--;

	iter += (controlheight/2);
	for (max = itemOn; (iter && max < currentMenu->numitems); max++)
	{
		if (currentMenu->menuitems[max].status != IT_GRAYEDOUT2)
			iter--;
	}

	if (iter)
	{
		iter += (controlheight/2);
		for (i = itemOn; ((iter || currentMenu->menuitems[i].status == IT_GRAYEDOUT2) && i > 0); i--)
		{
			if (currentMenu->menuitems[i].status != IT_GRAYEDOUT2)
				iter--;
		}
	}

	// draw title (or big pic)
	M_DrawMenuTitle();

	M_CentreText(28,
		(setupcontrolplayer > 1 ? va("\x86""Set controls for ""\x82""Player %d", setupcontrolplayer) :
		                          "\x86""Press ""\x82""ENTER""\x86"" to change, ""\x82""BACKSPACE""\x86"" to clear"));

	if (i)
		V_DrawCharacter(currentMenu->x - 16, y-(skullAnimCounter/5),
			'\x1A' | highlightflags, false); // up arrow
	if (max != currentMenu->numitems)
		V_DrawCharacter(currentMenu->x - 16, y+(SMALLLINEHEIGHT*(controlheight-1))+(skullAnimCounter/5) + (skullAnimCounter/5),
			'\x1B' | highlightflags, false); // down arrow

	for (; i < max; i++)
	{
		if (currentMenu->menuitems[i].status == IT_GRAYEDOUT2)
			continue;

		if (i == itemOn)
			cursory = y;

		if (currentMenu->menuitems[i].status == IT_CONTROL)
		{
			V_DrawString(x, y, ((i == itemOn) ? highlightflags|V_ALLOWLOWERCASE : V_ALLOWLOWERCASE), currentMenu->menuitems[i].text);
			keys[0] = setupcontrols[currentMenu->menuitems[i].alphaKey][0];
			keys[1] = setupcontrols[currentMenu->menuitems[i].alphaKey][1];

			tmp[0] ='\0';
			if (keys[0] == KEY_NULL && keys[1] == KEY_NULL)
			{
				strcpy(tmp, "---");
			}
			else
			{
				if (keys[0] != KEY_NULL)
					strcat (tmp, G_KeynumToString (keys[0]));

				if (keys[0] != KEY_NULL && keys[1] != KEY_NULL)
					strcat(tmp,", ");

				if (keys[1] != KEY_NULL)
					strcat (tmp, G_KeynumToString (keys[1]));

			}
			V_DrawRightAlignedString(BASEVIDWIDTH-currentMenu->x, y, highlightflags|V_ALLOWLOWERCASE, tmp);
		}
		else if ((currentMenu->menuitems[i].status == IT_HEADER) && (i != max-1))
			V_DrawString(19, y+6, highlightflags|V_ALLOWLOWERCASE, currentMenu->menuitems[i].text);
		else if (currentMenu->menuitems[i].status & IT_STRING)
		{
			V_DrawString(x, y, ((i == itemOn) ? highlightflags|V_ALLOWLOWERCASE : V_ALLOWLOWERCASE), currentMenu->menuitems[i].text);

			if (currentMenu->menuitems[i].status & IT_CVAR)
			{
				consvar_t *cv = (consvar_t *)currentMenu->menuitems[i].itemaction;

				// IT_HEADER matches IT_CVAR, for some reason...
				if (cv)
				{
					INT32 w = V_StringWidth(cv->string, 0);
					V_DrawString(BASEVIDWIDTH - x - w, y,
						((cv->flags & CV_CHEAT) && !CV_IsSetToDefault(cv) ? warningflags : highlightflags)|MENUCAPS, cv->string);
					if (i == itemOn)
					{
						V_DrawCharacter(BASEVIDWIDTH - x - 10 - w - (skullAnimCounter/5), y,
								'\x1C' | highlightflags, false); // left arrow
						V_DrawCharacter(BASEVIDWIDTH - x + 2 + (skullAnimCounter/5), y,
								'\x1D' | highlightflags, false); // right arrow
					}
				}
			}
		}

		y += SMALLLINEHEIGHT;
	}

	V_DrawScaledPatch(currentMenu->x - 20, cursory, 0, (patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));

	M_DoToolTips(currentMenu);
}

#undef controlheight

static INT32 controltochange;
static char controltochangetext[33];

static void M_ChangecontrolResponse(event_t *ev)
{
	INT32        control;
	INT32        found;
	INT32        ch = ev->data1;

	// ESCAPE cancels; dummy out PAUSE
	if (ch != KEY_ESCAPE && ch != KEY_PAUSE)
	{
		switch (ev->type)
		{
			// ignore mouse/joy movements, just get buttons
			case ev_mouse:
			case ev_joystick:
			case ev_joystick2:
			case ev_joystick3:
			case ev_joystick4:
				ch = KEY_NULL;      // no key
			break;

			// keypad arrows are converted for the menu in cursor arrows
			// so use the event instead of ch
			case ev_keydown:
				ch = ev->data1;
			break;

			default:
			break;
		}

		control = controltochange;

		// check if we already entered this key
		found = -1;
		if (setupcontrols[control][0] ==ch)
			found = 0;
		else if (setupcontrols[control][1] ==ch)
			found = 1;

		if (found >= 0)
		{
			// replace mouse and joy clicks by double clicks
			if (ch >= KEY_MOUSE1 && ch <= KEY_MOUSE1+MOUSEBUTTONS)
				setupcontrols[control][found] = ch-KEY_MOUSE1+KEY_DBLMOUSE1;
			else if (ch >= KEY_JOY1 && ch <= KEY_JOY1+JOYBUTTONS)
				setupcontrols[control][found] = ch-KEY_JOY1+KEY_DBLJOY1;
			else if (ch >= KEY_2JOY1 && ch <= KEY_2JOY1+JOYBUTTONS)
				setupcontrols[control][found] = ch-KEY_2JOY1+KEY_DBL2JOY1;
			else if (ch >= KEY_3JOY1 && ch <= KEY_3JOY1+JOYBUTTONS)
				setupcontrols[control][found] = ch-KEY_3JOY1+KEY_DBL3JOY1;
			else if (ch >= KEY_4JOY1 && ch <= KEY_4JOY1+JOYBUTTONS)
				setupcontrols[control][found] = ch-KEY_4JOY1+KEY_DBL4JOY1;
		}
		else
		{
			// check if change key1 or key2, or replace the two by the new
			found = 0;
			if (setupcontrols[control][0] == KEY_NULL)
				found++;
			if (setupcontrols[control][1] == KEY_NULL)
				found++;
			if (found == 2)
			{
				found = 0;
				setupcontrols[control][1] = KEY_NULL;  //replace key 1,clear key2
			}

			(void)G_CheckDoubleUsage(ch, true);
			setupcontrols[control][found] = ch;
		}

		S_StartSound(NULL, sfx_s221);
	}
	else if (ch == KEY_PAUSE)
	{
		// This buffer assumes a 125-character message plus a 32-character control name (per controltochangetext buffer size)
		static char tmp[158];
		menu_t *prev = currentMenu->prevMenu;

		if (controltochange == gc_pause)
			sprintf(tmp, M_GetText("The \x82Pause Key \x80is enabled, but \nyou may select another key. \n\nHit another key for\n%s\nESC for Cancel"),
				controltochangetext);
		else
			sprintf(tmp, M_GetText("The \x82Pause Key \x80is enabled, but \nit is not configurable. \n\nHit another key for\n%s\nESC for Cancel"),
				controltochangetext);

		M_StartMessage(tmp, M_ChangecontrolResponse, MM_EVENTHANDLER);
		currentMenu->prevMenu = prev;

		S_StartSound(NULL, sfx_s3k42);
		return;
	}
	else
		S_StartSound(NULL, sfx_s224);

	M_StopMessage(0);
}

static void M_ChangeControl(INT32 choice)
{
	// This buffer assumes a 35-character message (per below) plus a max control name limit of 32 chars (per controltochangetext)
	// If you change the below message, then change the size of this buffer!
	static char tmp[68];

	controltochange = currentMenu->menuitems[choice].alphaKey;
	sprintf(tmp, M_GetText("Hit the new key for\n%s\nESC for Cancel"),
		currentMenu->menuitems[choice].text);
	strlcpy(controltochangetext, currentMenu->menuitems[choice].text, 33);

	M_StartMessage(tmp, M_ChangecontrolResponse, MM_EVENTHANDLER);
}

static void M_ResetControlsResponse(INT32 ch)
{
	INT32 i;

	if (ch != 'y' && ch != KEY_ENTER)
		return;

	// clear all controls
	for (i = 0; i < num_gamecontrols; i++)
	{
		switch (setupcontrolplayer)
		{
			case 4:
				G_ClearControlKeys(gamecontrol[3], i);
				break;
			case 3:
				G_ClearControlKeys(gamecontrol[2], i);
				break;
			case 2:
				G_ClearControlKeys(gamecontrol[1], i);
				break;
			case 1:
			default:
				G_ClearControlKeys(gamecontrol[0], i);
				break;
		}
	}

	// Setup original defaults
	G_Controldefault(setupcontrolplayer);

	// Setup gamepad option defaults (yucky)
	for (UINT8 j = 0; j < MAXSPLITSCREENPLAYERS; j++)
	{
		if (setupcontrolplayer != j)
			continue;

		CV_StealthSet(&cv_usejoystick[j],	cv_usejoystick[j].defaultvalue);
		CV_StealthSet(&cv_turnaxis[j],		cv_turnaxis[j].defaultvalue);
		CV_StealthSet(&cv_camturnaxis[j],	cv_camturnaxis[j].defaultvalue);
		CV_StealthSet(&cv_camstrafeaxis[j],	cv_camstrafeaxis[j].defaultvalue);
		CV_StealthSet(&cv_moveaxis[j],		cv_moveaxis[j].defaultvalue);
		CV_StealthSet(&cv_brakeaxis[j],		cv_brakeaxis[j].defaultvalue);
		CV_StealthSet(&cv_aimaxis[j],		cv_aimaxis[j].defaultvalue);
		CV_StealthSet(&cv_lookaxis[j],		cv_lookaxis[j].defaultvalue);
		CV_StealthSet(&cv_fireaxis[j],		cv_fireaxis[j].defaultvalue);
		CV_StealthSet(&cv_driftaxis[j],		cv_driftaxis[j].defaultvalue);
		CV_StealthSet(&cv_lookbackaxis[j],	cv_lookbackaxis[j].defaultvalue);
		CV_StealthSet(&cv_custom1axis[j],	cv_custom1axis[j].defaultvalue);
		CV_StealthSet(&cv_custom2axis[j],	cv_custom2axis[j].defaultvalue);
		CV_StealthSet(&cv_custom3axis[j],	cv_custom3axis[j].defaultvalue);
	}
	S_StartSound(NULL, sfx_s224);
}

static void M_ResetControls(INT32 choice)
{
	(void)choice;
	M_StartMessage(va(M_GetText("Reset Player %d's controls to defaults?\n\n(Press 'Y' to confirm)\n"), setupcontrolplayer), M_ResetControlsResponse, MM_YESNO);
}

// ===============
// VIDEO MODE MENU
// ===============

//added : 30-01-98:
#define MAXCOLUMNMODES   12     //max modes displayed in one column
#define MAXMODEDESCS     (MAXCOLUMNMODES*3)

static modedesc_t modedescs[MAXMODEDESCS];

static void M_VideoModeMenu(INT32 choice)
{
	INT32 i, j, vdup, nummodes, width, height;
	const char *desc;

	(void)choice;

	memset(modedescs, 0, sizeof(modedescs));

	VID_PrepareModeList(); // FIXME: hack

	vidm_nummodes = 0;
	vidm_selected = 0;
	nummodes = VID_NumModes();

	// DOS does not skip mode 0, because mode 0 is ALWAYS present
	i = 0;
	for (; i < nummodes && vidm_nummodes < MAXMODEDESCS; i++)
	{
		desc = VID_GetModeName(i);
		if (desc)
		{
			vdup = 0;

			// when a resolution exists both under VGA and VESA, keep the
			// VESA mode, which is always a higher modenum
			for (j = 0; j < vidm_nummodes; j++)
			{
				if (fastcmp(modedescs[j].desc, desc))
				{
					// mode(0): 320x200 is always standard VGA, not vesa
					if (modedescs[j].modenum)
					{
						modedescs[j].modenum = i;
						vdup = 1;

						if (i == vid.modenum)
							vidm_selected = j;
					}
					else
						vdup = 1;

					break;
				}
			}

			if (!vdup)
			{
				modedescs[vidm_nummodes].modenum = i;
				modedescs[vidm_nummodes].desc = desc;

				if (i == vid.modenum)
					vidm_selected = vidm_nummodes;

				// Pull out the width and height
				sscanf(desc, "%u%*c%u", &width, &height);

				// Show multiples of 320x200 as green.
				if (SCR_IsAspectCorrect(width, height))
					modedescs[vidm_nummodes].goodratio = 1;

				vidm_nummodes++;
			}
		}
	}

	vidm_column_size = (vidm_nummodes+2) / 3;

	M_SetupNextMenu(&OP_VideoModeDef);
}

static void M_DrawVideoMenu(void)
{
	M_DrawGenericMenu();

	V_DrawRightAlignedString(BASEVIDWIDTH - currentMenu->x, currentMenu->y + OP_VideoOptionsMenu[0].alphaKey,
		(SCR_IsAspectCorrect(vid.width, vid.height) ? recommendedflags : highlightflags)|MENUCAPS,
			va("%dx%d", vid.width, vid.height));
}

static void M_DrawHUDOptions(void)
{
	const char *str0 = ")";
	const char *str1 = " Warning highlight";
	const char *str2 = ",";
	const char *str3 = "Good highlight";
	INT32 x = BASEVIDWIDTH - currentMenu->x + 2, y = currentMenu->y + 110;
	INT32 w0 = V_StringWidth(str0, 0), w1 = V_StringWidth(str1, 0), w2 = V_StringWidth(str2, 0), w3 = V_StringWidth(str3, 0);

	M_DrawGenericMenu();

	x -= w0;
	V_DrawString(x, y, highlightflags, str0);
	x -= w1;
	V_DrawString(x, y, warningflags, str1);
	x -= w2;
	V_DrawString(x, y, highlightflags, str2);
	x -= w3;
	V_DrawString(x, y, recommendedflags, str3);
	V_DrawRightAlignedString(x, y, highlightflags, "(");
}

static void M_CameraMenu(INT32 choice)
{
	(void)choice;
	OP_CamOptionsDef.prevMenu = currentMenu;
	M_SetupNextMenu(&OP_CamOptionsDef);
}

static void M_LocalSkinMenu(INT32 choice)
{
	(void)choice;

	multi_state = &states[mobjinfo[MT_PLAYER].seestate];
	multi_tics = multi_state->tics;

	OP_ForkedBirdDef.prevMenu = currentMenu;
	M_SetupNextMenu(&OP_ForkedBirdDef);
}

static void M_LocalSkinChange(INT32 choice)
{
	(void)choice;

	switch (itemOn) {
		case 3:
			COM_BufAddText(va("localskin %s -a", cv_fakelocalskin.string));
			break;
		case 4:
			COM_BufAddText(va("localskin %s -d 0", cv_fakelocalskin.string));
			break;
		case 5:
			COM_BufAddText(va("localskin %s", cv_fakelocalskin.string));
			break;
		default:
			break;
	}
	S_StartSound(NULL, sfx_s221);
}

// Display our localskin in our goofy ahhhh local skin menu
static void M_DrawLocalSkinMenu(void)
{
	INT32 mx, my, st, flags = 0;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
	patch_t *patch;
	UINT8 frame;
	INT32 skintodisplay = 0;
	UINT32 speenframe;
	skin_t displayskin;

	mx = OP_ForkedBirdDef.x;
	my = OP_ForkedBirdDef.y;

	// use generic drawer for cursor, items and title
	M_DrawGenericMenu();

#define charw 72

	// anim the player in the box
	multi_tics -= renderdeltatics;

	while (multi_tics <= 0)
	{
		st = cv_skinselectspin.value == SKINSELECTSPIN_PAIN ? S_KART_PAIN : multi_state->nextstate;

		if (st != S_NULL)
			multi_state = &states[st];

		if (multi_state->tics <= -1)
			multi_tics += 15*FRACUNIT;
		else
			multi_tics += multi_state->tics * FRACUNIT;
	}

	// skin 0 is default player sprite
	skintodisplay = R_AnySkinAvailable(cv_fakelocalskin.string);

	if (skintodisplay < 0)
	{
		// ATTEMPT TO FIND REAL SKIN
		skintodisplay = R_AnySkinAvailable(cv_skin.string);

		if (skintodisplay < 0) // STILL NOTHIN? use sonic instead
		{
			skintodisplay = 0;
		}
	}

	displayskin = allskins[skintodisplay];

	sprdef = &displayskin.spritedef;

	if (!sprdef->numframes) // No frames ??
		return; // Can't render!

	frame = multi_state->frame & FF_FRAMEMASK;
	if (frame >= sprdef->numframes) // Walking animation missing
		frame = 0; // Try to use standing frame

	// draw box around guy
	V_DrawFill(mx + 220 - (charw/2), my+54, charw, 84, 239);
#undef charw

	UINT8 *colormap = R_GetLocalTranslationColormap(&skins[displayskin.localnum], (displayskin.localskin ? &localskins[displayskin.localnum] : NULL), cv_playercolor.value, GTC_MENUCACHE, displayskin.localskin);

	V_DrawMappedPatch(mx, my+50, 0, (patch_t *)W_CachePatchName(displayskin.facewant, PU_PATCH), colormap);
	V_DrawMappedPatch(mx+8, my+85, 0, (patch_t *)W_CachePatchName(displayskin.facerank, PU_PATCH), colormap);

	V_DrawString(mx, my+108, V_ALLOWLOWERCASE, "Character");

	if (strlen(displayskin.realname) > 10)
		V_DrawThinString(mx+20, my+118, V_ALLOWLOWERCASE|highlightflags, displayskin.realname);
	else
		V_DrawString(mx+20, my+118, V_ALLOWLOWERCASE|highlightflags, displayskin.realname);

	// draw player sprite
	mx += 220;
	my += 120;

#ifdef HWRENDER
	md2_t *md2 = (displayskin.localskin ? &md2_localplayermodels[skintodisplay] : &md2_playermodels[skintodisplay]);

	// if we have 3d models enabled and a model exists
	// try to show it instead of the sprite
	if (rendermode == render_opengl && cv_glmdls.value
	&& !md2->error && !md2->notfound)
	{
		const angle_t ROTATE_PER_TIC = (UINT64)ANGLE_45 * cv_skinselectspin.value / TICRATE;
		angle_t angle = I_GetTime()*ROTATE_PER_TIC + FixedMul(cv_uncappedhud.value ? renderdeltatics : FRACUNIT, ROTATE_PER_TIC);
		HWR_Draw2DModel(md2, mx, my, skintodisplay, (skincolors_t)cv_playercolor.value, colormap, 8*FRACUNIT/3, frame, angle);
	}
	else
#endif
	{
		sprframe = &sprdef->spriteframes[frame];

		// minenice's speen css, it's a piece of shit but hey
		speenframe = (I_GetTime()*cv_skinselectspin.value/TICRATE + 1)%8;

		// this is a very shitty solution for checking if a sprite needs flipping
		// but it works
		if ((speenframe > 4) && (sprframe->lumppat[speenframe] == sprframe->lumppat[8-speenframe]))
		{
			flags = V_FLIP; // This sprite is left/right flipped!
		}

		patch = (patch_t *)W_CachePatchNum(sprframe->lumppat[speenframe], PU_PATCH);

		if (displayskin.flags & SF_HIRES)
		{
			V_DrawFixedPatch(mx<<FRACBITS, my<<FRACBITS, displayskin.highresscale, flags, patch, colormap);
		}
		else
		{
			V_DrawMappedPatch(mx, my, flags, patch, colormap);
		}
	}
}

// Draw the video modes list, a-la-Quake
static void M_DrawVideoMode(void)
{
	INT32 i, j, row, col;

	// draw title
	M_DrawMenuTitle();

	V_DrawCenteredString(BASEVIDWIDTH/2, OP_VideoModeDef.y,
		highlightflags|MENUCAPS, "Choose mode, reselect to change default");

	row = 41;
	col = OP_VideoModeDef.y + 14;
	for (i = 0; i < vidm_nummodes; i++)
	{
		if (i == vidm_selected)
			V_DrawString(row, col, highlightflags|MENUCAPS, modedescs[i].desc);
		// Show multiples of 320x200 as green.
		else
			V_DrawString(row, col, ((modedescs[i].goodratio) ? recommendedflags : 0)|MENUCAPS, modedescs[i].desc);

		col += 8;
		if ((i % vidm_column_size) == (vidm_column_size-1))
		{
			row += 7*13;
			col = OP_VideoModeDef.y + 14;
		}
	}

	if (vidm_testingmode > 0)
	{
		INT32 testtime = (vidm_testingmode/TICRATE) + 1;

		M_CentreText(OP_VideoModeDef.y + 116,
			va("Previewing mode %c%dx%d",
				(SCR_IsAspectCorrect(vid.width, vid.height)) ? 0x83 : 0x80,
				vid.width, vid.height));
		M_CentreText(OP_VideoModeDef.y + 138,
			"Press ENTER again to keep this mode");
		M_CentreText(OP_VideoModeDef.y + 150,
			va("Wait %d second%s", testtime, (testtime > 1) ? "s" : ""));
		M_CentreText(OP_VideoModeDef.y + 158,
			"or press ESC to return");
	}
	else
	{
		M_CentreText(OP_VideoModeDef.y + 116,
			va("Current mode is %c%dx%d",
				(SCR_IsAspectCorrect(vid.width, vid.height)) ? 0x83 : 0x80,
				vid.width, vid.height));
		M_CentreText(OP_VideoModeDef.y + 124,
			va("Default mode is %c%dx%d",
				(SCR_IsAspectCorrect(cv_scr_width.value, cv_scr_height.value)) ? 0x83 : 0x80,
				cv_scr_width.value, cv_scr_height.value));

		V_DrawCenteredString(BASEVIDWIDTH/2, OP_VideoModeDef.y + 138,
			recommendedflags|MENUCAPS, "Marked modes are recommended.");
		V_DrawCenteredString(BASEVIDWIDTH/2, OP_VideoModeDef.y + 146,
			highlightflags|MENUCAPS, "Other modes may have visual errors.");
		V_DrawCenteredString(BASEVIDWIDTH/2, OP_VideoModeDef.y + 158,
			highlightflags|MENUCAPS, "Larger modes may have performance issues.");
	}

	// Draw the cursor for the VidMode menu
	i = 41 - 10 + ((vidm_selected / vidm_column_size)*7*13);
	j = OP_VideoModeDef.y + 14 + ((vidm_selected % vidm_column_size)*8);

	V_DrawScaledPatch(i - 8, j, 0, (patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
}

// Just M_DrawGenericScrollMenu but showing a backing behind the headers.
static void M_DrawColorMenu(void)
{
	INT32 x, y, i, max, tempcentery, cursory = 0;

	// DRAW MENU
	x = currentMenu->x;
	y = currentMenu->y;

	if ((currentMenu->menuitems[itemOn].alphaKey*2 - currentMenu->menuitems[0].alphaKey*2) <= scrollareaheight)
		tempcentery = currentMenu->y - currentMenu->menuitems[0].alphaKey*2;
	else if ((currentMenu->menuitems[currentMenu->numitems-1].alphaKey*2 - currentMenu->menuitems[itemOn].alphaKey*2) <= scrollareaheight)
		tempcentery = currentMenu->y - currentMenu->menuitems[currentMenu->numitems-1].alphaKey*2 + 2*scrollareaheight;
	else
		tempcentery = currentMenu->y - currentMenu->menuitems[itemOn].alphaKey*2 + scrollareaheight;

	for (i = 0; i < currentMenu->numitems; i++)
	{
		if (currentMenu->menuitems[i].status != IT_DISABLED && currentMenu->menuitems[i].alphaKey*2 + tempcentery >= currentMenu->y)
			break;
	}

	for (max = currentMenu->numitems; max > 0; max--)
	{
		if (currentMenu->menuitems[max].status != IT_DISABLED && currentMenu->menuitems[max-1].alphaKey*2 + tempcentery <= (currentMenu->y + 2*scrollareaheight))
			break;
	}

	if (i)
		V_DrawString(currentMenu->x - 20, currentMenu->y, V_YELLOWMAP, "\x1A"); // up arrow
	if (max != currentMenu->numitems)
		V_DrawString(currentMenu->x - 20, currentMenu->y + 2*scrollareaheight, V_YELLOWMAP, "\x1B"); // down arrow

	// draw title (or big pic)
	M_DrawMenuTitle();

	for (; i < max; i++)
	{
		y = currentMenu->menuitems[i].alphaKey*2 + tempcentery;
		if (i == itemOn)
			cursory = y;
		switch (currentMenu->menuitems[i].status & IT_DISPLAY)
		{
			case IT_PATCH:
			case IT_DYBIGSPACE:
			case IT_BIGSLIDER:
			case IT_STRING2:
			case IT_DYLITLSPACE:
			case IT_GRAYPATCH:
			case IT_TRANSTEXT2:
				// unsupported
				break;
			case IT_NOTHING:
				break;
			case IT_STRING:
			case IT_WHITESTRING:
				if (i != itemOn && (currentMenu->menuitems[i].status & IT_DISPLAY)==IT_STRING)
					V_DrawString(x, y, MENUCAPS, currentMenu->menuitems[i].text);
				else
					V_DrawString(x, y, V_YELLOWMAP|MENUCAPS, currentMenu->menuitems[i].text);

				// Cvar specific handling
				switch (currentMenu->menuitems[i].status & IT_TYPE)
					case IT_CVAR:
					{
						consvar_t *cv = (consvar_t *)currentMenu->menuitems[i].itemaction;
						switch (currentMenu->menuitems[i].status & IT_CVARTYPE)
						{
							case IT_CV_SLIDER:
								M_DrawSlider(x, y, cv, (i == itemOn));
							case IT_CV_NOPRINT: // color use this
							case IT_CV_INVISSLIDER: // monitor toggles use this
								break;
							case IT_CV_STRING:
								if (y + 12 > (currentMenu->y + 2*scrollareaheight))
									break;
								M_DrawTextBox(x, y + 4, MAXSTRINGLENGTH, 1);

								if (itemOn == i)
									M_DrawTextInput(x + 8, y + 12, &menuinput, 0);
								else
									V_DrawString(x + 8, y + 12, V_ALLOWLOWERCASE, cv->string);

								break;
							default:
								V_DrawRightAlignedString(BASEVIDWIDTH - x, y,
									((cv->flags & CV_CHEAT) && !CV_IsSetToDefault(cv) ? V_REDMAP : V_YELLOWMAP), cv->string);
								break;
						}
						break;
					}
					break;
			case IT_TRANSTEXT:
				V_DrawString(x, y, V_TRANSLUCENT|MENUCAPS, currentMenu->menuitems[i].text);
				break;
			case IT_QUESTIONMARKS:
				V_DrawString(x, y, V_TRANSLUCENT|V_OLDSPACING|MENUCAPS, M_CreateSecretMenuOption(currentMenu->menuitems[i].text));
				break;
			case IT_HEADERTEXT:
				V_DrawString(x-16, y, V_YELLOWMAP|MENUCAPS, currentMenu->menuitems[i].text);
				break;
		}
	}

	// DRAW THE SKULL CURSOR
	V_DrawScaledPatch(currentMenu->x - 24, cursory, 0, (patch_t *)W_CachePatchName("M_CURSOR", PU_PATCH));
}


// special menuitem key handler for video mode list
static void M_HandleVideoMode(INT32 ch)
{
	if (vidm_testingmode > 0) switch (ch)
	{
		// change back to the previous mode quickly
		case KEY_ESCAPE:
			setmodeneeded = vidm_previousmode + 1;
			vidm_testingmode = 0;
			break;

		case KEY_ENTER:
			S_StartSound(NULL, sfx_menu1);
			vidm_testingmode = 0; // stop testing
	}

	else switch (ch)
	{
		case KEY_DOWNARROW:
			S_StartSound(NULL, sfx_menu1);
			if (++vidm_selected >= vidm_nummodes)
				vidm_selected = 0;
			break;

		case KEY_UPARROW:
			S_StartSound(NULL, sfx_menu1);
			if (--vidm_selected < 0)
				vidm_selected = vidm_nummodes - 1;
			break;

		case KEY_LEFTARROW:
			S_StartSound(NULL, sfx_menu1);
			vidm_selected -= vidm_column_size;
			if (vidm_selected < 0)
				vidm_selected = (vidm_column_size*3) + vidm_selected;
			if (vidm_selected >= vidm_nummodes)
				vidm_selected = vidm_nummodes - 1;
			break;

		case KEY_RIGHTARROW:
			S_StartSound(NULL, sfx_menu1);
			vidm_selected += vidm_column_size;
			if (vidm_selected >= (vidm_column_size*3))
				vidm_selected %= vidm_column_size;
			if (vidm_selected >= vidm_nummodes)
				vidm_selected = vidm_nummodes - 1;
			break;

		case KEY_ENTER:
			S_StartSound(NULL, sfx_menu1);
			if (vid.modenum == modedescs[vidm_selected].modenum)
				SCR_SetDefaultMode();
			else
			{
				vidm_testingmode = 15*TICRATE;
				vidm_previousmode = vid.modenum;
				if (!setmodeneeded) // in case the previous setmode was not finished
					setmodeneeded = modedescs[vidm_selected].modenum + 1;
			}
			break;

		case KEY_ESCAPE: // this one same as M_Responder
			if (currentMenu->prevMenu)
				M_SetupNextMenu(currentMenu->prevMenu);
			else
				M_ClearMenus(true);
			break;

		default:
			break;
	}
}

// ===============
// Monitor Toggles
// ===============
static consvar_t *kartitemcvs[NUMKARTRESULTS-1] = {
	&cv_sneaker,
	&cv_rocketsneaker,
	&cv_invincibility,
	&cv_banana,
	&cv_eggmanmonitor,
	&cv_orbinaut,
	&cv_jawz,
	&cv_mine,
	&cv_ballhog,
	&cv_selfpropelledbomb,
	&cv_grow,
	&cv_shrink,
	&cv_thundershield,
	&cv_hyudoro,
	&cv_pogospring,
	&cv_kitchensink,
	&cv_triplesneaker,
	&cv_triplebanana,
	&cv_decabanana,
	&cv_tripleorbinaut,
	&cv_quadorbinaut,
	&cv_dualjawz
};

static tic_t shitsfree = 0;

static void M_DrawMonitorToggles(void)
{
	const INT32 edges = 4;
	const INT32 height = 4;
	const INT32 spacing = 35;
	const INT32 column = itemOn/height;
	//const INT32 row = itemOn%height;
	INT32 leftdraw, rightdraw, totaldraw;
	INT32 x = currentMenu->x, y = currentMenu->y+(spacing/4);
	INT32 onx = 0, ony = 0;
	consvar_t *cv;
	INT32 i, translucent, drawnum;

	M_DrawMenuTitle();

	// Find the available space around column
	leftdraw = rightdraw = column;
	totaldraw = 0;
	for (i = 0; (totaldraw < edges*2 && i < edges*4); i++)
	{
		if (rightdraw+1 < (currentMenu->numitems/height)+1)
		{
			rightdraw++;
			totaldraw++;
		}
		if (leftdraw-1 >= 0)
		{
			leftdraw--;
			totaldraw++;
		}
	}

	for (i = leftdraw; i <= rightdraw; i++)
	{
		INT32 j;

		for (j = 0; j < height; j++)
		{
			const INT32 thisitem = (i*height)+j;

			if (thisitem >= currentMenu->numitems)
				continue;

			if (thisitem == itemOn)
			{
				onx = x;
				ony = y;
				y += spacing;
				continue;
			}

#ifdef ITEMTOGGLEBOTTOMRIGHT
			if (currentMenu->menuitems[thisitem].alphaKey == 255)
			{
				V_DrawScaledPatch(x, y, V_TRANSLUCENT, (patch_t *)W_CachePatchName("K_ISBG", PU_PATCH));
				continue;
			}
#endif
			if (currentMenu->menuitems[thisitem].alphaKey == 0)
			{
				V_DrawScaledPatch(x, y, 0, (patch_t *)W_CachePatchName("K_ISBG", PU_PATCH));
				V_DrawScaledPatch(x, y, 0, (patch_t *)W_CachePatchName("K_ISTOGL", PU_PATCH));
				continue;
			}

			cv = kartitemcvs[currentMenu->menuitems[thisitem].alphaKey-1];
			translucent = (cv->value ? 0 : V_TRANSLUCENT);

			switch (currentMenu->menuitems[thisitem].alphaKey)
			{
				case KRITEM_DUALJAWZ:
					drawnum = 2;
					break;
				case KRITEM_TRIPLESNEAKER:
				case KRITEM_TRIPLEBANANA:
				case KRITEM_TRIPLEORBINAUT:
					drawnum = 3;
					break;
				case KRITEM_QUADORBINAUT:
					drawnum = 4;
					break;
				case KRITEM_TENFOLDBANANA:
					drawnum = 10;
					break;
				default:
					drawnum = 0;
					break;
			}

			if (cv->value)
				V_DrawScaledPatch(x, y, 0, (patch_t *)W_CachePatchName("K_ISBG", PU_PATCH));
			else
				V_DrawScaledPatch(x, y, 0, (patch_t *)W_CachePatchName("K_ISBGD", PU_PATCH));

			if (drawnum != 0)
			{
				V_DrawScaledPatch(x, y, 0, (patch_t *)W_CachePatchName("K_ISMUL", PU_PATCH));
				V_DrawScaledPatch(x, y, translucent, (patch_t *)W_CachePatchName(K_GetItemPatch(currentMenu->menuitems[thisitem].alphaKey, true), PU_PATCH));
				V_DrawString(x+24, y+31, V_ALLOWLOWERCASE|translucent, va("x%d", drawnum));
			}
			else
				V_DrawScaledPatch(x, y, translucent, (patch_t *)W_CachePatchName(K_GetItemPatch(currentMenu->menuitems[thisitem].alphaKey, true), PU_PATCH));

			y += spacing;
		}

		x += spacing;
		y = currentMenu->y+(spacing/4);
	}

	{
#ifdef ITEMTOGGLEBOTTOMRIGHT
		if (currentMenu->menuitems[itemOn].alphaKey == 255)
		{
			V_DrawScaledPatch(onx-1, ony-2, V_TRANSLUCENT, (patch_t *)W_CachePatchName("K_ITBG", PU_PATCH));
			if (shitsfree)
			{
				INT32 trans = V_TRANSLUCENT;
				if (shitsfree-1 > TICRATE-5)
					trans = ((10-TICRATE)+shitsfree-1)<<V_ALPHASHIFT;
				else if (shitsfree < 5)
					trans = (10-shitsfree)<<V_ALPHASHIFT;
				V_DrawScaledPatch(onx-1, ony-2, trans, (patch_t *)W_CachePatchName("K_ITFREE", PU_PATCH));
			}
		}
		else
#endif
		if (currentMenu->menuitems[itemOn].alphaKey == 0)
		{
			V_DrawScaledPatch(onx-1, ony-2, 0, (patch_t *)W_CachePatchName("K_ITBG", PU_PATCH));
			V_DrawScaledPatch(onx-1, ony-2, 0, (patch_t *)W_CachePatchName("K_ITTOGL", PU_PATCH));
		}
		else
		{
			cv = kartitemcvs[currentMenu->menuitems[itemOn].alphaKey-1];
			translucent = (cv->value ? 0 : V_TRANSLUCENT);

			switch (currentMenu->menuitems[itemOn].alphaKey)
			{
				case KRITEM_DUALJAWZ:
					drawnum = 2;
					break;
				case KRITEM_TRIPLESNEAKER:
				case KRITEM_TRIPLEBANANA:
					drawnum = 3;
					break;
				case KRITEM_TENFOLDBANANA:
					drawnum = 10;
					break;
				default:
					drawnum = 0;
					break;
			}

			if (cv->value)
				V_DrawScaledPatch(onx-1, ony-2, 0, (patch_t *)W_CachePatchName("K_ITBG", PU_PATCH));
			else
				V_DrawScaledPatch(onx-1, ony-2, 0, (patch_t *)W_CachePatchName("K_ITBGD", PU_PATCH));

			if (drawnum != 0)
			{
				V_DrawScaledPatch(onx-1, ony-2, 0, (patch_t *)W_CachePatchName("K_ITMUL", PU_PATCH));
				V_DrawScaledPatch(onx-1, ony-2, translucent, (patch_t *)W_CachePatchName(K_GetItemPatch(currentMenu->menuitems[itemOn].alphaKey, false), PU_PATCH));
				V_DrawScaledPatch(onx+27, ony+39, translucent, (patch_t *)W_CachePatchName("K_ITX", PU_PATCH));
				V_DrawKartString(onx+37, ony+34, translucent, va("%d", drawnum));
			}
			else
				V_DrawScaledPatch(onx-1, ony-2, translucent, (patch_t *)W_CachePatchName(K_GetItemPatch(currentMenu->menuitems[itemOn].alphaKey, false), PU_PATCH));
		}
	}

	if (shitsfree && interpTimerHackAllow)
		shitsfree--;

	V_DrawCenteredString(BASEVIDWIDTH/2, currentMenu->y, highlightflags, va("* %s *", currentMenu->menuitems[itemOn].text));
}

static void M_HandleMonitorToggles(INT32 choice)
{
	const INT32 width = 6, height = 4;
	INT32 column = itemOn/height, row = itemOn%height;
	INT16 next;
	UINT8 i;
	boolean exitmenu = false;

	switch (choice)
	{
		case KEY_RIGHTARROW:
			S_StartSound(NULL, sfx_menu1);
			column++;
			if (((column*height)+row) >= currentMenu->numitems)
				column = 0;
			next = min(((column*height)+row), currentMenu->numitems-1);
			itemOn = next;
			break;

		case KEY_LEFTARROW:
			S_StartSound(NULL, sfx_menu1);
			column--;
			if (column < 0)
				column = width-1;
			if (((column*height)+row) >= currentMenu->numitems)
				column--;
			next = max(((column*height)+row), 0);
			if (next >= currentMenu->numitems)
				next = currentMenu->numitems-1;
			itemOn = next;
			break;

		case KEY_DOWNARROW:
			S_StartSound(NULL, sfx_menu1);
			row = (row+1) % height;
			if (((column*height)+row) >= currentMenu->numitems)
				row = 0;
			next = min(((column*height)+row), currentMenu->numitems-1);
			itemOn = next;
			break;

		case KEY_UPARROW:
			S_StartSound(NULL, sfx_menu1);
			row = (row-1) % height;
			if (row < 0)
				row = height-1;
			if (((column*height)+row) >= currentMenu->numitems)
				row--;
			next = max(((column*height)+row), 0);
			if (next >= currentMenu->numitems)
				next = currentMenu->numitems-1;
			itemOn = next;
			break;

		case KEY_ENTER:
#ifdef ITEMTOGGLEBOTTOMRIGHT
			if (currentMenu->menuitems[itemOn].alphaKey == 255)
			{
				//S_StartSound(NULL, sfx_s26d);
				if (!shitsfree)
				{
					shitsfree = TICRATE;
					S_StartSound(NULL, sfx_itfree);
				}
			}
			else
#endif
			if (currentMenu->menuitems[itemOn].alphaKey == 0)
			{
				INT32 v = cv_sneaker.value;
				S_StartSound(NULL, sfx_s1b4);
				for (i = 0; i < NUMKARTRESULTS-1; i++)
				{
					if (kartitemcvs[i]->value == v)
						CV_AddValue(kartitemcvs[i], 1);
				}
			}
			else
			{
				S_StartSound(NULL, sfx_s1ba);
				CV_AddValue(kartitemcvs[currentMenu->menuitems[itemOn].alphaKey-1], 1);
			}
			break;

		case KEY_ESCAPE:
			exitmenu = true;
			break;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu);
		else
			M_ClearMenus(true);
	}
}

// =========
// Quit Game
// =========
static const INT32 quitsounds[] =
{
	// holy shit we're changing things up!
	// srb2kart: you ain't seen nothing yet
	sfx_kc2e,
	sfx_kc2f,
	sfx_cdfm01,
	sfx_ddash,
	sfx_s3ka2,
	sfx_s3k49,
	sfx_slip,
	sfx_tossed,
	sfx_s3k7b,
	sfx_itrolf,
	sfx_itrole,
	sfx_cdpcm9,
	sfx_s3k4e,
	sfx_s259,
	sfx_3db06,
	sfx_s3k3a,
	sfx_peel,
	sfx_cdfm28,
	sfx_s3k96,
	sfx_s3kc0s,
	sfx_cdfm39,
	sfx_hogbom,
	sfx_kc5a,
	sfx_kc46,
	sfx_s3k92,
	sfx_s3k42,
	sfx_kpogos,
	sfx_screec
};

void M_QuitResponse(INT32 ch)
{
	tic_t ptime;
	INT32 mrand;

	if (ch != 'y' && ch != KEY_ENTER)
		return;

	if (!(netgame || cv_debug))
	{
		mrand = M_RandomKey(sizeof(quitsounds)/sizeof(INT32));
		if (quitsounds[mrand]) S_StartSound(NULL, quitsounds[mrand]);

		//added : 12-02-98: do that instead of I_WaitVbl which does not work
		ptime = I_GetTime() + NEWTICRATE*2; // Shortened the quit time, used to be 2 seconds Tails 03-26-2001
		while (ptime > I_GetTime())
		{
			V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31);
			V_DrawSmallScaledPatch(0, 0, 0, (patch_t *)W_CachePatchName("GAMEQUIT", PU_PATCH)); // Demo 3 Quit Screen Tails 06-16-2001
			I_FinishUpdate(); // Update the screen with the image Tails 06-19-2001
			I_Sleep(cv_sleep.value);
			I_UpdateTime(cv_timescale.value);
		}
	}

	I_Quit();
}

static void M_QuitSRB2(INT32 choice)
{
	// We pick index 0 which is language sensitive, or one at random,
	// between 1 and maximum number.
	(void)choice;
	M_StartMessage(quitmsg[M_RandomKey(NUM_QUITMESSAGES)], M_QuitResponse, MM_YESNO);
}

#ifdef HAVE_DISCORDRPC

// =====================================================================
// DiscordRPC specific options
// =====================================================================

static const tic_t confirmLength = 3*TICRATE/4;
static tic_t confirmDelay = 0;
static boolean confirmAccept = false;

static void M_HandleDiscordRequests(INT32 choice)
{
	if (confirmDelay > 0)
		return;

	switch (choice)
	{
		case KEY_ENTER:
			Discord_Respond(discordRequestList->userID, DISCORD_REPLY_YES);
			confirmAccept = true;
			confirmDelay = confirmLength;
			S_StartSound(NULL, sfx_s3k63);
			break;

		case KEY_ESCAPE:
			Discord_Respond(discordRequestList->userID, DISCORD_REPLY_NO);
			confirmAccept = false;
			confirmDelay = confirmLength;
			S_StartSound(NULL, sfx_s3kb2);
			break;
	}
}

static const char *M_GetDiscordName(discordRequest_t *r)
{
	if (r == NULL)
		return "";

	if (cv_discordstreamer.value)
		return DRPC_HideUsername(r->username);

	return r->username;
}

// (this goes in k_hud.c when merged into v2)
static void M_DrawSticker(INT32 x, INT32 y, INT32 width, INT32 flags, boolean isSmall)
{
	patch_t *stickerEnd;
	INT32 height;

	if (isSmall == true)
	{
		stickerEnd = (patch_t *)W_CachePatchName("K_STIKE2", PU_PATCH);
		height = 6;
	}
	else
	{
		stickerEnd = (patch_t *)W_CachePatchName("K_STIKEN", PU_PATCH);
		height = 11;
	}

	V_DrawFixedPatch(x*FRACUNIT, y*FRACUNIT, FRACUNIT, flags, stickerEnd, NULL);
	V_DrawFill(x, y, width, height, 24|flags);
	V_DrawFixedPatch((x + width)*FRACUNIT, y*FRACUNIT, FRACUNIT, flags|V_FLIP, stickerEnd, NULL);
}

static void M_DrawDiscordRequests(void)
{
	discordRequest_t *curRequest = discordRequestList;
	UINT8 *colormap;
	patch_t *hand = NULL;
	boolean removeRequest = false;

	const char *wantText = "...would like to join!";
	const char *controlText = "\x82" "ENTER" "\x80" " - Accept    " "\x82" "ESC" "\x80" " - Decline";

	INT32 x = 100;
	INT32 y = 133;

	INT32 slide = 0;
	INT32 maxYSlide = 18;

	if (confirmDelay > 0)
	{
		if (confirmAccept == true)
		{
			colormap = R_GetTranslationColormap(TC_DEFAULT, SKINCOLOR_GREEN, GTC_MENUCACHE);
			hand = (patch_t *)W_CachePatchName("K_LAPH02", PU_PATCH);
		}
		else
		{
			colormap = R_GetTranslationColormap(TC_DEFAULT, SKINCOLOR_RED, GTC_MENUCACHE);
			hand = (patch_t *)W_CachePatchName("K_LAPH03", PU_PATCH);
		}

		slide = confirmLength - confirmDelay;

		confirmDelay--;

		if (confirmDelay == 0)
			removeRequest = true;
	}
	else
	{
		colormap = R_GetTranslationColormap(TC_DEFAULT, SKINCOLOR_GREY, GTC_MENUCACHE);
	}

	V_DrawFixedPatch(56*FRACUNIT, 150*FRACUNIT, FRACUNIT, 0, (patch_t *)W_CachePatchName("K_LAPE01", PU_PATCH), colormap);

	if (hand != NULL)
	{
		fixed_t handoffset = (4 - abs((signed)(skullAnimCounter - 4))) * FRACUNIT;
		V_DrawFixedPatch(56*FRACUNIT, 150*FRACUNIT + handoffset, FRACUNIT, 0, hand, NULL);
	}

	M_DrawSticker(x + (slide * 32), y - 1, V_ThinStringWidth(M_GetDiscordName(curRequest), V_ALLOWLOWERCASE|V_6WIDTHSPACE), 0, false);
	V_DrawThinString(x + (slide * 32), y, V_ALLOWLOWERCASE|V_6WIDTHSPACE|V_YELLOWMAP, M_GetDiscordName(curRequest));

	M_DrawSticker(x, y + 12, V_ThinStringWidth(wantText, V_ALLOWLOWERCASE|V_6WIDTHSPACE), 0, true);
	V_DrawThinString(x, y + 10, V_ALLOWLOWERCASE|V_6WIDTHSPACE, wantText);

	M_DrawSticker(x, y + 26, V_ThinStringWidth(controlText, V_ALLOWLOWERCASE|V_6WIDTHSPACE), 0, true);
	V_DrawThinString(x, y + 24, V_ALLOWLOWERCASE|V_6WIDTHSPACE, controlText);

	y -= 18;

	while (curRequest->next != NULL)
	{
		INT32 ySlide = min(slide * 4, maxYSlide);

		curRequest = curRequest->next;

		M_DrawSticker(x, y - 1 + ySlide, V_ThinStringWidth(M_GetDiscordName(curRequest), V_ALLOWLOWERCASE|V_6WIDTHSPACE), 0, false);
		V_DrawThinString(x, y + ySlide, V_ALLOWLOWERCASE|V_6WIDTHSPACE, M_GetDiscordName(curRequest));

		y -= 12;
		maxYSlide = 12;
	}

	if (removeRequest == true)
	{
		DRPC_RemoveRequest(discordRequestList);

		if (discordRequestList == NULL)
		{
			// No other requests
			MPauseMenu[mpause_discordrequests].status = IT_GRAYEDOUT;

			if (currentMenu->prevMenu)
			{
				M_SetupNextMenu(currentMenu->prevMenu);
				if (currentMenu == &MPauseDef)
					itemOn = mpause_continue;
			}
			else
				M_ClearMenus(true);

			return;
		}
	}
}
#endif
