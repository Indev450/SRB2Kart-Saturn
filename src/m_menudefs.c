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
/// \file  m_menudefs.c
/// \brief XMOD's extremely revamped menu system.

#include "m_menu.h"

// Note: Never should we be jumping from one category of menu options to another
//       without first going to the Main Menu.
// Note: Ignore the above if you're working with the Pause menu.
// Note: (Prefix)_MainMenu should be the target of all Main Menu options that
//       point to submenus.


// the haxor message menu
menu_t MessageDef;

// Pause and Main Menu stuff
menu_t SPauseDef;
menu_t SR_MainDef, SR_UnlockChecklistDef;
menu_t SP_MainDef, MP_MainDef, OP_MainDef;
menu_t MISC_ScrambleTeamDef, MISC_ChangeTeamDef, MISC_ChangeSpectateDef;
menu_t SP_LevelStatsDef;
static menu_t SP_TimeAttackDef, SP_ReplayDef, SP_GuestReplayDef, SP_GhostDef;

// Control menus
menu_t OP_ControlsDef, OP_AllControlsDef;
menu_t OP_MouseOptionsDef;
menu_t OP_Joystick1Def, OP_Joystick2Def, OP_Joystick3Def, OP_Joystick4Def;

// Custom cvar menu
menu_t OP_CustomCvarMenuDef;

// Camera menus
menu_t OP_CamOptionsDef;
menu_t OP_Player1CamOptionsDef, OP_Player2CamOptionsDef, OP_Player3CamOptionsDef, OP_Player4CamOptionsDef;

// Video & Sound
menu_t OP_VideoOptionsDef, OP_VideoModeDef, OP_ExpOptionsDef, OP_ColorOptionsDef;
#ifdef HWRENDER
menu_t OP_OpenGLOptionsDef;
#endif
menu_t OP_SoundOptionsDef;
menu_t OP_SoundAdvancedDef;

menu_t OP_FocusOptionsDef;

//Misc
menu_t OP_DataOptionsDef, OP_ScreenshotOptionsDef, OP_EraseDataDef;
menu_t OP_ProtocolDef;
#ifdef HAVE_DISCORDRPC
menu_t OP_DiscordOptionsDef;
#endif
menu_t OP_HUDOptionsDef, OP_ChatOptionsDef;
menu_t OP_GameOptionsDef, OP_ServerOptionsDef;
menu_t OP_AdvServerOptionsDef;
menu_t OP_MonitorToggleDef;

menu_t OP_AccessibilityDef;

// Saturn
menu_t OP_SaturnDef;
menu_t OP_SaturnHudDef;
menu_t OP_HudOffsetDef;
menu_t OP_PlayerDistortDef;
menu_t OP_SaturnCreditsDef;

// Bird
menu_t OP_BirdDef;

// Stuff, yknow.
menu_t OP_ForkedBirdDef;
menu_t OP_LocalSkinDef;
menu_t OP_TiltDef;
menu_t OP_AdvancedBirdDef;

// Chaotic
menu_t OP_NametagDef;

// Driftgauge
menu_t OP_DriftGaugeDef;

// Replays
menu_t MISC_ReplayHutDef;
menu_t MISC_ReplayOptionsDef;

// ---------
// Main Menu
// ---------
static menuitem_t MainMenu[] =
{
	{IT_SUBMENU|IT_STRING, NULL, "Extras",      &SR_MainDef,        76},
	{IT_CALL   |IT_STRING, NULL, "Time Attack", M_TimeAttack,       84},
	{IT_CALL   |IT_STRING, NULL, "Multiplayer", M_Multiplayer,      92},
	{IT_CALL   |IT_STRING, NULL, "Options",     M_Options,          100},
	{IT_CALL   |IT_STRING, NULL, "Addons",      M_Addons,           108},
	{IT_CALL   |IT_STRING, NULL, "Quit  Game",  M_QuitSRB2,         116},
};

typedef enum
{
	secrets = 0,
	singleplr,
	multiplr,
	options,
	addons,
	quitdoom
} main_e;

static menuitem_t MISC_AddonsMenu[] =
{
	{IT_KEYHANDLER | IT_NOTHING, NULL, "", M_HandleAddons, 0},     // dummy menuitem for the control func
};

static menuitem_t MISC_ReplayHutMenu[] =
{
	{IT_KEYHANDLER|IT_NOTHING, NULL, "", M_HandleReplayHutList, 0}, // Dummy menuitem for the replay list
	{IT_NOTHING,               NULL, "", NULL,                  0}, // Dummy for handling wrapping to the top of the menu..
};

static menuitem_t MISC_ReplayStartMenu[] =
{
	{IT_CALL      |IT_STRING,  NULL, "Load Addons and Watch", M_HutStartReplay,   0},
	{IT_CALL      |IT_STRING,  NULL, "Watch Without Addons",  M_HutStartReplay,   10},
	{IT_CALL      |IT_STRING,  NULL, "Watch Replay",          M_HutStartReplay,   10},
	{IT_SUBMENU   |IT_STRING,  NULL, "Back",                  &MISC_ReplayHutDef, 30},
};

static menuitem_t MISC_ReplayOptionsMenu[] =
{
	{IT_CVAR|IT_STRING, NULL, "Record Replays",      			&cv_recordmultiplayerdemos,  0},
	{IT_CVAR|IT_STRING, NULL, "Save Replays on Map change",		&cv_demochangemap, 			10},
	{IT_CVAR|IT_STRING, NULL, "Sync Check Interval",			&cv_netdemosyncquality,     20},
	{IT_CVAR|IT_STRING, NULL, "Max demo size (MiB)",			&cv_maxdemosize,			30},
	{IT_CVAR|IT_STRING, NULL, "Replay Search Rate",				&cv_replaysearchrate,       40},
	{IT_CVAR|IT_STRING, NULL, "Replay Date Format",				&cv_demodateformat,         50},
};

static tic_t playback_last_menu_interaction_leveltime = 0;
static menuitem_t PlaybackMenu[] =
{
	{IT_CALL   | IT_STRING, "M_PHIDE",  "Hide Menu (Esc)", M_SelectableClearMenus,             0},

	{IT_CALL   | IT_STRING, "M_PREW",   "Rewind ([)",        M_PlaybackRewind,                20},
	{IT_CALL   | IT_STRING, "M_PPAUSE", "Pause (\\)",         M_PlaybackPause,                36},
	{IT_CALL   | IT_STRING, "M_PFFWD",  "Fast-Forward (])",  M_PlaybackFastForward,           52},
	{IT_CALL   | IT_STRING, "M_PSTEPB", "Backup Frame ([)",  M_PlaybackRewind,                20},
	{IT_CALL   | IT_STRING, "M_PRESUM", "Resume",        M_PlaybackPause,                     36},
	{IT_CALL   | IT_STRING, "M_PFADV",  "Advance Frame (])", M_PlaybackAdvance,               52},

	{IT_ARROWS | IT_STRING, "M_PVIEWS", "View Count (- and =)",  M_PlaybackSetViews,          72},
	{IT_ARROWS | IT_STRING, "M_PNVIEW", "Viewpoint (1)",   M_PlaybackAdjustView,              88},
	{IT_ARROWS | IT_STRING, "M_PNVIEW", "Viewpoint 2 (2)", M_PlaybackAdjustView,             104},
	{IT_ARROWS | IT_STRING, "M_PNVIEW", "Viewpoint 3 (3)", M_PlaybackAdjustView,             120},
	{IT_ARROWS | IT_STRING, "M_PNVIEW", "Viewpoint 4 (4)", M_PlaybackAdjustView,             136},

	{IT_CALL   | IT_STRING, "M_PVIEWS", "Toggle Free Camera (')",	M_PlaybackToggleFreecam, 156},
	{IT_CALL   | IT_STRING, "M_PEXIT",  "Stop Playback",   M_PlaybackQuit,                   172},
};
typedef enum
{
	playback_hide,
	playback_rewind,
	playback_pause,
	playback_fastforward,
	playback_backframe,
	playback_resume,
	playback_advanceframe,
	playback_viewcount,
	playback_view1,
	playback_view2,
	playback_view3,
	playback_view4,
	playback_freecamera,
	//playback_moreoptions,
	playback_quit
} playback_e;

// ---------------------------------
// Pause Menu Mode Attacking Edition
// ---------------------------------
static menuitem_t MAPauseMenu[] =
{
	{IT_CALL | IT_STRING,    NULL, "Continue",             M_SelectableClearMenus,48},
	{IT_CALL | IT_STRING,    NULL, "Retry",                M_ModeAttackRetry,     56},
	{IT_CALL | IT_STRING,    NULL, "Abort",                M_ModeAttackEndGame,   64},
};

typedef enum
{
	mapause_continue,
	mapause_retry,
	mapause_abort
} mapause_e;

// ---------------------
// Pause Menu MP Edition
// ---------------------
static menuitem_t MPauseMenu[] =
{
	{IT_STRING | IT_CALL,     NULL, "Addons...",          M_Addons,               8},
	{IT_STRING | IT_CALL,     NULL, "Add local skins...", M_LocalSkins,           16},
	{IT_STRING | IT_SUBMENU,  NULL, "Scramble Teams...", &MISC_ScrambleTeamDef,  24},
	{IT_STRING | IT_CALL,     NULL, "Switch Map..."    , M_MapChange,            32},

#ifdef HAVE_DISCORDRPC
	{IT_STRING | IT_SUBMENU,  NULL, "Ask To Join Requests...", &MISC_DiscordRequestsDef, 32},
#endif

	{IT_CALL | IT_STRING,    NULL, "Continue",           M_SelectableClearMenus, 40},
	{IT_CALL | IT_STRING,    NULL, "P1 Setup...",        M_SetupMultiPlayer,     48}, // splitscreen
	{IT_CALL | IT_STRING,    NULL, "P2 Setup...",        M_SetupMultiPlayer2,    56}, // splitscreen
	{IT_CALL | IT_STRING,    NULL, "P3 Setup...",        M_SetupMultiPlayer3,    64}, // splitscreen
	{IT_CALL | IT_STRING,    NULL, "P4 Setup...",        M_SetupMultiPlayer4,    72}, // splitscreen

	{IT_STRING | IT_CALL,    NULL, "Spectate",           M_ConfirmSpectate,      48}, // alone
	{IT_STRING | IT_CALL,    NULL, "Enter Game",         M_ConfirmEnterGame,     48}, // alone
	{IT_STRING | IT_CALL,    NULL, "Cancel Join",        M_ConfirmSpectate,      48}, // alone
	{IT_STRING | IT_SUBMENU, NULL, "Switch Team...",     &MISC_ChangeTeamDef,    48},
	{IT_STRING | IT_SUBMENU, NULL, "Enter/Spectate...",  &MISC_ChangeSpectateDef,48},
	{IT_CALL | IT_STRING,    NULL, "Player Setup...",    M_SetupMultiPlayer,     56}, // alone
	{IT_CALL | IT_STRING,    NULL, "Local Skin...",    	 M_LocalSkinMenu,     	 64}, // alone
	{IT_CALL | IT_STRING,    NULL, "Options",            M_Options,              72},

	{IT_CALL | IT_STRING,    NULL, "Return to Title",    M_EndGame,              88},
	{IT_CALL | IT_STRING,    NULL, "Quit Game",          M_QuitSRB2,             96},
};

typedef enum
{
	mpause_addons = 0,
	mpause_addlocalskins,
	mpause_scramble,
	mpause_switchmap,
#ifdef HAVE_DISCORDRPC
	mpause_discordrequests,
#endif
	mpause_continue,
	mpause_psetupsplit,
	mpause_psetupsplit2,
	mpause_psetupsplit3,
	mpause_psetupsplit4,

	mpause_spectate,
	mpause_entergame,
	mpause_canceljoin,
	mpause_switchteam,
	mpause_switchspectate,
	mpause_psetup,
	mpause_localskin,
	mpause_options,

	mpause_title,
	mpause_quit
} mpause_e;

// ---------------------
// Pause Menu SP Edition
// ---------------------
static menuitem_t SPauseMenu[] =
{
	{IT_CALL | IT_STRING,    NULL, "Continue",             M_SelectableClearMenus,48},
	{IT_CALL | IT_STRING,    NULL, "Retry",                M_Retry,               56},
	{IT_CALL | IT_STRING,    NULL, "Options",              M_Options,             64},

	{IT_CALL | IT_STRING,    NULL, "Return to Title",      M_EndGame,             80},
	{IT_CALL | IT_STRING,    NULL, "Quit Game",            M_QuitSRB2,            88},
};

typedef enum
{
	spause_continue = 0,
	spause_retry,
	spause_options,
	spause_title,
	spause_quit
} spause_e;

#ifdef HAVE_DISCORDRPC
static menuitem_t MISC_DiscordRequestsMenu[] =
{
	{IT_KEYHANDLER|IT_NOTHING, NULL, "", M_HandleDiscordRequests, 0},
};
#endif

// -----------------
// Misc menu options
// -----------------
// Prefix: MISC_
static menuitem_t MISC_ScrambleTeamMenu[] =
{
	{IT_STRING|IT_CVAR,      NULL, "Scramble Method", &cv_dummyscramble,     30},
	{IT_WHITESTRING|IT_CALL, NULL, "Confirm",         M_ConfirmTeamScramble, 90},
};

static menuitem_t MISC_ChangeTeamMenu[] =
{
	{IT_STRING|IT_CVAR,              NULL, "Player",            &cv_dummymenuplayer,    30},
	{IT_STRING|IT_CVAR,              NULL, "Team",              &cv_dummyteam,          40},
	{IT_WHITESTRING|IT_CALL,         NULL, "Confirm",           M_ConfirmTeamChange,    90},
};

static menuitem_t MISC_ChangeSpectateMenu[] =
{
	{IT_STRING|IT_CVAR,              NULL, "Player",        &cv_dummymenuplayer,        30},
	{IT_STRING|IT_CVAR,              NULL, "Status",        &cv_dummyspectate,          40},
	{IT_WHITESTRING|IT_CALL,         NULL, "Confirm",       M_ConfirmSpectateChange,    90},
};

static menuitem_t MISC_ChangeLevelMenu[] =
{
	{IT_STRING|IT_CVAR,              NULL, "Game Type",             &cv_newgametype,    68},
	{IT_STRING|IT_CVAR,              NULL, "Level",                 &cv_nextmap,        78},
	{IT_WHITESTRING|IT_CALL,         NULL, "Change Level",          M_ChangeLevel,     130},
};

static menuitem_t MISC_HelpMenu[] =
{
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL00", M_HandleImageDef, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL01", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL02", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL03", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL04", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL05", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL06", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL07", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL08", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL09", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL10", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL11", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL12", M_HandleImageDef, 1},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "MANUAL99", M_HandleImageDef, 0},
};

// --------------------------------
// Sky Room and all of its submenus
// --------------------------------
// Prefix: SR_


// Sky Room Custom Unlocks
static menuitem_t SR_MainMenu[] =
{
	{IT_STRING|IT_SUBMENU,                  NULL, "Unlockables", &SR_UnlockChecklistDef, 100},
	{IT_CALL|IT_STRING|IT_CALL_NOTMODIFIED, NULL, "Statistics",  M_Statistics,           108},
	{IT_CALL|IT_STRING,                     NULL, "Replay Hut",  M_ReplayHut,            116},
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom1
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom2
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom3
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom4
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom5
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom6
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom7
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom8
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom9
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom10
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom11
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom12
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom13
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom14
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom15
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom16
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom17
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom18
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom19
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom20
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom21
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom22
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom23
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom24
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom25
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom26
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom27
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom28
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom29
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom30
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom31
	{IT_DISABLED,         NULL, "",   NULL,                 0}, // Custom32

};

static menuitem_t SR_UnlockChecklistMenu[] =
{
	{IT_SUBMENU | IT_STRING,         NULL, "NEXT", &MainDef, 192},
};

static menuitem_t SR_MusicTestMenu[] =
{
	{IT_KEYHANDLER | IT_STRING, NULL, "", M_HandleMusicTest, 0},
};


// --------------------------------
// 1 Player and all of its submenus
// --------------------------------
// Prefix: SP_

// Single Player Main
static menuitem_t SP_MainMenu[] =
{
	{IT_SECRET,                                 NULL, "Record Attack", M_TimeAttack,     100},
	{IT_CALL | IT_STRING | IT_CALL_NOTMODIFIED, NULL, "Statistics",    M_Statistics,     108},
};

enum
{
	sprecordattack,
	spstatistics
};

