// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  g_demo.c
/// \brief Demo recording and playback

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

// for replay dates
#include <time.h>
#include <locale.h>

// menu demo things
UINT8  numDemos      = 0; //3; -- i'm FED UP of losing my skincolour to a broken demo. change this back when we make new ones
UINT32 demoDelayTime = 15*TICRATE;
UINT32 demoIdleTime  = 3*TICRATE;

boolean nodrawers = false; // for comparative timing purposes
boolean noblit = false; // for comparative timing purposes
static tic_t demostarttime; // for comparative timing purposes

//@TODO put these all in a struct for namespacing purposes?
static char demoname[128];
savebuffer_t demobuf = {0};
static UINT8 *demotime_p, *demoinfo_p;
static UINT8 *demoend;
static UINT8 demoflags;
static boolean demosynced = true; // console warning message

struct demovars_s demo = {};

consvar_t cv_resyncdemo = {"resyncdemo", "On", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

// extra data stuff (events registered this frame while recording)
static struct
{
	UINT8 flags; // EZT flags

	// EZT_COLOR
	UINT8 color, lastcolor;

	// EZT_SCALE
	fixed_t scale, lastscale;

	// EZT_KART
	INT32 kartitem, kartamount, kartbumpers;

	UINT8 desyncframes; // Don't try to resync unless we've been off for two frames, to monkeypatch a few trouble spots

	// EZT_HIT
	UINT16 hits;
	mobj_t **hitlist;
} ghostext[MAXPLAYERS];

// Your naming conventions are stupid and useless.
// There is no conflict here.
demoghost *ghosts = NULL;

static CV_PossibleValue_t recordmultiplayerdemos_cons_t[] = {{0, "Disabled"}, {1, "Manual Save"}, {2, "Auto Save"}, {0, NULL}};
consvar_t cv_recordmultiplayerdemos = {"netdemo_record", "Manual Save", CV_SAVE, recordmultiplayerdemos_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t netdemosyncquality_cons_t[] = {{1, "MIN"}, {35, "MAX"}, {0, NULL}};
consvar_t cv_netdemosyncquality = {"netdemo_syncquality", "1", CV_SAVE, netdemosyncquality_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

// Units are MiB.
static CV_PossibleValue_t maxdemosize_cons_t[] = {{10, "MIN"}, {100, "MAX"}, {0, NULL}};
consvar_t cv_maxdemosize = {"maxdemosize", "10", CV_SAVE, maxdemosize_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t demochangemap_cons_t[] = {{0, "Disabled"}, {1, "Diff Map"}, {2, "Always"}, {0, NULL}};
consvar_t cv_demochangemap = {"netdemo_savemapchange", "Disabled", CV_SAVE, demochangemap_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static CV_PossibleValue_t demodateformat_cons_t[] = {{0, "Automatic"}, {1, "EU"}, {2, "US"}, {0, NULL}};
consvar_t cv_demodateformat = {"netdemo_dateformat", "Automatic", CV_SAVE, demodateformat_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

#define DEMOVERSION 0x0002
#define DEMOHEADER  "\xF0" "KartReplay" "\x0F"

#define DF_GHOST        0x01 // This demo contains ghost data too!
#define DF_RECORDATTACK 0x02 // This demo is from record attack and contains its final completion time!
#define DF_NIGHTSATTACK 0x04 // This demo is from NiGHTS attack and contains its time left, score, and mares!
#define DF_ATTACKMASK   0x06 // This demo is from ??? attack and contains ???

#define DF_LUAVARS		0x20 // this demo contains extra lua vars; this is mostly used for backwards compability

#define DF_ATTACKSHIFT  1
#define DF_ENCORE       0x40
#define DF_MULTIPLAYER  0x80 // This demo was recorded in multiplayer mode!

#ifdef DEMO_COMPAT_100
#define DF_FILELIST     0x08 // This demo contains an extra files list
#define DF_GAMETYPEMASK 0x30
#define DF_GAMESHIFT    4
#endif

#define DEMO_SPECTATOR 0x40

// For demos
#define ZT_FWD     0x01
#define ZT_SIDE    0x02
#define ZT_ANGLE   0x04
#define ZT_BUTTONS 0x08
#define ZT_AIMING  0x10
#define ZT_DRIFT   0x20
#define ZT_LATENCY 0x40
#define DEMOMARKER 0x80 // demoend

UINT8 demo_extradata[MAXPLAYERS] = {0};
UINT8 demo_writerng = 0; // 0=no, 1=yes, 2=yes but on a timeout
static ticcmd_t oldcmd[MAXPLAYERS];

#define DW_END        0xFF // End of extradata block
#define DW_RNG        0xFE // Check RNG seed!

#define DW_EXTRASTUFF 0xFE // Numbers below this are reserved for writing player slot data

// Below consts are only used for demo extrainfo sections
#define DW_STANDING 0x00

// For Metal Sonic and time attack ghosts
#define GZT_XYZ    0x01
#define GZT_MOMXY  0x02
#define GZT_MOMZ   0x04
#define GZT_ANGLE  0x08
// Not used for Metal Sonic
#define GZT_SPRITE 0x10 // Animation frame
#define GZT_EXTRA  0x20
#define GZT_NIGHTS 0x40 // NiGHTS Mode stuff!

// GZT_EXTRA flags
#define EZT_THOK   0x01 // Spawned a thok object
#define EZT_SPIN   0x02 // Because one type of thok object apparently wasn't enough
#define EZT_REV    0x03 // And two types wasn't enough either yet
#define EZT_THOKMASK 0x03
#define EZT_COLOR  0x04 // Changed color (Super transformation, Mario fireflowers/invulnerability, etc.)
#define EZT_FLIP   0x08 // Reversed gravity
#define EZT_SCALE  0x10 // Changed size
#define EZT_HIT    0x20 // Damaged a mobj
#define EZT_SPRITE 0x40 // Changed sprite set completely out of PLAY (NiGHTS, SOCs, whatever)
#define EZT_KART   0x80 // SRB2Kart: Changed current held item/quantity and bumpers for battle

static mobj_t oldghost[MAXPLAYERS];

static void G_ResetDemoPlayback(void);

// Finds a skin with the closest stats if the expected skin doesn't exist.
static INT32 GetSkinNumClosestToStats(UINT8 kartspeed, UINT8 kartweight)
{
	INT32 i, closest_skin = 0;
	UINT8 closest_stats = UINT8_MAX, stat_diff;

	for (i = 0; i < numskins; i++)
	{
		stat_diff = abs(skins[i].kartspeed - kartspeed) + abs(skins[i].kartweight - kartweight);

		if (stat_diff < closest_stats)
		{
			closest_stats = stat_diff;
			closest_skin = i;
		}
	}

	return closest_skin;
}

static void FindClosestSkinForStats(UINT32 p, UINT8 kartspeed, UINT8 kartweight)
{
	INT32 closest_skin = GetSkinNumClosestToStats(kartspeed, kartweight);

	//CONS_Printf("Using %s instead...\n", skins[closest_skin].name);
	SetPlayerSkinByNum(p, closest_skin);
}

void G_ReadDemoExtraData(void)
{
	INT32 p, extradata, i;
	char name[17];

	if (leveltime > starttime)
	{
		rewind_t *rewind = CL_SaveRewindPoint(demobuf.p - demobuf.buffer);
		if (rewind)
		{
			memcpy(rewind->oldcmd, oldcmd, sizeof (oldcmd));
			memcpy(rewind->oldghost, oldghost, sizeof (oldghost));
		}
	}

	memset(name, '\0', 17);

	p = READUINT8(demobuf.p);

	while (p < DW_EXTRASTUFF)
	{
		extradata = READUINT8(demobuf.p);

		player_t *player = &players[p];

		if (extradata & DXD_RESPAWN)
		{
			if (player->mo)
				P_DamageMobj(player->mo, NULL, NULL, DMG_INSTAKILL); // Is this how this should work..?
		}

		if (extradata & DXD_SKIN)
		{
			UINT8 kartspeed, kartweight;

			// Skin
			memcpy(name, demobuf.p, 16);
			demobuf.p += 16;
			SetPlayerSkin(p, name);

			kartspeed = READUINT8(demobuf.p);
			kartweight = READUINT8(demobuf.p);

			if (!fasticmp(skins[player->skin].name, name))
				FindClosestSkinForStats(p, kartspeed, kartweight);

			player->kartspeed = kartspeed;
			player->kartweight = kartweight;
		}

		if (extradata & DXD_COLOR)
		{
			// Color
			memcpy(name, demobuf.p, 16);
			demobuf.p += 16;
			for (i = 0; i < MAXSKINCOLORS; i++)
				if (fasticmp(KartColor_Names[i], name)) // SRB2kart
				{
					player->skincolor = i;
					if (player->mo)
						player->mo->color = i;
					break;
				}
		}

		if (extradata & DXD_NAME)
		{
			// Name
			memcpy(player_names[p],demobuf.p,16);
			demobuf.p += 16;
		}

		if (extradata & DXD_PLAYSTATE)
		{
			extradata = READUINT8(demobuf.p);

			switch (extradata)
			{
				case DXD_PST_PLAYING:
					player->pflags |= PF_WANTSTOJOIN; // fuck you
					break;
				case DXD_PST_SPECTATING:
					player->pflags &= ~PF_WANTSTOJOIN; // double-fuck you

					if (!playeringame[p])
					{
						CL_ClearPlayer(p);
						playeringame[p] = true;
						G_AddPlayer(p);
						player->spectator = true;

						// There's likely an off-by-one error in timing recording or playback of joins. This hacks around it so I don't have to find out where that is. \o/
						if (oldcmd[p].forwardmove)
							P_RandomByte();
					}
					else
					{
						player->spectator = true;
						if (player->mo)
							P_DamageMobj(player->mo, NULL, NULL, DMG_INSTAKILL);
						else
							player->playerstate = PST_REBORN;
					}
					break;
				case DXD_PST_LEFT:
					CL_RemovePlayer(p, 0);
					break;
			}

			G_ResetViews(false); // dont reset our freecam pls thx!

			// maybe these are necessary?
			if (G_BattleGametype())
				K_CheckBumpers(); // SRB2Kart
			else if (G_RaceGametype())
				P_CheckRacers(); // also SRB2Kart
		}

		p = READUINT8(demobuf.p);
	}

	while (p != DW_END)
	{
		UINT32 rng;
		UINT32 checkrng;

		if (p == DW_RNG)
		{
			rng = READUINT32(demobuf.p);
			checkrng = P_GetRandSeed();

			if (checkrng != rng)
			{
				if (demosynced)
				{
					CONS_Alert(CONS_WARNING, M_GetText("Demo playback has desynced (RNG)!\n"));
					CONS_Printf("expected rng %d got %d\n", rng, checkrng);
				}

				demosynced = false;

				P_SetRandSeed(rng);
			}
		}

		p = READUINT8(demobuf.p);
	}

	if (!(demoflags & DF_GHOST) && *demobuf.p == DEMOMARKER)
	{
		// end of demo data stream
		G_CheckDemoStatus();
		return;
	}
}

void G_WriteDemoExtraData(void)
{
	INT32 i;
	char name[17];

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (demo_extradata[i])
		{
			WRITEUINT8(demobuf.p, i);
			WRITEUINT8(demobuf.p, demo_extradata[i]);

			//if (demo_extradata[i] & DXD_RESPAWN) has no extra data
			if (demo_extradata[i] & DXD_SKIN)
			{
				// Skin
				memset(name, 0, 16);
				strncpy(name, skins[players[i].skin].name, 16);
				memcpy(demobuf.p, name, 16);
				demobuf.p += 16;

				WRITEUINT8(demobuf.p, skins[players[i].skin].kartspeed);
				WRITEUINT8(demobuf.p, skins[players[i].skin].kartweight);
			}

			if (demo_extradata[i] & DXD_COLOR)
			{
				// Color
				memset(name, 0, 16);
				strncpy(name, KartColor_Names[players[i].skincolor], 16);
				memcpy(demobuf.p, name, 16);
				demobuf.p += 16;
			}

			if (demo_extradata[i] & DXD_NAME)
			{
				// Name
				memset(name, 0, 16);
				memcpy(name, player_names[i], 15); // Keeping 1 null byte for safety, sorry players with name containing more than 15 characters
				memcpy(demobuf.p, name, 16);
				demobuf.p += 16;
			}

			if (demo_extradata[i] & DXD_PLAYSTATE)
			{
				demo_writerng = 1;
				if (!playeringame[i])
					WRITEUINT8(demobuf.p, DXD_PST_LEFT);
				else if (
					players[i].spectator &&
					!(players[i].pflags & PF_WANTSTOJOIN) // <= fuck you specifically
				)
					WRITEUINT8(demobuf.p, DXD_PST_SPECTATING);
				else
					WRITEUINT8(demobuf.p, DXD_PST_PLAYING);
			}
		}

		demo_extradata[i] = 0;
	}

	// May not be necessary, but might as well play it safe...
	if ((leveltime & 255) == 128)
		demo_writerng = 1;

	{
		static UINT8 timeout = 0;

		if (timeout) timeout--;

		if (demo_writerng == 1 || (demo_writerng == 2 && timeout == 0))
		{
			demo_writerng = 0;
			timeout = 16;
			WRITEUINT8(demobuf.p, DW_RNG);
			WRITEUINT32(demobuf.p, P_GetRandSeed());
		}
	}

	WRITEUINT8(demobuf.p, DW_END);
}

void G_ReadDemoTiccmd(ticcmd_t *cmd, INT32 playernum)
{
	UINT8 ziptic;

	if (!demobuf.p || !demo.deferstart)
		return;
	ziptic = READUINT8(demobuf.p);

	if (ziptic & ZT_FWD)
		oldcmd[playernum].forwardmove = READSINT8(demobuf.p);
	if (ziptic & ZT_SIDE)
		oldcmd[playernum].sidemove = READSINT8(demobuf.p);
	if (ziptic & ZT_ANGLE)
		oldcmd[playernum].angleturn = READINT16(demobuf.p);
	if (ziptic & ZT_BUTTONS)
		oldcmd[playernum].buttons = READUINT16(demobuf.p);
	if (ziptic & ZT_AIMING)
		oldcmd[playernum].aiming = READINT16(demobuf.p);
	if (ziptic & ZT_DRIFT)
		oldcmd[playernum].driftturn = READINT16(demobuf.p);
	if (ziptic & ZT_LATENCY)
		oldcmd[playernum].latency = READUINT8(demobuf.p);

	G_CopyTiccmd(cmd, &oldcmd[playernum], 1);

	// what in the actual fuck is this???
	// SRB2kart: Copy-pasted from ticcmd building, removes that crappy demo cam
	if (((players[displayplayers[0]].mo && players[displayplayers[0]].speed > 0) // Moving
		|| (leveltime > starttime && (cmd->buttons & BT_ACCELERATE && cmd->buttons & BT_BRAKE)) // Rubber-burn turn
		|| (players[displayplayers[0]].kartstuff[k_respawn]) // Respawning
		|| (players[displayplayers[0]].spectator || objectplacing)) // Not a physical player
		&& !(players[displayplayers[0]].kartstuff[k_spinouttimer] && players[displayplayers[0]].kartstuff[k_sneakertimer])) // Spinning and boosting cancels out spinout
		localangle[0] += (cmd->angleturn<<16);

	if (!(demoflags & DF_GHOST) && *demobuf.p == DEMOMARKER)
	{
		// end of demo data stream
		G_CheckDemoStatus();
		return;
	}
}

void G_WriteDemoTiccmd(ticcmd_t *cmd, INT32 playernum)
{
	char ziptic = 0;
	UINT8 *ziptic_p;

	if (!demobuf.p)
		return;

	ziptic_p = demobuf.p++; // the ziptic, written at the end of this function

	if (cmd->forwardmove != oldcmd[playernum].forwardmove)
	{
		WRITEUINT8(demobuf.p,cmd->forwardmove);
		oldcmd[playernum].forwardmove = cmd->forwardmove;
		ziptic |= ZT_FWD;
	}

	if (cmd->sidemove != oldcmd[playernum].sidemove)
	{
		WRITEUINT8(demobuf.p,cmd->sidemove);
		oldcmd[playernum].sidemove = cmd->sidemove;
		ziptic |= ZT_SIDE;
	}

	if (cmd->angleturn != oldcmd[playernum].angleturn)
	{
		WRITEINT16(demobuf.p,cmd->angleturn);
		oldcmd[playernum].angleturn = cmd->angleturn;
		ziptic |= ZT_ANGLE;
	}

	if (cmd->buttons != oldcmd[playernum].buttons)
	{
		WRITEUINT16(demobuf.p,cmd->buttons);
		oldcmd[playernum].buttons = cmd->buttons;
		ziptic |= ZT_BUTTONS;
	}

	if (cmd->aiming != oldcmd[playernum].aiming)
	{
		WRITEINT16(demobuf.p,cmd->aiming);
		oldcmd[playernum].aiming = cmd->aiming;
		ziptic |= ZT_AIMING;
	}

	if (cmd->driftturn != oldcmd[playernum].driftturn)
	{
		WRITEINT16(demobuf.p,cmd->driftturn);
		oldcmd[playernum].driftturn = cmd->driftturn;
		ziptic |= ZT_DRIFT;
	}

	if (cmd->latency != oldcmd[playernum].latency)
	{
		WRITEUINT8(demobuf.p,cmd->latency);
		oldcmd[playernum].latency = cmd->latency;
		ziptic |= ZT_LATENCY;
	}

	*ziptic_p = ziptic;

	// attention here for the ticcmd size!
	// latest demos with mouse aiming byte in ticcmd
	if (!(demoflags & DF_GHOST) && ziptic_p > demoend - 9)
	{
		G_CheckDemoStatus(); // no more space
		return;
	}
}

void G_GhostAddThok(INT32 playernum)
{
	if (!demo.recording || !(demoflags & DF_GHOST))
		return;

	ghostext[playernum].flags = (ghostext[playernum].flags & ~EZT_THOKMASK) | EZT_THOK;
}

void G_GhostAddSpin(INT32 playernum)
{
	if (!demo.recording || !(demoflags & DF_GHOST))
		return;

	ghostext[playernum].flags = (ghostext[playernum].flags & ~EZT_THOKMASK) | EZT_SPIN;
}

void G_GhostAddRev(INT32 playernum)
{
	if (!demo.recording || !(demoflags & DF_GHOST))
		return;

	ghostext[playernum].flags = (ghostext[playernum].flags & ~EZT_THOKMASK) | EZT_REV;
}

void G_GhostAddFlip(INT32 playernum)
{
	if (!demo.recording || !(demoflags & DF_GHOST))
		return;

	ghostext[playernum].flags |= EZT_FLIP;
}

void G_GhostAddColor(INT32 playernum, ghostcolor_t color)
{
	if (!demo.recording || !(demoflags & DF_GHOST))
		return;

	if (ghostext[playernum].lastcolor == (UINT8)color)
	{
		ghostext[playernum].flags &= ~EZT_COLOR;
		return;
	}

	ghostext[playernum].flags |= EZT_COLOR;
	ghostext[playernum].color = (UINT8)color;
}

void G_GhostAddScale(INT32 playernum, fixed_t scale)
{
	if (!demo.recording || !(demoflags & DF_GHOST))
		return;

	if (ghostext[playernum].lastscale == scale)
	{
		ghostext[playernum].flags &= ~EZT_SCALE;
		return;
	}

	ghostext[playernum].flags |= EZT_SCALE;
	ghostext[playernum].scale = scale;
}

void G_GhostAddHit(INT32 playernum, mobj_t *victim)
{
	if (!demo.recording || !(demoflags & DF_GHOST))
		return;

	ghostext[playernum].flags |= EZT_HIT;
	ghostext[playernum].hits++;
	ghostext[playernum].hitlist = Z_Realloc(ghostext[playernum].hitlist, ghostext[playernum].hits * sizeof(mobj_t *), PU_LEVEL, &ghostext[playernum].hitlist);
	P_SetTarget(ghostext[playernum].hitlist + (ghostext[playernum].hits-1), victim);
}

void G_WriteAllGhostTics(void)
{
	if (!demobuf.p)
		return;

	UINT8 *save_demo_p = demobuf.p;
#define CHECKSPACE(num) if (demobuf.p+(num) > demoend) { demobuf.p = save_demo_p; G_CheckDemoStatus(); return; }

	boolean toobig = false;
	INT32 i, counter = leveltime;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (P_MobjWasRemoved(players[i].mo))
			continue;

		counter++;

		if (multiplayer && ((counter % cv_netdemosyncquality.value) != 0)) // Only write 1 in this many ghost datas per tic to cut down on multiplayer replay size.
			continue;

		CHECKSPACE(1);

		WRITEUINT8(demobuf.p, i);
		G_WriteGhostTic(players[i].mo, i);

		// attention here for the ticcmd size!
		// latest demos with mouse aiming byte in ticcmd
		if (demobuf.p >= demoend - (13 + 9 + 9))
		{
			toobig = true;
			break;
		}
	}

	CHECKSPACE(1);
	WRITEUINT8(demobuf.p, 0xFF);

	if (toobig)
	{
		G_CheckDemoStatus(); // no more space
		return;
	}

#undef CHECKSPACE
}

void G_WriteGhostTic(mobj_t *ghost, INT32 playernum)
{
	char ziptic = 0;
	UINT8 *ziptic_p;
	UINT32 i;
	UINT8 sprite;
	UINT8 frame;

	if (!demobuf.p)
		return;

	UINT8 *save_demo_p = demobuf.p;
#define CHECKSPACE(num) if (demobuf.p+(num) > demoend) { demobuf.p = save_demo_p; G_CheckDemoStatus(); return; }

	if (!(demoflags & DF_GHOST))
		return; // No ghost data to write.

	if (ghost->player && ghost->player->pflags & PF_NIGHTSMODE && ghost->tracer)
	{
		// We're talking about the NiGHTS thing, not the normal platforming thing!
		ziptic |= GZT_NIGHTS;
		ghost = ghost->tracer;
	}

	ziptic_p = demobuf.p++; // the ziptic, written at the end of this function

#define MAXMOM (0x7FFF<<8)

	// GZT_XYZ is only useful if you've moved 256 FRACUNITS or more in a single tic.
	if (abs(ghost->x-oldghost[playernum].x) > MAXMOM
	|| abs(ghost->y-oldghost[playernum].y) > MAXMOM
	|| abs(ghost->z-oldghost[playernum].z) > MAXMOM
	|| ((UINT8)(leveltime & 255) > 0 && (UINT8)(leveltime & 255) <= (UINT8)cv_netdemosyncquality.value)) // Hack to enable slightly nicer resyncing
	{
		oldghost[playernum].x = ghost->x;
		oldghost[playernum].y = ghost->y;
		oldghost[playernum].z = ghost->z;
		ziptic |= GZT_XYZ;

		CHECKSPACE(sizeof(fixed_t)*3);
		WRITEFIXED(demobuf.p, oldghost[playernum].x);
		WRITEFIXED(demobuf.p, oldghost[playernum].y);
		WRITEFIXED(demobuf.p, oldghost[playernum].z);
	}
	else
	{
		// For moving normally:
		// Store one full byte of movement, plus one byte of fractional movement.
		INT16 momx = (INT16)((ghost->x-oldghost[playernum].x + (1<<4))>>8);
		INT16 momy = (INT16)((ghost->y-oldghost[playernum].y + (1<<4))>>8);

		if (momx != oldghost[playernum].momx
		|| momy != oldghost[playernum].momy)
		{
			oldghost[playernum].momx = momx;
			oldghost[playernum].momy = momy;
			ziptic |= GZT_MOMXY;

			CHECKSPACE(4);

			WRITEINT16(demobuf.p, momx);
			WRITEINT16(demobuf.p, momy);
		}

		momx = (INT16)((ghost->z-oldghost[playernum].z + (1<<4))>>8);

		if (momx != oldghost[playernum].momz)
		{
			oldghost[playernum].momz = momx;
			ziptic |= GZT_MOMZ;

			CHECKSPACE(2);

			WRITEINT16(demobuf.p, momx);
		}

		// This SHOULD set oldghost.x/y/z to match ghost->x/y/z
		// but it keeps the fractional loss of one byte,
		// so it will hopefully be made up for in future tics.
		oldghost[playernum].x += oldghost[playernum].momx<<8;
		oldghost[playernum].y += oldghost[playernum].momy<<8;
		oldghost[playernum].z += oldghost[playernum].momz<<8;
	}

#undef MAXMOM

	// Only store the 8 most relevant bits of angle
	// because exact values aren't too easy to discern to begin with when only 8 angles have different sprites
	// and it does not affect this mode of movement at all anyway.
	if (ghost->angle>>24 != oldghost[playernum].angle)
	{
		oldghost[playernum].angle = ghost->angle>>24;
		ziptic |= GZT_ANGLE;

		CHECKSPACE(1);

		WRITEUINT8(demobuf.p, oldghost[playernum].angle);
	}

	// Store the sprite frame.
	frame = ghost->frame & 0xFF;
	if (frame != oldghost[playernum].frame)
	{
		oldghost[playernum].frame = frame;
		ziptic |= GZT_SPRITE;

		CHECKSPACE(1);

		WRITEUINT8(demobuf.p, oldghost[playernum].frame);
	}

	// Check for sprite set changes
	sprite = ghost->sprite;
	if (sprite != oldghost[playernum].sprite)
	{
		oldghost[playernum].sprite = sprite;
		ghostext[playernum].flags |= EZT_SPRITE;
	}

	if (ghost->player)
	{
		if (
			ghostext[playernum].kartitem != ghost->player->kartstuff[k_itemtype] ||
			ghostext[playernum].kartamount != ghost->player->kartstuff[k_itemamount] ||
			ghostext[playernum].kartbumpers != ghost->player->kartstuff[k_bumper]
		)
		{
			ghostext[playernum].flags |= EZT_KART;
			ghostext[playernum].kartitem = ghost->player->kartstuff[k_itemtype];
			ghostext[playernum].kartamount = ghost->player->kartstuff[k_itemamount];
			ghostext[playernum].kartbumpers = ghost->player->kartstuff[k_bumper];

		}
	}

	if (ghostext[playernum].color == ghostext[playernum].lastcolor)
		ghostext[playernum].flags &= ~EZT_COLOR;

	if (ghostext[playernum].scale == ghostext[playernum].lastscale)
		ghostext[playernum].flags &= ~EZT_SCALE;

	if (ghostext[playernum].flags)
	{
		ziptic |= GZT_EXTRA;
		WRITEUINT8(demobuf.p, ghostext[playernum].flags);

		if (ghostext[playernum].flags & EZT_COLOR)
		{
			CHECKSPACE(1);
			WRITEUINT8(demobuf.p, ghostext[playernum].color);
			ghostext[playernum].lastcolor = ghostext[playernum].color;
		}

		if (ghostext[playernum].flags & EZT_SCALE)
		{
			CHECKSPACE(sizeof(fixed_t));
			WRITEFIXED(demobuf.p, ghostext[playernum].scale);
			ghostext[playernum].lastscale = ghostext[playernum].scale;
		}

		if (ghostext[playernum].flags & EZT_HIT)
		{
			CHECKSPACE(2);
			WRITEUINT16(demobuf.p, ghostext[playernum].hits);

			for (i = 0; i < ghostext[playernum].hits; i++)
			{
				mobj_t *mo = ghostext[playernum].hitlist[i];

				CHECKSPACE(4+4+2+sizeof(fixed_t)*3+sizeof(angle_t));

				WRITEUINT32(demobuf.p, UINT32_MAX); // reserved for some method of determining exactly which mobj this is. (mobjnum doesn't work here.)
				WRITEUINT32(demobuf.p, mo->type);
				WRITEUINT16(demobuf.p, (UINT16)mo->health);
				WRITEFIXED(demobuf.p, mo->x);
				WRITEFIXED(demobuf.p, mo->y);
				WRITEFIXED(demobuf.p, mo->z);
				WRITEANGLE(demobuf.p, mo->angle);
				P_SetTarget(ghostext[playernum].hitlist+i, NULL);
			}

			Z_Free(ghostext[playernum].hitlist);
			ghostext[playernum].hitlist = NULL;
			ghostext[playernum].hits = 0;
		}

		if (ghostext[playernum].flags & EZT_SPRITE)
		{
			CHECKSPACE(1);
			WRITEUINT8(demobuf.p, sprite);
		}

		if (ghostext[playernum].flags & EZT_KART)
		{
			CHECKSPACE(12);
			WRITEINT32(demobuf.p, ghostext[playernum].kartitem);
			WRITEINT32(demobuf.p, ghostext[playernum].kartamount);
			WRITEINT32(demobuf.p, ghostext[playernum].kartbumpers);
		}

		ghostext[playernum].flags = 0;
	}

	*ziptic_p = ziptic;
#undef CHECKSPACE
}

void G_ConsAllGhostTics(void)
{
	UINT8 p;

	if (!demobuf.p || !demo.deferstart)
		return;

	p = READUINT8(demobuf.p);

	while (p != 0xFF)
	{
		G_ConsGhostTic(p);
		p = READUINT8(demobuf.p);
	}

	if (*demobuf.p == DEMOMARKER)
	{
		// end of demo data stream
		G_CheckDemoStatus();
		return;
	}
}

// Uses ghost data to do consistency checks on your position.
// This fixes desynchronising demos when fighting eggman.
void G_ConsGhostTic(INT32 playernum)
{
	UINT8 ziptic;
	fixed_t px,py,pz,gx,gy,gz;
	mobj_t *testmo;
	fixed_t syncleeway;
	boolean nightsfail = false;

	if (!(demoflags & DF_GHOST))
		return; // No ghost data to use.

	testmo = players[playernum].mo;

	// Grab ghost data.
	ziptic = READUINT8(demobuf.p);

	if (ziptic & GZT_XYZ)
	{
		oldghost[playernum].x = READFIXED(demobuf.p);
		oldghost[playernum].y = READFIXED(demobuf.p);
		oldghost[playernum].z = READFIXED(demobuf.p);
		syncleeway = 0;
	}
	else
	{
		if (ziptic & GZT_MOMXY)
		{
			oldghost[playernum].momx = READINT16(demobuf.p)<<8;
			oldghost[playernum].momy = READINT16(demobuf.p)<<8;
		}

		if (ziptic & GZT_MOMZ)
			oldghost[playernum].momz = READINT16(demobuf.p)<<8;

		oldghost[playernum].x += oldghost[playernum].momx;
		oldghost[playernum].y += oldghost[playernum].momy;
		oldghost[playernum].z += oldghost[playernum].momz;
		syncleeway = FRACUNIT;
	}

	if (ziptic & GZT_ANGLE)
		demobuf.p++;

	if (ziptic & GZT_SPRITE)
		demobuf.p++;

	if (ziptic & GZT_NIGHTS)
	{
		if (!testmo || !testmo->player || !(testmo->player->pflags & PF_NIGHTSMODE) || !testmo->tracer)
			nightsfail = true;
		else
			testmo = testmo->tracer;
	}

	if (ziptic & GZT_EXTRA)
	{ // But wait, there's more!
		ziptic = READUINT8(demobuf.p);

		if (ziptic & EZT_COLOR)
			demobuf.p++;

		if (ziptic & EZT_SCALE)
			demobuf.p += sizeof(fixed_t);

		if (ziptic & EZT_HIT)
		{ // Resync mob damage.
			UINT16 i;
			UINT16 count = READUINT16(demobuf.p);
			thinker_t *th;
			mobj_t *mobj;

			UINT32 type;
			UINT16 health;
			fixed_t x;
			fixed_t y;
			fixed_t z;

			for (i = 0; i < count; i++)
			{
				demobuf.p += 4; // reserved.
				type = READUINT32(demobuf.p);
				health = READUINT16(demobuf.p);
				x = READFIXED(demobuf.p);
				y = READFIXED(demobuf.p);
				z = READFIXED(demobuf.p);
				demobuf.p += sizeof(angle_t); // angle, unnecessary for cons.

				mobj = NULL;
				for (th = thinkercap.next; th != &thinkercap; th = th->next)
				{
					if (th->function != (actionf_p1)P_MobjThinker)
						continue;

					mobj = (mobj_t *)th;

					if (mobj->type == (mobjtype_t)type && mobj->x == x && mobj->y == y && mobj->z == z)
						break;

					mobj = NULL; // wasn't this one, keep searching.
				}

				if (mobj && mobj->health != health) // Wasn't damaged?! This is desync! Fix it!
				{
					if (demosynced)
					{
						CONS_Alert(CONS_WARNING, M_GetText("Demo playback has desynced (health)!\n"));
						CONS_Printf("expected health %d got %d\n", health, mobj->health);
					}

					demosynced = false;
					P_DamageMobj(mobj, players[0].mo, players[0].mo, 1);
				}
			}
		}

		if (ziptic & EZT_SPRITE)
			demobuf.p++;

		if (ziptic & EZT_KART)
		{
			ghostext[playernum].kartitem = READINT32(demobuf.p);
			ghostext[playernum].kartamount = READINT32(demobuf.p);
			ghostext[playernum].kartbumpers = READINT32(demobuf.p);
		}
	}

	if (testmo)
	{
		// Re-synchronise
		px = testmo->x;
		py = testmo->y;
		pz = testmo->z;
		gx = oldghost[playernum].x;
		gy = oldghost[playernum].y;
		gz = oldghost[playernum].z;

		if (nightsfail || abs(px-gx) > syncleeway || abs(py-gy) > syncleeway || abs(pz-gz) > syncleeway)
		{
			ghostext[playernum].desyncframes++;

			if (ghostext[playernum].desyncframes >= 2)
			{
				if (demosynced)
					CONS_Alert(CONS_WARNING, "Demo playback has desynced (player %s)!\n", player_names[playernum]);

				demosynced = false;

				if (cv_resyncdemo.value)
				{
					P_UnsetThingPosition(testmo);
					testmo->x = oldghost[playernum].x;
					testmo->y = oldghost[playernum].y;
					P_SetThingPosition(testmo);
					testmo->z = oldghost[playernum].z;
				}

				if (abs(testmo->z - testmo->floorz) < 4*FRACUNIT)
					testmo->z = testmo->floorz; // Sync players to the ground when they're likely supposed to be there...

				ghostext[playernum].desyncframes = 2;
			}
		}
		else
			ghostext[playernum].desyncframes = 0;

		if (
#ifdef DEMO_COMPAT_100
			demo.version != 0x0001 &&
#endif
			(
				players[playernum].kartstuff[k_itemtype] != ghostext[playernum].kartitem ||
				players[playernum].kartstuff[k_itemamount] != ghostext[playernum].kartamount ||
				players[playernum].kartstuff[k_bumper] != ghostext[playernum].kartbumpers
			)
		)
		{
			if (demosynced)
			{
				CONS_Alert(CONS_WARNING, "Demo playback has desynced (item/bumpers)!(player %s)!\n", player_names[playernum]);
				CONS_Printf("expected item type %d got %d\n", ghostext[playernum].kartitem, players[playernum].kartstuff[k_itemtype]);
				CONS_Printf("expected item amount %d got %d\n", ghostext[playernum].kartamount, players[playernum].kartstuff[k_itemamount]);
			}

			demosynced = false;

			players[playernum].kartstuff[k_itemtype] = ghostext[playernum].kartitem;
			players[playernum].kartstuff[k_itemamount] = ghostext[playernum].kartamount;
			players[playernum].kartstuff[k_bumper] = ghostext[playernum].kartbumpers;
		}
	}

	if (*demobuf.p == DEMOMARKER)
	{
		// end of demo data stream
		G_CheckDemoStatus();
		return;
	}
}

void G_GhostTicker(void)
{
	demoghost *g, *p;

	for (g = ghosts, p = NULL; g; g = g->next)
	{
		// Skip normal demo data.
		UINT8 ziptic;

		if (g->done)
		{
			continue;
		}

		ziptic = READUINT8(g->p);

fadeghost:
		// Demo ends after ghost data.
		if (ziptic == DEMOMARKER)
		{
			g->mo->momx = g->mo->momy = g->mo->momz = 0;

			g->done = true;
			if (p)
			{
				p->next = g->next;
			}

			continue;
		}

#ifdef DEMO_COMPAT_100
		if (g->version != 0x0001)
		{
#endif
		while (ziptic != DW_END) // Get rid of extradata stuff
		{
			if (ziptic < MAXPLAYERS)
			{
#ifdef DEVELOP
				UINT8 playerid = ziptic;
#endif
				// We want to skip *any* player extradata because some demos have extradata for bogus players,
				// but if there is tic data later for those players *then* we'll consider it invalid.

				ziptic = READUINT8(g->p);

				if (ziptic & DXD_SKIN)
					g->p += 18; // We _could_ read this info, but it shouldn't change anything in record attack...

				if (ziptic & DXD_COLOR)
					g->p += 16; // Same tbh

				if (ziptic & DXD_NAME)
					g->p += 16; // yea

				if (ziptic & DXD_PLAYSTATE)
				{
					UINT8 playstate = READUINT8(g->p);
					if (playstate != DXD_PST_PLAYING)
					{
#ifdef DEVELOP
						CONS_Alert(CONS_WARNING, "Ghost demo has non-playing playstate for player %d\n", playerid + 1);
#endif
						;
					}
				}
			}
			else if (ziptic == DW_RNG)
			{
				g->p += 4; // RNG seed
			}
			else
			{
				I_Error("Ghost is not a record attack ghost DXD (ziptic = %u)", ziptic); //@TODO lmao don't blow up like this
			}

			ziptic = READUINT8(g->p);
		}

		ziptic = READUINT8(g->p); // Back to actual ziptic stuff
#ifdef DEMO_COMPAT_100
		}
#endif

		if (ziptic & ZT_FWD)
			g->p++;
		if (ziptic & ZT_SIDE)
			g->p++;
		if (ziptic & ZT_ANGLE)
			g->p += 2;
		if (ziptic & ZT_BUTTONS)
			g->p += 2;
		if (ziptic & ZT_AIMING)
			g->p += 2;
		if (ziptic & ZT_DRIFT)
			g->p += 2;
		if (ziptic & ZT_LATENCY)
			g->p++;

		// Grab ghost data.
		ziptic = READUINT8(g->p);

#ifdef DEMO_COMPAT_100
		if (g->version != 0x0001)
		{
#endif
		if (ziptic == DEMOMARKER) // Had to end early for some reason
			goto fadeghost;
		if (ziptic == 0xFF)
			goto skippedghosttic; // Didn't write ghost info this frame
		if (ziptic != 0)
			I_Error("Ghost is not a record attack ghost ZIPTIC"); //@TODO lmao don't blow up like this
		ziptic = READUINT8(g->p);
#ifdef DEMO_COMPAT_100
		}
#endif
		if (ziptic & GZT_XYZ)
		{
			g->oldmo.x = READFIXED(g->p);
			g->oldmo.y = READFIXED(g->p);
			g->oldmo.z = READFIXED(g->p);
		}
		else
		{
			if (ziptic & GZT_MOMXY)
			{
				g->oldmo.momx = READINT16(g->p)<<8;
				g->oldmo.momy = READINT16(g->p)<<8;
			}

			if (ziptic & GZT_MOMZ)
				g->oldmo.momz = READINT16(g->p)<<8;

			g->oldmo.x += g->oldmo.momx;
			g->oldmo.y += g->oldmo.momy;
			g->oldmo.z += g->oldmo.momz;
		}

		if (ziptic & GZT_ANGLE)
			g->oldmo.angle = READUINT8(g->p)<<24;

		if (ziptic & GZT_SPRITE)
			g->oldmo.frame = READUINT8(g->p);

		// Update ghost
		P_UnsetThingPosition(g->mo);
		g->mo->x = g->oldmo.x;
		g->mo->y = g->oldmo.y;
		g->mo->z = g->oldmo.z;
		P_SetThingPosition(g->mo);
		g->mo->angle = g->oldmo.angle;
		g->mo->frame = g->oldmo.frame | tr_trans30<<FF_TRANSSHIFT;

		if (ziptic & GZT_EXTRA)
		{ // But wait, there's more!
			ziptic = READUINT8(g->p);

			if (ziptic & EZT_COLOR)
			{
				g->color = READUINT8(g->p);

				switch (g->color)
				{
					default:
					case GHC_NORMAL: // Go back to skin color
						g->mo->color = g->oldmo.color;
						break;
					// Handled below
					case GHC_SUPER:
					case GHC_INVINCIBLE:
						break;
					case GHC_FIREFLOWER: // Fireflower
						g->mo->color = SKINCOLOR_WHITE;
						break;
				}
			}

			if (ziptic & EZT_FLIP)
				g->mo->eflags ^= MFE_VERTICALFLIP;

			if (ziptic & EZT_SCALE)
			{
				g->mo->destscale = READFIXED(g->p);
				if (g->mo->destscale != g->mo->scale)
					P_SetScale(g->mo, g->mo->destscale);
			}

			if (ziptic & EZT_THOKMASK)
			{ // Let's only spawn ONE of these per frame, thanks.
				mobj_t *mobj;
				INT32 type = -1;

				if (g->mo->skin)
				{
					switch (ziptic & EZT_THOKMASK)
					{
						case EZT_THOK:
							type = (UINT32)mobjinfo[MT_PLAYER].painchance;
							break;
						case EZT_SPIN:
							type = (UINT32)mobjinfo[MT_PLAYER].damage;
							break;
						case EZT_REV:
							type = (UINT32)mobjinfo[MT_PLAYER].raisestate;
							break;
					}
				}
				if (type != -1)
				{
					if (type == MT_GHOST)
					{
						mobj = P_SpawnGhostMobj(g->mo); // does a large portion of the work for us
						mobj->frame = (mobj->frame & ~FF_FRAMEMASK)|tr_trans60<<FF_TRANSSHIFT; // P_SpawnGhostMobj sets trans50, we want trans60
					}
					else
					{
						mobj = P_SpawnMobj(g->mo->x, g->mo->y, g->mo->z - FixedDiv(FixedMul(g->mo->info->height, g->mo->scale) - g->mo->height,3*FRACUNIT), MT_THOK);
						mobj->sprite = states[mobjinfo[type].spawnstate].sprite;
						mobj->frame = (states[mobjinfo[type].spawnstate].frame & FF_FRAMEMASK) | tr_trans60<<FF_TRANSSHIFT;
						mobj->tics = -1; // nope.
						mobj->color = g->mo->color;

						if (g->mo->eflags & MFE_VERTICALFLIP)
						{
							mobj->flags2 |= MF2_OBJECTFLIP;
							mobj->eflags |= MFE_VERTICALFLIP;
						}

						P_SetScale(mobj, g->mo->scale);
						mobj->destscale = g->mo->scale;
					}

					mobj->floorz = mobj->z;
					mobj->ceilingz = mobj->z+mobj->height;
					P_UnsetThingPosition(mobj);
					mobj->flags = MF_NOBLOCKMAP|MF_NOCLIP|MF_NOCLIPHEIGHT|MF_NOGRAVITY; // make an ATTEMPT to curb crazy SOCs fucking stuff up...
					P_SetThingPosition(mobj);
					mobj->fuse = 8;
					P_SetTarget(&mobj->target, g->mo);
				}
			}

			if (ziptic & EZT_HIT)
			{ // Spawn hit poofs for killing things!
				UINT16 i, health;
				UINT16 count = READUINT16(g->p);
				//UINT32 type;
				fixed_t x,y,z;
				angle_t angle;
				mobj_t *poof;

				for (i = 0; i < count; i++)
				{
					g->p += 4; // reserved
					g->p += 4; // backwards compat., type used to be here
					health = READUINT16(g->p);
					x = READFIXED(g->p);
					y = READFIXED(g->p);
					z = READFIXED(g->p);
					angle = READANGLE(g->p);

					if (health != 0 || i >= 4) // only spawn for the first 4 hits per frame, to prevent ghosts from splode-spamming too bad.
						continue;

					poof = P_SpawnMobj(x, y, z, MT_GHOST);
					poof->angle = angle;
					poof->flags = MF_NOBLOCKMAP|MF_NOCLIP|MF_NOCLIPHEIGHT|MF_NOGRAVITY; // make an ATTEMPT to curb crazy SOCs fucking stuff up...
					poof->health = 0;
					P_SetMobjStateNF(poof, S_XPLD1);
				}
			}
			if (ziptic & EZT_SPRITE)
				g->mo->sprite = READUINT8(g->p);
			if (ziptic & EZT_KART)
				g->p += 12; // kartitem, kartamount, kartbumpers
		}

#ifdef DEMO_COMPAT_100
		if (g->version != 0x0001)
		{
#endif
		if (READUINT8(g->p) != 0xFF) // Make sure there isn't other ghost data here.
			I_Error("Ghost is not a record attack ghost GHOSTEND"); //@TODO lmao don't blow up like this
#ifdef DEMO_COMPAT_100
		}
#endif

skippedghosttic:
		// Tick ghost colors (Super and Mario Invincibility flashing)
		switch (g->color)
		{
			case GHC_SUPER: // Super Sonic (P_DoSuperStuff)
				g->mo->color = SKINCOLOR_SUPER1;
				g->mo->color += abs( ( (signed)( (unsigned)leveltime >> 1 ) % 9) - 4);
				break;
			case GHC_INVINCIBLE: // Mario invincibility (P_CheckInvincibilityTimer)
				g->mo->color = (UINT8)(leveltime % MAXSKINCOLORS);
				break;
			default:
				break;
		}

		p = g;
	}
}

// Demo rewinding functions
typedef struct rewindinfo_s {
	tic_t leveltime;

	struct {
		boolean ingame;
		player_t player;
		mobj_t mobj;
	} playerinfo[MAXPLAYERS];

	struct rewindinfo_s *prev;
} rewindinfo_t;

static tic_t currentrewindnum;
static rewindinfo_t *rewindhead = NULL; // Reverse chronological order

void G_InitDemoRewind(void)
{
	CL_ClearRewinds();

	while (rewindhead)
	{
		rewindinfo_t *p = rewindhead->prev;
		Z_Free(rewindhead);
		rewindhead = p;
	}

	currentrewindnum = 0;
}

void G_StoreRewindInfo(void)
{
	static UINT8 timetolog = 8;
	rewindinfo_t *info;
	size_t i;

	if (timetolog-- > 0)
		return;

	timetolog = 8;

	info = Z_Calloc(sizeof(rewindinfo_t), PU_STATIC, NULL);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
		{
			info->playerinfo[i].ingame = false;
			continue;
		}

		info->playerinfo[i].ingame = true;
		memcpy(&info->playerinfo[i].player, &players[i], sizeof(player_t));

		if (players[i].mo)
			memcpy(&info->playerinfo[i].mobj, players[i].mo, sizeof(mobj_t));
	}

	info->leveltime = leveltime;
	info->prev = rewindhead;
	rewindhead = info;
}

void G_PreviewRewind(tic_t previewtime)
{
	SINT8 i;
	size_t j;
	fixed_t tweenvalue = 0;
	rewindinfo_t *info = rewindhead, *next_info = rewindhead;

	if (!info)
		return;

	while (info->leveltime > previewtime && info->prev)
	{
		next_info = info;
		info = info->prev;
	}

	if (info != next_info)
		tweenvalue = FixedDiv(previewtime - info->leveltime, next_info->leveltime - info->leveltime);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
		{
			if (info->playerinfo[i].player.mo)
			{
				//@TODO spawn temp object to act as a player display
			}

			continue;
		}

		if (!info->playerinfo[i].ingame || !info->playerinfo[i].player.mo)
		{
			if (players[i].mo)
				players[i].mo->flags2 |= MF2_DONTDRAW;

			continue;
		}

		if (!players[i].mo)
			continue; //@TODO spawn temp object to act as a player display

		players[i].mo->flags2 &= ~MF2_DONTDRAW;

		P_UnsetThingPosition(players[i].mo);
#define TWEEN(pr) info->playerinfo[i].mobj.pr + FixedMul((INT32) (next_info->playerinfo[i].mobj.pr - info->playerinfo[i].mobj.pr), tweenvalue)
		players[i].mo->x = TWEEN(x);
		players[i].mo->y = TWEEN(y);
		players[i].mo->z = TWEEN(z);
		players[i].mo->angle = TWEEN(angle);
#undef TWEEN
		P_SetThingPosition(players[i].mo);

		players[i].frameangle = info->playerinfo[i].player.frameangle + FixedMul((INT32) (next_info->playerinfo[i].player.frameangle - info->playerinfo[i].player.frameangle), tweenvalue);

		players[i].mo->sprite = info->playerinfo[i].mobj.sprite;
		players[i].mo->frame = info->playerinfo[i].mobj.frame;

		players[i].realtime = info->playerinfo[i].player.realtime;

		for (j = 0; j < NUMKARTSTUFF; j++)
			players[i].kartstuff[j] = info->playerinfo[i].player.kartstuff[j];
	}

	for (i = splitscreen; i >= 0; i--)
		P_ResetCamera(&players[displayplayers[i]], &camera[i]);
}

void G_ConfirmRewind(tic_t rewindtime)
{
	SINT8 i;
	tic_t j;
	boolean oldmenuactive = menuactive, oldsounddisabled = sound_disabled;

	INT32 olddp1 = displayplayers[0], olddp2 = displayplayers[1], olddp3 = displayplayers[2], olddp4 = displayplayers[3];
	UINT8 oldss = splitscreen;

	menuactive = false; // Prevent loops

	CV_StealthSetValue(&cv_renderview, 0);

	if (rewindtime <= starttime)
	{
		demo.rewinding = false;
		G_DoPlayDemo(NULL); // Restart the current demo
	}
	else
	{
		rewind_t *rewind;
		sound_disabled = true; // Prevent sound spam
		demo.rewinding = true;

		rewind = CL_RewindToTime(rewindtime);

		if (rewind)
		{
			demobuf.p = demobuf.buffer + rewind->demopos;
			memcpy(oldcmd, rewind->oldcmd, sizeof (oldcmd));
			memcpy(oldghost, rewind->oldghost, sizeof (oldghost));
			paused = false;
		}
		else
		{
			demo.rewinding = true;
			G_DoPlayDemo(NULL); // Restart the current demo
		}
	}

	for (j = 0; j < rewindtime && leveltime < rewindtime; j++)
	{
		G_Ticker((j % NEWTICRATERATIO) == 0);
	}

	demo.rewinding = false;
	menuactive = oldmenuactive; // Bring the menu back up
	sound_disabled = oldsounddisabled; // Re-enable SFX

	wipegamestate = gamestate; // No fading back in!

	COM_BufInsertText("renderview on\n");

	splitscreen = oldss;
	displayplayers[0] = olddp1;
	displayplayers[1] = olddp2;
	displayplayers[2] = olddp3;
	displayplayers[3] = olddp4;
	R_ExecuteSetViewSize();
	G_ResetViews(true);

	for (i = splitscreen; i >= 0; i--)
		P_ResetCamera(&players[displayplayers[i]], &camera[i]);
}

//
// G_RecordDemo
//
void G_RecordDemo(const char *name)
{
	INT32 maxsize;

	demobuf.p = NULL;
	G_ResetDemoRecording();
	demoend = NULL;

	if (cv_recordmultiplayerdemos.value)
	{
		CONS_Printf("Recording demo %s.lmp\n", name);

		strcpy(demoname, name);
		strcat(demoname, ".lmp");

		maxsize = cv_maxdemosize.value*1024*1024;

		demobuf.buffer = Z_Malloc(maxsize + 100*1024, PU_STATIC, NULL);
		demoend = demobuf.buffer + maxsize;

		if (demobuf.buffer)
			demo.recording = true;
		else
			CONS_Alert(CONS_ERROR, "Failed to allocate demo buffer\n");
	}
}

void G_BeginRecording(void)
{
	UINT8 i, p;
	char name[17];
	player_t *player = &players[consoleplayer];

	char *filename;
	UINT8 totalfiles;
	UINT8 *m;

	if (!cv_recordmultiplayerdemos.value)
	{
		G_ResetDemoRecording();
		return;
	}

	if (demobuf.buffer == NULL)
	{
		CONS_Alert(CONS_ERROR, "No demo buffer allocated\n");
		G_ResetDemoRecording();
		return;
	}

	if (demobuf.p)
	{
		G_ResetDemoRecording();
		return;
	}

	memset(name,0,sizeof(name));

	demobuf.p = demobuf.buffer;
	demoflags = DF_GHOST|(multiplayer ? DF_MULTIPLAYER : (modeattacking<<DF_ATTACKSHIFT));

	if (encoremode)
		demoflags |= DF_ENCORE;

	if (!modeattacking && gL)	// Ghosts don't read luavars, and you shouldn't ever need to save Lua in replays, you doof!
								// SERIOUSLY THOUGH WHY WOULD YOU LOAD HOSTMOD AND RECORD A GHOST WITH IT !????
		demoflags |= DF_LUAVARS;

	// Setup header.
	memcpy(demobuf.p, DEMOHEADER, 12); demobuf.p += 12;
	WRITEUINT8(demobuf.p,VERSION);
	WRITEUINT8(demobuf.p,SUBVERSION);
	WRITEUINT16(demobuf.p,DEMOVERSION);

	// Full replay title
	demobuf.p += 64;
	{
		char demotitlename[65];
		char *title = G_BuildMapTitle(gamemap);

		// Print to a separate temp buffer instead of demo.titlename, so we can use it in M_TextInputSetString
		if (title)
		{
			snprintf(demotitlename, 64, "%s - %s", title, modeattacking ? "Time Attack" : connectedservername);
			Z_Free(title);
		}
		else
			snprintf(demotitlename, 64, "%s", modeattacking ? "Time Attack" : connectedservername);

		// Init just in case it isn't initialized already
		M_TextInputInit(&demo.titlenameinput, demo.titlename, sizeof(demo.titlename));

		// This will indirectly assign to demo.titlename too
		M_TextInputSetString(&demo.titlenameinput, demotitlename);
	}

	// demo checksum
	demobuf.p += 16;

	// game data
	memcpy(demobuf.p, "PLAY", 4); demobuf.p += 4;
	WRITEINT16(demobuf.p,gamemap);
	memcpy(demobuf.p, mapmd5, 16); demobuf.p += 16;

	WRITEUINT8(demobuf.p, demoflags);
	WRITEUINT8(demobuf.p, gametype & 0xFF);

	// file list
	m = demobuf.p;/* file count */
	demobuf.p += 1;

	totalfiles = 0;

	for (i = mainwads; ++i < numwadfiles;)
	{
		if (!wadfiles[i]->important)
			continue;

		nameonly((filename = va("%s", wadfiles[i]->filename)));
		WRITESTRINGL(demobuf.p, filename, MAX_WADPATH);
		WRITEMEM(demobuf.p, wadfiles[i]->md5sum, 16);

		totalfiles++;
	}

	WRITEUINT8(m, totalfiles);

	switch ((demoflags & DF_ATTACKMASK) >> DF_ATTACKSHIFT)
	{
		case ATTACKING_NONE: // 0
			break;
		case ATTACKING_RECORD: // 1
			demotime_p = demobuf.p;
			WRITEUINT32(demobuf.p,UINT32_MAX); // time
			WRITEUINT32(demobuf.p,UINT32_MAX); // lap
			break;
		default: // 3
			break;
	}

	WRITEUINT32(demobuf.p, P_GetInitSeed());

	// Reserved for extrainfo location from start of file
	demoinfo_p = demobuf.p;
	WRITEUINT32(demobuf.p, 0);

	// Save netvars
	CV_SaveNetVars(&demobuf.p, true);

	// Now store some info for each in-game player
	for (p = 0; p < MAXPLAYERS; p++)
	{
		if (!playeringame[p])
			continue;

		player = &players[p];

		WRITEUINT8(demobuf.p, p | (player->spectator ? DEMO_SPECTATOR : 0));

		// Name
		memset(name, 0, 16);
		memcpy(name, player_names[p], 15);
		memcpy(demobuf.p, name, 16);
		demobuf.p += 16;

		// Skin
		memset(name, 0, 16);
		strncpy(name, skins[player->skin].name, 16);
		memcpy(demobuf.p, name, 16);
		demobuf.p += 16;

		// Color
		memset(name, 0, 16);
		strncpy(name, KartColor_Names[player->skincolor], 16);
		memcpy(demobuf.p, name, 16);
		demobuf.p += 16;

		// Score, since Kart uses this to determine where you start on the map
		WRITEUINT32(demobuf.p, player->score);

		// Kart speed and weight
		WRITEUINT8(demobuf.p, skins[player->skin].kartspeed);
		WRITEUINT8(demobuf.p, skins[player->skin].kartweight);
	}

	WRITEUINT8(demobuf.p, 0xFF); // Denote the end of the player listing

	// player lua vars, always saved even if empty... Unless it's record attack.
	if (demoflags & DF_LUAVARS)
		LUA_Archive(&demobuf, false);

	memset(&oldcmd,0,sizeof(oldcmd));
	memset(&oldghost,0,sizeof(oldghost));
	memset(&ghostext,0,sizeof(ghostext));

	for (i = 0; i < MAXPLAYERS; i++)
	{
		ghostext[i].lastcolor = ghostext[i].color = GHC_NORMAL;
		ghostext[i].lastscale = ghostext[i].scale = FRACUNIT;

		if (players[i].mo)
		{
			oldghost[i].x = players[i].mo->x;
			oldghost[i].y = players[i].mo->y;
			oldghost[i].z = players[i].mo->z;
			oldghost[i].angle = players[i].mo->angle;

			// preticker started us gravity flipped
			if (players[i].mo->eflags & MFE_VERTICALFLIP)
				ghostext[i].flags |= EZT_FLIP;
		}
	}
}

void G_WriteStanding(UINT8 ranking, char *name, INT32 skinnum, UINT8 color, UINT32 val)
{
	char temp[17];

	if (!demobuf.p)
		return;

	if (demoinfo_p && *(UINT32 *)demoinfo_p == 0 && demobuf.buffer != NULL)
	{
		WRITEUINT8(demobuf.p, DEMOMARKER); // add the demo end marker
		*(UINT32 *)demoinfo_p = demobuf.p - demobuf.buffer;
	}

	WRITEUINT8(demobuf.p, DW_STANDING);
	WRITEUINT8(demobuf.p, ranking);

	// Name
	memset(temp, 0, 16);
	strncpy(temp, name, 16);
	memcpy(demobuf.p,temp,16);
	demobuf.p += 16;

	// Skin
	memset(temp, 0, 16);
	strncpy(temp, skins[skinnum].name, 16);
	memcpy(demobuf.p,temp,16);
	demobuf.p += 16;

	// Color
	memset(temp, 0, 16);
	strncpy(temp, KartColor_Names[color], 16);
	memcpy(demobuf.p,temp,16);
	demobuf.p += 16;

	// Score/time/whatever
	WRITEUINT32(demobuf.p, val);
}

void G_SetDemoTime(UINT32 ptime, UINT32 plap)
{
	if (!demo.recording || !demotime_p)
		return;

	if (demoflags & DF_RECORDATTACK)
	{
		WRITEUINT32(demotime_p, ptime);
		WRITEUINT32(demotime_p, plap);
		demotime_p = NULL;
	}
}

static void G_LoadDemoExtraFiles(UINT8 **pp)
{
	UINT8 totalfiles;
	char filename[MAX_WADPATH];
	UINT8 md5sum[16];
	filestatus_t ncs = FS_NOTFOUND;
	boolean toomany = false;
	boolean alreadyloaded;
	UINT8 i, j;

	totalfiles = READUINT8((*pp)); // i like pp uwu

	for (i = 0; i < totalfiles; ++i)
	{
		if (toomany)
			SKIPSTRING((*pp));
		else
		{
			strlcpy(filename, (char *)(*pp), sizeof filename);
			SKIPSTRING((*pp));
		}

		READMEM((*pp), md5sum, 16);

		if (!toomany)
		{
			alreadyloaded = false;

			for (j = 0; j < numwadfiles; ++j)
			{
				if (memcmp(md5sum, wadfiles[j]->md5sum, 16) == 0)
				{
					alreadyloaded = true;
					break;
				}
			}

			if (alreadyloaded)
				continue;

			if (numwadfiles >= MAX_WADFILES)
				toomany = true;
			else
				ncs = findfile(filename, md5sum, false);

			if (toomany)
			{
				CONS_Alert(CONS_WARNING, M_GetText("Too many files loaded to add anymore for demo playback\n"));
				if (!CON_Ready())
					M_StartMessage(M_GetText("There are too many files loaded to add this demo's addons.\n\nDemo playback may desync.\n\nPress ESC\n"), NULL, MM_NOTHING);
			}
			else if (ncs != FS_FOUND)
			{
				if (ncs == FS_NOTFOUND)
					CONS_Alert(CONS_NOTICE, M_GetText("You do not have a copy of %s\n"), filename);
				else if (ncs == FS_MD5SUMBAD)
					CONS_Alert(CONS_NOTICE, M_GetText("Checksum mismatch on %s\n"), filename);
				else
					CONS_Alert(CONS_NOTICE, M_GetText("Unknown error finding file %s\n"), filename);

				if (!CON_Ready())
					M_StartMessage(M_GetText("There were errors trying to add this demo's addons. Check the console for more information.\n\nDemo playback may desync.\n\nPress ESC\n"), NULL, MM_NOTHING);
			}
			else
			{
				P_PartialAddWadFile(filename, false);
			}
		}
	}

	if (P_PartialAddGetStage() >= 0)
		P_MultiSetupWadFiles(true); // in case any partial adds were done
}

static void G_SkipDemoExtraFiles(UINT8 **pp)
{
	UINT8 totalfiles;
	UINT8 i;

	totalfiles = READUINT8((*pp));
	for (i = 0; i < totalfiles; ++i)
	{
		SKIPSTRING((*pp));// file name
		(*pp) += 16;// md5
	}
}

// G_CheckDemoExtraFiles: checks if our loaded WAD list matches the demo's.
// Enabling quick prevents filesystem checks to see if needed files are available to load.
static UINT8 G_CheckDemoExtraFiles(UINT8 **pp, boolean quick)
{
	UINT8 totalfiles, filesloaded, nmusfilecount;
	char filename[MAX_WADPATH];
	UINT8 md5sum[16];
	boolean toomany = false;
	boolean alreadyloaded;
	UINT8 i, j;
	UINT8 error = 0;

	totalfiles = READUINT8((*pp));
	filesloaded = 0;

	for (i = 0; i < totalfiles; ++i)
	{
		if (toomany)
			SKIPSTRING((*pp));
		else
		{
			strlcpy(filename, (char *)(*pp), sizeof filename);
			SKIPSTRING((*pp));
		}

		READMEM((*pp), md5sum, 16); // hue hue peepee

		if (!toomany)
		{
			alreadyloaded = false;
			nmusfilecount = 0;

			for (j = 0; j < numwadfiles; ++j)
			{
				if (wadfiles[j]->important && j > mainwads)
					nmusfilecount++;
				else
					continue;

				if (memcmp(md5sum, wadfiles[j]->md5sum, 16) == 0)
				{
					alreadyloaded = true;

					if (i != nmusfilecount-1 && error < DFILE_ERROR_OUTOFORDER)
						error |= DFILE_ERROR_OUTOFORDER;

					break;
				}
			}

			if (alreadyloaded)
			{
				filesloaded++;
				continue;
			}

			if (numwadfiles >= MAX_WADFILES)
				error = DFILE_ERROR_CANNOTLOAD;
			else if (!quick && findfile(filename, md5sum, false) != FS_FOUND)
				error = DFILE_ERROR_CANNOTLOAD;
			else if (error < DFILE_ERROR_INCOMPLETEOUTOFORDER)
				error |= DFILE_ERROR_NOTLOADED;
		} else
			error = DFILE_ERROR_CANNOTLOAD;
	}

	// Get final file count
	nmusfilecount = 0;

	for (j = 0; j < numwadfiles; ++j)
	{
		if (wadfiles[j]->important && j > mainwads)
			nmusfilecount++;
	}

	if (!error && filesloaded < nmusfilecount)
		error = DFILE_ERROR_EXTRAFILES;

	return error;
}

// Returns bitfield:
// 1 == new demo has lower time
// 2 == new demo has higher score
// 4 == new demo has higher rings
UINT8 G_CmpDemoTime(char *oldname, char *newname)
{
	CLEANUP(Z_Pfree) UINT8 *buffer = NULL;
	UINT8 *p;
	UINT8 flags;
	UINT32 oldtime, newtime, oldlap, newlap;
	UINT16 oldversion;
	size_t bufsize ATTRUNUSED;
	UINT8 c;
	UINT16 s ATTRUNUSED;
	UINT8 aflags = 0;

	// load the new file
	FIL_DefaultExtension(newname, ".lmp");
	bufsize = FIL_ReadFile(newname, &buffer);
	I_Assert(bufsize != 0);
	p = buffer;

	// read demo header
	I_Assert(!memcmp(p, DEMOHEADER, 12));
	p += 12; // DEMOHEADER
	c = READUINT8(p); // VERSION
	I_Assert(c == VERSION);
	c = READUINT8(p); // SUBVERSION
	I_Assert(c == SUBVERSION);
	s = READUINT16(p);
	I_Assert(s == DEMOVERSION);
	p += 64; // full demo title
	p += 16; // demo checksum
	I_Assert(!memcmp(p, "PLAY", 4));
	p += 4; // PLAY
	p += 2; // gamemap
	p += 16; // map md5
	flags = READUINT8(p); // demoflags
	p++; // gametype

	G_SkipDemoExtraFiles(&p);

	aflags = flags & (DF_RECORDATTACK|DF_NIGHTSATTACK);
	I_Assert(aflags);

	if (flags & DF_RECORDATTACK)
	{
		newtime = READUINT32(p);
		newlap = READUINT32(p);
	}
	else // appease compiler
		return 0;

	Z_Free(buffer);
	buffer = NULL;

	// load old file
	FIL_DefaultExtension(oldname, ".lmp");

	if (!FIL_ReadFile(oldname, &buffer))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Failed to read file '%s'.\n"), oldname);
		return UINT8_MAX;
	}

	p = buffer;

	// read demo header
	if (memcmp(p, DEMOHEADER, 12))
	{
		CONS_Alert(CONS_NOTICE, M_GetText("File '%s' invalid format. It will be overwritten.\n"), oldname);
		return UINT8_MAX;
	}

	p += 12; // DEMOHEADER
	p++; // VERSION
	p++; // SUBVERSION
	oldversion = READUINT16(p);

	switch (oldversion) // demoversion
	{
		case DEMOVERSION: // latest always supported
			p += 64; // full demo title
			break;
	#ifdef DEMO_COMPAT_100
		case 0x0001:
			// Old replays gotta go :]
			CONS_Alert(CONS_NOTICE, M_GetText("File '%s' outdated version. It will be overwritten. Nyeheheh.\n"), oldname);
			return UINT8_MAX;
#endif
		// too old, cannot support.
		default:
			CONS_Alert(CONS_NOTICE, M_GetText("File '%s' invalid format. It will be overwritten.\n"), oldname);
			return UINT8_MAX;
	}

	p += 16; // demo checksum

	if (memcmp(p, "PLAY", 4))
	{
		CONS_Alert(CONS_NOTICE, M_GetText("File '%s' invalid format. It will be overwritten.\n"), oldname);
		return UINT8_MAX;
	}

	p += 4; // "PLAY"
	p += 2; // gamemap
	p += 16; // mapmd5
	flags = READUINT8(p);
	p++; // gametype
	G_SkipDemoExtraFiles(&p);

	if (!(flags & aflags))
	{
		CONS_Alert(CONS_NOTICE, M_GetText("File '%s' not from same game mode. It will be overwritten.\n"), oldname);
		return UINT8_MAX;
	}

	if (flags & DF_RECORDATTACK)
	{
		oldtime = READUINT32(p);
		oldlap = READUINT32(p);
	}
	else // appease compiler
		return UINT8_MAX;

	c = 0;

	if (newtime < oldtime
	|| (newtime == oldtime && (newlap < oldlap)))
		c |= 1; // Better time

	if (newlap < oldlap
	|| (newlap == oldlap && newtime < oldtime))
		c |= 1<<1; // Better lap time

	return c;
}

void G_LoadDemoInfo(menudemo_t *pdemo)
{
	UINT8 *info_p, *extrainfo_p;
	CLEANUP(Z_Pfree) UINT8 *infobuffer = NULL;
	UINT8 version, subversion, pdemoflags;
	UINT16 pdemoversion, count;

	if (!FIL_ReadFile(pdemo->filepath, &infobuffer))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Failed to read file '%s'.\n"), pdemo->filepath);
		pdemo->type = MD_INVALID;
		sprintf(pdemo->title, "INVALID REPLAY");
		return;
	}

	info_p = infobuffer;

	if (memcmp(info_p, DEMOHEADER, 12))
	{
		CONS_Alert(CONS_ERROR, M_GetText("%s is not a SRB2Kart replay file.\n"), pdemo->filepath);
		pdemo->type = MD_INVALID;
		sprintf(pdemo->title, "INVALID REPLAY");
		return;
	}

	pdemo->type = MD_LOADED;

	info_p += 12; // DEMOHEADER

	version = READUINT8(info_p);
	subversion = READUINT8(info_p);
	pdemoversion = READUINT16(info_p);

	memset(pdemo->version, 0, sizeof(pdemo->version));
	snprintf(pdemo->version, sizeof(pdemo->version), "v%d.%d", version, subversion);

	switch (pdemoversion)
	{
		case DEMOVERSION: // latest always supported
			// demo title
			memcpy(pdemo->title, info_p, 64);
			info_p += 64;
			break;
#ifdef DEMO_COMPAT_100
		case 0x0001:
			pdemo->type = MD_OUTDATED;
			sprintf(pdemo->title, "Legacy Replay");
			break;
#endif
		// too old, cannot support.
		default:
			CONS_Alert(CONS_ERROR, M_GetText("%s is an incompatible replay format and cannot be played.\n"), pdemo->filepath);
			pdemo->type = MD_INVALID;
			sprintf(pdemo->title, "INVALID REPLAY");
			return;
	}

	if (version != VERSION || subversion != SUBVERSION)
		pdemo->type = MD_OUTDATED;

	info_p += 16; // demo checksum

	if (memcmp(info_p, "PLAY", 4))
	{
		CONS_Alert(CONS_ERROR, M_GetText("%s is the wrong type of recording and cannot be played.\n"), pdemo->filepath);
		pdemo->type = MD_INVALID;
		sprintf(pdemo->title, "INVALID REPLAY");
		return;
	}

	info_p += 4; // "PLAY"
	pdemo->map = READINT16(info_p);
	info_p += 16; // mapmd5

	pdemoflags = READUINT8(info_p);

	// temp?
	if (!(pdemoflags & DF_MULTIPLAYER))
	{
		CONS_Alert(CONS_ERROR, M_GetText("%s is not a multiplayer replay and can't be listed on this menu fully yet.\n"), pdemo->filepath);
		return;
	}
#ifdef DEMO_COMPAT_100
	else if (pdemoversion == 0x0001)
	{
		CONS_Alert(CONS_ERROR, M_GetText("%s is a legacy multiplayer replay and cannot be played.\n"), pdemo->filepath);
		pdemo->type = MD_INVALID;
		sprintf(pdemo->title, "INVALID REPLAY");
		return;
	}
#endif

	pdemo->gametype = READUINT8(info_p);

	pdemo->addonstatus = G_CheckDemoExtraFiles(&info_p, true);
	info_p += 4; // RNG seed

	extrainfo_p = infobuffer + READUINT32(info_p);

	// Pared down version of CV_LoadNetVars to find the kart speed
	pdemo->kartspeed = 1; // Default to normal speed
	count = READUINT16(info_p);

	while (count--)
	{
		UINT16 netid;
		char *svalue;

		netid = READUINT16(info_p);
		svalue = (char *)info_p;
		SKIPSTRING(info_p);
		info_p++; // stealth

		if (netid == cv_kartspeed.netid)
		{
			for (UINT8 j = 0; kartspeed_cons_t[j].strvalue; j++)
			{
				if (fasticmp(kartspeed_cons_t[j].strvalue, svalue))
					pdemo->kartspeed = kartspeed_cons_t[j].value;
			}
		}
		else if (netid == cv_basenumlaps.netid && pdemo->gametype == GT_RACE)
			pdemo->numlaps = atoi(svalue);
	}

	if (pdemoflags & DF_ENCORE)
		pdemo->kartspeed |= DF_ENCORE;

	// Read standings!
	count = 0;

	while (READUINT8(extrainfo_p) == DW_STANDING) // Assume standings are always first in the extrainfo
	{
		INT32 i;
		char temp[16];

		pdemo->standings[count].ranking = READUINT8(extrainfo_p);

		// Name
		memcpy(pdemo->standings[count].name, extrainfo_p, 16);
		extrainfo_p += 16;

		// Skin
		memcpy(temp,extrainfo_p,16);
		extrainfo_p += 16;
		pdemo->standings[count].skin = UINT8_MAX;

		for (i = 0; i < numskins; i++)
		{
			if (fasticmp(skins[i].name, temp))
			{
				pdemo->standings[count].skin = i;
				break;
			}
		}

		// Color
		memcpy(temp,extrainfo_p,16);
		extrainfo_p += 16;

		for (i = 0; i < MAXSKINCOLORS; i++)
		{
			if (fasticmp(KartColor_Names[i],temp)) // SRB2kart
			{
				pdemo->standings[count].color = i;
				break;
			}
		}

		// Score/time/whatever
		pdemo->standings[count].timeorscore = READUINT32(extrainfo_p);

		count++;

		if (count >= MAXPLAYERS)
			break; //@TODO still cycle through the rest of these if extra demo data is ever used
	}

	// I think that's everything we need?
}

