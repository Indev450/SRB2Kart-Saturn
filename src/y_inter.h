// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2004-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  y_inter.h
/// \brief Tally screens, or "Intermissions" as they were formally called in Doom

extern boolean usebuffer;

void Y_IntermissionDrawer(void);
void Y_Ticker(void);
void Y_StartIntermission(void);
void Y_EndIntermission(void);

void Y_VoteDrawer(void);
void Y_VoteTicker(void);
void Y_StartVote(void);
void Y_EndVote(void);
void Y_SetupVoteFinish(SINT8 pick, SINT8 level);

typedef struct
{
	UINT8 *color[MAXPLAYERS]; // Winner's color #
	INT32 *character[MAXPLAYERS]; // Winner's character #
	INT32 num[MAXPLAYERS]; // Winner's player #
	char *name[MAXPLAYERS]; // Winner's name
	INT32 numplayers; // Number of players being displayed
	char levelstring[64]; // holds levelnames up to 64 characters
	// SRB2kart
	UINT8 increase[MAXPLAYERS]; // how much did the score increase by?
	UINT8 jitter[MAXPLAYERS]; // wiggle
	UINT32 val[MAXPLAYERS]; // Gametype-specific value
	UINT8 pos[MAXPLAYERS]; // player positions. used for ties
	boolean rankingsmode; // rankings mode
	boolean encore; // encore mode
} y_data_t;

typedef enum
{
	int_none,
	int_timeattack,  // Time Attack
	int_match,       // Match
	int_teammatch,   // Team Match
//	int_tag,         // Tag
	int_ctf,         // CTF
	int_spec,        // Special Stage
	int_nights,      // NiGHTS into Dreams
	int_nightsspec,  // NiGHTS special stage
	int_race,        // Race
	int_classicrace, // Competition
} intertype_t;
extern intertype_t intertype;

// VEXTRN - Vote (V) Extra (EXT) Race (R) Normal (N - Normal sized patch)
// VEXTRW - Vote (V) Extra (EXT) Race (R) Normal (W - Wide patch)
// VEXTBN - Vote (V) Extra (EXT) Battle (B) Normal (N - Normal sized patch)
// VEXTBW - Vote (V) Extra (EXT) Battle (B) Normal (W - Wide patch)
typedef struct
{
	char Prefix[5];                 // Race = INTSX, Battle = BTLSX
	char luaPrefix[5];              // prefix for lua votescreens

	INT32 currentAnimFrame;         // current animated background frame

	INT32 foundLuaVoteFrames;       // normal lua patch frames
	INT32 foundLuaVoteWideFrames;   // wide lua patch frames

	boolean racereplaced;           // non lua race patch replaced
	boolean wideracereplaced;       // non lua wide race patch replaced
	boolean battlereplaced;         // non lua battle patch replaced
	boolean widebattlereplaced;     // non lua wide battle patch replaced

	patch_t *bgpatch;               // votebackground patch
	patch_t *widebgpatch;           // wide votebackground patch
} votescreen_t;
extern votescreen_t VoteScreen;