// Single Player Time Attack
static menuitem_t SP_TimeAttackMenu[] =
{
	{IT_STRING|IT_CVAR|IT_CV_STRING, NULL, "Name",       &cv_playername,        0},
	{IT_STRING|IT_CVAR,              NULL, "Character",  &cv_chooseskin,       13},
	{IT_STRING|IT_CVAR,              NULL, "Color",      &cv_playercolor,      26},
	{IT_STRING|IT_CVAR,              NULL, "Level",      &cv_nextmap,          78},

	{IT_DISABLED,                                NULL, "Guest...",      &SP_GuestReplayDef,    98},
	{IT_DISABLED,                                NULL, "Replay...",     &SP_ReplayDef,        108},
	{IT_WHITESTRING|IT_SUBMENU,                  NULL, "Ghosts...",     &SP_GhostDef,         118},
	{IT_WHITESTRING|IT_CALL|IT_CALL_NOTMODIFIED, NULL, "Start",         M_ChooseTimeAttack,   130},
};

enum
{
	taname,
	taplayer,
	tacolor,
	talevel,

	taguest,
	tareplay,
	taghost,
	tastart
};

static menuitem_t SP_ReplayMenu[] =
{
	{IT_WHITESTRING|IT_CALL, NULL, "Replay Best Time",  M_ReplayTimeAttack,  90},
	{IT_WHITESTRING|IT_CALL, NULL, "Replay Best Lap",   M_ReplayTimeAttack,  98},

	{IT_WHITESTRING|IT_CALL, NULL, "Replay Last",       M_ReplayTimeAttack, 106},
	{IT_WHITESTRING|IT_CALL, NULL, "Replay Guest",      M_ReplayTimeAttack, 114},
	{IT_WHITESTRING|IT_KEYHANDLER, NULL, "Replay Staff",M_HandleStaffReplay,122},

	{IT_WHITESTRING|IT_SUBMENU, NULL, "Back",           &SP_TimeAttackDef,  130}
};

static menuitem_t SP_GuestReplayMenu[] =
{
	{IT_WHITESTRING|IT_CALL, NULL, "Save Best Time as Guest",  M_SetGuestReplay, 94},
	{IT_WHITESTRING|IT_CALL, NULL, "Save Best Lap as Guest",   M_SetGuestReplay,102},
	{IT_WHITESTRING|IT_CALL, NULL, "Save Last as Guest",       M_SetGuestReplay,110},

	{IT_WHITESTRING|IT_CALL, NULL, "Delete Guest Replay",      M_SetGuestReplay,120},

	{IT_WHITESTRING|IT_SUBMENU, NULL, "Back",                &SP_TimeAttackDef, 130}
};

static menuitem_t SP_GhostMenu[] =
{
	{IT_STRING|IT_CVAR,         NULL, "Best Time",   &cv_ghost_besttime, 88},
	{IT_STRING|IT_CVAR,         NULL, "Best Lap",    &cv_ghost_bestlap,  96},
	{IT_STRING|IT_CVAR,         NULL, "Last",        &cv_ghost_last,    104},
	{IT_DISABLED,               NULL, "Guest",       &cv_ghost_guest,   112},
	{IT_DISABLED,               NULL, "Staff Attack",&cv_ghost_staff,   120},

	{IT_WHITESTRING|IT_SUBMENU, NULL, "Back",        &SP_TimeAttackDef, 130}
};

enum
{
	nalevel,
	narecords,

	naguest,
	nareplay,
	naghost,
	nastart
};

// Statistics
static menuitem_t SP_LevelStatsMenu[] =
{
	{IT_KEYHANDLER | IT_NOTHING, NULL, "", M_HandleLevelStats, '\0'},     // dummy menuitem for the control func
};

// -----------------------------------
// Multiplayer and all of its submenus
// -----------------------------------
// Prefix: MP_

static menuitem_t MP_MainMenu[] =
{
	{IT_HEADER, NULL, "Players", NULL, 0},
	{IT_STRING|IT_CVAR,      NULL, "Number of local players",     &cv_splitplayers,            10},

	{IT_STRING|IT_KEYHANDLER,NULL, "Player setup...",     M_SetupMultiHandler,                 18},

	{IT_HEADER, NULL, "Host a game", NULL, 100-24},
	{IT_STRING|IT_CALL,       NULL, "Internet/LAN...",           M_PreStartServerMenu,     110-24},
	{IT_STRING|IT_CALL,       NULL, "Offline...",                M_StartOfflineServerMenu, 118-24},

	{IT_HEADER, NULL, "Join a game", NULL, 132-24},
#ifndef MASTERSERVER
	{IT_GRAYEDOUT,       NULL, "Internet server browser...",NULL,                          142-24},
#else
	{IT_STRING|IT_CALL,       NULL, "Internet server browser...",M_PreConnectMenu,         142-24},
#endif
	{IT_STRING|IT_CALL, NULL, "Join last server",     M_ConnectLastServer,                 150-24},
	{IT_STRING|IT_KEYHANDLER, NULL, "Specify IPv4 address:",     M_HandleConnectIP,        158-24},
};

static menuitem_t MP_ServerMenu[] =
{
	{IT_STRING|IT_CVAR,                NULL, "Max. Player Count",     &cv_maxplayers,        10},
#ifndef MASTERSERVER
	{IT_GRAYEDOUT,                NULL, "Advertise",             NULL,         20},
#else
	{IT_STRING|IT_CVAR,                NULL, "Advertise",             &cv_advertise,         20},
#endif
	{IT_STRING|IT_CVAR|IT_CV_STRING,   NULL, "Server Name",           &cv_servername,        30},

	{IT_STRING|IT_CVAR,                NULL, "Game Type",             &cv_newgametype,       68},
	{IT_STRING|IT_CVAR,                NULL, "Level",                 &cv_nextmap,           78},

	{IT_WHITESTRING|IT_CALL,           NULL, "Start",                 M_StartServer,        130},
};

// Separated offline and normal servers.
static menuitem_t MP_OfflineServerMenu[] =
{
	{IT_STRING|IT_CVAR,      NULL, "Game Type",             &cv_newgametype,       68},
	{IT_STRING|IT_CVAR,      NULL, "Level",                 &cv_nextmap,           78},

	{IT_WHITESTRING|IT_CALL, NULL, "Start",                 M_StartServer,        130},
};
//Char select
static menuitem_t MP_PlayerSetupMenu[] =
{
	{IT_KEYHANDLER | IT_STRING,   NULL, "Name",      M_HandleSetupMultiPlayer,   0},
	{IT_KEYHANDLER | IT_STRING,   NULL, "Character", M_HandleSetupMultiPlayer,  16}, // Tails 01-18-2001
	{IT_KEYHANDLER | IT_STRING,   NULL, "Color",     M_HandleSetupMultiPlayer, 152},
};

static menuitem_t MP_ConnectMenu[] =
{
	{IT_STRING | IT_CVAR,       NULL, "Sort By",  &cv_serversort,        0},
	{IT_STRING | IT_KEYHANDLER, NULL, "Page",     M_HandleServerPage,    8},
	{IT_STRING | IT_CALL,       NULL, "Refresh",  M_Refresh,            16},
	{IT_STRING | IT_KEYHANDLER, NULL, "",         M_HandleServerSearch, 25},

	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,          48},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,          60},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,          72},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,          84},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,          96},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,         108},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,         120},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,         132},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,         144},
	{IT_STRING | IT_SPACE, NULL, "",              M_Connect,         156},
};

enum
{
	mp_connect_sort,
	mp_connect_page,
	mp_connect_refresh,
	mp_connect_search,
	FIRSTSERVERLINE
};

// ------------------------------------
// Options and most (?) of its submenus
// ------------------------------------
// Prefix: OP_
static menuitem_t OP_MainMenu[] =
{
	{IT_SUBMENU|IT_STRING,		NULL, "Control Setup...",		&OP_ControlsDef,			  0},

	{IT_SUBMENU|IT_STRING,		NULL, "Video Options...",		&OP_VideoOptionsDef,		 13},
	{IT_SUBMENU|IT_STRING,		NULL, "Sound Options...",		&OP_SoundOptionsDef,		 23},
	{IT_SUBMENU|IT_STRING,		NULL, "Game Focus Options...",	&OP_FocusOptionsDef,		 33},

	{IT_SUBMENU|IT_STRING,		NULL, "HUD Options...",			&OP_HUDOptionsDef,			 46},
	{IT_CALL|IT_STRING,		    NULL, "Camera Options...",		M_CameraMenu,			     56},
	{IT_SUBMENU|IT_STRING,		NULL, "Gameplay Options...",	&OP_GameOptionsDef,			 66},
	{IT_SUBMENU|IT_STRING,		NULL, "Server Options...",		&OP_ServerOptionsDef,		 76},

	{IT_SUBMENU|IT_STRING,		NULL, "Data Options...",		&OP_DataOptionsDef,			 89},
	{IT_CALL|IT_STRING, 		NULL, "Custom Addon Options...", M_CustomCvarMenu,   		 99},

	{IT_CALL|IT_STRING,			NULL, "Tricks & Secrets (F1)",	M_Manual,					109},
	{IT_CALL|IT_STRING,			NULL, "Play Credits",			M_Credits,					119},

	{IT_SUBMENU|IT_STRING,		NULL, "Accessibility Options...", &OP_AccessibilityDef,		132},
	{IT_SUBMENU|IT_STRING,		NULL, "Saturn Options...",		&OP_SaturnDef,				142},

	{IT_SUBMENU|IT_STRING,		NULL, "Bird...",				&OP_BirdDef,				152},
	{IT_CALL|IT_STRING,			NULL, "Local Skin Options...",	M_LocalSkinMenu,			162},
};

enum
{
	ctrlsetup,
	vidopt,
	soundopt,
	focusopt,
	hudopt,
	camopt,
	gameopt,
	serveropt,
	dataopt,
	addonopt,
	tricksandshit,
	credits,
	accesability,
	satopt,
	bird,
	localskin
};

static menuitem_t OP_ControlsMenu[] =
{
	{IT_CALL | IT_STRING, NULL, "Player 1 Controls...", M_Setup1PControlsMenu,  10},
	{IT_CALL | IT_STRING, NULL, "Player 2 Controls...", M_Setup2PControlsMenu,  20},

	{IT_CALL | IT_STRING, NULL, "Player 3 Controls...", &M_Setup3PControlsMenu, 30},
	{IT_CALL | IT_STRING, NULL, "Player 4 Controls...", &M_Setup4PControlsMenu, 40},

	{IT_SUBMENU | IT_STRING, NULL, "Mouse Options...",  &OP_MouseOptionsDef,    55},

	{IT_STRING | IT_CVAR, NULL, "Controls per key",     &cv_controlperkey,      75},
};

static const char* OP_ControlsTooltips[] =
{
	"Setup player 1 controls.",
	"Setup player 2 controls.",
	"Setup player 3 controls.",
	"Setup player 4 controls.",
	"Options for mouse control.",
	"Allowed amount of controls per key.",
};

static menuitem_t OP_AllControlsMenu[] =
{
	{IT_SUBMENU|IT_STRING, NULL, "Gamepad Options...", &OP_Joystick1Def, 0},
	{IT_CALL|IT_STRING, NULL, "Reset to defaults", M_ResetControls, 8},

	{IT_HEADER, NULL, "Control Options", NULL, 0},
	{IT_SPACE, NULL, NULL, NULL, 0},

	// Cvars are set in M_SetupControlsMenu
	{IT_STRING|IT_CVAR, NULL, "Lite Steer", &cv_litesteer[0], 18},
	{IT_STRING|IT_CVAR, NULL, "Digital turn easing",  &cv_turnsmooth[0],         26},

	//{IT_SPACE, NULL, NULL, NULL, 0},
	{IT_HEADER, NULL, "Gameplay Controls", NULL, 0},
	{IT_SPACE, NULL, NULL, NULL, 0},
	{IT_CONTROL, NULL, "Accelerate",            M_ChangeControl, gc_accelerate },
	{IT_CONTROL, NULL, "Turn Left",             M_ChangeControl, gc_turnleft   },
	{IT_CONTROL, NULL, "Turn Right",            M_ChangeControl, gc_turnright  },
	{IT_CONTROL, NULL, "Drift",                 M_ChangeControl, gc_drift      },
	{IT_CONTROL, NULL, "Brake",                 M_ChangeControl, gc_brake      },
	{IT_CONTROL, NULL, "Use/Throw Item",        M_ChangeControl, gc_fire       },
	{IT_CONTROL, NULL, "Aim Forward",           M_ChangeControl, gc_aimforward },
	{IT_CONTROL, NULL, "Aim Backward",          M_ChangeControl, gc_aimbackward},
	{IT_CONTROL, NULL, "Look Backward",         M_ChangeControl, gc_lookback   },

	{IT_HEADER, NULL, "Miscellaneous Controls", NULL, 0},
	{IT_SPACE, NULL, NULL, NULL, 0},
	{IT_CONTROL, NULL, "Chat",                  M_ChangeControl, gc_talkkey    },
	{IT_CONTROL, NULL, "Show Rankings",         M_ChangeControl, gc_scores     },
	{IT_CONTROL, NULL, "Pause",                 M_ChangeControl, gc_pause      },
	{IT_CONTROL, NULL, "Screenshot",            M_ChangeControl, gc_screenshot },
	{IT_CONTROL, NULL, "Toggle GIF Recording",  M_ChangeControl, gc_recordgif  },
	{IT_CONTROL, NULL, "Open/Close Menu (ESC)", M_ChangeControl, gc_systemmenu },
	{IT_CONTROL, NULL, "Developer Console",     M_ChangeControl, gc_console    },

	{IT_HEADER, NULL, "Camera Controls", NULL, 0},
	{IT_SPACE, NULL, NULL, NULL, 0},
	{IT_CONTROL, NULL, "Toggle Freecam",        M_ChangeControl, gc_freecam    },
	{IT_CONTROL, NULL, "Look Up",               M_ChangeControl, gc_lookup     },
	{IT_CONTROL, NULL, "Look Down",             M_ChangeControl, gc_lookdown   },
	{IT_CONTROL, NULL, "Center View",           M_ChangeControl, gc_centerview },
	{IT_CONTROL, NULL, "Float",                 M_ChangeControl, gc_camfloat   },
	{IT_CONTROL, NULL, "Sink",                  M_ChangeControl, gc_camsink    },
	{IT_CONTROL, NULL, "Change Viewpoint",      M_ChangeControl, gc_viewpoint  },
	{IT_CONTROL, NULL, "Reset Camera",          M_ChangeControl, gc_camreset   },
	{IT_CONTROL, NULL, "Strafe Left",           M_ChangeControl, gc_strafeleft },
	{IT_CONTROL, NULL, "Strafe Right",          M_ChangeControl, gc_straferight},

	{IT_HEADER, NULL, "Spectator Controls", NULL, 0},
	{IT_SPACE, NULL, NULL, NULL, 0},
	{IT_CONTROL, NULL, "Become Spectator",      M_ChangeControl, gc_spectate   },
	{IT_CONTROL, NULL, "Toggle Director",       M_ChangeControl, gc_director   },

	{IT_HEADER, NULL, "Custom Lua Actions", NULL, 0},
	{IT_SPACE, NULL, NULL, NULL, 0},
	{IT_CONTROL, NULL, "Custom Action 1",       M_ChangeControl, gc_custom1    },
	{IT_CONTROL, NULL, "Custom Action 2",       M_ChangeControl, gc_custom2    },
	{IT_CONTROL, NULL, "Custom Action 3",       M_ChangeControl, gc_custom3    },
};

// Has to be same length as OP_AllControlsMenu, but otherwise its mostly empty
static const char* OP_AllControlsTooltips[sizeof(OP_AllControlsMenu)/sizeof(OP_AllControlsMenu[0])] = {
	NULL,
	NULL,
	NULL,
	NULL,
	"Hold DOWN on d-pad/keyboard for shallow turns.",
	"Turn smoothing for non-analog turning.",
	// The rest is null, for now
};