#if defined (_WIN32)
// return the file creation time
// useful for demos that were renamed
static long G_GetCreationTime(char *filepath)
{
	struct stat fileinfo;

	if (stat(filepath, &fileinfo) == 0)
		return fileinfo.st_ctime;

	return 0;
}
#endif

static char *G_GetDemoDate(menudemo_t *pdemo)
{
	char *datetime;
	datetime = malloc(sizeof(pdemo->date)); // mallocma balls

	// no mallocma balls... :c
	if (!datetime)
	{
		return NULL;
	}

	time_t file_time = 0;

	// get le filepath
	char *filename;
	filename = strdup(pdemo->filepath);

#if defined (_WIN32)
	if (!filename)
	{
		// if we cant get a filename try just getting the file create time
		file_time = G_GetCreationTime(pdemo->filepath);
		goto skipfilenametime;
	}
#else
	if (!filename)
	{
		free(datetime);
		return NULL;
	}
#endif

	// get the actual filename Zzz...
	nameonly(filename);

	// convert it to long Zzz....
	file_time = strtol(filename, NULL, 10);
	free(filename); // dont need this anymore a

#if defined (_WIN32)
skipfilenametime:
#endif

	// then throw it into localtime to get an actual human readable format lmao
	struct tm *tm_buf = NULL;
	tm_buf = localtime(&file_time);

	// cant believe we ended up in 1970
	if (tm_buf == NULL || tm_buf->tm_year <= 110)
	{
#if defined (_WIN32)
		// uh ohh, we got an invalid time
		// try one more time getting the creation time
		file_time = G_GetCreationTime(pdemo->filepath);
		tm_buf = localtime(&file_time);

		if (tm_buf == NULL || tm_buf->tm_year <= 110)
		{
			free(datetime);
			return NULL;
		}

		goto gotcreationtime;
#else
		free(datetime);
		return NULL;
#endif
	}

#if defined (_WIN32)
gotcreationtime:
#endif

	const char *format;

	// US ppl are special (:
	if (cv_demodateformat.value == 2)
		format = "%m.%d.%Y";
	else if (cv_demodateformat.value == 1)
		format = "%d.%m.%Y";
	else
		format = strstr(setlocale(LC_TIME, NULL), "en_US") ? "%m.%d.%Y" : "%d.%m.%Y";

	strftime(datetime, sizeof(pdemo->date), format, tm_buf);

	return datetime;
}