#define OP_JOYMENU(pnum)                                                                         \
	{IT_HEADER, NULL, "Gameplay Controls", NULL, 7},                                             \
	{IT_STRING | IT_CVAR,  NULL, "Aim Forward/Back"           , &cv_aimaxis[pnum]        ,  15}, \
	{IT_STRING | IT_CVAR,  NULL, "Turn Left/Right"            , &cv_turnaxis[pnum]       ,  20}, \
	{IT_STRING | IT_CVAR,  NULL, "Accelerate"                 , &cv_moveaxis[pnum]       ,  25}, \
	{IT_STRING | IT_CVAR,  NULL, "Brake"                      , &cv_brakeaxis[pnum]      ,  30}, \
	{IT_STRING | IT_CVAR,  NULL, "Drift"                      , &cv_driftaxis[pnum]      ,  35}, \
	{IT_STRING | IT_CVAR,  NULL, "Use Item"                   , &cv_fireaxis[pnum]       ,  40}, \
	{IT_STRING | IT_CVAR,  NULL, "Look Backward"              , &cv_lookbackaxis[pnum]   ,  45}, \
	{IT_STRING | IT_CVAR,  NULL, "Custom Button 1"            , &cv_custom1axis[pnum]    ,  50}, \
	{IT_STRING | IT_CVAR,  NULL, "Custom Button 2"            , &cv_custom2axis[pnum]    ,  55}, \
	{IT_STRING | IT_CVAR,  NULL, "Custom Button 3"            , &cv_custom3axis[pnum]    ,  60}, \
	{IT_HEADER, NULL, "Camera Controls", NULL, 67},                                              \
	{IT_STRING | IT_CVAR,  NULL, "Look Up/Down"               , &cv_lookaxis[pnum]       ,  75}, \
	{IT_STRING | IT_CVAR,  NULL, "Turn Left/Right"            , &cv_camturnaxis[pnum]    ,  80}, \
	{IT_STRING | IT_CVAR,  NULL, "Strafe Left/Right"          , &cv_camstrafeaxis[pnum]  ,  85}, \
	{IT_HEADER, NULL, "Deadzones", NULL, 92},                                                    \
	{IT_STRING | IT_CVAR,  NULL, "X deadzone"                 , &cv_xdeadzone[pnum]      , 100}, \
	{IT_STRING | IT_CVAR,  NULL, "Y deadzone"                 , &cv_ydeadzone[pnum]      , 105}, \
	{IT_HEADER, NULL, "Miscellaneous", NULL, 112},                                               \
	{IT_STRING | IT_CVAR,  NULL, "Controller Rumble"          , &cv_rumble[pnum]         , 120}, \
	{IT_STRING | IT_CVAR,  NULL, "Controller Rumble Strength" , &cv_rumblestrength[pnum] , 125}, \
	{IT_STRING | IT_CVAR,  NULL, "Set LED to Player color"    , &cv_gamepadled[pnum]     , 130},

static menuitem_t OP_Joystick1Menu[] =
{
	{IT_STRING | IT_CALL,  NULL, "Select Gamepad...", M_Setup1PJoystickMenu, 0},
	OP_JOYMENU(0)
};

static menuitem_t OP_Joystick2Menu[] =
{
	{IT_STRING | IT_CALL,  NULL, "Select Gamepad...", M_Setup2PJoystickMenu, 0},
	OP_JOYMENU(1)
};

static menuitem_t OP_Joystick3Menu[] =
{
	{IT_STRING | IT_CALL,  NULL, "Select Gamepad...", M_Setup3PJoystickMenu, 0},
	OP_JOYMENU(2)
};

static menuitem_t OP_Joystick4Menu[] =
{
	{IT_STRING | IT_CALL,  NULL, "Select Gamepad...", M_Setup4PJoystickMenu, 0},
	OP_JOYMENU(3)
};

static menuitem_t OP_JoystickSetMenu[] =
{
	{IT_CALL | IT_NOTHING, "None", NULL, M_AssignJoystick,  LINEHEIGHT+5},
	{IT_CALL | IT_NOTHING, "",     NULL, M_AssignJoystick, (LINEHEIGHT*2)+5},
	{IT_CALL | IT_NOTHING, "",     NULL, M_AssignJoystick, (LINEHEIGHT*3)+5},
	{IT_CALL | IT_NOTHING, "",     NULL, M_AssignJoystick, (LINEHEIGHT*4)+5},
	{IT_CALL | IT_NOTHING, "",     NULL, M_AssignJoystick, (LINEHEIGHT*5)+5},
	{IT_CALL | IT_NOTHING, "",     NULL, M_AssignJoystick, (LINEHEIGHT*6)+5},
	{IT_CALL | IT_NOTHING, "",     NULL, M_AssignJoystick, (LINEHEIGHT*7)+5},
	{IT_CALL | IT_NOTHING, "",     NULL, M_AssignJoystick, (LINEHEIGHT*8)+5},
};

//WTF
static menuitem_t OP_MouseOptionsMenu[] =
{
	{IT_STRING | IT_CVAR,                NULL, "Use Mouse",      &cv_usemouse,     10},
	{IT_STRING | IT_CVAR,                NULL, "Mouse Turning",  &cv_mouseturn,    20},
	{IT_STRING | IT_CVAR,                NULL, "Invert Mouse",   &cv_invertmouse,  30},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Mouse X Speed",  &cv_mousexsens,   40},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Mouse Y Speed",  &cv_mouseysens,   50},
};

static const char* OP_MouseTooltips[] =
{
	"Enable the use of the mouse.",
	"Turn using the mouse.",
	"Invert mouse movements.",
	"Mouse horizontal sensitivity.",
	"Mouse vertical sensitivity.",
};

static menuitem_t OP_VideoOptionsMenu[] =
{
	{IT_STRING | IT_CALL,	NULL,	"Set Resolution...",		  M_VideoModeMenu,		  10},
#if defined (__unix__) || defined (UNIXCOMMON) || defined (HAVE_SDL)
	{IT_STRING|IT_CVAR,		NULL,	"Fullscreen",				  &cv_fullscreen,		  20},
#endif
#ifdef BACKWARDSCOMPATCORRECTION
	{IT_STRING | IT_CVAR | IT_CV_SLIDER,
							NULL,	"Gamma",				  	  &cv_globalgamma,		  30},
#endif

	{IT_STRING | IT_CVAR | IT_CV_SLIDER,
							NULL,	"Brightness",				  &cv_globalbrightness,	  40},

	{IT_STRING | IT_CVAR | IT_CV_SLIDER,
	                        NULL, 	"Saturation",      			  &cv_globalsaturation ,  50},

	{IT_SUBMENU|IT_STRING, NULL, 	"Advanced Color Settings...", &OP_ColorOptionsDef,    60},

	{IT_STRING | IT_CVAR,	NULL,	"Draw Distance",			  &cv_drawdist,			  75},
	{IT_STRING | IT_CVAR,	NULL,	"Weather Draw Distance",	  &cv_drawdist_precip,	  85},

	{IT_STRING | IT_CVAR,	NULL,	"Show FPS",					  &cv_ticrate,			 105},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Sync",			  &cv_vidwait,			 115},
	{IT_STRING | IT_CVAR,   NULL,   "FPS Cap",              	  &cv_fpscap,            125},
	{IT_STRING | IT_CVAR,   NULL,   "Drift spark pulse size",	  &cv_driftsparkpulse,   135},
	{IT_STRING | IT_CVAR, 	NULL, 	"VHS effect", 				  &cv_vhseffect, 		 145},
#ifdef HWRENDER
	{IT_SUBMENU|IT_STRING,	NULL,	"OpenGL Options...",		  &OP_OpenGLOptionsDef,	 155},
#endif
	{IT_SUBMENU|IT_STRING,  NULL,   "Advanced Video Options...",  &OP_ExpOptionsDef,     165},
};

static const char* OP_VideoTooltips[] =
{
	"Resolution game runs at.",
#if defined (__unix__) || defined (UNIXCOMMON) || defined (HAVE_SDL)
	"Enable fullscreen.",
#endif
	"Increase the gamma of the displayed image.",
	"Increase or decrease the brightness of the displayed image.",
	"Reduce the saturation of the displayed image.",
	"Advanced color settings of the game.",
	"How far objects can be drawn.",
	"Affects how far weather visuals can be drawn.",
	"Show current game framerate and select the style.",
	"Sync game framerate to refresh rate of monitor.",
	"Set manual framerate cap.",
	"Size of drift spark pulse.",
	"Show a VHS-like effect when the game is paused\n or youre rewinding replays.",
#ifdef HWRENDER
	"Options for OpenGL renderer.",
#endif
	"Advanced graphical options.",
};

enum
{
	op_video_res = 0,
#if defined (__unix__) || defined (UNIXCOMMON) || defined (HAVE_SDL)
	op_video_fullscreen,
#endif
#ifdef BACKWARDSCOMPATCORRECTION
	op_video_gamma,
#endif
	op_video_bright,
	op_video_sat,
	op_video_color,
	op_video_dd,
	op_video_wdd,
	op_video_fps,
	op_video_vsync,
	op_video_fpscap,
	op_video_driftsparkpulse,
	op_exp_vhs,
#ifdef HWRENDER
	op_video_ogl,
#endif
	op_video_exp,
};

static menuitem_t OP_VideoModeMenu[] =
{
	{IT_KEYHANDLER | IT_NOTHING, NULL, "", M_HandleVideoMode, '\0'},     // dummy menuitem for the control func
};

static menuitem_t OP_ColorOptionsMenu[] =
{
	{IT_STRING | IT_CALL, NULL, "Reset all", M_ResetCvars, 0},

	{IT_HEADER, NULL, "Red", NULL, 9},
	{IT_DISABLED, NULL, NULL, NULL, 35},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Hue",          &cv_rhue,         15},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Saturation",   &cv_rsaturation,  20},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Brightness",   &cv_rbrightness,       25},

	{IT_HEADER, NULL, "Yellow", NULL, 34},
	{IT_DISABLED, NULL, NULL, NULL, 73},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Hue",          &cv_yhue,         40},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Saturation",   &cv_ysaturation,  45},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Brightness",   &cv_ybrightness,       50},

	{IT_HEADER, NULL, "Green", NULL, 59},
	{IT_DISABLED, NULL, NULL, NULL, 112},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Hue",          &cv_ghue,         65},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Saturation",   &cv_gsaturation,  70},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Brightness",   &cv_gbrightness,       75},

	{IT_HEADER, NULL, "Cyan", NULL, 84},
	{IT_DISABLED, NULL, NULL, NULL, 255},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Hue",          &cv_chue,         90},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Saturation",   &cv_csaturation,  95},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Brightness",   &cv_cbrightness,      100},

	{IT_HEADER, NULL, "Blue", NULL, 109},
	{IT_DISABLED, NULL, NULL, NULL, 152},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Hue",          &cv_bhue,        115},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Saturation",   &cv_bsaturation, 120},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Brightness",   &cv_bbrightness,      125},

	{IT_HEADER, NULL, "Magenta", NULL, 134},
	{IT_DISABLED, NULL, NULL, NULL, 181},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Hue",          &cv_mhue,        140},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Saturation",   &cv_msaturation, 145},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Brightness",   &cv_mbrightness,      150},
};

static menuitem_t OP_ExpOptionsMenu[] =
{
	{IT_HEADER, NULL, "Advanced Video Options", NULL, 0},
	{IT_STRING|IT_CVAR,		NULL, "Interpolation Distance",			&cv_maxinterpdist,		 	 10},

	{IT_STRING | IT_CVAR, 	NULL, "Less Weather Effects", 			&cv_lessprecip, 		 	 20},

	{IT_STRING | IT_CVAR,   NULL, "Minimum Sector Brightness",		&cv_secbright,	  		 	 30},

	//{IT_STRING | IT_CVAR,  NULL, "Randomized Directional Light",	&cv_randomdirlight,	  		 45}, // should this ever come back

	{IT_STRING | IT_CVAR,	NULL, "Skyboxes",						&cv_skybox,				 	 40},
	{IT_STRING | IT_CVAR,	NULL, "Skydome",						&cv_skydome,				 45},

	{IT_STRING | IT_CVAR,	NULL, "Precache Level Textures",		&cv_precachetextures,		 55},

	{IT_STRING | IT_CVAR,	NULL, "FPS counter sampling",			&cv_accuratefps,			 65},

	{IT_STRING | IT_CVAR,	NULL, "Frameskip",						&cv_frameskip,			 	 75},

	{IT_STRING | IT_CVAR,	NULL, "Votescreen Scaling",				&cv_votebgscaling,			 85},

#ifdef HWRENDER
	{IT_STRING | IT_CVAR, 	NULL, "Screen Textures", 				&cv_glscreentextures, 		 95},
#ifdef USE_FBO_OGL
	{IT_STRING | IT_CVAR, 	NULL, "FBO Downsampling support", 		&cv_glframebuffer, 			100},
	{IT_STRING | IT_CVAR, 	NULL, "Palette Depth", 					&cv_glpalettedepth, 		110},
	{IT_DISABLED, 			NULL, "", 								NULL,     			 		120}, // dummy text
#else
	{IT_STRING | IT_CVAR, 	NULL, "Palette Depth", 					&cv_glpalettedepth, 		110},
	{IT_DISABLED, 			NULL, "", 								NULL,     			 		120}, // dummy text
#endif
#endif
};

static const char* OP_ExpTooltips[] =
{
	NULL,
	"How far Mobj interpolation should take effect.",
	"When weather is on this will cut the object amount used in half.",
	"Sets minimum sector brightness, useful for dark areas.",
	//"Should the directional lightning be randomized each map?\nTakes effect on next map load.",
	"Toggle being able to see the sky.",
	"Toggle skydome.", // idk man someone give a better description of this
	"Preload all level textures on level load.\nMassively reduces texture related stuttering during gameplay\nat the cost of longer level loading times.\nDisable this if you experience timeouts during level switches.",
	"Change the FPS counter sampling method\nInaccurate updates slower\nand might miss sudden framerate changes and drops,\nproviding a more averaged result.\nAccurate updates faster, but might be less readable.", // how to ingles??
	"Skips rendering frames if game logic takes too long\npreventing gameplay issues during performance drops.", // idk im shit as describing things
	"Different methods of scaling the votescreen backgrounds.",
#ifdef HWRENDER
	"Disabling Screen Textures may result in a performance boost\nbut will break certain effects.\nScreen textures are required at resolutions lower than your desktop resolution!"
#ifdef USE_FBO_OGL
	"Allows the game to downsample from a higher resolution\nthan your display in OpenGL renderer mode.\nRequires a GPU with atleast OpenGL 2.1 support.",
#endif
	"Change the bit depth of the Lookup Palette in Palette rendering mode\n 16 bits is like software looks ingame\nwhile 24 bits is how software looks in screenshots.",
#endif
};

enum
{
	op_exp_header,
	op_exp_interpdist,
	op_exp_lessprecip,
	op_exp_secbright,
	//op_exp_dirlight,
	op_exp_skybox,
	op_exp_skydome,
	op_exp_texcache,
	op_exp_accuratefps,
	op_exp_frameskip,
	op_exp_votescrn,
#ifdef HWRENDER
	op_exp_glscrtx,
#ifdef USE_FBO_OGL
	op_exp_fbo,
#endif
	op_exp_paldepth,
#endif
};


#ifdef HWRENDER
static menuitem_t OP_OpenGLOptionsMenu[] =
{
	{IT_STRING | IT_CVAR,	NULL, "3D Models",					&cv_glmdls,					15},
	{IT_STRING | IT_CVAR,	NULL, "Fallback Player 3D Model",	&cv_glfallbackplayermodel,	20},
	{IT_STRING | IT_CVAR,	NULL, "Shaders",					&cv_glshaders,				25},
	{IT_STRING | IT_CVAR,	NULL, "Palette Rendering",			&cv_glpaletterendering,		30},

	{IT_STRING | IT_CVAR,	NULL, "Texture Quality",			&cv_gltexturedepth,			40},
	{IT_STRING | IT_CVAR,	NULL, "Texture Filter",				&cv_glfiltermode,			45},
	{IT_STRING | IT_CVAR,	NULL, "Anisotropic",				&cv_glanisotropicmode,		50},
	{IT_STRING | IT_CVAR,	NULL, "Visual Portals",		  		&cv_glportals,				55},

	{IT_STRING | IT_CVAR,	NULL, "Wall Contrast Style",		&cv_glfakecontrast,			65},
	{IT_STRING | IT_CVAR,	NULL, "Slope Contrast",				&cv_glslopecontrast,		70},
	{IT_STRING | IT_CVAR, 	NULL, "Dithered Lightning", 		&cv_gllightdither,			75},
	{IT_STRING | IT_CVAR,	NULL, "Sprite Billboarding",		&cv_glspritebillboarding,	80},
	{IT_STRING | IT_CVAR,	NULL, "Software Perspective",		&cv_glshearing,				85},
	{IT_STRING | IT_CVAR,	NULL, "Rendering Distance",			&cv_glrenderdistance,		90},
};

static const char* OP_OpenGLTooltips[] =
{
	"Use 3D models.",
	"Fallback 3D model for characters that don't have any.",
	"Graphical Shaders.",
	"Recreates the look of software mode.",
	"Bit-depth of textures.",
	"Filter to use on textures.",
	"Anisotropic filtering.",
	"Recreates an effect from software mode that is used on some maps.",
	"The look of the wall contrast effect.",
	"Wall contrast but for slopes.",
	"Should shader lightning be dithered?",
	"Should sprites always face the camera?",
	"Recreates the look of software mode camera perspective.",
	"How far the game world should be drawn.",
};

enum
{
	op_gl_mdls,
	op_gl_falbckmdls,
	op_gl_shader,
	op_gl_palrender,
	op_gl_scrdepth,
	op_gl_filter,
	op_gl_anis,
	op_gl_portal,
	op_gl_wallcont,
	op_gl_slopecont,
	op_gl_lightdither,
	op_gl_bill,
	op_gl_shearing,
	op_gl_renderdist,
};

#endif

static menuitem_t OP_SoundOptionsMenu[] =
{
	{IT_STRING|IT_CVAR|IT_CV_NOPRINT,			NULL, "SFX",							&cv_gamesounds,			 	10},
	{IT_STRING|IT_CVAR|IT_CV_SLIDER,			NULL, "SFX Volume",						&cv_soundvolume,		 	18},

	{IT_STRING|IT_CVAR|IT_CV_NOPRINT,			NULL, "Music",							&cv_gamedigimusic,		 	30},
	{IT_STRING|IT_CVAR|IT_CV_SLIDER,			NULL, "Music Volume",					&cv_digmusicvolume,		 	38},

//#ifndef NO_MIDI
#if 0
	{IT_STRING|IT_CVAR|IT_CV_SLIDER, 			NULL, "MIDI Volume",					&cv_midimusicvolume,	 	46},

	{IT_STRING|IT_CVAR,							NULL, "Reverse L/R Channels",			&stereoreverse,			 	60},

	{IT_STRING|IT_CVAR,							NULL, "Chat Notifications",				&cv_chatnotifications,	 	75},
	{IT_STRING|IT_CVAR,							NULL, "Character voices",				&cv_kartvoices,			 	85},
	{IT_STRING|IT_CVAR,							NULL, "Hit Em Delay",				    &cv_karthitemdialog,		95},
	{IT_STRING|IT_CVAR,							NULL, "Powerup Warning",				&cv_kartinvinsfx,		 	105},

	{IT_KEYHANDLER|IT_STRING,					NULL, "Sound Test",						M_HandleSoundTest,			115},
	{IT_STRING|IT_CALL,							NULL, "Music Test",						M_MusicTest,				125},

	{IT_STRING|IT_SUBMENU, 						NULL, "Advanced Settings...", 			&OP_SoundAdvancedDef, 		135}
#else
	{IT_STRING|IT_CVAR,							NULL, "Reverse L/R Channels",			&stereoreverse,			 	60},

	{IT_STRING|IT_CVAR,							NULL, "Chat Notifications",				&cv_chatnotifications,	 	75},
	{IT_STRING|IT_CVAR,							NULL, "Character voices",				&cv_kartvoices,			 	85},
	{IT_STRING|IT_CVAR,							NULL, "Hit Em Delay",				    &cv_karthitemdialog,		95},
	{IT_STRING|IT_CVAR,							NULL, "Powerup Warning",				&cv_kartinvinsfx,		 	105},

	{IT_KEYHANDLER|IT_STRING,					NULL, "Sound Test",						M_HandleSoundTest,			115},
	{IT_STRING|IT_CALL,							NULL, "Music Test",						M_MusicTest,				125},

	{IT_STRING|IT_SUBMENU, 						NULL, "Advanced Settings...", 			&OP_SoundAdvancedDef, 		135}
#endif
};

static const char* OP_SoundTooltips[] =
{
	"Turn Sound effects on or off.",
	"Volume of Sound effects.",
	"Turn Music on or off.",
	"Volume of Music.",
//#ifndef NO_MIDI
#if 0
	"Volume of Midi Music.",
#endif
	"Reverse left and right channels of audio.",
	"Chat notification sound.",
	"Frequency of character voice lines.",
	"Play 'Hit Em' character line after other player's hurt line.",
	"Should the powerup warning be a sound effect or music?",
	"Testing sounds...",
	"Testing music...",
	"Options for advanced sound settings.",
};


static menuitem_t OP_SoundAdvancedMenu[] =
{
#ifdef HAVE_OPENMPT
	{IT_HEADER, NULL, "Tracker Module Options", NULL, 0},

	{IT_STRING | IT_CVAR, 	NULL, "Instrument Filter", 			&cv_modfilter, 		 10},
	{IT_STRING | IT_CVAR,	NULL, "Amiga Resampler", 			&cv_amigafilter, 	 20},
#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
	{IT_STRING | IT_CVAR, 	NULL, "Amiga Type", 				&cv_amigatype, 		 25},
#endif
	{IT_STRING | IT_CVAR, 	NULL, "Stereo Seperation", 			&cv_stereosep, 		 35},
#endif

	{IT_HEADER, 			NULL, "Misc", 						NULL, 				 45},

	{IT_STRING | IT_CVAR, 	NULL, "Same Sound Limit", 			&cv_samesoundlimit,  55},

	{IT_STRING | IT_CVAR, 	NULL, "Grow Music", 				&cv_growmusic, 		 65},
	{IT_STRING | IT_CVAR, 	NULL, "Invulnerability Music", 		&cv_supermusic, 	 70},

	{IT_STRING | IT_CVAR, 	NULL, "Keep Map Music", 			&cv_keepmusic, 		 80},
	{IT_STRING | IT_CVAR, 	NULL, "Skip Intro Music", 			&cv_skipintromusic,  85},

	{IT_STRING | IT_CVAR, 	NULL, "Cache Sound", 				&cv_cachesound,   90},

	{IT_STRING | IT_CVAR, 	NULL, "Audio Buffer Size", 			&cv_audbuffersize,   95},
	{IT_DISABLED, 			NULL, "", 							NULL,     			105},	// dummy text
};

static const char* OP_SoundAdvancedTooltips[] =
{
#ifdef HAVE_OPENMPT
	NULL,

	"Filter used to resample tracker instruments.",
	"Resample tracker modules to sound similar to Paula hardware.",
#if OPENMPT_API_VERSION_MAJOR < 1 && OPENMPT_API_VERSION_MINOR > 4
	"Which Amiga should be used for .mod playback?",
#endif
	"How far should the four channels in .mod be panned?",
#endif
	NULL,
	"How many times is the same sound allowed to play at once?\nIf 0 theres no limit.",
	"Should the Grow music be on or off?",
	"Should the Invulnerability music be on or off?",
	"Should music be kept when restarting the map?",
	"Should the Intro fanfare be skipped\nand map music be played on map start?",
	"Controls Sound effects caching.\nKeep will retain previously played sound effects in cache." // man idk how to describe this in a non awful way lmao
	"\nOn will preload all sound effects.\nEliminates sound-related lag and stutters but increases memory usage.",
	"Size of the Audio Buffer\nreducing it will result in less sound latency\nbut may cause issues such as crackling or distorted Sound.",
	NULL,
};

static menuitem_t OP_FocusOptionsMenu[] =
{
	{IT_HEADER, NULL, "Game Focus Options", NULL, 0},

	{IT_STRING|IT_CVAR,	NULL, "Play Music While Unfocused",					&cv_playmusicifunfocused, 	 10},
	{IT_STRING|IT_CVAR,	NULL, "Play SFX While Unfocused",					&cv_playsoundifunfocused, 	 20},

	{IT_STRING|IT_CVAR,	NULL, "Pause Game While Unfocused",					&cv_pauseifunfocused,		 40},

	{IT_STRING|IT_CVAR,	NULL, "Background FPS Cap",         				&cv_fpscapbg,          		 60},

	{IT_STRING|IT_CVAR,	NULL, "Show \"FOCUS LOST\"",						&cv_showfocuslost,		     80},

	{IT_STRING|IT_CVAR,	NULL, "Visible Mouse",								&cv_mousevisible,	        100},

	{IT_STRING|IT_CVAR,	NULL, "Gamepad Inputs While Unfocused",				&cv_gamepadifunfocused, 	120},
	{IT_STRING|IT_CVAR,	NULL, "Gamepad LED Color While Unfocused ",			&cv_gamepadledifunfocused, 	130},
	{IT_STRING|IT_CVAR,	NULL, "Gamepad Rumble While Unfocused",				&cv_rumbleifunfocused, 	    140},
};

static const char* OP_FocusOptionsTooltips[] =
{
	NULL,
	"Should music play while the game is unfocused?",
	"Should soundeffects play while the game is unfocused?",
	"Should the game pause while the game is unfocused?",
	"Set manual framerate cap while the game is unfocused.",
	"Should the FOCUS LOST window appear\n while the game is unfocused?",
	"Displays the mouse cursor while the game is in focus.",
	"Should gamepad still control the game when unfocused?",
	"Should gamepad LED indicator change color when unfocused?",
	"Should gamepad rumble apply when unfocused?",
};

static menuitem_t OP_DataOptionsMenu[] =
{
	{IT_STRING | IT_CALL,		NULL, "Screenshot Options...",	M_ScreenshotOptions,	 10},
	{IT_STRING | IT_CALL,		NULL, "Addon Options...",		M_AddonsOptions,		 20},
	{IT_STRING | IT_SUBMENU,	NULL, "Replay Options...",		&MISC_ReplayOptionsDef,	 30},
	{IT_STRING | IT_SUBMENU,	NULL, "Protocol options...",	&OP_ProtocolDef,		 40},
#ifdef HAVE_DISCORDRPC
	{IT_STRING | IT_SUBMENU,	NULL, "Discord Options...",		&OP_DiscordOptionsDef,	 50},

	{IT_STRING | IT_SUBMENU,	NULL, "Erase Data...",			&OP_EraseDataDef,		 70},
#else
	{IT_STRING | IT_SUBMENU,	NULL, "Erase Data...",			&OP_EraseDataDef,		 60},
#endif
};

static menuitem_t OP_ScreenshotOptionsMenu[] =
{
	{IT_HEADER, NULL, "Screenshots (F8)", NULL, 5},
	{IT_STRING|IT_CVAR, NULL, "Storage Location",  &cv_screenshot_option,          10},
	{IT_STRING|IT_CVAR|IT_CV_STRING, NULL, "Custom Folder", &cv_screenshot_folder, 15},
	{IT_STRING|IT_CVAR, NULL, "Memory Level",      &cv_zlib_memory,                30},
	{IT_STRING|IT_CVAR, NULL, "Compression Level", &cv_zlib_level,                 35},
	{IT_STRING|IT_CVAR, NULL, "Strategy",          &cv_zlib_strategy,              40},
	{IT_STRING|IT_CVAR, NULL, "Window Size",       &cv_zlib_window_bits,           45},

	{IT_HEADER, NULL, "Movie Mode (F9)", NULL, 55},
	{IT_STRING|IT_CVAR, NULL, "Storage Location",  &cv_movie_option,              60},
	{IT_STRING|IT_CVAR|IT_CV_STRING, NULL, "Custom Folder", &cv_movie_folder, 	  65},
	{IT_STRING|IT_CVAR, NULL, "Capture Mode",      &cv_moviemode,                 80},

	{IT_STRING|IT_CVAR, NULL, "Region Optimizing", &cv_gif_optimize,              90},
	{IT_STRING|IT_CVAR, NULL, "Downscaling",       &cv_gif_downscale,             95},
};

enum
{
	op_screenshot_folder = 2,
	op_movie_folder = 9,
	op_screenshot_capture = 10,
	op_screenshot_gif_start = 11,
	op_screenshot_gif_end = 12,
};

static menuitem_t OP_EraseDataMenu[] =
{
	{IT_STRING | IT_CALL, NULL, "Erase Record Data", M_EraseData, 10},
	{IT_STRING | IT_CALL, NULL, "Erase Unlockable Data", M_EraseData, 20},

	{IT_STRING | IT_CALL, NULL, "\x85" "Erase ALL Data", M_EraseData, 40},
};

static menuitem_t OP_AddonsOptionsMenu[] =
{
	{IT_HEADER,                      NULL, "Menu",                        NULL,                    0},
	{IT_STRING|IT_CVAR,              NULL, "Location",                    &cv_addons_option,      10},
	{IT_STRING|IT_CVAR|IT_CV_STRING, NULL, "Custom Folder",               &cv_addons_folder,      20},
	{IT_STRING|IT_CVAR,              NULL, "Identify addons via",         &cv_addons_md5,         48},
	{IT_STRING|IT_CVAR,              NULL, "Show unsupported file types", &cv_addons_showall,     58},

	{IT_HEADER,                      NULL, "Search",                      NULL,                   76},
	{IT_STRING|IT_CVAR,              NULL, "Matching",                    &cv_addons_search_type, 86},
	{IT_STRING|IT_CVAR,              NULL, "Case-sensitive",              &cv_addons_search_case, 96},
};

enum
{
	op_addons_folder = 2,
};

static menuitem_t OP_ProtocolMenu[] =
{
	{IT_STRING | IT_CALL, NULL, "Register protocol", D_CreateProtocol, 10},
	{IT_STRING | IT_CALL, NULL, "\x85" "Disable and delete protocols", M_DeleteProtocol, 20},
};

#ifdef HAVE_DISCORDRPC
static menuitem_t OP_DiscordOptionsMenu[] =
{
	{IT_STRING | IT_CVAR,		NULL, "Rich Presence",			&cv_discordrp,			 10},

	{IT_HEADER,					NULL, "Rich Presence Settings",	NULL,					 30},
	{IT_STRING | IT_CVAR,		NULL, "Streamer Mode",			&cv_discordstreamer,	 40},

	{IT_STRING | IT_CVAR,		NULL, "Allow Ask To Join",		&cv_discordasks,		 60},
	{IT_STRING | IT_CVAR,		NULL, "Allow Invites",			&cv_discordinvites,		 70},
};
#endif

static menuitem_t OP_HUDOptionsMenu[] =
{
	{IT_STRING | IT_CVAR, NULL,		"Show HUD (F3)",			&cv_showhud,			     10},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER,
	                      NULL,		"HUD Visibility",			&cv_translucenthud,		     20},

	{IT_STRING | IT_SUBMENU, NULL,	"Online HUD options...",	&OP_ChatOptionsDef,		     35},
	{IT_STRING | IT_CVAR, NULL,		"Background Glass",			&cons_backcolor,		     45},

	{IT_STRING | IT_CVAR | IT_CV_SLIDER,
						  NULL,		"Minimap Visibility",		&cv_kartminimap,		     60},
	{IT_STRING | IT_CVAR, NULL,		"Speedometer Display",		&cv_kartspeedometer,	     70},
	{IT_STRING | IT_CVAR, NULL,		"Show \"CHECK\"",			&cv_kartcheck,			     80},

	{IT_STRING | IT_CVAR, NULL,		"Menu Highlights",			&cons_menuhighlight,	     95},
	// highlight info - (GOOD HIGHLIGHT, WARNING HIGHLIGHT) - 110 (see M_DrawHUDOptions)

	{IT_STRING | IT_CVAR, NULL,		"Console Text Size",		  &cv_constextsize,		    120},
	{IT_STRING | IT_CVAR, NULL,		"Show Console Prints in Menu",&cons_consoleprintinmenu, 130},

	{IT_STRING | IT_CVAR, NULL,		"Show Track Addon Name",	&cv_showtrackaddon,   	    145},

	{IT_STRING | IT_CVAR, NULL,		"Show All Maps",			&cv_showallmaps,		    155},

	{IT_STRING | IT_CVAR, NULL,		"2D character select",		&cv_skinselectmenu,		    165},
};

static menuitem_t OP_CamOptionsMenu[] =
{
	{IT_HEADER, NULL, "Camera Options", NULL, 0},

	{IT_STRING | IT_CVAR | IT_CV_BIGFLOAT,	NULL,	"Field of View",&cv_fov,				  	 30},

	{IT_STRING | IT_CVAR,                   NULL,   "Flipcam Mode", &cv_flipcammode,             40},

	{IT_STRING | IT_SUBMENU,	NULL, "Player 1 Camera options...",	&OP_Player1CamOptionsDef,	 60},
	{IT_STRING | IT_SUBMENU,	NULL, "Player 2 Camera options...",	&OP_Player2CamOptionsDef,	 70},
	{IT_STRING | IT_SUBMENU,	NULL, "Player 3 Camera options...",	&OP_Player3CamOptionsDef,	 80},
	{IT_STRING | IT_SUBMENU,	NULL, "Player 4 Camera options...",	&OP_Player4CamOptionsDef,	 90},
};

static const char* OP_CamOptionsTooltips[] =
{
	NULL,
	"Player field of view.",
	"Change flipcam mode.\nDisplayplayer - use currently viewed player option.\nLocal - use local flipcam toggle option.",
	NULL,
	NULL,
	NULL,
	NULL,
};