void G_LoadDemoTitle(menudemo_t *pdemo)
{
	UINT8 infobuffer[96], *info_p;
	UINT16 pdemoversion;
	size_t count;

	FILE *handle = fopen(pdemo->filepath, "rb");

	if (!handle)
	{
		CONS_Alert(CONS_ERROR, M_GetText("Failed to read file '%s'.\n"), pdemo->filepath);
		sprintf(pdemo->title, "INVALID REPLAY");
		return;
	}

	count = fread(infobuffer, 1, 96, handle);
	fclose(handle);
	info_p = infobuffer;

	// First check isn't too accurate technically, but well, valid replays should be larger than that anyway
	if (count < 96 || memcmp(info_p, DEMOHEADER, 12))
	{
		CONS_Alert(CONS_ERROR, M_GetText("%s is not a SRB2Kart replay file.\n"), pdemo->filepath);
		sprintf(pdemo->title, "INVALID REPLAY");
		return;
	}

	info_p += 12; // DEMOHEADER
	info_p++; // VERSION
	info_p++; // SUBVERSION

	pdemoversion = READUINT16(info_p);

	memset(pdemo->date, 0, sizeof(pdemo->date));

	switch (pdemoversion)
	{
		case DEMOVERSION: // latest always supported
			// demo title
			memcpy(pdemo->title, info_p, 64);

			// demo date
			char *demodate;
			demodate = G_GetDemoDate(pdemo);

			if (demodate)
				strncpy(pdemo->date, demodate, sizeof(pdemo->date));

			free(demodate);
			break;
#ifdef DEMO_COMPAT_100
		case 0x0001:
			sprintf(pdemo->title, "Legacy Replay");
			break;
#endif
		// too old, cannot support.
		default:
			CONS_Alert(CONS_ERROR, M_GetText("%s is an incompatible replay format and cannot be played.\n"), pdemo->filepath);
			sprintf(pdemo->title, "INVALID REPLAY");
	}
}

//
// G_PlayDemo
//
void G_DeferedPlayDemo(const char *name)
{
	COM_BufAddText("playdemo \"");
	COM_BufAddText(name);
	COM_BufAddText("\" -addfiles\n");
}

//
// Start a demo from a .LMP file or from a wad resource
//
#define SKIPERRORS
void G_DoPlayDemo(char *defdemoname)
{
	UINT8 i, p;
	lumpnum_t l;
	char skin[17], color[17], *n;
	CLEANUP(Z_Pfree) char *pdemoname = NULL;
	UINT8 version, subversion;
	UINT32 randseed;
	char msg[1024];
#if defined(SKIPERRORS) && !defined(DEVELOP)
	boolean skiperrors = false;
#endif
	boolean spectator;
	UINT8 slots[MAXPLAYERS], kartspeed[MAXPLAYERS], kartweight[MAXPLAYERS], numslots = 0;

	G_InitDemoRewind();

	skin[16] = '\0';
	color[16] = '\0';

	// No demo name means we're restarting the current demo
	if (defdemoname == NULL)
	{
		demobuf.p = demobuf.buffer;
		pdemoname = ZZ_Alloc(1); // Easier than adding checks for this everywhere it's freed
	}
	else
	{
		n = defdemoname + strlen(defdemoname);

		while (*n != '/' && *n != '\\' && n != defdemoname)
			n--;
		if (n != defdemoname)
			n++;

		pdemoname = ZZ_Alloc(strlen(n)+1);
		strcpy(pdemoname,n);

		M_SetPlaybackMenuPointer();

		// Internal if no extension, external if one exists
		if (FIL_CheckExtension(defdemoname))
		{
			//FIL_DefaultExtension(defdemoname, ".lmp");
			if (!FIL_ReadFile(defdemoname, &demobuf.buffer))
			{
				snprintf(msg, 1024, M_GetText("Failed to read file '%s'.\n"), defdemoname);
				CONS_Alert(CONS_ERROR, "%s", msg);
				gameaction = ga_nothing;
				M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
				return;
			}

			demobuf.p = demobuf.buffer;
		}
		// load demo resource from WAD
		else if ((l = W_CheckNumForName(defdemoname)) == LUMPERROR)
		{
			snprintf(msg, 1024, M_GetText("Failed to read lump '%s'.\n"), defdemoname);
			CONS_Alert(CONS_ERROR, "%s", msg);
			gameaction = ga_nothing;
			M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
			return;
		}
		else // it's an internal demo
		{
			demobuf.buffer = demobuf.p = W_CacheLumpNum(l, PU_STATIC);
#if defined(SKIPERRORS) && !defined(DEVELOP)
			skiperrors = true; // SRB2Kart: Don't print warnings for staff ghosts, since they'll inevitably happen when we make bugfixes/changes...
#endif
		}
	}

	// read demo header
	gameaction = ga_nothing;
	demo.playback = true;

	if (memcmp(demobuf.p, DEMOHEADER, 12))
	{
		snprintf(msg, 1024, M_GetText("%s is not a SRB2Kart replay file.\n"), pdemoname);
		CONS_Alert(CONS_ERROR, "%s", msg);
		M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
		G_ResetDemoPlayback();
		return;
	}

	demobuf.p += 12; // DEMOHEADER

	version = READUINT8(demobuf.p);
	subversion = READUINT8(demobuf.p);
	demo.version = READUINT16(demobuf.p);

	switch (demo.version)
	{
		case DEMOVERSION: // latest always supported
			// demo title
			memcpy(demo.titlename, demobuf.p, 64);
			demobuf.p += 64;
			break;
#ifdef DEMO_COMPAT_100
		case 0x0001:
			break;
#endif
		// too old, cannot support.
		default:
			snprintf(msg, 1024, M_GetText("%s is an incompatible replay format and cannot be played.\n"), pdemoname);
			CONS_Alert(CONS_ERROR, "%s", msg);
			M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
			G_ResetDemoPlayback();
			return;
	}

	demobuf.p += 16; // demo checksum

	if (memcmp(demobuf.p, "PLAY", 4))
	{
		snprintf(msg, 1024, M_GetText("%s is the wrong type of recording and cannot be played.\n"), pdemoname);
		CONS_Alert(CONS_ERROR, "%s", msg);
		M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
		G_ResetDemoPlayback();
		return;
	}

	demobuf.p += 4; // "PLAY"
	gamemap = READINT16(demobuf.p);
	demobuf.p += 16; // mapmd5
	demoflags = READUINT8(demobuf.p);

#ifdef DEMO_COMPAT_100
	if (demo.version == 0x0001)
	{
		if (demoflags & DF_MULTIPLAYER)
		{
			snprintf(msg, 1024, M_GetText("%s is an alpha multiplayer replay and cannot be played.\n"), pdemoname);
			CONS_Alert(CONS_ERROR, "%s", msg);
			M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
			G_ResetDemoPlayback();
			return;
		}
	}
	else
	{
#endif
	gametype = READUINT8(demobuf.p);

	if (demo.title) // Titledemos should always play and ought to always be compatible with whatever wadlist is running.
		G_SkipDemoExtraFiles(&demobuf.p);
	else if (demo.loadfiles)
		G_LoadDemoExtraFiles(&demobuf.p);
	else if (demo.ignorefiles)
		G_SkipDemoExtraFiles(&demobuf.p);
	else
	{
		UINT8 error = G_CheckDemoExtraFiles(&demobuf.p, false);

		if (error)
		{
			switch (error)
			{
				case DFILE_ERROR_NOTLOADED:
					snprintf(msg, 1024,
						"Required files for this demo are not loaded.\n\nUse\n\"playdemo %s -addfiles\"\nto load them and play the demo.\n",
						 pdemoname);
					break;

				case DFILE_ERROR_OUTOFORDER:
					snprintf(msg, 1024,
						"Required files for this demo are loaded out of order.\n\nUse\n\"playdemo %s -force\"\nto play the demo anyway.\n",
						 pdemoname);
					break;

				case DFILE_ERROR_INCOMPLETEOUTOFORDER:
					snprintf(msg, 1024,
						"Required files for this demo are not loaded, and some are out of order.\n\nUse\n\"playdemo %s -addfiles\"\nto load needed files and play the demo.\n",
						 pdemoname);
					break;

				case DFILE_ERROR_CANNOTLOAD:
					snprintf(msg, 1024,
						"Required files for this demo cannot be loaded.\n\nUse\n\"playdemo %s -force\"\nto play the demo anyway.\n",
						 pdemoname);
					break;

				case DFILE_ERROR_EXTRAFILES:
					snprintf(msg, 1024,
						"You have additional files loaded beyond the demo's file list.\n\nUse\n\"playdemo %s -force\"\nto play the demo anyway.\n",
						 pdemoname);
					break;
			}

			CONS_Alert(CONS_ERROR, "%s", msg);

			if (!CON_Ready()) // In the console they'll just see the notice there! No point pulling them out.
				M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);

			G_ResetDemoPlayback();
			return;
		}
	}
#ifdef DEMO_COMPAT_100
	}