#define OP_CAMMENU(pnum) \
	{IT_STRING | IT_CVAR, NULL,						"Flipcam",   				&cv_flipcam[pnum],			30}, \
	{IT_STRING | IT_CVAR | IT_CV_BIGFLOAT, NULL,	"Camera Distance",   		&cv_cam_dist[pnum],		    50}, \
	{IT_STRING | IT_CVAR | IT_CV_BIGFLOAT, NULL,	"Camera Height",   			&cv_cam_height[pnum],		60}, \
	{IT_STRING | IT_CVAR, NULL,						"Camera Slope Pitch",   	&cv_cam_pitch[pnum],		70}, \
	{IT_STRING | IT_CVAR, NULL,						"Camera Speed",   			&cv_cam_speed[pnum],		80}, \
	{IT_STRING | IT_CVAR, NULL, 					"Camera Lookback Momentum", &cv_cam_lookbackmom[pnum], 100}, \
	{IT_STRING | IT_CVAR, NULL, 					"Camera Vertical Look",     &cv_verticallook[pnum],    110}, \
	{IT_STRING | IT_CVAR, NULL,						"Freecam Speed",   			&cv_freecam_speed[pnum],   130}, \
	{IT_STRING | IT_CVAR, NULL,						"Third Person Camera",   	&cv_chasecam[pnum],	       145}, \

static menuitem_t OP_Player1CamOptionsMenu[] =
{
	{IT_HEADER, NULL, "Player 1 Camera Options", NULL, 0},
	OP_CAMMENU(0)
};

static menuitem_t OP_Player2CamOptionsMenu[] =
{
	{IT_HEADER, NULL, "Player 2 Camera Options", NULL, 0},
	OP_CAMMENU(1)
};

static menuitem_t OP_Player3CamOptionsMenu[] =
{
	{IT_HEADER, NULL, "Player 3 Camera Options", NULL, 0},
	OP_CAMMENU(2)
};

static menuitem_t OP_Player4CamOptionsMenu[] =
{
	{IT_HEADER, NULL, "Player 4 Camera Options", NULL, 0},
	OP_CAMMENU(3)
};

static const char* OP_PlayerCamOptionsTooltips[] =
{
	NULL,
	"Should the Camera flip on gravity flipped sections?",
	"Camera distance relative to the Player.",
	"Height of the Camera.",
	"Pitch Camera on Upwards or Downhill Slopes.",
	"Speed of the Camera.",
	"Should looking back inherit the Players Momentum?\nEither inherit Player Momentum or double of it\nmay make looking back while boosting or going in high speed less jarring.",
	"Allows looking up/down by holding\naim forward/backward while standing still.",
	"Speed of the Freecam/Spectator Camera.",
	"Toggle between Third or First Person camera.",
};

// Ok it's still called chatoptions but we'll put ping display in here to be clean
static menuitem_t OP_ChatOptionsMenu[] =
{
	// will ANYONE who doesn't know how to use the console want to touch this one?
	{IT_STRING | IT_CVAR, NULL, "Chat Mode",				&cv_consolechat,		5}, // nonetheless...

	{IT_STRING | IT_CVAR | IT_CV_SLIDER,
	                      NULL, "Chat Box Width",			&cv_chatwidth,			20},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER,
	                      NULL, "Chat Box Height",			&cv_chatheight,			30},
	{IT_STRING | IT_CVAR, NULL, "Center Text in Chat",		&cv_chatcentertext,		40},

	{IT_STRING | IT_CVAR, NULL, "Chat Background Tint",		&cv_chatbacktint,		55},
	{IT_STRING | IT_CVAR, NULL, "Message Fadeout Time",		&cv_chattime,			65},
	{IT_STRING | IT_CVAR, NULL, "Spam Protection",			&cv_chatspamprotection,	75},
	{IT_STRING | IT_CVAR, NULL, "Max Chat Messages",		&cv_chatlogsize,		85},

	{IT_STRING | IT_CVAR, NULL, "Local ping display",		&cv_showping,			105},	// shows ping above the framerate if we want to.
	{IT_STRING | IT_CVAR, NULL, "Ping display style",		&cv_pingstyle,			115},
	{IT_STRING | IT_CVAR, NULL, "Ping measurement",			&cv_pingmeasurement,	125},
	{IT_STRING | IT_CVAR, NULL, "Ping icon",				&cv_pingicon,			135},

	{IT_STRING | IT_CVAR, NULL, "Show IP address in playerlist",		&cv_shownodeip,	145},
};

static const char* OP_ChatOptionsTooltips[] =
{
	"Chat mode used for in-game chat.",
	"Width of chat box.",
	"Height of chat box.",
	"Center text in the chat message pop ups.",
	"Chatbox background.",
	"Fadeout time for new chat message.",
	"Spam protection for in-game chat.",
	"Maxiumum amount of chat messages to look back at.",
	"Show player ping.",
	"Choose the looks of the ping display.", // this is ass idk english lmao
	"Measurement used for ping.",
	"Visibility of ping icon.",
	"Should Player IP addresses be printed when using\nthe nodes or listplayers command?",
};

static menuitem_t OP_GameOptionsMenu[] =
{
	{IT_STRING | IT_SUBMENU, NULL, "Random Item Toggles...",	&OP_MonitorToggleDef,	 10},

	{IT_STRING | IT_CVAR, NULL, "Game Speed",					&cv_kartspeed,			 30},
	{IT_STRING | IT_CVAR, NULL, "Frantic Items",				&cv_kartfrantic,		 40},
	{IT_SECRET,           NULL, "Encore Mode",					&cv_kartencore,			 50},

	{IT_STRING | IT_CVAR, NULL, "Number of Laps",				&cv_basenumlaps,		 70},
	{IT_STRING | IT_CVAR, NULL, "Exit Countdown Timer",			&cv_countdowntime,		 80},

	{IT_STRING | IT_CVAR, NULL, "Time Limit",					&cv_timelimit,			100},
	{IT_STRING | IT_CVAR, NULL, "Starting Bumpers",				&cv_kartbumpers,		110},
	{IT_STRING | IT_CVAR, NULL, "Karma Comeback",				&cv_kartcomeback,		120},

	{IT_STRING | IT_CVAR, NULL, "Force Character",				&cv_forceskin,          140},
	{IT_STRING | IT_CVAR, NULL, "Restrict Character Changes",	&cv_restrictskinchange, 150},
};

enum
{
	op_game_ittggl,
	op_game_spd,
	op_game_frantic,
	op_game_encore,
	op_game_numlaps,
	op_game_countdown,
	op_game_timelimit,
	op_game_startbump,
	op_game_karma,
	op_game_forcechar,
	op_game_restrictchar,
};

static const char* OP_GameTooltips[] =
{
	"Toggles for all in-game items.",
	"Driving speed of game in race mode.",
	"Crazier item rolls.",
	"Mirror mode.",
	"Number of laps.",
	"Countdown for other players when people finish.",
	"Time limit for battle mode.",
	"Number of bumpers to start with in battle mode.",
	"Allow players to come back via karma in battle mode.",
	"Force everyone to use the same character.",
	"Prevent character changes.",
};

static menuitem_t OP_ServerOptionsMenu[] =
{
	{IT_STRING | IT_CVAR | IT_CV_STRING,
	                         NULL, "Server Name",					&cv_servername,			 10},
	{IT_STRING | IT_CVAR,    NULL, "Intermission Timer",			&cv_inttime,			 40},
	{IT_STRING | IT_CVAR,    NULL, "Map Progression",				&cv_advancemap,			 50},
	{IT_STRING | IT_CVAR,    NULL, "Voting Timer",					&cv_votetime,			 60},
	{IT_STRING | IT_CVAR,    NULL, "Voting Rule Changes",			&cv_kartvoterulechanges, 70},

	{IT_STRING | IT_CVAR,    NULL, "Max. Player Count",				&cv_maxplayers,			 90},
	{IT_STRING | IT_CVAR,    NULL, "Allow Players to Join",			&cv_allownewplayer,		100},
	{IT_STRING | IT_CVAR,    NULL, "Allow Addon Downloading",		&cv_downloading,		110},
	{IT_STRING | IT_CVAR,    NULL, "Pause Permission",				&cv_pause,				120},
	{IT_STRING | IT_CVAR,    NULL, "Mute All Chat",					&cv_mute,				130},

	{IT_SUBMENU|IT_STRING,   NULL, "Advanced Options...",			&OP_AdvServerOptionsDef,150},
};

static const char* OP_ServerOptionsTooltips[] =
{
	"Name of server.",
	"Length of intermission after races.",
	"How the next map to be played is choosen.",
	"How long map voting is.",
	"How often should other gamemodes appear.",
	"Max amount of players allowed in this server.",
	"Allow players to join this server.",
	"Allow players to download addons.",
	"Who has permission to pause the server?",
	"Completely mute in game chat.",
	"Options for advanced server settings.",
};

static menuitem_t OP_AdvServerOptionsMenu[] =
{
#ifndef MASTERSERVER
	{IT_GRAYEDOUT, 			 NULL, "Server Browser Address",		NULL,		 			 10},
#else
	{IT_STRING | IT_CVAR | IT_CV_STRING,
							 NULL, "Server Browser Address",		&cv_masterserver,	 	 10},
#endif
	{IT_STRING | IT_CVAR,    NULL, "Attempts to resynchronise",		&cv_resynchattempts,	 25},
#ifdef SATURNPAK
	{IT_STRING | IT_CVAR,    NULL, "Resend Gamestate Cooldown",		&cv_resynchcooldown,	 35},
	{IT_STRING | IT_CVAR,    NULL, "Resend Gamestate Attempts",		&cv_gamestateattempts,	 40},

	{IT_STRING | IT_CVAR,    NULL, "Delay limit (frames)",			&cv_maxping,			 50},
	{IT_STRING | IT_CVAR,    NULL, "Delay timeout (s)",				&cv_pingtimeout,		 55},
	{IT_STRING | IT_CVAR,    NULL, "Connection timeout (tics)",		&cv_nettimeout,			 60},
	{IT_STRING | IT_CVAR,    NULL, "Join timeout (tics)",			&cv_jointimeout,		 65},

	{IT_STRING | IT_CVAR,    NULL, "Max. file transfer send (KB)",	&cv_maxsend,			 75},
	{IT_STRING | IT_CVAR,    NULL, "File transfer packet rate",		&cv_downloadspeed,		 80},

	{IT_STRING | IT_CVAR,    NULL, "Log join addresses",			&cv_showjoinaddress,	 90},
	{IT_STRING | IT_CVAR,    NULL, "Log resyncs",					&cv_blamecfail,			 95},
	{IT_STRING | IT_CVAR,    NULL, "Log file transfers",			&cv_noticedownload,		100},
#else
	{IT_STRING | IT_CVAR,    NULL, "Delay limit (frames)",			&cv_maxping,			 35},
	{IT_STRING | IT_CVAR,    NULL, "Delay timeout (s)",				&cv_pingtimeout,		 40},
	{IT_STRING | IT_CVAR,    NULL, "Connection timeout (tics)",		&cv_nettimeout,			 45},
	{IT_STRING | IT_CVAR,    NULL, "Join timeout (tics)",			&cv_jointimeout,		 50},

	{IT_STRING | IT_CVAR,    NULL, "Max. file transfer send (KB)",	&cv_maxsend,			 60},
	{IT_STRING | IT_CVAR,    NULL, "File transfer packet rate",		&cv_downloadspeed,		 65},

	{IT_STRING | IT_CVAR,    NULL, "Log join addresses",			&cv_showjoinaddress,	 70},
	{IT_STRING | IT_CVAR,    NULL, "Log resyncs",					&cv_blamecfail,			 75},
	{IT_STRING | IT_CVAR,    NULL, "Log file transfers",			&cv_noticedownload,		 80},
#endif
};

static const char* OP_AdvServerOptionsTooltips[] =
{
	"Server used for master server.",
	"Attempts to resynchronise player to server.",
#ifdef SATURNPAK
	"Cooldown in seconds before the Server attempts\nto resend the gamestate to a Saturn Client\nbetween resending attempts.",
	"Attempts to resend the Gamestate to player\nThis increments a Counter everytime a gamestate resend occurs\nThis Counter decrements at double the time of the Cooldown\nonce the Counter reaches the threshold, the client gets kicked.",
#endif
	"Maximum allowed delay.",
	"Delay timeout in seconds.",
	"Connection timeout in tics.",
	"Join timeout in tics",
	"Max file size sent in kilobytes.",
	"Packet rate for file transfers.",
	"Log ip addresses of players who join.",
	"Log player resync attempts.",
	"Log player file transfers.",
};

#define ITEMTOGGLEBOTTOMRIGHT

static menuitem_t OP_MonitorToggleMenu[] =
{
	// Mostly handled by the drawing function.
	// Instead of using this for dumb monitors, lets use the new item bools we have :V
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Sneakers",				M_HandleMonitorToggles, KITEM_SNEAKER},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Sneakers x3",			M_HandleMonitorToggles, KRITEM_TRIPLESNEAKER},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Rocket Sneakers",		M_HandleMonitorToggles, KITEM_ROCKETSNEAKER},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Toggle All",			M_HandleMonitorToggles, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Bananas",				M_HandleMonitorToggles, KITEM_BANANA},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Bananas x3",			M_HandleMonitorToggles, KRITEM_TRIPLEBANANA},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Bananas x10",			M_HandleMonitorToggles, KRITEM_TENFOLDBANANA},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Eggman Monitors",		M_HandleMonitorToggles, KITEM_EGGMAN},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Orbinauts",				M_HandleMonitorToggles, KITEM_ORBINAUT},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Orbinauts x3",			M_HandleMonitorToggles, KRITEM_TRIPLEORBINAUT},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Orbinauts x4",			M_HandleMonitorToggles, KRITEM_QUADORBINAUT},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Mines",					M_HandleMonitorToggles, KITEM_MINE},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Jawz",					M_HandleMonitorToggles, KITEM_JAWZ},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Jawz x2",				M_HandleMonitorToggles, KRITEM_DUALJAWZ},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Ballhogs",				M_HandleMonitorToggles, KITEM_BALLHOG},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Self-Propelled Bombs",	M_HandleMonitorToggles, KITEM_SPB},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Invincibility",			M_HandleMonitorToggles, KITEM_INVINCIBILITY},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Grow",					M_HandleMonitorToggles, KITEM_GROW},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Shrink",				M_HandleMonitorToggles, KITEM_SHRINK},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Thunder Shields",		M_HandleMonitorToggles, KITEM_THUNDERSHIELD},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Hyudoros",				M_HandleMonitorToggles, KITEM_HYUDORO},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Pogo Springs",		 	M_HandleMonitorToggles, KITEM_POGOSPRING},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Kitchen Sinks",			M_HandleMonitorToggles, KITEM_KITCHENSINK},
#ifdef ITEMTOGGLEBOTTOMRIGHT
	{IT_KEYHANDLER | IT_NOTHING, NULL, "---",					M_HandleMonitorToggles, 255},
#endif
};

static menuitem_t OP_AccessibilityMenu[] =
{
	{IT_HEADER, NULL, "Accessibility options", NULL, 0},

	{IT_HEADER, NULL, "Visual", NULL, 10},

	{IT_STRING|IT_CVAR|IT_CV_SLIDER,   NULL,   "Brightness",                     &cv_globalgamma,        20},
	{IT_STRING|IT_CVAR|IT_CV_SLIDER,   NULL,   "Saturation",                     &cv_globalsaturation,   25},
	{IT_SUBMENU|IT_STRING,             NULL,   "Video Color Settings...",        &OP_ColorOptionsDef,    30},

	{IT_STRING|IT_CVAR,                NULL,   "Reduce Effects",                 &cv_reducevfx,          35},
	{IT_STRING|IT_CVAR,                NULL,   "Less Midnight Channel Flicker",  &cv_lessflicker,        40}, // obsolete now? or still better be a seperate toggle?

	{IT_STRING|IT_CVAR,                NULL,   "Minimum Sector Brightness",      &cv_secbright,          45},

	{IT_STRING|IT_CVAR,                NULL,   "Water Surface Ripples",          &cv_ripplewater,        50},

	{IT_STRING|IT_CVAR,                NULL,   "Fade Players near Camera",       &cv_playerfade,         55},

	{IT_STRING|IT_CVAR,                NULL,   "Quake Screenshakes",             &cv_screenquake,        60},

	{IT_CALL|IT_STRING,                NULL,   "Camera Options...",              M_CameraMenu,           65},

	{IT_HEADER, NULL, "Audio", NULL, 75},

	{IT_STRING|IT_CVAR,                NULL,   "Reverse L/R Channels",           &stereoreverse,         85},
	{IT_STRING|IT_CVAR,                NULL,   "Same Sound Limit",               &cv_samesoundlimit,     90},

	{IT_HEADER, NULL, "Controls", NULL, 100},

	{IT_STRING|IT_CVAR,                NULL,   "Automatic Acceleration (P1)",    &cv_autoaccel[0],       110},
	{IT_STRING|IT_CVAR,                NULL,   "Automatic Acceleration (P2)",    &cv_autoaccel[1],       115},
	{IT_STRING|IT_CVAR,                NULL,   "Automatic Acceleration (P3)",    &cv_autoaccel[2],       120},
	{IT_STRING|IT_CVAR,                NULL,   "Automatic Acceleration (P4)",    &cv_autoaccel[3],       125},
};

static const char* OP_AccessibilityTooltips[] =
{
	NULL,
	NULL,
	"Gamma (brightness) of the game.",
	"Saturation of the game.",
	"Advanced color settings of the game.",
	"Reduces or disables certain effects players might be sensitive to\nSuch as flashing, flickering, certain screen effects and more.",
	"Disables the flicker effect on Midnight Channel.",
	"Sets minimum sector brightness, useful for dark areas",
	"Toggles the ripple effect on water surfaces",
	"Fades other Players that are close to the camera in and out",
	"Toggles the screen shake effect during Earthquakes",
	"Camera Options",
	NULL,
	"Reverse left and right channels of audio.",
	"Change how often the same Sound is allowed to play at once.",
	NULL,
	"Automatically hold acceleration button (Player 1).",
	"Automatically hold acceleration button (Player 2).",
	"Automatically hold acceleration button (Player 3).",
	"Automatically hold acceleration button (Player 4).",
};

static menuitem_t OP_SaturnMenu[] =
{
	{IT_HEADER, NULL, "Saturn Options", NULL, 0},

	{IT_STRING | IT_CVAR, NULL, "Serverqueue waittime", 				&cv_connectawaittime, 	 	 10},

	{IT_STRING | IT_CVAR, NULL, "Minimum Input Delay", 					&cv_mindelay, 	 	 		 20},
	{IT_STRING | IT_CVAR, NULL, "Gentlemens Ping", 						&cv_gentlemens, 	 	 	 25},
	{IT_STRING | IT_CVAR, NULL, "Server Info Screen", 				    &cv_serverinfoscreen, 	 	 30},
	{IT_STRING | IT_CVAR, NULL, "Server Addons Info", 				    &cv_serveraddoninfo, 	 	 35},

	{IT_STRING | IT_CVAR, NULL, "Skin Select Spinning Speed",		 	&cv_skinselectspin, 	 	 45},

	{IT_STRING | IT_CVAR, NULL, "Colorized Speedlines", 				&cv_coloredspeedlines, 		 55},
	{IT_STRING | IT_CVAR, NULL, "Colorized Sneakertrails", 				&cv_coloredsneakertrail, 	 60},

	{IT_STRING | IT_CVAR, NULL, "Player Blendeffects", 					&cv_playerblendeffects, 	 70},

	{IT_STRING | IT_CVAR, NULL, "Bananadrag Jitter", 					&cv_bananajitter, 	 		 80},

	{IT_STRING | IT_CVAR, NULL, "Midair Driftsparks", 					&cv_airsparks, 	 		 	 90},

	{IT_STRING | IT_CVAR, NULL, "Show Localskin Menus", 				&cv_showlocalskinmenus, 	100},

	{IT_STRING | IT_CVAR, NULL, "Uppercase Menu",						&cv_menucaps,   		    110},

	{IT_STRING | IT_CVAR, NULL, "Keyboard Layout",						&cv_keyboardlayout,   	   	120},

	{IT_STRING | IT_CVAR, NULL, "Less Midnight Channel Flicker", 		&cv_lessflicker, 		   	130},

	{IT_SUBMENU|IT_STRING,	NULL,	"Saturn Hud...", 					&OP_SaturnHudDef,		   	140},
	{IT_SUBMENU|IT_STRING,	NULL,	"Sprite Distortion...", 			&OP_PlayerDistortDef,	   	145},
	{IT_SUBMENU|IT_STRING,	NULL,	"Saturn Credits", 					&OP_SaturnCreditsDef,	   	150}, // uwu
};

static const char* OP_SaturnTooltips[] =
{
	NULL,
	"How long can the game wait before it kicks you out from the server\nconnecting screen.",
	"Practice for online play! 0 = instant response.",
	"Simulate online input delay when hosting a server.\nValue choosen by Player with the lowest ping",
	"Show a screen before joining a server displaying important information about it",
	"Show amount and types of addons that are loaded by server when joining",
	"How much speen do you want?",
	"Colourize the speedlines in your skincolor if you go fast enough!",
	"Colourize the sneaker flame trails in your skincolor!",
	"Enables blending effects for certain player effects when boosting.",
	"Makes bananas and other items jump and jitter\nwhen dragged behind.",
	"Keep your driftsparks going while in air.",
	"Show Localskin Menus.",
	"Force menu to only use uppercase.",
	"Use your desired Keyboard Layout for Text Input\nthis is either the Default, Native or Azerty\nNative does not affect Gameplay only Text!",
	"Disables the flicker effect on Midnight Channel.",
	"Options for Saturn specific HUD things.",
	"Options for sprite distortion effects.",
	"See the people who helped make this project possible!",
};

enum
{
	sm_header,
	sm_waittime,
	sm_mindelay,
	sm_gentlemens,
	sm_skinselspeed,
	sm_colorlines,
	sm_colorflames,
	sm_blendeffects,
	sm_bananjumpy,
	sm_airsparks,
	sm_showlocalskin,
	op_uppercase_menu,
	sm_nativkey,
	sm_pisschannel,
	sm_hud,
	sm_distortionmenu,
	sm_credits,
};

static menuitem_t OP_PlayerDistortMenu[] =
{
	{IT_HEADER, NULL, "Sprite Distortion", NULL, 0},

	{IT_STRING | IT_CVAR, 				 NULL, "Sprite Slope Rotation",			 &cv_sloperoll,		 	10},
	{IT_STRING | IT_CVAR, 				 NULL, "Slope Rotation Distance",		 &cv_sloperolldist,  	15},

	{IT_STRING | IT_CVAR,				 NULL, "Rotate Players when Sliptiding", &cv_sliptideroll,	 	25},
	{IT_STRING | IT_CVAR,				 NULL, "Stair Janking Effect",           &cv_stairjank,	 	    30},
	{IT_STRING | IT_CVAR,				 NULL, "Rotate Sparks and Boost Trails", &cv_sparkroll,		 	35},
	{IT_STRING | IT_CVAR,				 NULL, "Rotate Bananas on Throw",		 &cv_bananthrowroll, 	40},

	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Player Stretch Factor",			 &cv_gravstretch,	 	50},
	{IT_STRING | IT_CVAR,				 NULL, "Squish Sound Effect",			 &cv_slamsound,		 	55},

	{IT_STRING | IT_CVAR,				 NULL, "Saltyhop",						 &cv_saltyhop,		 	65},
	{IT_STRING | IT_CVAR | IT_CV_SLIDER, NULL, "Saltyhop Height",				 &cv_saltyheight,	 	70},
	{IT_STRING | IT_CVAR,				 NULL, "Saltyhop Sound Effect",			 &cv_saltyhopsfx,		75},
	{IT_STRING | IT_CVAR,				 NULL, "Saltyhop Squish",				 &cv_saltysquish,	 	80},
	{IT_STRING | IT_CVAR,				 NULL, "Saltyhop Roll",				 	 &cv_saltyroll,	 	 	85},

	{IT_STRING | IT_CVAR,				 NULL, "Squishdance",				 	 &cv_squishdance,	 	95},
	{IT_STRING | IT_CVAR,				 NULL, "Squishdance Speed",				 &cv_squishdancespeed,	101}, // why is this offset by 6? idk cant remember lulul
};

static const char* OP_PlayerDistortTooltips[] =
{
	NULL,
	"Sprite rotation on slopes. Can either be just players or all objects.",
	"Distance object rotation should be visable.",
	"Player rotation when sliptiding.",
	"Slightly tilt the player when driving over stairs or small bumps.",
	"Rotation of a player's boost trails and drift sparks.",
	"Should banans rotate when thrown?\nAnd should they stay rotated when on the ground?",
	"Player squash and stretch.",
	"Player landing sound effect.",
	"Kart hopping while drifting. This is purely visual.",
	"Jump Height for Kart hopping.",
	"Player hop sound effect.",
	"Player hop squash and stretch.",
	"Should the player rotation be kept during player hop.",
	"Do a funny squishy dance when holding CUSTOM 3.",
	"Speed of the Squishy dance in SPM (Squishes Per Minute).",
};

enum
{
	spriteheader,
	sloperotate,
	slrotatedist,
	sliptide,
	stairjank,
	sparkrotate,
	bananrotat,
	stretchyplayer,
	squishsound,
	salthmmm,
	saltheight,
	saltsound,
	saltsquishy,
	saltroll,
	squishdance,
	squishdancespd,
};

static menuitem_t OP_SaturnHudMenu[] =
{
	{IT_HEADER, NULL, "Saturn Hud Options", NULL, 0},

	{IT_STRING|IT_CVAR,    NULL, "Speedometer Style",           &cv_newspeedometer,       10},
	{IT_STRING|IT_CVAR,    NULL, "Battle Speedometer",          &cv_battlespeedo,         15},

	{IT_STRING|IT_CVAR,    NULL, "Colourized HUD",              &cv_colorizedhud,         25},
	{IT_STRING|IT_CVAR,    NULL, "Colourized Itembox",          &cv_colorizeditembox,     30},
	{IT_STRING|IT_CVAR,    NULL, "Colourized HUD Color",        &cv_colorizedhudcolor,    35},

	{IT_STRING|IT_CVAR,    NULL, "Input Display",               &cv_showinput,            45},

	{IT_STRING|IT_CVAR,    NULL, "Stat Display",                &cv_showstats,            55},
	{IT_STRING|IT_CVAR,    NULL, "Skin Name in Stat Display",   &cv_showstats_skinname,   60},

	{IT_STRING|IT_CVAR,    NULL, "Higher Resolution Portraits", &cv_highresportrait,      70},

	{IT_STRING|IT_CVAR,    NULL, "Small Positionnumber",        &cv_smallposnum,          80},
	{IT_STRING|IT_CVAR,    NULL, "Positionnumber Animation",    &cv_posanim,              85},

	{IT_STRING|IT_CVAR,    NULL, "Flash Lap Times",             &cv_showlaptimes,         95},

	{IT_STRING|IT_CVAR,    NULL, "Multi-Item icons",            &cv_multiitemicon,       105},
	{IT_STRING|IT_CVAR,    NULL, "Item Amount Number",          &cv_huditemamount,       110},
	{IT_STRING|IT_CVAR,    NULL, "Animated Roulette",           &cv_fancyroulette,       115},

	{IT_STRING|IT_CVAR,    NULL, "Item Roulette Color",         &cv_roulettecolor,       120},

	{IT_STRING|IT_CVAR,    NULL, "Show Lap Emblem",             &cv_showlapemblem,       130},
	{IT_STRING|IT_CVAR,    NULL, "Show Cecho Messages",         &cv_cechotoggle,         135},

	{IT_STRING|IT_CVAR,    NULL, "Show Names on Minimap",       &cv_showminimapnames,    145},
	{IT_STRING|IT_CVAR,    NULL, "Show Finished on Minimap",    &cv_showminimapfinished, 150},
	{IT_STRING|IT_CVAR,    NULL, "Small Minimap Players",       &cv_minihead,            155},
	{IT_STRING|IT_CVAR,    NULL, "Spin Minimap Icons",          &cv_spinoutroll,         160},
	{IT_STRING|IT_CVAR,    NULL, "Player Angle Visual",         &cv_showminimapangle,    165},

	{IT_STRING|IT_CVAR,    NULL, "Music Credits",               &cv_songcredits,         175},
	{IT_STRING|IT_CVAR,    NULL, "Music Credits on Pause",      &cv_pausesongcredits,    180},

	{IT_STRING|IT_CVAR,    NULL, "Beta Intermissionscreen",     &cv_betainterscreen,     190},

	{IT_STRING|IT_CVAR,    NULL, "Show Director Prompt",        &cv_showdirectorhud,     200},

	{IT_STRING|IT_SUBMENU, NULL, "Nametags...",                 &OP_NametagDef,          210},
	{IT_STRING|IT_SUBMENU, NULL, "Driftgauge...",               &OP_DriftGaugeDef,       215},

	{IT_SUBMENU|IT_STRING, NULL, "Hud Offsets...",              &OP_HudOffsetDef,        225},
};

static const char* OP_SaturnHudTooltips[] =
{
	NULL,
	"Change what style the speedometer is.",
	"Draw the Speedometer in Battle.",
	"Enable colourized hud.",
	"Enable the colourized itembox when colourized hud is enabled.",
	"The color to use instead of the player color when\ncolourized hud is enabled.",
	"Displays the input display and lets you choose its style.",
	"Enable the stat display.",
	"Show skin name in the stat display.",
	"Enable the use of the higher resolution want icons instead of rank\nfor some places.",
	"Make the Postionnumber half the size.",
	"Disable the animation of the Positionnumber\nwhen overtaking someone.",
	"Flash current Lap Time when doing a Lap on the Timer.",
	"Use extra graphics for multiple sneakers, bananas and jawz.",
	"Change when the item amount is to be displayed.\nMultiple will always display the number.\nWhen you have multiple of the same Item.\nAlways will always display the number regardless of Item amount.",
	"Enables an animation while the roulette is active.",
	"Changes the way items are colorized in roulette.",
	"Show the big 'LAP' text on a lap change.",
	"Show the big Cecho Messages.",
	"Show player names on the minimap.",
	"Show players that have finished already on the minimap.",
	"Minimize the player icons on the minimap.",
	"Erratically rotate player icons during spinouts.",
	"Visualize the player facing angle.",
	"Show the Music Credits and which style.",
	"Show the Music Credits while in pause menu.",
	"Make the Intermission screen look like in beta versions of Kart!\nEither with background or just the rest.",
	"Show the Director Toggle prompt when spectating.",
	"Nametag Options.",
	"Driftgauge Options.",
	"Move position of HUD elements.",
};

enum
{
	sh_header,
	sh_speedometer,
	sh_battlespeedo,
	sh_colorhud,
	sh_coloritem,
	sh_colorhud_customcolor,
	sh_input,
	sh_statdisplay,
	sh_highresport,
	sh_smolpos,
	sh_posanim,
	sh_multicon,
	sh_itamnum,
	sh_laptime,
	sh_fancy,
	sh_lapemblem,
	sh_cechotogle,
	sh_mapname,
	sh_smallmap,
	sh_iconspinout,
	sh_minidot,
	sh_songcred,
	sh_betainter,
	sh_directorhud,
	sh_nametagmen,
	sh_driftgaugemen,
	sh_hudoffsets,
};

static menuitem_t OP_HudOffsetMenu[] =
{
	{IT_HEADER, NULL, "Kart Hud Offsets", NULL, 0},

	{IT_STRING | IT_CALL, NULL, "Reset all", M_ResetCvars, 5},

	{IT_HEADER, NULL, "Itembox", NULL, 15},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",			&cv_item_xoffset, 		20},
	{IT_STRING | IT_CVAR, 	NULL, 	"Vertical Offset",				&cv_item_yoffset,     	25},

	{IT_HEADER, NULL, "Timer", NULL, 35},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",			&cv_time_xoffset, 		40},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Offset",				&cv_time_yoffset,     	45},

	{IT_HEADER, NULL, "Lap Count", NULL, 55},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",     		&cv_laps_xoffset, 		60},
	{IT_STRING | IT_CVAR, 	NULL, 	"Vertical Offset",       		&cv_laps_yoffset,     	65},

	{IT_HEADER, NULL, "Speedometer", NULL, 75},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",  			&cv_speed_xoffset, 		80},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Offset",	  			&cv_speed_yoffset,     	85},

	{IT_HEADER, NULL, "Mini Rankings", NULL, 95},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",  			&cv_face_xoffset, 		100},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Offset",	  			&cv_face_yoffset,     	105},

	{IT_HEADER, NULL, "Minimap", NULL, 115},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",  			&cv_mini_xoffset, 		120},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Offset",	  	 		&cv_mini_yoffset,     	125},

	{IT_HEADER, NULL, "Position Number", NULL, 135},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",  	  		&cv_posi_xoffset, 		140},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Offset",	  	  		&cv_posi_yoffset,     	145},

	{IT_HEADER, NULL, "R.A. Wheel", NULL, 155},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",  	  		&cv_wheel_xoffset, 		160},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Offset",	  	  		&cv_wheel_yoffset,     	165},

	{IT_HEADER, NULL, "Stat Display", NULL, 175},
	{IT_STRING | IT_CVAR, 	NULL, 	"Horizontal Offset",  	  		&cv_stat_xoffset, 		180},
	{IT_STRING | IT_CVAR,	NULL,	"Vertical Offset",	  	  		&cv_stat_yoffset,     	185},
};