#endif

	modeattacking = (demoflags & DF_ATTACKMASK)>>DF_ATTACKSHIFT;
	multiplayer = !!(demoflags & DF_MULTIPLAYER);
	CON_ToggleOff();

	hu_demotime = UINT32_MAX;
	hu_demolap = UINT32_MAX;

	switch (modeattacking)
	{
		case ATTACKING_NONE: // 0
			break;
		case ATTACKING_RECORD: // 1
			hu_demotime = READUINT32(demobuf.p);
			hu_demolap  = READUINT32(demobuf.p);
			break;
		default: // 3
			modeattacking = ATTACKING_NONE;
			break;
	}

	// Random seed
	randseed = READUINT32(demobuf.p);

#ifdef DEMO_COMPAT_100
	if (demo.version != 0x0001)
#endif
	demobuf.p += 4; // Extrainfo location

#ifdef DEMO_COMPAT_100
	if (demo.version == 0x0001)
	{
		// Player name
		memcpy(player_names[0],demobuf.p,16);
		demobuf.p += 16;

		// Skin
		memcpy(skin,demobuf.p,16);
		demobuf.p += 16;

		// Color
		memcpy(color,demobuf.p,16);
		demobuf.p += 16;

		demobuf.p += 5; // Backwards compat - some stats
		// SRB2kart
		kartspeed[0] = READUINT8(demobuf.p);
		kartweight[0] = READUINT8(demobuf.p);
		//
		demobuf.p += 9; // Backwards compat - more stats

		// Skin not loaded?
		if (!SetPlayerSkin(0, skin))
		{
			snprintf(msg, 1024, M_GetText("%s features a character that is not currently loaded.\n"), pdemoname);
			CONS_Alert(CONS_ERROR, "%s", msg);
			M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
			G_ResetDemoPlayback();
			return;
		}

		// ...*map* not loaded?
		if (!gamemap || (gamemap > NUMMAPS) || !mapheaderinfo[gamemap-1] || !(mapheaderinfo[gamemap-1]->menuflags & LF2_EXISTSHACK))
		{
			snprintf(msg, 1024, M_GetText("%s features a course that is not currently loaded.\n"), pdemoname);
			CONS_Alert(CONS_ERROR, "%s", msg);
			M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
			G_ResetDemoPlayback();
			return;
		}

		// Set color
		for (i = 0; i < MAXSKINCOLORS; i++)
		{
			if (fasticmp(KartColor_Names[i],color)) // SRB2kart
			{
				players[0].skincolor = i;
				break;
			}
		}

		// net var data
		demobuf.p += CV_LoadNetVars(demobuf.p);

		// Sigh ... it's an empty demo.
		if (*demobuf.p == DEMOMARKER)
		{
			snprintf(msg, 1024, M_GetText("%s contains no data to be played.\n"), pdemoname);
			CONS_Alert(CONS_ERROR, "%s", msg);
			M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
			G_ResetDemoPlayback();
			return;
		}

		memset(&oldcmd,0,sizeof(oldcmd));
		memset(&oldghost,0,sizeof(oldghost));
		memset(&ghostext,0,sizeof(ghostext));

		CONS_Alert(CONS_WARNING, M_GetText("Demo version does not match game version. Desyncs may occur.\n"));

		// console warning messages
#if defined(SKIPERRORS) && !defined(DEVELOP)
		demosynced = (!skiperrors);
#else
		demosynced = true;
#endif

		// didn't start recording right away.
		demo.deferstart = false;

		consoleplayer = 0;
		memset(displayplayers, 0, sizeof(displayplayers));
		memset(playeringame, 0, sizeof(playeringame));
		playeringame[0] = true;

		goto post_compat;
	}