// FIXME: WE EFFECTIVELY HAVE NO MORE SPACE FOR GREEN RES
// i had to put multiple names into single lines at this point
// we ought to really do something better for this at this point
// absolutely crazy how many people put work into this at this point <3
static menuitem_t OP_SaturnCreditsMenu[] =
{
	// in no particular order besides me and indev <3
	{IT_HEADER, NULL, "Thanks to all contributers <3", 									NULL,       0},

	{IT_STRING2+IT_SPACE, NULL, 	"Alug    Indev",      								NULL, 	   10},
	{IT_STRING2+IT_SPACE, NULL, 	"Haya    Nepdisk",       							NULL,      20},
	{IT_STRING2+IT_SPACE, NULL, 	"GenericHeroGuy    xyzzy", 		 					NULL, 	   30},
	{IT_STRING2+IT_SPACE, NULL, 	"Chearii    riomccloud", 		 					NULL, 	   40},
	{IT_STRING2+IT_SPACE, NULL, 	"chromaticpipe    PAS", 		 					NULL, 	   50},
	{IT_STRING2+IT_SPACE, NULL, 	"$HOME    yama", 		 							NULL, 	   60},
	{IT_STRING2+IT_SPACE, NULL, 	"Achii    scizor300", 		 						NULL, 	   70},
	{IT_STRING2+IT_SPACE, NULL, 	"Superjustinbros    Lugent", 		 				NULL, 	   80},

	{IT_HEADER, 		  NULL, 	"", 												NULL,      74},

	{IT_STRING2+IT_SPACE, NULL, 	"Sunflower    Yuz", 		 						NULL, 	  110},
	{IT_STRING2+IT_SPACE, NULL, 	"Democrab    EXpand", 		  						NULL, 	  120},
	{IT_STRING2+IT_SPACE, NULL, 	"Nexit    Spee", 		  							NULL, 	  130},
	{IT_STRING2+IT_SPACE, NULL, 	"jin    Mompi", 		 							NULL, 	  140},
	{IT_STRING2+IT_SPACE, NULL, 	"Phoenix", 		 									NULL, 	  150},

	{IT_HEADER, 		  NULL, 	"Special Thanks <3", 								NULL,     130},

	{IT_STRING2+IT_SPACE, NULL,		"All of Sunflower's Garden",	      				NULL,     140},
	{IT_STRING2+IT_SPACE, NULL, 	"Blankart",       									NULL,     150},
	{IT_STRING2+IT_SPACE, NULL, 	"SRB2 Legacy",       								NULL,     160},
	{IT_STRING2+IT_SPACE, NULL, 	"SRB2 Classic",       								NULL,     170},
	{IT_STRING2+IT_SPACE, NULL, 	"The Moe Mansion and Birdhouse Team",       		NULL,     180},
	{IT_STRING2+IT_SPACE, NULL, 	"Galactice for Galaxy",       						NULL,     190},

	{IT_STRING+IT_SPACE, NULL, "", 														NULL,     228}, // dummy text I
	{IT_STRING, NULL, "", 																NULL,     278}, // dummy text II
};

// sry we dont have space for this anymore :c < now we have again, for now..... TODO: finally stop being a lazy ass and make a cool credits screen
static const char* OP_CreditTooltips[] =
{
	// this sucks lol note to self: we need as many NULLs as we have entries in OP_SaturnCreditsMenu - 1
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
	"Thanks everyone! <3"
};


static menuitem_t OP_BirdMenu[] =
{
	{IT_HEADER, NULL, "Crazy", NULL, 0},
	{IT_STRING | IT_SUBMENU, NULL, "Screen Tilting...", &OP_TiltDef, 10},

	{IT_HEADER, NULL, "HUD", NULL, 30},
	{IT_STRING | IT_CVAR, NULL, "Show Viewpoint Text in Replays", &cv_showviewpointtext, 40},
	{IT_STRING | IT_CVAR, NULL, "Show FREE PLAY Text",            &cv_showfreeplay,      50},

	//{IT_HEADER, NULL, "Voting", NULL, 70},
	//{IT_STRING | IT_CVAR, NULL, "Only Show One Battle Choice", &cv_lessbattlevotes, 80},
	//{IT_STRING | IT_CVAR, NULL, "Encore Choices",              &cv_encorevotes,     90},

	{IT_HEADER, NULL, "Music", NULL, 70},
	{IT_STRING | IT_CVAR, NULL, "Resume Level Music",    &cv_resume,            80},
	{IT_STRING | IT_CVAR, NULL, "Restart Special Music", &cv_resetspecialmusic, 95},

	{IT_STRING | IT_SUBMENU, NULL, "Advanced Music Options...", &OP_AdvancedBirdDef, 120},
};

static const char* OP_BirdTooltips[] =
{
	NULL,
	"Options for screen tilting.",
	NULL,
	"Show player names in replay playback.",
	"Show FREE PLAY text when in a empty server.",
	//NULL,
	//"Only show one battle choice in map vote",
	//"Amount of encore choices in map vote.",
	NULL,
	"Resume level music from last position after music change.",
	"Restart Special music if item is used again.",
	"Options for advanced music settings.",
};

enum
{
	headercrzy,
	tiltmenu,
	headerhud,
	viewpointext,
	freeplaytext,
	headermusic,
	lvlresum,
	spclresum,
	advmusic,
};

static menuitem_t OP_ForkedBirdMenu[] =
{
	{IT_HEADER, NULL, "Local Skins", NULL, 0},
	{IT_STRING | IT_CVAR | IT_CV_STRING, NULL, "Local Skin Name", &cv_fakelocalskin, 10},
	{IT_STRING2 | IT_SPACE, NULL, "SET TO NONE FOR NO LOCAL SKIN", NULL, 20},
	{IT_STRING | IT_CALL, NULL, "Apply to All Players", M_LocalSkinChange, 140},
	{IT_STRING | IT_CALL, NULL, "Apply to Displaying Player", M_LocalSkinChange, 150},
	{IT_STRING | IT_CALL, NULL, "Apply to Yourself", M_LocalSkinChange, 160},
	{IT_STRING | IT_CVAR, NULL, "Lua Immersion", &cv_luaimmersion, 170},
};

static menuitem_t OP_NametagMenu[] =
{
	{IT_HEADER, NULL, "Nametag", NULL, 0},
	{IT_STRING | IT_CVAR, NULL, "Nametag", &cv_nametag, 20},
	{IT_STRING | IT_CVAR, NULL, "Show Char image in Nametag", &cv_nametagfacerank, 30},
	{IT_STRING | IT_CVAR, NULL, "Show Own Nametag", &cv_showownnametag, 40},
	{IT_STRING | IT_CVAR, NULL, "Nametag Max distance", &cv_nametagdist, 50},
	{IT_STRING | IT_CVAR, NULL, "Nametag Max Display Players", &cv_nametagmaxplayers, 60},
	{IT_STRING | IT_CVAR, NULL, "Nametag Transparency", &cv_nametagtrans, 70},
	{IT_STRING | IT_CVAR, NULL, "Nametag Score", &cv_nametagscore, 80},
	{IT_STRING | IT_CVAR, NULL, "Nametag Restat", &cv_nametagrestat, 90},
	{IT_STRING | IT_CVAR, NULL, "Small Nametags", &cv_smallnametags, 100},
	{IT_STRING | IT_CVAR, NULL, "Show Nametags while Spectating", &cv_shownametagspectator, 110},
	//{IT_STRING | IT_CVAR, NULL, "Nametag Scaling", &cv_nametagscaling, 70}
};

static const char* OP_NametagTooltips[] =
{
	NULL,
	"Enable nametags in game.",
	"Show character icon in nametag.",
	"Show your own nametag in game.",
	"Distance nametags are visible.",
	"Maximum amount of nametags on screen.",
	"Transparency of nametags.",
	"Show player score in nametag.",
	"Show stats in nametags.",
	"Alternative smaller nametags.",
	"Show Nametags when you are spectating.",
};

enum
{
	nt_header,
	nt_nametag,
	nt_ntchar,
	nt_owntag,
	nt_maxdist,
	nt_maxplayer,
	nt_nttrans,
	nt_ntpscore,
	nt_ntrestat,
	nt_smol,
	nt_spec,
};

static menuitem_t OP_DriftGaugeMenu[] =
{
	{IT_HEADER, NULL, "Driftgauge", NULL, 0},
	{IT_STRING | IT_CVAR, NULL, "Driftgauge", &cv_driftgauge, 20},
	{IT_STRING | IT_CVAR, NULL, "Driftgauge Transparency", &cv_driftgaugetrans, 30},
	{IT_STRING | IT_CVAR | IT_CV_BIGFLOAT, NULL, "Driftgauge Offset", &cv_driftgaugeofs, 40},
	{IT_STRING | IT_CVAR, NULL, "Driftgauge Style", &cv_driftgaugestyle, 50},
};

static const char* OP_DriftGaugeTooltips[] =
{
	NULL,
	"Enable driftgauge in game.",
	"Have driftgauge use hud transparency.",
	"Vertical driftgauge offset.",
	"Driftgauge style.",
};

enum
{
	dg_header,
	dg_dg,
	dg_dgtrans,
	dg_dgoffset,
	dg_dgstyle,
};


static menuitem_t OP_TiltMenu[] =
{
	{IT_STRING | IT_CVAR, NULL, "Camera Tilting", &cv_tilting, 0},
	{IT_STRING | IT_CVAR, NULL, "Tilt While Turning", &cv_quaketilt, 10},
	{IT_STRING | IT_CVAR, NULL, "Smoothing Divisor", &cv_tiltsmoothing, 20},

	{IT_STRING | IT_CVAR, NULL, "Also Tilt During Quakes", &cv_actionmovie, 40},
};

static const char* OP_TiltTooltips[] =
{
	"Camera tilting during gameplay.",
	"Tilt camera while turning.",
	"Smoothing value used.",
	"Tilt during screen quakes.",
};

enum
{
	tilting,
	tiltwturning,
	smoothening,
	tiltwquakes,
	windowshake,
};

static menuitem_t OP_AdvancedBirdMenu[] =
{
	{IT_STRING | IT_CVAR, NULL, "Fading",                        &cv_fading,                  0},
	{IT_STRING | IT_CVAR, NULL, "Fade Back from Invincibility",  &cv_invincmusicfade,        10},
	{IT_STRING | IT_CVAR, NULL, "Fade Back from Grow",           &cv_growmusicfade,          20},
	{IT_STRING | IT_CVAR, NULL, "Fade Out Before Respawning",    &cv_respawnfademusicout,    30},
	{IT_STRING | IT_CVAR, NULL, "Fade Back In While Respawning", &cv_respawnfademusicback,   40},

	{IT_STRING | IT_CVAR, NULL, "Resync Threshold",          &cv_music_resync_threshold,     60},
	{IT_STRING | IT_CVAR, NULL, "Resync Special Music Only", &cv_music_resync_powerups_only, 70},
};

static const char* OP_AdvancedBirdTooltips[] =
{
	"Music fading.",
	"Fade in music after Invincibility.",
	"Fade in music after Grow.",
	"Fade out music before respawning.",
	"Fade in music while respawning.",
	"Threshold for syncing music.",
	"Only sync special music.",
};

enum
{
	fading,
	fadeinvinc,
	fadegrow,
	respawnfadeout,
	respawnfadein,
	syncthreshold,
	syncspecialonly,
};

menuitem_t OP_CustomCvarMenu[MAXMENUCCVARS];

// ==========================================================================
// ALL MENU DEFINITIONS GO HERE
// ==========================================================================

// Main Menu and related
menu_t MainDef = CENTERMENUSTYLE(NULL, MainMenu, NULL, 72);

menu_t MISC_AddonsDef =
{
	NULL,
	sizeof (MISC_AddonsMenu)/sizeof (menuitem_t),
	&OP_DataOptionsDef,
	MISC_AddonsMenu,
	M_DrawAddons,
	M_AddonsRefresh,
	50, 28,
	0,
	NULL,
	NULL
};

menu_t MISC_ReplayHutDef =
{
	NULL,
	sizeof (MISC_ReplayHutMenu)/sizeof (menuitem_t),
	NULL,
	MISC_ReplayHutMenu,
	M_DrawReplayHut,
	NULL,
	30, 80,
	0,
	M_QuitReplayHut,
	NULL
};

menu_t MISC_ReplayOptionsDef =
{
	"M_REPOPT",
	sizeof (MISC_ReplayOptionsMenu)/sizeof (menuitem_t),
	&OP_DataOptionsDef,
	MISC_ReplayOptionsMenu,
	M_DrawGenericMenu,
	NULL,
	27, 40,
	0,
	NULL,
	NULL
};

menu_t MISC_ReplayStartDef =
{
	NULL,
	sizeof (MISC_ReplayStartMenu)/sizeof (menuitem_t),
	&MISC_ReplayHutDef,
	MISC_ReplayStartMenu,
	M_DrawReplayStartMenu,
	NULL,
	30, 90,
	0,
	NULL,
	NULL
};

menu_t PlaybackMenuDef = {
	NULL,
	sizeof (PlaybackMenu)/sizeof (menuitem_t),
	NULL,
	PlaybackMenu,
	M_DrawPlaybackMenu,
	NULL,
	//BASEVIDWIDTH/2 - 94, 2,
	BASEVIDWIDTH/2 - 88, 2,
	0,
	NULL,
	NULL
};

menu_t MAPauseDef = PAUSEMENUSTYLE(MAPauseMenu, 40, 72);
menu_t SPauseDef  = PAUSEMENUSTYLE(SPauseMenu, 40, 72);
menu_t MPauseDef  = PAUSEMENUSTYLE(MPauseMenu, 40, 72);

#ifdef HAVE_DISCORDRPC
menu_t MISC_DiscordRequestsDef = {
	NULL,
	sizeof (MISC_DiscordRequestsMenu)/sizeof (menuitem_t),
	&MPauseDef,
	MISC_DiscordRequestsMenu,
	M_DrawDiscordRequests,
	NULL,
	0, 0,
	0,
	NULL,
	NULL
};
#endif

// Misc Main Menu
menu_t MISC_ScrambleTeamDef   = DEFAULTMENUSTYLE(NULL, MISC_ScrambleTeamMenu, &MPauseDef, 27, 40, NULL);
menu_t MISC_ChangeTeamDef     = DEFAULTMENUSTYLE(NULL, MISC_ChangeTeamMenu, &MPauseDef, 27, 40, NULL);
menu_t MISC_ChangeSpectateDef = DEFAULTMENUSTYLE(NULL, MISC_ChangeSpectateMenu, &MPauseDef, 27, 40, NULL);
menu_t MISC_ChangeLevelDef    = MAPICONMENUSTYLE(NULL, MISC_ChangeLevelMenu, &MPauseDef);
menu_t MISC_HelpDef           = IMAGEDEF(MISC_HelpMenu);

// Sky Room
menu_t SR_MainDef = CENTERMENUSTYLE(NULL, SR_MainMenu, &MainDef, 72);

menu_t SR_UnlockChecklistDef =
{
	NULL,
	1,
	&SR_MainDef,
	SR_UnlockChecklistMenu,
	M_DrawChecklist,
	NULL,
	280, 185,
	0,
	NULL,
	NULL
};

menu_t SR_MusicTestDef =
{
	NULL,
	sizeof (SR_MusicTestMenu)/sizeof (menuitem_t),
	&OP_SoundOptionsDef,
	SR_MusicTestMenu,
	M_DrawMusicTest,
	NULL,
	60, 150,
	0,
	NULL,
	NULL
};

// Single Player
menu_t SP_MainDef = CENTERMENUSTYLE(NULL, SP_MainMenu, &MainDef, 72);

menu_t SP_LevelStatsDef =
{
	"M_STATS",
	1,
	&SR_MainDef,
	SP_LevelStatsMenu,
	M_DrawLevelStats,
	NULL,
	280, 185,
	0,
	NULL,
	NULL
};

static menu_t SP_TimeAttackDef =
{
	"M_ATTACK",
	sizeof (SP_TimeAttackMenu)/sizeof (menuitem_t),
	&MainDef,  // Doesn't matter.
	SP_TimeAttackMenu,
	M_DrawTimeAttackMenu,
	NULL,
	34, 40,
	0,
	M_QuitTimeAttackMenu,
	NULL
};
static menu_t SP_ReplayDef =
{
	"M_ATTACK",
	sizeof(SP_ReplayMenu)/sizeof(menuitem_t),
	&SP_TimeAttackDef,
	SP_ReplayMenu,
	M_DrawTimeAttackMenu,
	NULL,
	34, 40,
	0,
	NULL,
	NULL
};
static menu_t SP_GuestReplayDef =
{
	"M_ATTACK",
	sizeof(SP_GuestReplayMenu)/sizeof(menuitem_t),
	&SP_TimeAttackDef,
	SP_GuestReplayMenu,
	M_DrawTimeAttackMenu,
	NULL,
	34, 40,
	0,
	NULL,
	NULL
};
static menu_t SP_GhostDef =
{
	"M_ATTACK",
	sizeof(SP_GhostMenu)/sizeof(menuitem_t),
	&SP_TimeAttackDef,
	SP_GhostMenu,
	M_DrawTimeAttackMenu,
	NULL,
	34, 40,
	0,
	NULL,
	NULL
};