#endif

	// net var data
	demobuf.p += CV_LoadNetVars(demobuf.p);

	// Sigh ... it's an empty demo.
	if (*demobuf.p == DEMOMARKER)
	{
		snprintf(msg, 1024, M_GetText("%s contains no data to be played.\n"), pdemoname);
		CONS_Alert(CONS_ERROR, "%s", msg);
		M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
		G_ResetDemoPlayback();
		return;
	}

	memset(&oldcmd, 0, sizeof(oldcmd));
	memset(&oldghost, 0, sizeof(oldghost));
	memset(&ghostext, 0, sizeof(ghostext));

#if defined(SKIPERRORS) && !defined(DEVELOP)
	if ((VERSION != version || SUBVERSION != subversion) && !skiperrors)
#else
	if (VERSION != version || SUBVERSION != subversion)
#endif
		CONS_Alert(CONS_WARNING, M_GetText("Demo version does not match game version. Desyncs may occur.\n"));

	// console warning messages
#if defined(SKIPERRORS) && !defined(DEVELOP)
	demosynced = (!skiperrors);
#else
	demosynced = true;
#endif

	// didn't start recording right away.
	demo.deferstart = false;

	//LUA_HookInt(gamemap, HOOK(MapChange));

	consoleplayer = 0;
	memset(playeringame, 0, sizeof(playeringame));
	memset(displayplayers, 0, sizeof(displayplayers));
	memset(camera, 0, sizeof(camera)); // reset freecam

	// Load players that were in-game when the map started
	p = READUINT8(demobuf.p);

	while (p != 0xFF)
	{
		spectator = false;

		if (p & DEMO_SPECTATOR)
		{
			spectator = true;
			p &= ~DEMO_SPECTATOR;

			if (modeattacking)
			{
				snprintf(msg, 1024, M_GetText("%s is a Record Attack replay with spectators, and is thus invalid.\n"), pdemoname);
				CONS_Alert(CONS_ERROR, "%s", msg);
				M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
				G_ResetDemoPlayback();
				return;
			}
		}

		slots[numslots] = p; numslots++;

		if (modeattacking && numslots > 1)
		{
			snprintf(msg, 1024, M_GetText("%s is a Record Attack replay with multiple players, and is thus invalid.\n"), pdemoname);
			CONS_Alert(CONS_ERROR, "%s", msg);
			M_StartMessage(msg, M_ReturnToTitleFromError, MM_EVENTHANDLER);
			G_ResetDemoPlayback();
			return;
		}

		if (!playeringame[displayplayers[0]] || players[displayplayers[0]].spectator)
			displayplayers[0] = consoleplayer = serverplayer = p;

		playeringame[p] = true;
		players[p].spectator = spectator;

		// Name
		memcpy(player_names[p],demobuf.p,16);
		demobuf.p += 16;

		// Skin
		memcpy(skin,demobuf.p,16);
		demobuf.p += 16;
		SetPlayerSkin(p, skin);

		// Color
		memcpy(color,demobuf.p,16);
		demobuf.p += 16;

		for (i = 0; i < MAXSKINCOLORS; i++)
		{
			if (fasticmp(KartColor_Names[i],color)) // SRB2kart
			{
				players[p].skincolor = i;
				break;
			}
		}

		// Score, since Kart uses this to determine where you start on the map
		players[p].score = READUINT32(demobuf.p);

		// Kart stats, temporarily
		kartspeed[p] = READUINT8(demobuf.p);
		kartweight[p] = READUINT8(demobuf.p);

		if (!fasticmp(skins[players[p].skin].name, skin))
			FindClosestSkinForStats(p, kartspeed[p], kartweight[p]);

		// Look for the next player
		p = READUINT8(demobuf.p);
	}

	// end of player read (the 0xFF marker)
	// so this is where we are to read our lua variables (if possible!)
	if (demoflags & DF_LUAVARS) // again, used for compability, lua shit will be saved to replays regardless of if it's even been loaded
	{
		if (!gL) // No Lua state! ...I guess we'll just start one...
			LUA_ClearState();

		// No modeattacking check, DF_LUAVARS won't be present here.
		LUA_UnArchive(&demobuf, false);
	}

	splitscreen = 0;

	if (demo.title)
	{
		splitscreen = M_RandomKey(6)-1;
		splitscreen = min(min(3, numslots-1), splitscreen); // Bias toward 1p and 4p views

		for (p = 0; p <= splitscreen; p++)
			G_ResetView(p+1, slots[M_RandomKey(numslots)], false);
	}

	R_ExecuteSetViewSize();

#ifdef DEMO_COMPAT_100
post_compat:
#endif

	P_SetRandSeed(randseed);
	G_InitNew(demoflags & DF_ENCORE, G_BuildMapName(gamemap), true, true); // Doesn't matter whether you reset or not here, given changes to resetplayer.

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (players[i].mo)
		{
			players[i].mo->color = players[i].skincolor;
			oldghost[i].x = players[i].mo->x;
			oldghost[i].y = players[i].mo->y;
			oldghost[i].z = players[i].mo->z;
		}

		// Set saved attribute values
		// No cheat checking here, because even if they ARE wrong...
		// it would only break the replay if we clipped them.
		players[i].kartspeed = kartspeed[i];
		players[i].kartweight = kartweight[i];
	}

	if (cv_director.value)
		CV_SetValue(&cv_director, 0);

	demo.deferstart = true;
}
#undef SKIPERRORS

void G_AddGhost(char *defdemoname)
{
	INT32 i;
	lumpnum_t l;
	char name[17], skin[17], color[17], *n, md5[16];
	CLEANUP(Z_Pfree) char *pdemoname = NULL;
	demoghost *gh;
	UINT8 *p;
	UINT8 flags;
	CLEANUP(Z_Pfree) UINT8 *buffer = NULL;
	mapthing_t *mthing;
	UINT16 count, ghostversion;
	skin_t *ghskin = &skins[0];
	UINT8 kartspeed = UINT8_MAX, kartweight = UINT8_MAX;

	name[16] = '\0';
	skin[16] = '\0';
	color[16] = '\0';

	n = defdemoname + strlen(defdemoname);

	while (*n != '/' && *n != '\\' && n != defdemoname)
		n--;
	if (n != defdemoname)
		n++;

	pdemoname = ZZ_Alloc(strlen(n)+1);
	strcpy(pdemoname, n);

	// Internal if no extension, external if one exists
	if (FIL_CheckExtension(defdemoname))
	{
		//FIL_DefaultExtension(defdemoname, ".lmp");
		if (!FIL_ReadFileTag(defdemoname, &buffer, PU_LEVEL))
		{
			CONS_Alert(CONS_ERROR, M_GetText("Failed to read file '%s'.\n"), defdemoname);
			return;
		}

		p = buffer;
	}
	// load demo resource from WAD
	else if ((l = W_CheckNumForName(defdemoname)) == LUMPERROR)
	{
		CONS_Alert(CONS_ERROR, M_GetText("Failed to read lump '%s'.\n"), defdemoname);
		return;
	}
	else // it's an internal demo
		buffer = p = W_CacheLumpNum(l, PU_LEVEL);

	// read demo header
	if (memcmp(p, DEMOHEADER, 12))
	{
		CONS_Alert(CONS_NOTICE, M_GetText("Ghost %s: Not a SRB2Kart replay.\n"), pdemoname);
		return;
	}

	p += 12; // DEMOHEADER
	p++; // VERSION
	p++; // SUBVERSION

	ghostversion = READUINT16(p);

	switch (ghostversion)
	{
		case DEMOVERSION: // latest always supported
			p += 64; // title
			break;
#ifdef DEMO_COMPAT_100
		case 0x0001:
			break;
#endif
		// too old, cannot support.
		default:
			CONS_Alert(CONS_NOTICE, "Ghost %s: Demo version incompatible.\n", pdemoname);
			return;
	}

	memcpy(md5, p, 16); p += 16; // demo checksum

	for (gh = ghosts; gh; gh = gh->next)
	{
		if (!memcmp(md5, gh->checksum, 16)) // another ghost in the game already has this checksum?
		{ // Don't add another one, then!
			CONS_Debug(DBG_SETUP, "Rejecting duplicate ghost %s (MD5 was matched)\n", pdemoname);
			return;
		}
	}

	if (memcmp(p, "PLAY", 4))
	{
		CONS_Alert(CONS_NOTICE, "Ghost %s: Demo format unacceptable.\n", pdemoname);
		return;
	}

	p += 4; // "PLAY"
	p += 2; // gamemap
	p += 16; // mapmd5 (possibly check for consistency?)

	flags = READUINT8(p);

	if (!(flags & DF_GHOST))
	{
		CONS_Alert(CONS_NOTICE, "Ghost %s: No ghost data in this demo.\n", pdemoname);
		return;
	}

#ifdef DEMO_COMPAT_100
	if (ghostversion != 0x0001)
#endif
		p++; // gametype

#ifdef DEMO_COMPAT_100
	if (ghostversion != 0x0001)
#endif
		G_SkipDemoExtraFiles(&p); // Don't wanna modify the file list for ghosts.

	switch ((flags & DF_ATTACKMASK) >> DF_ATTACKSHIFT)
	{
		case ATTACKING_NONE: // 0
			break;
		case ATTACKING_RECORD: // 1
			p += 8; // demo time, lap
			break;
		default: // 3
			break;
	}

	p += 4; // random seed

#ifdef DEMO_COMPAT_100
	if (ghostversion == 0x0001)
	{
		// Player name (TODO: Display this somehow if it doesn't match cv_playername!)
		memcpy(name, p,16);
		p += 16;

		// Skin
		memcpy(skin, p,16);
		p += 16;

		// Color
		memcpy(color, p,16);
		p += 16;

		// Ghosts do not have a player structure to put this in.
		p++; // charability
		p++; // charability2
		p++; // actionspd
		p++; // mindash
		p++; // maxdash
		// SRB2kart
		p++; // kartspeed
		p++; // kartweight
		//
		p++; // normalspeed
		p++; // runspeed
		p++; // thrustfactor
		p++; // accelstart
		p++; // acceleration
		p += 4; // jumpfactor
	}
	else
#endif
	p += 4; // Extra data location reference

	// net var data
	count = READUINT16(p);
	while (count--)
	{
		p += 2;
		SKIPSTRING(p);
		p++;
	}

	if (*p == DEMOMARKER)
	{
		CONS_Alert(CONS_NOTICE, M_GetText("Failed to add ghost %s: Replay is empty.\n"), pdemoname);
		return;
	}

#ifdef DEMO_COMPAT_100
	if (ghostversion != 0x0001)
	{
#endif
	if (READUINT8(p) != 0)
	{
		CONS_Alert(CONS_NOTICE, M_GetText("Failed to add ghost %s: Invalid player slot.\n"), pdemoname);
		return;
	}

	// Player name (TODO: Display this somehow if it doesn't match cv_playername!)
	memcpy(name, p, 16);
	p += 16;

	// Skin
	memcpy(skin, p, 16);
	p += 16;

	// Color
	memcpy(color, p, 16);
	p += 16;

	p += 4; // score

	kartspeed = READUINT8(p);
	kartweight = READUINT8(p);

	if (READUINT8(p) != 0xFF)
	{
		CONS_Alert(CONS_NOTICE, M_GetText("Failed to add ghost %s: Invalid player slot.\n"), pdemoname);
		return;
	}
#ifdef DEMO_COMPAT_100
	}
#endif

	for (i = 0; i < numskins; i++)
	{
		if (fasticmp(skins[i].name, skin))
		{
			ghskin = &skins[i];
			break;
		}
	}

	if (i == numskins)
	{
		if (kartspeed != UINT8_MAX && kartweight != UINT8_MAX)
			ghskin = &skins[GetSkinNumClosestToStats(kartspeed, kartweight)];

		CONS_Alert(CONS_NOTICE, M_GetText("Ghost %s: Invalid character. Falling back to %s.\n"), pdemoname, ghskin->name);
	}

	gh = Z_Calloc(sizeof(demoghost), PU_LEVEL, NULL);
	gh->next = ghosts;
	gh->buffer = buffer;
	memcpy(gh->checksum, md5, 16);
	gh->p = p;
	buffer = NULL; // buffer can't be freed now!

	ghosts = gh;

	gh->version = ghostversion;
	mthing = playerstarts[0];
	I_Assert(mthing);

	{ // A bit more complex than P_SpawnPlayer because ghosts aren't solid and won't just push themselves out of the ceiling.
		fixed_t z, f, c;

		gh->mo = P_SpawnMobj(mthing->x << FRACBITS, mthing->y << FRACBITS, 0, MT_GHOST);
		gh->mo->angle = FixedAngle(mthing->angle*FRACUNIT);
		f = gh->mo->floorz;
		c = gh->mo->ceilingz - mobjinfo[MT_PLAYER].height;

		if (!!(mthing->options & MTF_AMBUSH) ^ !!(mthing->options & MTF_OBJECTFLIP))
		{
			z = c;

			if (mthing->options >> ZSHIFT)
				z -= ((mthing->options >> ZSHIFT) << FRACBITS);
			if (z < f)
				z = f;
		}
		else
		{
			z = f;

			if (mthing->options >> ZSHIFT)
				z += ((mthing->options >> ZSHIFT) << FRACBITS);
			if (z > c)
				z = c;
		}

		gh->mo->z = z;
	}

	gh->mo->state = states+S_KART_STND1; // SRB2kart - was S_PLAY_STND
	gh->mo->sprite = gh->mo->state->sprite;
	gh->mo->frame = (gh->mo->state->frame & FF_FRAMEMASK) | tr_trans20<<FF_TRANSSHIFT;
	gh->mo->tics = -1;

	gh->oldmo.x = gh->mo->x;
	gh->oldmo.y = gh->mo->y;
	gh->oldmo.z = gh->mo->z;

	// Set skin
	gh->mo->skin = gh->oldmo.skin = ghskin;

	// Set color
	gh->mo->color = ((skin_t*)gh->mo->skin)->prefcolor;

	for (i = 0; i < MAXSKINCOLORS; i++)
	{
		if (fasticmp(KartColor_Names[i],color)) // SRB2kart
		{
			gh->mo->color = (UINT8)i;
			break;
		}
	}

	gh->oldmo.color = gh->mo->color;

	CONS_Printf(M_GetText("Added ghost %s from %s\n"), name, pdemoname);
}