// Multiplayer
menu_t MP_MainDef =
{
	"M_MULTI",
	sizeof (MP_MainMenu)/sizeof (menuitem_t),
	&MainDef,
	MP_MainMenu,
	M_DrawMPMainMenu,
	NULL,
	42, 30,
	0,
	M_CancelConnect,
	NULL
};

menu_t MP_OfflineServerDef = MAPICONMENUSTYLE("M_MULTI", MP_OfflineServerMenu, &MP_MainDef);

menu_t MP_ServerDef = MAPICONMENUSTYLE("M_MULTI", MP_ServerMenu, &MP_MainDef);

menu_t MP_ConnectDef =
{
	"M_MULTI",
	sizeof (MP_ConnectMenu)/sizeof (menuitem_t),
	&MP_MainDef,
	MP_ConnectMenu,
	M_DrawConnectMenu,
	NULL,
	27,24,
	0,
	M_CancelConnect,
	NULL
};

menu_t MP_PlayerSetupDef =
{
	NULL, //"M_SPLAYR"
	sizeof (MP_PlayerSetupMenu)/sizeof (menuitem_t),
	&MP_MainDef,
	MP_PlayerSetupMenu,
	M_DrawSetupMultiPlayerMenu,
	NULL,
	36, 14,
	0,
	M_QuitMultiPlayerMenu,
	NULL
};

// Options
menu_t OP_MainDef =
{
	"M_OPTTTL",
	sizeof (OP_MainMenu)/sizeof (menuitem_t),
	&MainDef,
	OP_MainMenu,
	M_DrawGenericMenu,
	NULL,
	60, 25,
	0,
	NULL,
	NULL
};

menu_t OP_ControlsDef     = DEFAULTMENUSTYLE("M_CONTRO", OP_ControlsMenu, &OP_MainDef, 40, 15, OP_ControlsTooltips);
//WTF
menu_t OP_MouseOptionsDef = DEFAULTMENUSTYLE("M_CONTRO", OP_MouseOptionsMenu, &OP_ControlsDef, 60, 30, OP_MouseTooltips);
menu_t OP_AllControlsDef  = CONTROLMENUSTYLE(OP_AllControlsMenu, &OP_ControlsDef, OP_AllControlsTooltips);
menu_t OP_Joystick1Def    = DEFAULTSCROLLSTYLE("M_CONTRO", OP_Joystick1Menu, &OP_AllControlsDef, 30, 36, NULL);
menu_t OP_Joystick2Def    = DEFAULTSCROLLSTYLE("M_CONTRO", OP_Joystick2Menu, &OP_AllControlsDef, 30, 36, NULL);
menu_t OP_Joystick3Def    = DEFAULTSCROLLSTYLE("M_CONTRO", OP_Joystick3Menu, &OP_AllControlsDef, 30, 36, NULL);
menu_t OP_Joystick4Def    = DEFAULTSCROLLSTYLE("M_CONTRO", OP_Joystick4Menu, &OP_AllControlsDef, 30, 36, NULL);

menu_t OP_JoystickSetDef  =
{
	"M_CONTRO",
	sizeof (OP_JoystickSetMenu)/sizeof (menuitem_t),
	&OP_Joystick1Def,
	OP_JoystickSetMenu,
	M_DrawJoystick,
	NULL,
	50, 40,
	0,
	NULL,
	NULL
};

menu_t OP_VideoOptionsDef =
{
	"M_VIDEO",
	sizeof(OP_VideoOptionsMenu)/sizeof(menuitem_t),
	&OP_MainDef,
	OP_VideoOptionsMenu,
	M_DrawVideoMenu,
	NULL,
	30, 15,
	0,
	NULL,
	OP_VideoTooltips
};

menu_t OP_VideoModeDef =
{
	"M_VIDEO",
	1,
	&OP_VideoOptionsDef,
	OP_VideoModeMenu,
	M_DrawVideoMode,
	NULL,
	48, 26,
	0,
	NULL,
	NULL
};

menu_t OP_ColorOptionsDef =
{
	"M_VIDEO",
	sizeof (OP_ColorOptionsMenu)/sizeof (menuitem_t),
	&OP_VideoOptionsDef,
	OP_ColorOptionsMenu,
	M_DrawColorMenu,
	NULL,
	30, 30,
	0,
	NULL,
	NULL
};

menu_t OP_SoundOptionsDef =
{
	"M_SOUND",
	sizeof (OP_SoundOptionsMenu)/sizeof (menuitem_t),
	&OP_MainDef,
	OP_SoundOptionsMenu,
	M_DrawSkyRoom,
	NULL,
	30, 20,
	0,
	NULL,
	OP_SoundTooltips
};


menu_t OP_HUDOptionsDef =
{
	"M_HUD",
	sizeof (OP_HUDOptionsMenu)/sizeof (menuitem_t),
	&OP_MainDef,
	OP_HUDOptionsMenu,
	M_DrawHUDOptions,
	NULL,
	30, 20,
	0,
	NULL,
	NULL
};

menu_t OP_CamOptionsDef        = DEFAULTMENUSTYLE(NULL, OP_CamOptionsMenu, &OP_MainDef, 30, 30, OP_CamOptionsTooltips);
menu_t OP_Player1CamOptionsDef = DEFAULTMENUSTYLE(NULL, OP_Player1CamOptionsMenu, &OP_CamOptionsDef, 30, 30, OP_PlayerCamOptionsTooltips);
menu_t OP_Player2CamOptionsDef = DEFAULTMENUSTYLE(NULL, OP_Player2CamOptionsMenu, &OP_CamOptionsDef, 30, 30, OP_PlayerCamOptionsTooltips);
menu_t OP_Player3CamOptionsDef = DEFAULTMENUSTYLE(NULL, OP_Player3CamOptionsMenu, &OP_CamOptionsDef, 30, 30, OP_PlayerCamOptionsTooltips);
menu_t OP_Player4CamOptionsDef = DEFAULTMENUSTYLE(NULL, OP_Player4CamOptionsMenu, &OP_CamOptionsDef, 30, 30, OP_PlayerCamOptionsTooltips);

menu_t OP_ChatOptionsDef = DEFAULTMENUSTYLE("M_HUD", OP_ChatOptionsMenu, &OP_HUDOptionsDef, 30, 30, OP_ChatOptionsTooltips);

menu_t OP_SoundAdvancedDef = DEFAULTSCROLLSTYLE("M_SOUND", OP_SoundAdvancedMenu, &OP_SoundOptionsDef, 30, 30, OP_SoundAdvancedTooltips);

menu_t OP_FocusOptionsDef = DEFAULTMENUSTYLE(NULL, OP_FocusOptionsMenu, &OP_MainDef, 17, 30, OP_FocusOptionsTooltips);

menu_t OP_GameOptionsDef = DEFAULTMENUSTYLE("M_GAME", OP_GameOptionsMenu, &OP_MainDef, 30, 20, OP_GameTooltips);
menu_t OP_ServerOptionsDef = DEFAULTMENUSTYLE("M_SERVER", OP_ServerOptionsMenu, &OP_MainDef, 24, 20, OP_ServerOptionsTooltips);
menu_t OP_AdvServerOptionsDef = DEFAULTSCROLLSTYLE("M_SERVER", OP_AdvServerOptionsMenu, &OP_ServerOptionsDef, 24, 30, OP_AdvServerOptionsTooltips);

menu_t OP_MonitorToggleDef =
{
	"M_GAME",
	sizeof (OP_MonitorToggleMenu)/sizeof (menuitem_t),
	&OP_GameOptionsDef,
	OP_MonitorToggleMenu,
	M_DrawMonitorToggles,
	NULL,
	47, 30,
	0,
	NULL,
	NULL
};

#ifdef HWRENDER
menu_t OP_OpenGLOptionsDef = DEFAULTSCROLLSTYLE("M_VIDEO", OP_OpenGLOptionsMenu, &OP_VideoOptionsDef, 30, 30, OP_OpenGLTooltips);
#endif

menu_t OP_ExpOptionsDef = DEFAULTSCROLLSTYLE("M_VIDEO", OP_ExpOptionsMenu, &OP_VideoOptionsDef, 30, 25, OP_ExpTooltips);

menu_t OP_DataOptionsDef       = DEFAULTMENUSTYLE("M_DATA", OP_DataOptionsMenu, &OP_MainDef, 60, 30, NULL);
menu_t OP_ScreenshotOptionsDef = DEFAULTSCROLLSTYLE("M_SCSHOT", OP_ScreenshotOptionsMenu, &OP_DataOptionsDef, 30, 30, NULL);
menu_t OP_AddonsOptionsDef     = DEFAULTMENUSTYLE("M_ADDONS", OP_AddonsOptionsMenu, &OP_DataOptionsDef, 30, 30, NULL);
menu_t OP_ProtocolDef          = DEFAULTMENUSTYLE(NULL, OP_ProtocolMenu, &OP_DataOptionsDef, 30, 30, NULL);
#ifdef HAVE_DISCORDRPC
menu_t OP_DiscordOptionsDef    = DEFAULTMENUSTYLE(NULL, OP_DiscordOptionsMenu, &OP_DataOptionsDef, 30, 30, NULL);
#endif
menu_t OP_EraseDataDef         = DEFAULTMENUSTYLE("M_DATA", OP_EraseDataMenu, &OP_DataOptionsDef, 30, 30, NULL);

menu_t OP_AccessibilityDef     = DEFAULTSCROLLSTYLE(NULL, OP_AccessibilityMenu, &OP_MainDef, 30, 30, OP_AccessibilityTooltips);

menu_t OP_SaturnDef        = DEFAULTSCROLLSTYLE(NULL, OP_SaturnMenu, &OP_MainDef, 30, 30, OP_SaturnTooltips);
menu_t OP_SaturnCreditsDef = DEFAULTMENUSTYLE(NULL, OP_SaturnCreditsMenu, &OP_SaturnDef, 30, 2, OP_CreditTooltips); // OP_CreditTooltips no space :c

menu_t OP_SaturnHudDef     = DEFAULTSCROLLSTYLE(NULL, OP_SaturnHudMenu, &OP_SaturnDef, 30, 30, OP_SaturnHudTooltips);
menu_t OP_PlayerDistortDef = DEFAULTSCROLLSTYLE("M_VIDEO", OP_PlayerDistortMenu, &OP_SaturnDef, 30, 30, OP_PlayerDistortTooltips);
menu_t OP_HudOffsetDef     = DEFAULTSCROLLSTYLE(NULL, OP_HudOffsetMenu, &OP_SaturnHudDef, 30, 30, NULL);

menu_t OP_BirdDef = DEFAULTMENUSTYLE(NULL, OP_BirdMenu, &OP_MainDef, 30, 30, OP_BirdTooltips);

menu_t OP_NametagDef    = DEFAULTMENUSTYLE(NULL, OP_NametagMenu, &OP_SaturnHudDef, 30, 40, OP_NametagTooltips);
menu_t OP_DriftGaugeDef = DEFAULTMENUSTYLE(NULL, OP_DriftGaugeMenu, &OP_SaturnHudDef, 30, 40, OP_DriftGaugeTooltips);

menu_t OP_TiltDef         = DEFAULTMENUSTYLE(NULL, OP_TiltMenu, &OP_BirdDef, 30, 60, OP_TiltTooltips);
menu_t OP_AdvancedBirdDef = DEFAULTMENUSTYLE(NULL, OP_AdvancedBirdMenu, &OP_BirdDef, 30, 60, OP_AdvancedBirdTooltips);


menu_t OP_CustomCvarMenuDef = DEFAULTSCROLLSTYLE("M_ADDONS", OP_CustomCvarMenu, &OP_MainDef, 10, 30, NULL);

menu_t OP_ForkedBirdDef = {
	NULL,
	sizeof(OP_ForkedBirdMenu)/sizeof(menuitem_t),
	&OP_MainDef,
	OP_ForkedBirdMenu,
	M_DrawLocalSkinMenu,
	NULL,
	30, 6,
	0,
	NULL,
	NULL
};

menu_t OP_LocalSkinDef = DEFAULTMENUSTYLE(NULL, OP_TiltMenu, &OP_ForkedBirdDef, 30, 60, NULL);

// Onchange functions to show/hide/move things in menus
void Screenshot_option_Onchange(void)
{
	OP_ScreenshotOptionsMenu[op_screenshot_folder].status =
		(cv_screenshot_option.value == 3 ? IT_CVAR|IT_STRING|IT_CV_STRING : IT_DISABLED);
}

void Moviemode_mode_Onchange(void)
{
	INT32 i, cstart, cend;
	for (i = op_screenshot_gif_start; i <= op_screenshot_gif_end; ++i)
		OP_ScreenshotOptionsMenu[i].status = IT_DISABLED;

	switch (cv_moviemode.value)
	{
		case MM_GIF:
			cstart = op_screenshot_gif_start;
			cend = op_screenshot_gif_end;
			break;
		default:
			return;
	}

	for (i = cstart; i <= cend; ++i)
		OP_ScreenshotOptionsMenu[i].status = IT_STRING|IT_CVAR;
}

void Addons_option_Onchange(void)
{
	OP_AddonsOptionsMenu[op_addons_folder].status =
		(cv_addons_option.value == 3 ? IT_CVAR|IT_STRING|IT_CV_STRING : IT_DISABLED);
}

void Moviemode_option_Onchange(void)
{
	OP_ScreenshotOptionsMenu[op_movie_folder].status =
		(cv_movie_option.value == 3 ? IT_CVAR|IT_STRING|IT_CV_STRING : IT_DISABLED);
}

void PDistort_menu_Onchange(void)
{
	if (cv_sloperoll.value)
	{
		OP_PlayerDistortMenu[slrotatedist].status = IT_STRING | IT_CVAR;
		OP_PlayerDistortMenu[saltroll].status = IT_STRING | IT_CVAR;
	}
	else
	{
		OP_PlayerDistortMenu[slrotatedist].status = IT_GRAYEDOUT;
		OP_PlayerDistortMenu[saltroll].status = IT_GRAYEDOUT;
	}
}

void Bird_menu_Onchange(void)
{
	UINT16 status;

	if (cv_tilting.value)
		status = IT_STRING | IT_CVAR;
	else
		status = IT_GRAYEDOUT;

	OP_TiltMenu[tiltwturning].status = status;
	OP_TiltMenu[smoothening].status = status;
	OP_TiltMenu[tiltwquakes].status = status;

	if (cv_fading.value)
		status = IT_STRING | IT_CVAR;
	else
		status = IT_GRAYEDOUT;

	OP_AdvancedBirdMenu[fadeinvinc].status = status;
	OP_AdvancedBirdMenu[fadegrow].status = status;
	OP_AdvancedBirdMenu[respawnfadeout].status = status;
	OP_AdvancedBirdMenu[respawnfadein].status = status;
}

// menu code is nice
void SaturnHud_menu_Onchange(void)
{
	UINT16 status;

	if (cv_colorizedhud.value)
		status = IT_STRING | IT_CVAR;
	else
		status = IT_GRAYEDOUT;

	OP_SaturnHudMenu[sh_coloritem].status = status;
	OP_SaturnHudMenu[sh_colorhud_customcolor].status = status;
}

void GameFocus_menu_Onchange(void)
{
	OP_FocusOptionsMenu[5].status = cv_usemouse.value ? IT_GRAYEDOUT : IT_STRING | IT_CVAR;
}

#ifdef HWRENDER
void M_UpdateOGLMenu(void)
{
#ifdef USE_FBO_OGL
	OP_ExpOptionsMenu[op_exp_fbo].status = (!supportFBO || cv_glscreentextures.value != 2) ? IT_GRAYEDOUT : IT_STRING | IT_CVAR;
#endif
	OP_ExpOptionsMenu[op_exp_paldepth].status = (!HWR_ShouldUsePaletteRendering()) ? IT_GRAYEDOUT : IT_STRING | IT_CVAR;

	OP_OpenGLOptionsMenu[op_gl_falbckmdls].status = (!cv_glmdls.value) ? IT_GRAYEDOUT : IT_STRING | IT_CVAR;

	OP_OpenGLOptionsMenu[op_gl_lightdither].status = (!HWR_UseShader()) ? IT_GRAYEDOUT : IT_STRING | IT_CVAR;

	OP_OpenGLOptionsMenu[op_gl_palrender].status = (cv_glscreentextures.value != 2 || !HWR_UseShader()) ? IT_GRAYEDOUT : IT_STRING | IT_CVAR;
}
#endif