// A simplified version of G_AddGhost...
void G_UpdateStaffGhostName(lumpnum_t l)
{
	UINT8 *p;
	CLEANUP(Z_Pfree) UINT8 *buffer = NULL;
	UINT16 ghostversion;
	UINT8 flags;

	buffer = p = W_CacheLumpNum(l, PU_CACHE);

	// read demo header
	if (memcmp(p, DEMOHEADER, 12))
	{
		return;
	}

	p += 12; // DEMOHEADER
	p++; // VERSION
	p++; // SUBVERSION

	ghostversion = READUINT16(p);
	switch (ghostversion)
	{
		case DEMOVERSION: // latest always supported
			p += 64; // full demo title
			break;
#ifdef DEMO_COMPAT_100
		case 0x0001:
			break;
#endif
		// too old, cannot support.
		default:
			return;
	}

	p += 16; // demo checksum

	if (memcmp(p, "PLAY", 4))
	{
		return;
	}

	p += 4; // "PLAY"
	p += 2; // gamemap
	p += 16; // mapmd5 (possibly check for consistency?)

	flags = READUINT8(p);
	if (!(flags & DF_GHOST))
	{
		return; // we don't NEED to do it here, but whatever
	}

#ifdef DEMO_COMPAT_100
	if (ghostversion != 0x0001)
#endif
	p++; // Gametype

#ifdef DEMO_COMPAT_100
	if (ghostversion != 0x0001)
#endif
	G_SkipDemoExtraFiles(&p);

	switch ((flags & DF_ATTACKMASK)>>DF_ATTACKSHIFT)
	{
		case ATTACKING_NONE: // 0
			break;
		case ATTACKING_RECORD: // 1
			p += 8; // demo time, lap
			break;
		default: // 3
			break;
	}

	p += 4; // random seed


#ifdef DEMO_COMPAT_100
	if (ghostversion == 0x0001)
	{
		// Player name
		memcpy(dummystaffname, p,16);
		dummystaffname[16] = '\0';
		return; // Not really a failure but whatever
	}
#endif

	p += 4; // Extrainfo location marker

	// Ehhhh don't need ghostversion here (?) so I'll reuse the var here
	ghostversion = READUINT16(p);

	while (ghostversion--)
	{
		p += 2;
		SKIPSTRING(p);
		p++; // stealth
	}

	// Assert first player is in and then read name
	if (READUINT8(p) != 0)
		return;

	memcpy(dummystaffname, p, 16);
	dummystaffname[16] = '\0';

	// Ok, no longer any reason to care, bye
}

//
// G_TimeDemo
// NOTE: name is a full filename for external demos
//
static INT32 restorecv_vidwait;

void G_TimeDemo(const char *name)
{
	nodrawers = M_CheckParm("-nodraw");
	noblit = M_CheckParm("-noblit");
	restorecv_vidwait = cv_vidwait.value;
	if (cv_vidwait.value)
		CV_Set(&cv_vidwait, "0");
	demo.timing = true;
	singletics = true;
	framecount = 0;
	demostarttime = I_GetTime();
	G_DeferedPlayDemo(name);
}

void G_DoneLevelLoad(void)
{
	CONS_Printf(M_GetText("Loaded level in %f sec\n"), (double)(I_GetTime() - demostarttime) / TICRATE);
	framecount = 0;
	demostarttime = I_GetTime();
}

/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

// reset engine variable set for the demos
// called from stopdemo command, map command, and g_checkdemoStatus.
void G_StopDemo(void)
{
	G_ResetDemoPlayback();
	demo.timing = false;
	singletics = false;

	UINT8 i;
	for (i = 0; i < MAXSPLITSCREENPLAYERS; ++i)
	{
		camera[i].freecam = false;
		camera[i].localangle = 0;
		camera[i].localaiming = 0;
	}

	CV_SetValue(&cv_playbackspeed, 1);
	demo.rewinding = false;
	CL_ClearRewinds();

	D_ClearState();
}

// Stops timing a demo.
static void G_StopTimingDemo(void)
{
	INT32 demotime;
	double f1, f2;
	demotime = I_GetTime() - demostarttime;
	if (!demotime)
		return;
	G_StopDemo();
	demo.timing = false;
	f1 = (double)demotime;
	f2 = (double)framecount*TICRATE;

	CONS_Printf(M_GetText("timed %u gametics in %d realtics - %u frames\n%f seconds, %f avg fps\n"),
				leveltime, demotime, (UINT32)framecount, f1/TICRATE, f2/f1);

	if (restorecv_vidwait != cv_vidwait.value)
		CV_SetValue(&cv_vidwait, restorecv_vidwait);

	D_StartTitle();
}

// Clean up all ghosts
void G_FreeGhosts(void)
{
	while (ghosts)
	{
		demoghost *next = ghosts->next;
		Z_Free(ghosts);
		ghosts = next;
	}
	ghosts = NULL;
}

boolean G_CheckDemoStatus(void)
{
	G_FreeGhosts();

	if (demo.timing)
	{
		G_StopTimingDemo();
		return true;
	}

	if (demo.playback)
	{
		if (demo.quitafterplaying)
			I_Quit();

		if (multiplayer && !demo.title)
		{
			G_ExitLevel();
		}
		else if (modeattacking && !demo.title) // nooo dont crash our titledemos
		{
			G_StopDemo();
			M_EndModeAttackRun();
		}
		else
		{
			G_StopDemo();
			D_StartTitle();
		}

		return true;
	}

	if (!demo.recording)
		return false;

	if (modeattacking || demo.savemode != DSM_NOTSAVING)
	{
		G_SaveDemo();
		return true;
	}

	G_ResetDemoRecording();

	return false;
}

void G_ResetDemoRecording(void)
{
	Z_Free(demobuf.buffer);
	demobuf.buffer = NULL;
	demo.recording = false;
}

static void G_ResetDemoPlayback(void)
{
	Z_Free(demobuf.buffer);
	demobuf.buffer = NULL;
	demo.playback = false;
	if (demo.title)
		modeattacking = ATTACKING_NONE;
	demo.title = false;
}

void G_SaveDemo(void)
{
	if (!demobuf.p)
	{
		CONS_Alert(CONS_ERROR, "Failed to save Demo. No Demo pointer exists!\n");
		// reset the demo buffer
		G_ResetDemoRecording();
		return;
	}

	if (demobuf.buffer == NULL)
	{
		CONS_Alert(CONS_ERROR, "Failed to save Demo. No Demo buffer allocated!\n");
		// reset the demo buffer
		G_ResetDemoRecording();
		return;
	}

	UINT8 *p = demobuf.buffer+16; // after version
	UINT32 length;

	// Ensure extrainfo pointer is always available, even if no info is present.
	if (demoinfo_p && *(UINT32 *)demoinfo_p == 0)
	{
		WRITEUINT8(demobuf.p, DEMOMARKER); // add the demo end marker
		*(UINT32 *)demoinfo_p = demobuf.p - demobuf.buffer;
	}
	WRITEUINT8(demobuf.p, DW_END); // Mark end of demo extra data.

	memcpy(p, demo.titlename, 64); // Write demo title here
	p += 64;

	if (multiplayer)
	{
		// Change the demo's name to be a slug of the title
		char demo_slug[128];
		char *writepoint;
		size_t i, strindex = 0;
		boolean dash = true;

		//for (i = 0; demo.titlename[i] && i < 127; i++) ?????
		for (i = 0; i < 64 && demo.titlename[i]; i++)
		{
			if ((demo.titlename[i] >= 'a' && demo.titlename[i] <= 'z') ||
				(demo.titlename[i] >= '0' && demo.titlename[i] <= '9'))
			{
				demo_slug[strindex] = demo.titlename[i];
				strindex++;
				dash = false;
			}
			else if (demo.titlename[i] >= 'A' && demo.titlename[i] <= 'Z')
			{
				demo_slug[strindex] = demo.titlename[i] + 'a' - 'A';
				strindex++;
				dash = false;
			}
			else if (strindex && !dash)
			{
				demo_slug[strindex] = '-';
				strindex++;
				dash = true;
			}
		}

		if (dash && strindex)
		{
			strindex--;
		}
		demo_slug[strindex] = '\0';

		if (demo_slug[0] != '\0')
		{
			// Slug is valid, write the chosen filename.
			writepoint = strstr(demoname, "-");
			if (!writepoint)
				return;

			writepoint++;

			size_t flen = 128 - (writepoint - demoname) - 4;
			snprintf(writepoint, flen, "%s.lmp", demo_slug);
		}
	}

	length = *(UINT32 *)demoinfo_p;
	WRITEUINT32(demoinfo_p, length);
#ifdef NOMD5
	for (UINT8 k = 0; k < 16; k++, p++)
		*p = M_RandomByte(); // This MD5 was chosen by fair dice roll and most likely < 50% correct.
#else
	// Make a checksum of everything after the checksum in the file up to the end of the standard data. Extrainfo is freely modifiable.
	md5_buffer((char *)p+16, (demobuf.buffer + length) - (p+16), p);
#endif

	if (FIL_WriteFile(va(pandf, srb2home, demoname), demobuf.buffer, demobuf.p - demobuf.buffer)) // finally output the file.
		demo.savemode = DSM_SAVED;

	G_ResetDemoRecording();

	if (modeattacking != ATTACKING_RECORD)
	{
		if (demo.savemode == DSM_SAVED)
			CONS_Printf(M_GetText("Demo %s recorded\n"), demoname);
		else
			CONS_Alert(CONS_WARNING, M_GetText("Demo %s not saved\n"), demoname);
	}
}

boolean G_DemoTitleResponder(event_t *ev)
{
	INT32 ch;

	if (ev->type != ev_keydown)
		return false;

	ch = (INT32)ev->data1;

	// Only ESC and non-keyboard keys abort connection
	if (ch == KEY_ESCAPE)
	{
		demo.savemode = (cv_recordmultiplayerdemos.value == 2) ? DSM_WILLAUTOSAVE : DSM_NOTSAVING;
		return true;
	}

	if (ch == KEY_ENTER || ch >= KEY_MOUSE1)
	{
		demo.savemode = DSM_WILLSAVE;
		return true;
	}

	M_TextInputHandle(&demo.titlenameinput, ch);

	return true;
}
