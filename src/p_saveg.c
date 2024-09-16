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
/// \file  p_saveg.c
/// \brief Archiving: SaveGame I/O

#include "doomdef.h"
#include "byteptr.h"
#include "d_main.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_random.h"
#include "m_misc.h"
#include "p_local.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "r_fps.h"
#include "r_things.h"
#include "r_state.h"
#include "w_wad.h"
#include "y_inter.h"
#include "z_zone.h"
#include "r_main.h"
#include "r_sky.h"
#include "p_polyobj.h"
#include "lua_script.h"
#include "p_slopes.h"

savedata_t savedata;

// Block UINT32s to attempt to ensure that the correct data is
// being sent and received
#define ARCHIVEBLOCK_MISC     0x7FEEDEED
#define ARCHIVEBLOCK_PLAYERS  0x7F448008
#define ARCHIVEBLOCK_WORLD    0x7F8C08C0
#define ARCHIVEBLOCK_POBJS    0x7F928546
#define ARCHIVEBLOCK_THINKERS 0x7F37037C
#define ARCHIVEBLOCK_SPECIALS 0x7F228378

// Note: This cannot be bigger
// than an UINT16
typedef enum
{
//	RFLAGPOINT = 0x01,
//	BFLAGPOINT = 0x02,
	CAPSULE    = 0x04,
	AWAYVIEW   = 0x08,
	FIRSTAXIS  = 0x10,
	SECONDAXIS = 0x20,
} player_saveflags;

//
// P_ArchivePlayer
//
static inline void P_ArchivePlayer(savebuffer_t *save)
{
	const player_t *player = &players[consoleplayer];
	INT32 pllives = player->lives;
	if (pllives < 3) // Bump up to 3 lives if the player
		pllives = 3; // has less than that.

	WRITEUINT8(save->p, player->skincolor);
	WRITEUINT16(save->p, player->skin);

	WRITEUINT32(save->p, player->score);
	WRITEINT32(save->p, pllives);
	WRITEINT32(save->p, player->continues);

	if (botskin)
	{
		WRITEUINT16(save->p, botskin);
		WRITEUINT8(save->p, botcolor);
	}
}

//
// P_UnArchivePlayer
//
static inline void P_UnArchivePlayer(savebuffer_t *save)
{
	savedata.skincolor = READUINT8(save->p);
	savedata.skin = READUINT16(save->p);

	savedata.score = READINT32(save->p);
	savedata.lives = READINT32(save->p);
	savedata.continues = READINT32(save->p);

	if (savedata.botcolor)
	{
		savedata.botskin = READUINT16(save->p);
		if (savedata.botskin-1 >= numskins)
			savedata.botskin = 0;
		savedata.botcolor = READUINT8(save->p);
	}
	else
		savedata.botskin = 0;
}

//
// P_NetArchivePlayers
//
static void P_NetArchivePlayers(savebuffer_t *save, boolean resending)
{
	INT32 i, j;
	UINT16 flags;
//	size_t q;

	WRITEUINT32(save->p, ARCHIVEBLOCK_PLAYERS);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (resending)
			WRITESINT8(save->p, (SINT8)adminplayers[i]);

		if (!playeringame[i])
			continue;

		flags = 0;

		// no longer send ticcmds

		if (resending)
			WRITESTRINGN(save->p, player_names[i], MAXPLAYERNAME);

		WRITEANGLE(save->p, players[i].aiming);
		WRITEANGLE(save->p, players[i].awayviewaiming);
		WRITEINT32(save->p, players[i].awayviewtics);
		WRITEINT32(save->p, players[i].health);

		WRITESINT8(save->p, players[i].pity);
		WRITEINT32(save->p, players[i].currentweapon);
		WRITEINT32(save->p, players[i].ringweapons);

		for (j = 0; j < NUMPOWERS; j++)
			WRITEUINT16(save->p, players[i].powers[j]);
		for (j = 0; j < NUMKARTSTUFF; j++)
			WRITEINT32(save->p, players[i].kartstuff[j]);

		WRITEANGLE(save->p, players[i].frameangle);

		WRITEUINT8(save->p, players[i].playerstate);
		WRITEUINT32(save->p, players[i].pflags);
		WRITEUINT8(save->p, players[i].panim);
		WRITEUINT8(save->p, players[i].spectator);

		WRITEUINT16(save->p, players[i].flashpal);
		WRITEUINT16(save->p, players[i].flashcount);

		if (resending)
		{
			WRITEUINT8(save->p, players[i].skincolor);
			WRITEINT32(save->p, players[i].skin);
		}

		WRITEUINT32(save->p, players[i].score);
		WRITEFIXED(save->p, players[i].dashspeed);
		WRITEINT32(save->p, players[i].dashtime);
		WRITESINT8(save->p, players[i].lives);
		WRITESINT8(save->p, players[i].continues);
		WRITESINT8(save->p, players[i].xtralife);
		WRITEUINT8(save->p, players[i].gotcontinue);
		WRITEFIXED(save->p, players[i].speed);
		WRITEUINT8(save->p, players[i].jumping);
		WRITEUINT8(save->p, players[i].secondjump);
		WRITEUINT8(save->p, players[i].fly1);
		WRITEUINT8(save->p, players[i].scoreadd);
		WRITEUINT32(save->p, players[i].glidetime);
		WRITEUINT8(save->p, players[i].climbing);
		WRITEINT32(save->p, players[i].deadtimer);
		WRITEUINT32(save->p, players[i].exiting);
		WRITEUINT8(save->p, players[i].homing);
		WRITEUINT32(save->p, players[i].skidtime);

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		WRITEFIXED(save->p, players[i].cmomx); // Conveyor momx
		WRITEFIXED(save->p, players[i].cmomy); // Conveyor momy
		WRITEFIXED(save->p, players[i].rmomx); // "Real" momx (momx - cmomx)
		WRITEFIXED(save->p, players[i].rmomy); // "Real" momy (momy - cmomy)

		/////////////////////
		// Race Mode Stuff //
		/////////////////////
		WRITEINT16(save->p, players[i].numboxes);
		WRITEINT16(save->p, players[i].totalring);
		WRITEUINT32(save->p, players[i].realtime);
		WRITEUINT8(save->p, players[i].laps);

		////////////////////
		// CTF Mode Stuff //
		////////////////////
		WRITEINT32(save->p, players[i].ctfteam);
		WRITEUINT16(save->p, players[i].gotflag);

		WRITEINT32(save->p, players[i].weapondelay);
		WRITEINT32(save->p, players[i].tossdelay);

		WRITEUINT32(save->p, players[i].starposttime);
		WRITEINT16(save->p, players[i].starpostx);
		WRITEINT16(save->p, players[i].starposty);
		WRITEINT16(save->p, players[i].starpostz);
		WRITEINT32(save->p, players[i].starpostnum);
		WRITEANGLE(save->p, players[i].starpostangle);

		WRITEANGLE(save->p, players[i].angle_pos);
		WRITEANGLE(save->p, players[i].old_angle_pos);

		WRITEINT32(save->p, players[i].flyangle);
		WRITEUINT32(save->p, players[i].drilltimer);
		WRITEINT32(save->p, players[i].linkcount);
		WRITEUINT32(save->p, players[i].linktimer);
		WRITEINT32(save->p, players[i].anotherflyangle);
		WRITEUINT32(save->p, players[i].nightstime);
		WRITEUINT32(save->p, players[i].bumpertime);
		WRITEINT32(save->p, players[i].drillmeter);
		WRITEUINT8(save->p, players[i].drilldelay);
		WRITEUINT8(save->p, players[i].bonustime);
		WRITEUINT8(save->p, players[i].mare);

		WRITEUINT32(save->p, players[i].marebegunat);
		WRITEUINT32(save->p, players[i].startedtime);
		WRITEUINT32(save->p, players[i].finishedtime);
		WRITEINT16(save->p, players[i].finishedrings);
		WRITEUINT32(save->p, players[i].marescore);
		WRITEUINT32(save->p, players[i].lastmarescore);
		WRITEUINT8(save->p, players[i].lastmare);
		WRITEINT32(save->p, players[i].maxlink);
		WRITEUINT8(save->p, players[i].texttimer);
		WRITEUINT8(save->p, players[i].textvar);

		if (players[i].capsule)
			flags |= CAPSULE;

		if (players[i].awayviewmobj)
			flags |= AWAYVIEW;

		if (players[i].axis1)
			flags |= FIRSTAXIS;

		if (players[i].axis2)
			flags |= SECONDAXIS;

		WRITEINT16(save->p, players[i].lastsidehit);
		WRITEINT16(save->p, players[i].lastlinehit);

		WRITEUINT32(save->p, players[i].losstime);

		WRITEUINT8(save->p, players[i].timeshit);

		WRITEINT32(save->p, players[i].onconveyor);

		WRITEUINT32(save->p, players[i].jointime);
		WRITEUINT32(save->p, players[i].spectatorreentry);

		WRITEUINT32(save->p, players[i].grieftime);
		WRITEUINT8(save->p, players[i].griefstrikes);

		WRITEUINT8(save->p, players[i].splitscreenindex);

		WRITEUINT16(save->p, flags);

		if (flags & CAPSULE)
			WRITEUINT32(save->p, players[i].capsule->mobjnum);

		if (flags & FIRSTAXIS)
			WRITEUINT32(save->p, players[i].axis1->mobjnum);

		if (flags & SECONDAXIS)
			WRITEUINT32(save->p, players[i].axis2->mobjnum);

		if (flags & AWAYVIEW)
			WRITEUINT32(save->p, players[i].awayviewmobj->mobjnum);

		WRITEUINT32(save->p, players[i].charflags);
		// SRB2kart
		WRITEUINT8(save->p, players[i].kartspeed);
		WRITEUINT8(save->p, players[i].kartweight);
		//

		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			WRITEINT16(save->p, players[i].lturn_max[j]);
			WRITEINT16(save->p, players[i].rturn_max[j]);
		}
	}
}

//
// P_NetUnArchivePlayers
//
static void P_NetUnArchivePlayers(savebuffer_t *save, boolean resending)
{
	INT32 i, j;
	UINT16 flags;

	if (READUINT32(save->p) != ARCHIVEBLOCK_PLAYERS)
		I_Error("Bad $$$.sav at archive block Players");

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (resending)
			adminplayers[i] = (INT32)READSINT8(save->p);

		// Do NOT memset player struct to 0
		// other areas may initialize data elsewhere
		//memset(&players[i], 0, sizeof (player_t));
		if (!playeringame[i])
			continue;

		// NOTE: sending tics should (hopefully) no longer be necessary

		if (resending)
			READSTRINGN(save->p, player_names[i], MAXPLAYERNAME);

		players[i].aiming = READANGLE(save->p);
		players[i].awayviewaiming = READANGLE(save->p);
		players[i].awayviewtics = READINT32(save->p);
		players[i].health = READINT32(save->p);

		players[i].pity = READSINT8(save->p);
		players[i].currentweapon = READINT32(save->p);
		players[i].ringweapons = READINT32(save->p);

		for (j = 0; j < NUMPOWERS; j++)
			players[i].powers[j] = READUINT16(save->p);
		for (j = 0; j < NUMKARTSTUFF; j++)
			players[i].kartstuff[j] = READINT32(save->p);

		players[i].frameangle = READANGLE(save->p);

		players[i].playerstate = READUINT8(save->p);
		players[i].pflags = READUINT32(save->p);
		players[i].panim = READUINT8(save->p);
		players[i].spectator = READUINT8(save->p);

		players[i].flashpal = READUINT16(save->p);
		players[i].flashcount = READUINT16(save->p);

		if (resending)
		{
			players[i].skincolor = READUINT8(save->p);
			players[i].skin = READINT32(save->p);
		}

		players[i].score = READUINT32(save->p);
		players[i].dashspeed = READFIXED(save->p); // dashing speed
		players[i].dashtime = READINT32(save->p); // dashing speed
		players[i].lives = READSINT8(save->p);
		players[i].continues = READSINT8(save->p); // continues that player has acquired
		players[i].xtralife = READSINT8(save->p); // Ring Extra Life counter
		players[i].gotcontinue = READUINT8(save->p); // got continue from stage
		players[i].speed = READFIXED(save->p); // Player's speed (distance formula of MOMX and MOMY values)
		players[i].jumping = READUINT8(save->p); // Jump counter
		players[i].secondjump = READUINT8(save->p);
		players[i].fly1 = READUINT8(save->p); // Tails flying
		players[i].scoreadd = READUINT8(save->p); // Used for multiple enemy attack bonus
		players[i].glidetime = READUINT32(save->p); // Glide counter for thrust
		players[i].climbing = READUINT8(save->p); // Climbing on the wall
		players[i].deadtimer = READINT32(save->p); // End game if game over lasts too long
		players[i].exiting = READUINT32(save->p); // Exitlevel timer
		players[i].homing = READUINT8(save->p); // Are you homing?
		players[i].skidtime = READUINT32(save->p); // Skid timer

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		players[i].cmomx = READFIXED(save->p); // Conveyor momx
		players[i].cmomy = READFIXED(save->p); // Conveyor momy
		players[i].rmomx = READFIXED(save->p); // "Real" momx (momx - cmomx)
		players[i].rmomy = READFIXED(save->p); // "Real" momy (momy - cmomy)

		/////////////////////
		// Race Mode Stuff //
		/////////////////////
		players[i].numboxes = READINT16(save->p); // Number of item boxes obtained for Race Mode
		players[i].totalring = READINT16(save->p); // Total number of rings obtained for Race Mode
		players[i].realtime = READUINT32(save->p); // integer replacement for leveltime
		players[i].laps = READUINT8(save->p); // Number of laps (optional)

		////////////////////
		// CTF Mode Stuff //
		////////////////////
		players[i].ctfteam = READINT32(save->p); // 1 == Red, 2 == Blue
		players[i].gotflag = READUINT16(save->p); // 1 == Red, 2 == Blue Do you have the flag?

		players[i].weapondelay = READINT32(save->p);
		players[i].tossdelay = READINT32(save->p);

		players[i].starposttime = READUINT32(save->p);
		players[i].starpostx = READINT16(save->p);
		players[i].starposty = READINT16(save->p);
		players[i].starpostz = READINT16(save->p);
		players[i].starpostnum = READINT32(save->p);
		players[i].starpostangle = READANGLE(save->p);

		players[i].angle_pos = READANGLE(save->p);
		players[i].old_angle_pos = READANGLE(save->p);

		players[i].flyangle = READINT32(save->p);
		players[i].drilltimer = READUINT32(save->p);
		players[i].linkcount = READINT32(save->p);
		players[i].linktimer = READUINT32(save->p);
		players[i].anotherflyangle = READINT32(save->p);
		players[i].nightstime = READUINT32(save->p);
		players[i].bumpertime = READUINT32(save->p);
		players[i].drillmeter = READINT32(save->p);
		players[i].drilldelay = READUINT8(save->p);
		players[i].bonustime = (boolean)READUINT8(save->p);
		players[i].mare = READUINT8(save->p);

		players[i].marebegunat = READUINT32(save->p);
		players[i].startedtime = READUINT32(save->p);
		players[i].finishedtime = READUINT32(save->p);
		players[i].finishedrings = READINT16(save->p);
		players[i].marescore = READUINT32(save->p);
		players[i].lastmarescore = READUINT32(save->p);
		players[i].lastmare = READUINT8(save->p);
		players[i].maxlink = READINT32(save->p);
		players[i].texttimer = READUINT8(save->p);
		players[i].textvar = READUINT8(save->p);

		players[i].lastsidehit = READINT16(save->p);
		players[i].lastlinehit = READINT16(save->p);

		players[i].losstime = READUINT32(save->p);

		players[i].timeshit = READUINT8(save->p);

		players[i].onconveyor = READINT32(save->p);

		players[i].jointime = READUINT32(save->p);
		players[i].spectatorreentry = READUINT32(save->p);

		players[i].grieftime = READUINT32(save->p);
		players[i].griefstrikes = READUINT8(save->p);

		players[i].splitscreenindex = READUINT8(save->p);

		flags = READUINT16(save->p);

		if (flags & CAPSULE)
			players[i].capsule = (mobj_t *)(size_t)READUINT32(save->p);

		if (flags & FIRSTAXIS)
			players[i].axis1 = (mobj_t *)(size_t)READUINT32(save->p);

		if (flags & SECONDAXIS)
			players[i].axis2 = (mobj_t *)(size_t)READUINT32(save->p);

		if (flags & AWAYVIEW)
			players[i].awayviewmobj = (mobj_t *)(size_t)READUINT32(save->p);

		players[i].viewheight = 32<<FRACBITS;

		//SetPlayerSkinByNum(i, players[i].skin);
		players[i].charflags = READUINT32(save->p);
		// SRB2kart
		players[i].kartspeed = READUINT8(save->p);
		players[i].kartweight = READUINT8(save->p);
		//

		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			players[i].lturn_max[j] = READINT16(save->p);
			players[i].rturn_max[j] = READINT16(save->p);
		}
	}
}

///
/// World Archiving
///

#define SD_FLOORHT  0x01
#define SD_CEILHT   0x02
#define SD_FLOORPIC 0x04
#define SD_CEILPIC  0x08
#define SD_LIGHT    0x10
#define SD_SPECIAL  0x20
#define SD_DIFF2    0x40
#define SD_FFLOORS  0x80

// diff2 flags
#define SD_FXOFFS    0x01
#define SD_FYOFFS    0x02
#define SD_CXOFFS    0x04
#define SD_CYOFFS    0x08
#define SD_TAG       0x10
#define SD_FLOORANG  0x20
#define SD_CEILANG   0x40
#define SD_TAGLIST   0x80

#define LD_FLAG     0x01
#define LD_SPECIAL  0x02
#define LD_CLLCOUNT 0x04
#define LD_S1TEXOFF 0x08
#define LD_S1TOPTEX 0x10
#define LD_S1BOTTEX 0x20
#define LD_S1MIDTEX 0x40
#define LD_DIFF2    0x80

// diff2 flags
#define LD_S2TEXOFF 0x01
#define LD_S2TOPTEX 0x02
#define LD_S2BOTTEX 0x04
#define LD_S2MIDTEX 0x08

//
// P_NetArchiveWorld
//
static void P_NetArchiveWorld(savebuffer_t *save)
{
	size_t i;
	const line_t *li = lines;
	const side_t *si;
	UINT8 *put;

	// reload the map just to see difference
	virtres_t* virt = vres_GetMap(lastloadedmaplumpnum);
	mapsector_t  *ms  = (mapsector_t*) vres_Find(virt, "SECTORS")->data;
	mapsidedef_t *msd = (mapsidedef_t*) vres_Find(virt, "SIDEDEFS")->data;
	maplinedef_t *mld = (maplinedef_t*) vres_Find(virt, "LINEDEFS")->data;
	const sector_t *ss = sectors;
	UINT8 diff, diff2;

	WRITEUINT32(save->p, ARCHIVEBLOCK_WORLD);
	put = save->p;

	for (i = 0; i < numsectors; i++, ss++, ms++)
	{
		diff = diff2 = 0;
		if (ss->floorheight != SHORT(ms->floorheight)<<FRACBITS)
			diff |= SD_FLOORHT;
		if (ss->ceilingheight != SHORT(ms->ceilingheight)<<FRACBITS)
			diff |= SD_CEILHT;
		//
		// flats
		//
		if (ss->floorpic != P_CheckLevelFlat(ms->floorpic))
			diff |= SD_FLOORPIC;
		if (ss->ceilingpic != P_CheckLevelFlat(ms->ceilingpic))
			diff |= SD_CEILPIC;

		if (ss->lightlevel != SHORT(ms->lightlevel))
			diff |= SD_LIGHT;
		if (ss->special != SHORT(ms->special))
			diff |= SD_SPECIAL;

		if (ss->floor_xoffs != ss->spawn_flr_xoffs)
			diff2 |= SD_FXOFFS;
		if (ss->floor_yoffs != ss->spawn_flr_yoffs)
			diff2 |= SD_FYOFFS;
		if (ss->ceiling_xoffs != ss->spawn_ceil_xoffs)
			diff2 |= SD_CXOFFS;
		if (ss->ceiling_yoffs != ss->spawn_ceil_yoffs)
			diff2 |= SD_CYOFFS;
		if (ss->floorpic_angle != ss->spawn_flrpic_angle)
			diff2 |= SD_FLOORANG;
		if (ss->ceilingpic_angle != ss->spawn_flrpic_angle)
			diff2 |= SD_CEILANG;

		if (ss->tag != SHORT(ms->tag))
			diff2 |= SD_TAG;
		if (ss->nexttag != ss->spawn_nexttag || ss->firsttag != ss->spawn_firsttag)
			diff2 |= SD_TAGLIST;

		// Check if any of the sector's FOFs differ from how they spawned
		if (ss->ffloors)
		{
			ffloor_t *rover;
			for (rover = ss->ffloors; rover; rover = rover->next)
			{
				if (rover->flags != rover->spawnflags
				|| rover->alpha != rover->spawnalpha)
					{
						diff |= SD_FFLOORS; // we found an FOF that changed!
						break; // don't bother checking for more, we do that later
					}
			}
		}

		if (diff2)
			diff |= SD_DIFF2;

		if (diff)
		{
			WRITEUINT16(put, i);
			WRITEUINT8(put, diff);
			if (diff & SD_DIFF2)
				WRITEUINT8(put, diff2);
			if (diff & SD_FLOORHT)
				WRITEFIXED(put, ss->floorheight);
			if (diff & SD_CEILHT)
				WRITEFIXED(put, ss->ceilingheight);
			if (diff & SD_FLOORPIC)
				WRITEMEM(put, levelflats[ss->floorpic].name, 8);
			if (diff & SD_CEILPIC)
				WRITEMEM(put, levelflats[ss->ceilingpic].name, 8);
			if (diff & SD_LIGHT)
				WRITEINT16(put, ss->lightlevel);
			if (diff & SD_SPECIAL)
				WRITEINT16(put, ss->special);
			if (diff2 & SD_FXOFFS)
				WRITEFIXED(put, ss->floor_xoffs);
			if (diff2 & SD_FYOFFS)
				WRITEFIXED(put, ss->floor_yoffs);
			if (diff2 & SD_CXOFFS)
				WRITEFIXED(put, ss->ceiling_xoffs);
			if (diff2 & SD_CYOFFS)
				WRITEFIXED(put, ss->ceiling_yoffs);
			if (diff2 & SD_TAG) // save only the tag
				WRITEINT16(put, ss->tag);
			if (diff2 & SD_FLOORANG)
				WRITEANGLE(put, ss->floorpic_angle);
			if (diff2 & SD_CEILANG)
				WRITEANGLE(put, ss->ceilingpic_angle);
			if (diff2 & SD_TAGLIST) // save both firsttag and nexttag
			{ // either of these could be changed even if tag isn't
				WRITEINT32(put, ss->firsttag);
				WRITEINT32(put, ss->nexttag);
			}

			// Special case: save the stats of all modified ffloors along with their ffloor "number"s
			// we don't bother with ffloors that haven't changed, that would just add to savegame even more than is really needed
			if (diff & SD_FFLOORS)
			{
				size_t j = 0; // ss->ffloors is saved as ffloor #0, ss->ffloors->next is #1, etc
				ffloor_t *rover;
				UINT8 fflr_diff;
				for (rover = ss->ffloors; rover; rover = rover->next)
				{
					fflr_diff = 0; // reset diff flags
					if (rover->flags != rover->spawnflags)
						fflr_diff |= 1;
					if (rover->alpha != rover->spawnalpha)
						fflr_diff |= 2;

					if (fflr_diff)
					{
						WRITEUINT16(put, j); // save ffloor "number"
						WRITEUINT8(put, fflr_diff);
						if (fflr_diff & 1)
							WRITEUINT32(put, rover->flags);
						if (fflr_diff & 2)
							WRITEINT16(put, rover->alpha);
					}
					j++;
				}
				WRITEUINT16(put, 0xffff);
			}
		}
	}

	WRITEUINT16(put, 0xffff);

	// do lines
	for (i = 0; i < numlines; i++, mld++, li++)
	{
		diff = diff2 = 0;

		if (li->special != SHORT(mld->special))
			diff |= LD_SPECIAL;

		if (SHORT(mld->special) == 321 || SHORT(mld->special) == 322) // only reason li->callcount would be non-zero is if either of these are involved
			diff |= LD_CLLCOUNT;

		if (li->sidenum[0] != 0xffff)
		{
			si = &sides[li->sidenum[0]];
			if (si->textureoffset != SHORT(msd[li->sidenum[0]].textureoffset)<<FRACBITS)
				diff |= LD_S1TEXOFF;
			//SoM: 4/1/2000: Some textures are colormaps. Don't worry about invalid textures.
			if (R_CheckTextureNumForName(msd[li->sidenum[0]].toptexture) != -1
					&& si->toptexture != R_TextureNumForName(msd[li->sidenum[0]].toptexture))
				diff |= LD_S1TOPTEX;
			if (R_CheckTextureNumForName(msd[li->sidenum[0]].bottomtexture) != -1
					&& si->bottomtexture != R_TextureNumForName(msd[li->sidenum[0]].bottomtexture))
				diff |= LD_S1BOTTEX;
			if (R_CheckTextureNumForName(msd[li->sidenum[0]].midtexture) != -1
					&& si->midtexture != R_TextureNumForName(msd[li->sidenum[0]].midtexture))
				diff |= LD_S1MIDTEX;
		}
		if (li->sidenum[1] != 0xffff)
		{
			si = &sides[li->sidenum[1]];
			if (si->textureoffset != SHORT(msd[li->sidenum[1]].textureoffset)<<FRACBITS)
				diff2 |= LD_S2TEXOFF;
			if (R_CheckTextureNumForName(msd[li->sidenum[1]].toptexture) != -1
					&& si->toptexture != R_TextureNumForName(msd[li->sidenum[1]].toptexture))
				diff2 |= LD_S2TOPTEX;
			if (R_CheckTextureNumForName(msd[li->sidenum[1]].bottomtexture) != -1
					&& si->bottomtexture != R_TextureNumForName(msd[li->sidenum[1]].bottomtexture))
				diff2 |= LD_S2BOTTEX;
			if (R_CheckTextureNumForName(msd[li->sidenum[1]].midtexture) != -1
					&& si->midtexture != R_TextureNumForName(msd[li->sidenum[1]].midtexture))
				diff2 |= LD_S2MIDTEX;
			if (diff2)
				diff |= LD_DIFF2;
		}

		if (diff)
		{
			WRITEINT16(put, i);
			WRITEUINT8(put, diff);
			if (diff & LD_DIFF2)
				WRITEUINT8(put, diff2);
			if (diff & LD_FLAG)
				WRITEINT16(put, li->flags);
			if (diff & LD_SPECIAL)
				WRITEINT16(put, li->special);
			if (diff & LD_CLLCOUNT)
				WRITEINT16(put, li->callcount);

			si = &sides[li->sidenum[0]];
			if (diff & LD_S1TEXOFF)
				WRITEFIXED(put, si->textureoffset);
			if (diff & LD_S1TOPTEX)
				WRITEINT32(put, si->toptexture);
			if (diff & LD_S1BOTTEX)
				WRITEINT32(put, si->bottomtexture);
			if (diff & LD_S1MIDTEX)
				WRITEINT32(put, si->midtexture);

			si = &sides[li->sidenum[1]];
			if (diff2 & LD_S2TEXOFF)
				WRITEFIXED(put, si->textureoffset);
			if (diff2 & LD_S2TOPTEX)
				WRITEINT32(put, si->toptexture);
			if (diff2 & LD_S2BOTTEX)
				WRITEINT32(put, si->bottomtexture);
			if (diff2 & LD_S2MIDTEX)
				WRITEINT32(put, si->midtexture);
		}
	}
	WRITEUINT16(put, 0xffff);
	R_ClearTextureNumCache(false);

	vres_Free(virt);
	save->p = put;
}

//
// P_NetUnArchiveWorld
//
static void P_NetUnArchiveWorld(savebuffer_t *save)
{
	UINT16 i;
	line_t *li;
	side_t *si;
	UINT8 *get;
	UINT8 diff, diff2;

	if (READUINT32(save->p) != ARCHIVEBLOCK_WORLD)
		I_Error("Bad $$$.sav at archive block World");

	get = save->p;

	for (;;)
	{
		i = READUINT16(get);

		if (i == 0xffff)
			break;

		if (i > numsectors)
			I_Error("Invalid sector number %u from server (expected end at %s)", i, sizeu1(numsectors));

		diff = READUINT8(get);
		if (diff & SD_DIFF2)
			diff2 = READUINT8(get);
		else
			diff2 = 0;

		if (diff & SD_FLOORHT)
			sectors[i].floorheight = READFIXED(get);
		if (diff & SD_CEILHT)
			sectors[i].ceilingheight = READFIXED(get);
		if (diff & SD_FLOORPIC)
		{
			sectors[i].floorpic = P_AddLevelFlatRuntime((char *)get);
			get += 8;
		}
		if (diff & SD_CEILPIC)
		{
			sectors[i].ceilingpic = P_AddLevelFlatRuntime((char *)get);
			get += 8;
		}
		if (diff & SD_LIGHT)
			sectors[i].lightlevel = READINT16(get);
		if (diff & SD_SPECIAL)
			sectors[i].special = READINT16(get);

		if (diff2 & SD_FXOFFS)
			sectors[i].floor_xoffs = READFIXED(get);
		if (diff2 & SD_FYOFFS)
			sectors[i].floor_yoffs = READFIXED(get);
		if (diff2 & SD_CXOFFS)
			sectors[i].ceiling_xoffs = READFIXED(get);
		if (diff2 & SD_CYOFFS)
			sectors[i].ceiling_yoffs = READFIXED(get);
		if (diff2 & SD_TAG)
			sectors[i].tag = READINT16(get); // DON'T use P_ChangeSectorTag
		if (diff2 & SD_TAGLIST)
		{
			sectors[i].firsttag = READINT32(get);
			sectors[i].nexttag = READINT32(get);
		}
		if (diff2 & SD_FLOORANG)
			sectors[i].floorpic_angle  = READANGLE(get);
		if (diff2 & SD_CEILANG)
			sectors[i].ceilingpic_angle = READANGLE(get);

		if (diff & SD_FFLOORS)
		{
			UINT16 j = 0; // number of current ffloor in loop
			UINT16 fflr_i; // saved ffloor "number" of next modified ffloor
			UINT16 fflr_diff; // saved ffloor diff
			ffloor_t *rover;

			rover = sectors[i].ffloors;
			if (!rover) // it is assumed sectors[i].ffloors actually exists, but just in case...
				I_Error("Sector does not have any ffloors!");

			fflr_i = READUINT16(get); // get first modified ffloor's number ready
			for (;;) // for some reason the usual for (rover = x; ...) thing doesn't work here?
			{
				if (fflr_i == 0xffff) // end of modified ffloors list, let's stop already
					break;
				// should NEVER need to be checked
				//if (rover == NULL)
					//break;
				if (j != fflr_i) // this ffloor was not modified
				{
					j++;
					rover = rover->next;
					continue;
				}

				fflr_diff = READUINT8(get);

				if (fflr_diff & 1)
					rover->flags = READUINT32(get);
				if (fflr_diff & 2)
					rover->alpha = READINT16(get);

				fflr_i = READUINT16(get); // get next ffloor "number" ready

				j++;
				rover = rover->next;
			}
		}
	}

	for (;;)
	{
		i = READUINT16(get);

		if (i == 0xffff)
			break;
		if (i > numlines)
			I_Error("Invalid line number %u from server", i);

		diff = READUINT8(get);
		li = &lines[i];

		if (diff & LD_DIFF2)
			diff2 = READUINT8(get);
		else
			diff2 = 0;
		if (diff & LD_FLAG)
			li->flags = READINT16(get);
		if (diff & LD_SPECIAL)
			li->special = READINT16(get);
		if (diff & LD_CLLCOUNT)
			li->callcount = READINT16(get);

		si = &sides[li->sidenum[0]];
		if (diff & LD_S1TEXOFF)
			si->textureoffset = READFIXED(get);
		if (diff & LD_S1TOPTEX)
			si->toptexture = READINT32(get);
		if (diff & LD_S1BOTTEX)
			si->bottomtexture = READINT32(get);
		if (diff & LD_S1MIDTEX)
			si->midtexture = READINT32(get);

		si = &sides[li->sidenum[1]];
		if (diff2 & LD_S2TEXOFF)
			si->textureoffset = READFIXED(get);
		if (diff2 & LD_S2TOPTEX)
			si->toptexture = READINT32(get);
		if (diff2 & LD_S2BOTTEX)
			si->bottomtexture = READINT32(get);
		if (diff2 & LD_S2MIDTEX)
			si->midtexture = READINT32(get);
	}

	save->p = get;
}

//
// Thinkers
//

typedef enum
{
	MD_SPAWNPOINT  = 1,
	MD_POS         = 1<<1,
	MD_TYPE        = 1<<2,
	MD_MOM         = 1<<3,
	MD_RADIUS      = 1<<4,
	MD_HEIGHT      = 1<<5,
	MD_FLAGS       = 1<<6,
	MD_HEALTH      = 1<<7,
	MD_RTIME       = 1<<8,
	MD_STATE       = 1<<9,
	MD_TICS        = 1<<10,
	MD_SPRITE      = 1<<11,
	MD_FRAME       = 1<<12,
	MD_EFLAGS      = 1<<13,
	MD_PLAYER      = 1<<14,
	MD_MOVEDIR     = 1<<15,
	MD_MOVECOUNT   = 1<<16,
	MD_THRESHOLD   = 1<<17,
	MD_LASTLOOK    = 1<<18,
	MD_TARGET      = 1<<19,
	MD_TRACER      = 1<<20,
	MD_FRICTION    = 1<<21,
	MD_MOVEFACTOR  = 1<<22,
	MD_FLAGS2      = 1<<23,
	MD_FUSE        = 1<<24,
	MD_WATERTOP    = 1<<25,
	MD_WATERBOTTOM = 1<<26,
	MD_SCALE       = 1<<27,
	MD_DSCALE      = 1<<28,
	MD_BLUEFLAG    = 1<<29,
	MD_REDFLAG     = 1<<30,
	MD_MORE        = 1<<31
} mobj_diff_t;

typedef enum
{
	MD2_CUSVAL      = 1,
	MD2_CVMEM       = 1<<1,
	MD2_SKIN        = 1<<2,
	MD2_COLOR       = 1<<3,
	MD2_SCALESPEED  = 1<<4,
	MD2_EXTVAL1     = 1<<5,
	MD2_EXTVAL2     = 1<<6,
	MD2_HNEXT       = 1<<7,
	MD2_HPREV       = 1<<8,
	MD2_COLORIZED	= 1<<9,
	MD2_WAYPOINTCAP	= 1<<10
	, MD2_SLOPE       = 1<<11
} mobj_diff2_t;

typedef enum
{
	tc_mobj,
	tc_ceiling,
	tc_floor,
	tc_flash,
	tc_strobe,
	tc_glow,
	tc_fireflicker,
	tc_thwomp,
	tc_camerascanner,
	tc_elevator,
	tc_continuousfalling,
	tc_bouncecheese,
	tc_startcrumble,
	tc_marioblock,
	tc_marioblockchecker,
	tc_spikesector,
	tc_floatsector,
	tc_bridgethinker,
	tc_crushceiling,
	tc_scroll,
	tc_friction,
	tc_pusher,
	tc_laserflash,
	tc_lightfade,
	tc_executor,
	tc_raisesector,
	tc_noenemies,
	tc_eachtime,
	tc_disappear,
	tc_polyrotate, // haleyjd 03/26/06: polyobjects
	tc_polymove,
	tc_polywaypoint,
	tc_polyslidedoor,
	tc_polyswingdoor,
	tc_polyflag,
	tc_polydisplace,
	tc_end
} specials_e;

static inline UINT32 SaveMobjnum(const mobj_t *mobj)
{
	if (mobj) return mobj->mobjnum;
	return 0;
}

static UINT32 SaveSector(const sector_t *sector)
{
	if (sector) return (UINT32)(sector - sectors);
	return 0xFFFFFFFF;
}

static UINT32 SaveLine(const line_t *line)
{
	if (line) return (UINT32)(line - lines);
	return 0xFFFFFFFF;
}

static inline UINT32 SavePlayer(const player_t *player)
{
	if (player) return (UINT32)(player - players);
	return 0xFFFFFFFF;
}


//
// SaveMobjThinker
//
// Saves a mobj_t thinker
//
static void SaveMobjThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const mobj_t *mobj = (const mobj_t *)th;
	UINT32 diff;
	UINT16 diff2;

	// Ignore stationary hoops - these will be respawned from mapthings.
	if (mobj->type == MT_HOOP)
		return;

	// These are NEVER saved.
	if (mobj->type == MT_HOOPCOLLIDE)
		return;

	// This hoop has already been collected.
	if (mobj->type == MT_HOOPCENTER && mobj->threshold == 4242)
		return;

	if (mobj->spawnpoint && mobj->info->doomednum != -1)
	{
		// spawnpoint is not modified but we must save it since it is an identifier
		diff = MD_SPAWNPOINT;

		if ((mobj->x != mobj->spawnpoint->x << FRACBITS) ||
			(mobj->y != mobj->spawnpoint->y << FRACBITS) ||
			(mobj->angle != FixedAngle(mobj->spawnpoint->angle*FRACUNIT)))
			diff |= MD_POS;

		if (mobj->info->doomednum != mobj->spawnpoint->type)
			diff |= MD_TYPE;
	}
	else
		diff = MD_POS | MD_TYPE; // not a map spawned thing so make it from scratch

	diff2 = 0;

	// not the default but the most probable
	if (mobj->momx != 0 || mobj->momy != 0 || mobj->momz != 0)
		diff |= MD_MOM;
	if (mobj->radius != mobj->info->radius)
		diff |= MD_RADIUS;
	if (mobj->height != mobj->info->height)
		diff |= MD_HEIGHT;
	if (mobj->flags != mobj->info->flags)
		diff |= MD_FLAGS;
	if (mobj->flags2)
		diff |= MD_FLAGS2;
	if (mobj->health != mobj->info->spawnhealth)
		diff |= MD_HEALTH;
	if (mobj->reactiontime != mobj->info->reactiontime)
		diff |= MD_RTIME;
	if ((statenum_t)(mobj->state-states) != mobj->info->spawnstate)
		diff |= MD_STATE;
	if (mobj->tics != mobj->state->tics)
		diff |= MD_TICS;
	if (mobj->sprite != mobj->state->sprite)
		diff |= MD_SPRITE;
	if (mobj->frame != mobj->state->frame)
		diff |= MD_FRAME;
	if (mobj->anim_duration != (UINT16)mobj->state->var2)
		diff |= MD_FRAME;
	if (mobj->eflags)
		diff |= MD_EFLAGS;
	if (mobj->player)
		diff |= MD_PLAYER;

	if (mobj->movedir)
		diff |= MD_MOVEDIR;
	if (mobj->movecount)
		diff |= MD_MOVECOUNT;
	if (mobj->threshold)
		diff |= MD_THRESHOLD;
	if (mobj->lastlook != -1)
		diff |= MD_LASTLOOK;
	if (mobj->target)
		diff |= MD_TARGET;
	if (mobj->tracer)
		diff |= MD_TRACER;
	if (mobj->friction != ORIG_FRICTION)
		diff |= MD_FRICTION;
	if (mobj->movefactor != FRACUNIT) //if (mobj->movefactor != ORIG_FRICTION_FACTOR)
		diff |= MD_MOVEFACTOR;
	if (mobj->fuse)
		diff |= MD_FUSE;
	if (mobj->watertop)
		diff |= MD_WATERTOP;
	if (mobj->waterbottom)
		diff |= MD_WATERBOTTOM;
	if (mobj->scale != FRACUNIT)
		diff |= MD_SCALE;
	if (mobj->destscale != mobj->scale)
		diff |= MD_DSCALE;
	if (mobj->scalespeed != mapobjectscale/12)
		diff2 |= MD2_SCALESPEED;

	if (mobj == redflag)
		diff |= MD_REDFLAG;
	if (mobj == blueflag)
		diff |= MD_BLUEFLAG;

	if (mobj->cusval)
		diff2 |= MD2_CUSVAL;
	if (mobj->cvmem)
		diff2 |= MD2_CVMEM;
	if (mobj->color)
		diff2 |= MD2_COLOR;
	if (mobj->skin)
		diff2 |= MD2_SKIN;
	if (mobj->extravalue1)
		diff2 |= MD2_EXTVAL1;
	if (mobj->extravalue2)
		diff2 |= MD2_EXTVAL2;
	if (mobj->hnext)
		diff2 |= MD2_HNEXT;
	if (mobj->hprev)
		diff2 |= MD2_HPREV;
	if (mobj->standingslope)
		diff2 |= MD2_SLOPE;
	if (mobj->colorized)
		diff2 |= MD2_COLORIZED;
	if (mobj == waypointcap)
		diff2 |= MD2_WAYPOINTCAP;
	if (diff2 != 0)
		diff |= MD_MORE;

	// Scrap all of that. If we're a hoop center, this is ALL we're saving.
	if (mobj->type == MT_HOOPCENTER)
		diff = MD_SPAWNPOINT;

	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, diff);
	if (diff & MD_MORE)
		WRITEUINT16(save->p, diff2);

	// save pointer, at load time we will search this pointer to reinitilize pointers
	WRITEUINT32(save->p, (size_t)mobj);

	WRITEFIXED(save->p, mobj->z); // Force this so 3dfloor problems don't arise.
	WRITEFIXED(save->p, mobj->floorz);
	WRITEFIXED(save->p, mobj->ceilingz);

	if (diff & MD_SPAWNPOINT)
	{
		size_t z;

		for (z = 0; z < nummapthings; z++)
			if (&mapthings[z] == mobj->spawnpoint)
				WRITEUINT16(save->p, z);
		if (mobj->type == MT_HOOPCENTER)
			return;
	}

	if (diff & MD_TYPE)
		WRITEUINT32(save->p, mobj->type);
	if (diff & MD_POS)
	{
		WRITEFIXED(save->p, mobj->x);
		WRITEFIXED(save->p, mobj->y);
		WRITEANGLE(save->p, mobj->angle);
	}
	if (diff & MD_MOM)
	{
		WRITEFIXED(save->p, mobj->momx);
		WRITEFIXED(save->p, mobj->momy);
		WRITEFIXED(save->p, mobj->momz);
	}
	if (diff & MD_RADIUS)
		WRITEFIXED(save->p, mobj->radius);
	if (diff & MD_HEIGHT)
		WRITEFIXED(save->p, mobj->height);
	if (diff & MD_FLAGS)
		WRITEUINT32(save->p, mobj->flags);
	if (diff & MD_FLAGS2)
		WRITEUINT32(save->p, mobj->flags2);
	if (diff & MD_HEALTH)
		WRITEINT32(save->p, mobj->health);
	if (diff & MD_RTIME)
		WRITEINT32(save->p, mobj->reactiontime);
	if (diff & MD_STATE)
		WRITEUINT16(save->p, mobj->state-states);
	if (diff & MD_TICS)
		WRITEINT32(save->p, mobj->tics);
	if (diff & MD_SPRITE)
		WRITEUINT16(save->p, mobj->sprite);
	if (diff & MD_FRAME)
	{
		WRITEUINT32(save->p, mobj->frame);
		WRITEUINT16(save->p, mobj->anim_duration);
	}
	if (diff & MD_EFLAGS)
		WRITEUINT16(save->p, mobj->eflags);
	if (diff & MD_PLAYER)
		WRITEUINT8(save->p, mobj->player-players);
	if (diff & MD_MOVEDIR)
		WRITEANGLE(save->p, mobj->movedir);
	if (diff & MD_MOVECOUNT)
		WRITEINT32(save->p, mobj->movecount);
	if (diff & MD_THRESHOLD)
		WRITEINT32(save->p, mobj->threshold);
	if (diff & MD_LASTLOOK)
		WRITEINT32(save->p, mobj->lastlook);
	if (diff & MD_TARGET)
		WRITEUINT32(save->p, mobj->target->mobjnum);
	if (diff & MD_TRACER)
		WRITEUINT32(save->p, mobj->tracer->mobjnum);
	if (diff & MD_FRICTION)
		WRITEFIXED(save->p, mobj->friction);
	if (diff & MD_MOVEFACTOR)
		WRITEFIXED(save->p, mobj->movefactor);
	if (diff & MD_FUSE)
		WRITEINT32(save->p, mobj->fuse);
	if (diff & MD_WATERTOP)
		WRITEFIXED(save->p, mobj->watertop);
	if (diff & MD_WATERBOTTOM)
		WRITEFIXED(save->p, mobj->waterbottom);
	if (diff & MD_SCALE)
		WRITEFIXED(save->p, mobj->scale);
	if (diff & MD_DSCALE)
		WRITEFIXED(save->p, mobj->destscale);
	if (diff2 & MD2_SCALESPEED)
		WRITEFIXED(save->p, mobj->scalespeed);
	if (diff2 & MD2_CUSVAL)
		WRITEINT32(save->p, mobj->cusval);
	if (diff2 & MD2_CVMEM)
		WRITEINT32(save->p, mobj->cvmem);
	if (diff2 & MD2_SKIN)
		WRITEUINT16(save->p, (UINT16)((skin_t *)mobj->skin - skins));
	if (diff2 & MD2_COLOR)
		WRITEUINT8(save->p, mobj->color);
	if (diff2 & MD2_EXTVAL1)
		WRITEINT32(save->p, mobj->extravalue1);
	if (diff2 & MD2_EXTVAL2)
		WRITEINT32(save->p, mobj->extravalue2);
	if (diff2 & MD2_HNEXT)
		WRITEUINT32(save->p, mobj->hnext->mobjnum);
	if (diff2 & MD2_HPREV)
		WRITEUINT32(save->p, mobj->hprev->mobjnum);
	if (diff2 & MD2_SLOPE)
		WRITEUINT16(save->p, mobj->standingslope->id);
	if (diff2 & MD2_COLORIZED)
		WRITEUINT8(save->p, mobj->colorized);

	WRITEUINT32(save->p, mobj->mobjnum);
}

//
// SaveSpecialLevelThinker
//
// Saves a levelspecthink_t thinker
//
static void SaveSpecialLevelThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const levelspecthink_t *ht  = (const void *)th;
	size_t i;
	WRITEUINT8(save->p, type);
	for (i = 0; i < 16; i++)
	{
		WRITEFIXED(save->p, ht->vars[i]); //var[16]
		WRITEFIXED(save->p, ht->var2s[i]); //var[16]
	}
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
	WRITEUINT32(save->p, SaveSector(ht->sector));
}

//
// SaveCeilingThinker
//
// Saves a ceiling_t thinker
//
static void SaveCeilingThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const ceiling_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEFIXED(save->p, ht->bottomheight);
	WRITEFIXED(save->p, ht->topheight);
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->oldspeed);
	WRITEFIXED(save->p, ht->delay);
	WRITEFIXED(save->p, ht->delaytimer);
	WRITEUINT8(save->p, ht->crush);
	WRITEINT32(save->p, ht->texture);
	WRITEINT32(save->p, ht->direction);
	WRITEINT32(save->p, ht->tag);
	WRITEINT32(save->p, ht->olddirection);
	WRITEFIXED(save->p, ht->origspeed);
	WRITEFIXED(save->p, ht->sourceline);
}

//
// SaveFloormoveThinker
//
// Saves a floormove_t thinker
//
static void SaveFloormoveThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const floormove_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEUINT8(save->p, ht->crush);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->direction);
	WRITEINT32(save->p, ht->texture);
	WRITEFIXED(save->p, ht->floordestheight);
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->origspeed);
	WRITEFIXED(save->p, ht->delay);
	WRITEFIXED(save->p, ht->delaytimer);
}

//
// SaveLightflashThinker
//
// Saves a lightflash_t thinker
//
static void SaveLightflashThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const lightflash_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->maxlight);
	WRITEINT32(save->p, ht->minlight);
}

//
// SaveStrobeThinker
//
// Saves a strobe_t thinker
//
static void SaveStrobeThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const strobe_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->count);
	WRITEINT32(save->p, ht->minlight);
	WRITEINT32(save->p, ht->maxlight);
	WRITEINT32(save->p, ht->darktime);
	WRITEINT32(save->p, ht->brighttime);
}

//
// SaveGlowThinker
//
// Saves a glow_t thinker
//
static void SaveGlowThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const glow_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->minlight);
	WRITEINT32(save->p, ht->maxlight);
	WRITEINT32(save->p, ht->direction);
	WRITEINT32(save->p, ht->speed);
}

//
// SaveFireflickerThinker
//
// Saves a fireflicker_t thinker
//
static inline void SaveFireflickerThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const fireflicker_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->count);
	WRITEINT32(save->p, ht->resetcount);
	WRITEINT32(save->p, ht->maxlight);
	WRITEINT32(save->p, ht->minlight);
}

//
// SaveElevatorThinker
//
// Saves a elevator_t thinker
//
static void SaveElevatorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const elevator_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEUINT32(save->p, SaveSector(ht->actionsector));
	WRITEINT32(save->p, ht->direction);
	WRITEFIXED(save->p, ht->floordestheight);
	WRITEFIXED(save->p, ht->ceilingdestheight);
	WRITEFIXED(save->p, ht->speed);
	WRITEFIXED(save->p, ht->origspeed);
	WRITEFIXED(save->p, ht->low);
	WRITEFIXED(save->p, ht->high);
	WRITEFIXED(save->p, ht->distance);
	WRITEFIXED(save->p, ht->delay);
	WRITEFIXED(save->p, ht->delaytimer);
	WRITEFIXED(save->p, ht->floorwasheight);
	WRITEFIXED(save->p, ht->ceilingwasheight);
	WRITEUINT32(save->p, SavePlayer(ht->player)); // was dummy
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
}

//
// SaveScrollThinker
//
// Saves a scroll_t thinker
//
static inline void SaveScrollThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const scroll_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEFIXED(save->p, ht->dx);
	WRITEFIXED(save->p, ht->dy);
	WRITEINT32(save->p, ht->affectee);
	WRITEINT32(save->p, ht->control);
	WRITEFIXED(save->p, ht->last_height);
	WRITEFIXED(save->p, ht->vdx);
	WRITEFIXED(save->p, ht->vdy);
	WRITEINT32(save->p, ht->accel);
	WRITEINT32(save->p, ht->exclusive);
	WRITEUINT8(save->p, ht->type);
}

//
// SaveFrictionThinker
//
// Saves a friction_t thinker
//
static inline void SaveFrictionThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const friction_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->friction);
	WRITEINT32(save->p, ht->movefactor);
	WRITEINT32(save->p, ht->affectee);
	WRITEINT32(save->p, ht->referrer);
	WRITEUINT8(save->p, ht->roverfriction);
}

//
// SavePusherThinker
//
// Saves a pusher_t thinker
//
static inline void SavePusherThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const pusher_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT8(save->p, ht->type);
	WRITEINT32(save->p, ht->x_mag);
	WRITEINT32(save->p, ht->y_mag);
	WRITEINT32(save->p, ht->magnitude);
	WRITEINT32(save->p, ht->radius);
	WRITEINT32(save->p, ht->x);
	WRITEINT32(save->p, ht->y);
	WRITEINT32(save->p, ht->z);
	WRITEINT32(save->p, ht->affectee);
	WRITEUINT8(save->p, ht->roverpusher);
	WRITEINT32(save->p, ht->referrer);
	WRITEINT32(save->p, ht->exclusive);
	WRITEINT32(save->p, ht->slider);
}

//
// SaveLaserThinker
//
// Saves a laserthink_t thinker
//
static void SaveLaserThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const laserthink_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEUINT32(save->p, SaveSector(ht->sec));
	WRITEUINT32(save->p, SaveLine(ht->sourceline));
}

//
// SaveLightlevelThinker
//
// Saves a lightlevel_t thinker
//
static void SaveLightlevelThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const lightlevel_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->destlevel);
	WRITEINT32(save->p, ht->speed);
}

//
// SaveExecutorThinker
//
// Saves a executor_t thinker
//
static void SaveExecutorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const executor_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, SaveLine(ht->line));
	WRITEUINT32(save->p, SaveMobjnum(ht->caller));
	WRITEUINT32(save->p, SaveSector(ht->sector));
	WRITEINT32(save->p, ht->timer);
}

//
// SaveDisappearThinker
//
// Saves a disappear_t thinker
//
static void SaveDisappearThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const disappear_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEUINT32(save->p, ht->appeartime);
	WRITEUINT32(save->p, ht->disappeartime);
	WRITEUINT32(save->p, ht->offset);
	WRITEUINT32(save->p, ht->timer);
	WRITEINT32(save->p, ht->affectee);
	WRITEINT32(save->p, ht->sourceline);
	WRITEINT32(save->p, ht->exists);
}

//
// SavePolyrotateThinker
//
// Saves a polyrotate_t thinker
//
static inline void SavePolyrotatetThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyrotate_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->distance);
	WRITEUINT8(save->p, ht->turnobjs);
}

//
// SavePolymoveThinker
//
// Saves a polymovet_t thinker
//
static void SavePolymoveThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polymove_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->speed);
	WRITEFIXED(save->p, ht->momx);
	WRITEFIXED(save->p, ht->momy);
	WRITEINT32(save->p, ht->distance);
	WRITEANGLE(save->p, ht->angle);
}

//
// SavePolywaypointThinker
//
// Saves a polywaypoint_t thinker
//
static void SavePolywaypointThinker(savebuffer_t *save, const thinker_t *th, UINT8 type)
{
	const polywaypoint_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->sequence);
	WRITEINT32(save->p, ht->pointnum);
	WRITEINT32(save->p, ht->direction);
	WRITEUINT8(save->p, ht->comeback);
	WRITEUINT8(save->p, ht->wrap);
	WRITEUINT8(save->p, ht->continuous);
	WRITEUINT8(save->p, ht->stophere);
	WRITEFIXED(save->p, ht->diffx);
	WRITEFIXED(save->p, ht->diffy);
	WRITEFIXED(save->p, ht->diffz);
}

//
// SavePolyslidedoorThinker
//
// Saves a polyslidedoor_t thinker
//
static void SavePolyslidedoorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyslidedoor_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->delay);
	WRITEINT32(save->p, ht->delayCount);
	WRITEINT32(save->p, ht->initSpeed);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->initDistance);
	WRITEINT32(save->p, ht->distance);
	WRITEUINT32(save->p, ht->initAngle);
	WRITEUINT32(save->p, ht->angle);
	WRITEUINT32(save->p, ht->revAngle);
	WRITEFIXED(save->p, ht->momx);
	WRITEFIXED(save->p, ht->momy);
	WRITEUINT8(save->p, ht->closing);
}

//
// SavePolyswingdoorThinker
//
// Saves a polyswingdoor_t thinker
//
static void SavePolyswingdoorThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polyswingdoor_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEINT32(save->p, ht->delay);
	WRITEINT32(save->p, ht->delayCount);
	WRITEINT32(save->p, ht->initSpeed);
	WRITEINT32(save->p, ht->speed);
	WRITEINT32(save->p, ht->initDistance);
	WRITEINT32(save->p, ht->distance);
	WRITEUINT8(save->p, ht->closing);
}

static void SavePolydisplaceThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
{
	const polydisplace_t *ht = (const void *)th;
	WRITEUINT8(save->p, type);
	WRITEINT32(save->p, ht->polyObjNum);
	WRITEUINT32(save->p, SaveSector(ht->controlSector));
	WRITEFIXED(save->p, ht->dx);
	WRITEFIXED(save->p, ht->dy);
	WRITEFIXED(save->p, ht->oldHeights);
}

//
// P_NetArchiveThinkers
//
//
static void P_NetArchiveThinkers(savebuffer_t *save)
{
	const thinker_t *th;
	UINT32 numsaved = 0;

	WRITEUINT32(save->p, ARCHIVEBLOCK_THINKERS);

	// save off the current thinkers
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (!(th->function.acp1 == (actionf_p1)P_RemoveThinkerDelayed
		 || th->function.acp1 == (actionf_p1)P_NullPrecipThinker))
			numsaved++;

		if (th->function.acp1 == (actionf_p1)P_MobjThinker)
		{
			SaveMobjThinker(save, th, tc_mobj);
			continue;
		}
#ifdef PARANOIA
		else if (th->function.acp1 == (actionf_p1)P_NullPrecipThinker);
#endif
		else if (th->function.acp1 == (actionf_p1)T_MoveCeiling)
		{
			SaveCeilingThinker(save, th, tc_ceiling);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_CrushCeiling)
		{
			SaveCeilingThinker(save, th, tc_crushceiling);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_MoveFloor)
		{
			SaveFloormoveThinker(save, th, tc_floor);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_LightningFlash)
		{
			SaveLightflashThinker(save, th, tc_flash);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_StrobeFlash)
		{
			SaveStrobeThinker(save, th, tc_strobe);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_Glow)
		{
			SaveGlowThinker(save, th, tc_glow);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_FireFlicker)
		{
			SaveFireflickerThinker(save, th, tc_fireflicker);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_MoveElevator)
		{
			SaveElevatorThinker(save, th, tc_elevator);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_ContinuousFalling)
		{
			SaveSpecialLevelThinker(save, th, tc_continuousfalling);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_ThwompSector)
		{
			SaveSpecialLevelThinker(save, th, tc_thwomp);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_NoEnemiesSector)
		{
			SaveSpecialLevelThinker(save, th, tc_noenemies);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_EachTimeThinker)
		{
			SaveSpecialLevelThinker(save, th, tc_eachtime);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_RaiseSector)
		{
			SaveSpecialLevelThinker(save, th, tc_raisesector);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_CameraScanner)
		{
			SaveElevatorThinker(save, th, tc_camerascanner);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_Scroll)
		{
			SaveScrollThinker(save, th, tc_scroll);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_Friction)
		{
			SaveFrictionThinker(save, th, tc_friction);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_Pusher)
		{
			SavePusherThinker(save, th, tc_pusher);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_BounceCheese)
		{
			SaveSpecialLevelThinker(save, th, tc_bouncecheese);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_StartCrumble)
		{
			SaveElevatorThinker(save, th, tc_startcrumble);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_MarioBlock)
		{
			SaveSpecialLevelThinker(save, th, tc_marioblock);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_MarioBlockChecker)
		{
			SaveSpecialLevelThinker(save, th, tc_marioblockchecker);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_SpikeSector)
		{
			SaveSpecialLevelThinker(save, th, tc_spikesector);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_FloatSector)
		{
			SaveSpecialLevelThinker(save, th, tc_floatsector);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_BridgeThinker)
		{
			SaveSpecialLevelThinker(save, th, tc_bridgethinker);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_LaserFlash)
		{
			SaveLaserThinker(save, th, tc_laserflash);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_LightFade)
		{
			SaveLightlevelThinker(save, th, tc_lightfade);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_ExecutorDelay)
		{
			SaveExecutorThinker(save, th, tc_executor);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_Disappear)
		{
			SaveDisappearThinker(save, th, tc_disappear);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_PolyObjRotate)
		{
			SavePolyrotatetThinker(save, th, tc_polyrotate);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_PolyObjMove)
		{
			SavePolymoveThinker(save, th, tc_polymove);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_PolyObjWaypoint)
		{
			SavePolywaypointThinker(save, th, tc_polywaypoint);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_PolyDoorSlide)
		{
			SavePolyslidedoorThinker(save, th, tc_polyslidedoor);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_PolyDoorSwing)
		{
			SavePolyswingdoorThinker(save, th, tc_polyswingdoor);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_PolyObjFlag)
		{
			SavePolymoveThinker(save, th, tc_polyflag);
			continue;
		}
		else if (th->function.acp1 == (actionf_p1)T_PolyObjDisplace)
		{
			SavePolydisplaceThinker(save, th, tc_polydisplace);
			continue;
		}
#ifdef PARANOIA
		else if (th->function.acv != P_RemoveThinkerDelayed) // wait garbage collection
			I_Error("unknown thinker type %p", th->function.acp1);
#endif
	}

	CONS_Debug(DBG_NETPLAY, "%u thinkers saved\n", numsaved);

	WRITEUINT8(save->p, tc_end);
}

// Now save the pointers, tracer and target, but at load time we must
// relink to this; the savegame contains the old position in the pointer
// field copyed in the info field temporarily, but finally we just search
// for the old position and relink to it.
mobj_t *P_FindNewPosition(UINT32 oldposition)
{
	thinker_t *th;
	mobj_t *mobj;

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mobj = (mobj_t *)th;

		if (mobj->mobjnum == oldposition)
			return mobj;
	}
	CONS_Debug(DBG_GAMELOGIC, "mobj not found\n");
	return NULL;
}

static inline mobj_t *LoadMobj(UINT32 mobjnum)
{
	if (mobjnum == 0) return NULL;
	return (mobj_t *)(size_t)mobjnum;
}

static sector_t *LoadSector(UINT32 sector)
{
	if (sector >= numsectors) return NULL;
	return &sectors[sector];
}

static line_t *LoadLine(UINT32 line)
{
	if (line >= numlines) return NULL;
	return &lines[line];
}

static inline player_t *LoadPlayer(UINT32 player)
{
	if (player >= MAXPLAYERS) return NULL;
	return &players[player];
}

//
// LoadMobjThinker
//
// Loads a mobj_t from a save game
//
static void LoadMobjThinker(savebuffer_t *save, actionf_p1 thinker)
{
	thinker_t *next;
	mobj_t *mobj;
	UINT32 diff;
	UINT16 diff2;
	INT32 i;
	fixed_t z, floorz, ceilingz;

	diff = READUINT32(save->p);
	if (diff & MD_MORE)
		diff2 = READUINT16(save->p);
	else
		diff2 = 0;

	next = (void *)(size_t)READUINT32(save->p);

	z = READFIXED(save->p); // Force this so 3dfloor problems don't arise.
	floorz = READFIXED(save->p);
	ceilingz = READFIXED(save->p);

	if (diff & MD_SPAWNPOINT)
	{
		UINT16 spawnpointnum = READUINT16(save->p);

		if (mapthings[spawnpointnum].type == 1705 || mapthings[spawnpointnum].type == 1713) // NiGHTS Hoop special case
		{
			P_SpawnHoopsAndRings(&mapthings[spawnpointnum]);
			return;
		}

		mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);

		mobj->spawnpoint = &mapthings[spawnpointnum];
		mapthings[spawnpointnum].mobj = mobj;
	}
	else
		mobj = Z_Calloc(sizeof (*mobj), PU_LEVEL, NULL);

	// declare this as a valid mobj as soon as possible.
	mobj->thinker.function.acp1 = thinker;

	mobj->z = z;
	mobj->floorz = floorz;
	mobj->ceilingz = ceilingz;

	if (diff & MD_TYPE)
		mobj->type = READUINT32(save->p);
	else
	{
		for (i = 0; i < NUMMOBJTYPES; i++)
			if (mobj->spawnpoint && mobj->spawnpoint->type == mobjinfo[i].doomednum)
				break;
		if (i == NUMMOBJTYPES)
		{
			if (mobj->spawnpoint)
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing type %d\n", mobj->spawnpoint->type);
			else
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing type NULL\n");
			I_Error("Savegame corrupted");
		}
		mobj->type = i;
	}
	mobj->info = &mobjinfo[mobj->type];
	if (diff & MD_POS)
	{
		mobj->x = READFIXED(save->p);
		mobj->y = READFIXED(save->p);
		mobj->angle = READANGLE(save->p);
	}
	else
	{
		mobj->x = mobj->spawnpoint->x << FRACBITS;
		mobj->y = mobj->spawnpoint->y << FRACBITS;
		mobj->angle = FixedAngle(mobj->spawnpoint->angle*FRACUNIT);
	}
	if (diff & MD_MOM)
	{
		mobj->momx = READFIXED(save->p);
		mobj->momy = READFIXED(save->p);
		mobj->momz = READFIXED(save->p);
	} // otherwise they're zero, and the memset took care of it

	if (diff & MD_RADIUS)
		mobj->radius = READFIXED(save->p);
	else
		mobj->radius = mobj->info->radius;
	if (diff & MD_HEIGHT)
		mobj->height = READFIXED(save->p);
	else
		mobj->height = mobj->info->height;
	if (diff & MD_FLAGS)
		mobj->flags = READUINT32(save->p);
	else
		mobj->flags = mobj->info->flags;
	if (diff & MD_FLAGS2)
		mobj->flags2 = READUINT32(save->p);
	if (diff & MD_HEALTH)
		mobj->health = READINT32(save->p);
	else
		mobj->health = mobj->info->spawnhealth;
	if (diff & MD_RTIME)
		mobj->reactiontime = READINT32(save->p);
	else
		mobj->reactiontime = mobj->info->reactiontime;

	if (diff & MD_STATE)
		mobj->state = &states[READUINT16(save->p)];
	else
		mobj->state = &states[mobj->info->spawnstate];
	if (diff & MD_TICS)
		mobj->tics = READINT32(save->p);
	else
		mobj->tics = mobj->state->tics;
	if (diff & MD_SPRITE)
		mobj->sprite = READUINT16(save->p);
	else
		mobj->sprite = mobj->state->sprite;
	if (diff & MD_FRAME)
	{
		mobj->frame = READUINT32(save->p);
		mobj->anim_duration = READUINT16(save->p);
	}
	else
	{
		mobj->frame = mobj->state->frame;
		mobj->anim_duration = (UINT16)mobj->state->var2;
	}
	if (diff & MD_EFLAGS)
		mobj->eflags = READUINT16(save->p);
	if (diff & MD_PLAYER)
	{
		i = READUINT8(save->p);
		mobj->player = &players[i];
		mobj->player->mo = mobj;
		// added for angle prediction
		if (consoleplayer == i)
			localangle[0] = mobj->angle;
		if (displayplayers[1] == i)
			localangle[1] = mobj->angle;
		if (displayplayers[2] == i)
			localangle[2] = mobj->angle;
		if (displayplayers[3] == i)
			localangle[3] = mobj->angle;
	}
	if (diff & MD_MOVEDIR)
		mobj->movedir = READANGLE(save->p);
	if (diff & MD_MOVECOUNT)
		mobj->movecount = READINT32(save->p);
	if (diff & MD_THRESHOLD)
		mobj->threshold = READINT32(save->p);
	if (diff & MD_LASTLOOK)
		mobj->lastlook = READINT32(save->p);
	else
		mobj->lastlook = -1;
	if (diff & MD_TARGET)
		mobj->target = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff & MD_TRACER)
		mobj->tracer = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff & MD_FRICTION)
		mobj->friction = READFIXED(save->p);
	else
		mobj->friction = ORIG_FRICTION;
	if (diff & MD_MOVEFACTOR)
		mobj->movefactor = READFIXED(save->p);
	else
		mobj->movefactor = FRACUNIT; //mobj->movefactor = ORIG_FRICTION_FACTOR;
	if (diff & MD_FUSE)
		mobj->fuse = READINT32(save->p);
	if (diff & MD_WATERTOP)
		mobj->watertop = READFIXED(save->p);
	if (diff & MD_WATERBOTTOM)
		mobj->waterbottom = READFIXED(save->p);
	if (diff & MD_SCALE)
		mobj->scale = READFIXED(save->p);
	else
		mobj->scale = FRACUNIT;
	if (diff & MD_DSCALE)
		mobj->destscale = READFIXED(save->p);
	else
		mobj->destscale = mobj->scale;
	if (diff2 & MD2_SCALESPEED)
		mobj->scalespeed = READFIXED(save->p);
	else
		mobj->scalespeed = mapobjectscale/12;
	if (diff2 & MD2_CUSVAL)
		mobj->cusval = READINT32(save->p);
	if (diff2 & MD2_CVMEM)
		mobj->cvmem = READINT32(save->p);
	if (diff2 & MD2_SKIN)
		mobj->skin = &skins[READUINT16(save->p)];
	if (diff2 & MD2_COLOR)
		mobj->color = READUINT8(save->p);
	if (diff2 & MD2_EXTVAL1)
		mobj->extravalue1 = READINT32(save->p);
	if (diff2 & MD2_EXTVAL2)
		mobj->extravalue2 = READINT32(save->p);
	if (diff2 & MD2_HNEXT)
		mobj->hnext = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff2 & MD2_HPREV)
		mobj->hprev = (mobj_t *)(size_t)READUINT32(save->p);
	if (diff2 & MD2_SLOPE)
		mobj->standingslope = P_SlopeById(READUINT16(save->p));
	if (diff2 & MD2_COLORIZED)
		mobj->colorized = READUINT8(save->p);

	//{ Saturn stuff, needs to be set, but shouldnt be synched

	// Sprite Rotation
	mobj->rollangle = 0;
	mobj->pitch = 0;
	mobj->roll = 0;
	mobj->sloperoll = 0;
	mobj->slopepitch = 0;
	mobj->pitch_sprite = 0;
	mobj->roll_sprite = 0;

	// Horizontal flip
	mobj->mirrored = 0;

	// Sprite Rendering stuff
	mobj->spritexoffset = 0;
	mobj->spriteyoffset = 0;
	mobj->spritexscale = FRACUNIT;
	mobj->spriteyscale = FRACUNIT;
	mobj->realxscale = FRACUNIT;
	mobj->realyscale = FRACUNIT;
	mobj->stretchslam = 0;

	mobj->stretchslam = 0;
	mobj->slamsoundtimer = 0;

	mobj->mirrored = 0;

	// Timer for slam sound effect
	mobj->slamsoundtimer = 0;

	//}

	if (diff & MD_REDFLAG)
	{
		redflag = mobj;
		rflagpoint = mobj->spawnpoint;
	}
	if (diff & MD_BLUEFLAG)
	{
		blueflag = mobj;
		bflagpoint = mobj->spawnpoint;
	}

	// set sprev, snext, bprev, bnext, subsector
	P_SetThingPosition(mobj);

	mobj->mobjnum = READUINT32(save->p);

	if (mobj->player)
	{
		if (mobj->eflags & MFE_VERTICALFLIP)
			mobj->player->viewz = mobj->z + mobj->height - mobj->player->viewheight;
		else
			mobj->player->viewz = mobj->player->mo->z + mobj->player->viewheight;
	}

	if (mobj->type == MT_SKYBOX)
	{
		if (mobj->spawnpoint->options & MTF_OBJECTSPECIAL)
			skyboxmo[1] = mobj;
		else
			skyboxmo[0] = mobj;
	}

	P_AddThinker(&mobj->thinker);

	if (diff2 & MD2_WAYPOINTCAP)
		P_SetTarget(&waypointcap, mobj);

	mobj->info = (mobjinfo_t *)next; // temporarily, set when leave this function

	R_AddMobjInterpolator(mobj);
}

//
// LoadSpecialLevelThinker
//
// Loads a levelspecthink_t from a save game
//
// floorOrCeiling:
//		0 - Don't set
//		1 - Floor Only
//		2 - Ceiling Only
//		3 - Both
//
static void LoadSpecialLevelThinker(savebuffer_t *save, actionf_p1 thinker, UINT8 floorOrCeiling)
{
	levelspecthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	size_t i;
	ht->thinker.function.acp1 = thinker;
	for (i = 0; i < 16; i++)
	{
		ht->vars[i] = READFIXED(save->p); //var[16]
		ht->var2s[i] = READFIXED(save->p); //var[16]
	}
	ht->sourceline = LoadLine(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));

	if (ht->sector)
	{
		if (floorOrCeiling & 2)
			ht->sector->ceilingdata = ht;
		if (floorOrCeiling & 1)
			ht->sector->floordata = ht;
	}

	P_AddThinker(&ht->thinker);
}

//
// LoadCeilingThinker
//
// Loads a ceiling_t from a save game
//
static void LoadCeilingThinker(savebuffer_t *save, actionf_p1 thinker)
{
	ceiling_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->sector = LoadSector(READUINT32(save->p));
	ht->bottomheight = READFIXED(save->p);
	ht->topheight = READFIXED(save->p);
	ht->speed = READFIXED(save->p);
	ht->oldspeed = READFIXED(save->p);
	ht->delay = READFIXED(save->p);
	ht->delaytimer = READFIXED(save->p);
	ht->crush = READUINT8(save->p);
	ht->texture = READINT32(save->p);
	ht->direction = READINT32(save->p);
	ht->tag = READINT32(save->p);
	ht->olddirection = READINT32(save->p);
	ht->origspeed = READFIXED(save->p);
	ht->sourceline = READFIXED(save->p);
	if (ht->sector)
		ht->sector->ceilingdata = ht;
	P_AddThinker(&ht->thinker);
}

//
// LoadFloormoveThinker
//
// Loads a floormove_t from a save game
//
static void LoadFloormoveThinker(savebuffer_t *save, actionf_p1 thinker)
{
	floormove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->crush = READUINT8(save->p);
	ht->sector = LoadSector(READUINT32(save->p));
	ht->direction = READINT32(save->p);
	ht->texture = READINT32(save->p);
	ht->floordestheight = READFIXED(save->p);
	ht->speed = READFIXED(save->p);
	ht->origspeed = READFIXED(save->p);
	ht->delay = READFIXED(save->p);
	ht->delaytimer = READFIXED(save->p);
	if (ht->sector)
		ht->sector->floordata = ht;
	P_AddThinker(&ht->thinker);
}

//
// LoadLightflashThinker
//
// Loads a lightflash_t from a save game
//
static void LoadLightflashThinker(savebuffer_t *save, actionf_p1 thinker)
{
	lightflash_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->maxlight = READINT32(save->p);
	ht->minlight = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	P_AddThinker(&ht->thinker);
}

//
// LoadStrobeThinker
//
// Loads a strobe_t from a save game
//
static void LoadStrobeThinker(savebuffer_t *save, actionf_p1 thinker)
{
	strobe_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->count = READINT32(save->p);
	ht->minlight = READINT32(save->p);
	ht->maxlight = READINT32(save->p);
	ht->darktime = READINT32(save->p);
	ht->brighttime = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	P_AddThinker(&ht->thinker);
}

//
// LoadGlowThinker
//
// Loads a glow_t from a save game
//
static void LoadGlowThinker(savebuffer_t *save, actionf_p1 thinker)
{
	glow_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->minlight = READINT32(save->p);
	ht->maxlight = READINT32(save->p);
	ht->direction = READINT32(save->p);
	ht->speed = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	P_AddThinker(&ht->thinker);
}

//
// LoadFireflickerThinker
//
// Loads a fireflicker_t from a save game
//
static void LoadFireflickerThinker(savebuffer_t *save, actionf_p1 thinker)
{
	fireflicker_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->count = READINT32(save->p);
	ht->resetcount = READINT32(save->p);
	ht->maxlight = READINT32(save->p);
	ht->minlight = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	P_AddThinker(&ht->thinker);
}

//
// LoadElevatorThinker
//
// Loads a elevator_t from a save game
//
static void LoadElevatorThinker(savebuffer_t *save, actionf_p1 thinker, UINT8 floorOrCeiling)
{
	elevator_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->sector = LoadSector(READUINT32(save->p));
	ht->actionsector = LoadSector(READUINT32(save->p));
	ht->direction = READINT32(save->p);
	ht->floordestheight = READFIXED(save->p);
	ht->ceilingdestheight = READFIXED(save->p);
	ht->speed = READFIXED(save->p);
	ht->origspeed = READFIXED(save->p);
	ht->low = READFIXED(save->p);
	ht->high = READFIXED(save->p);
	ht->distance = READFIXED(save->p);
	ht->delay = READFIXED(save->p);
	ht->delaytimer = READFIXED(save->p);
	ht->floorwasheight = READFIXED(save->p);
	ht->ceilingwasheight = READFIXED(save->p);
	ht->player = LoadPlayer(READUINT32(save->p)); // was dummy
	ht->sourceline = LoadLine(READUINT32(save->p));

	if (ht->sector)
	{
		if (floorOrCeiling & 2)
			ht->sector->ceilingdata = ht;
		if (floorOrCeiling & 1)
			ht->sector->floordata = ht;
	}

	P_AddThinker(&ht->thinker);
}

//
// LoadScrollThinker
//
// Loads a scroll_t from a save game
//
static void LoadScrollThinker(savebuffer_t *save, actionf_p1 thinker)
{
	scroll_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->dx = READFIXED(save->p);
	ht->dy = READFIXED(save->p);
	ht->affectee = READINT32(save->p);
	ht->control = READINT32(save->p);
	ht->last_height = READFIXED(save->p);
	ht->vdx = READFIXED(save->p);
	ht->vdy = READFIXED(save->p);
	ht->accel = READINT32(save->p);
	ht->exclusive = READINT32(save->p);
	ht->type = READUINT8(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadFrictionThinker
//
// Loads a friction_t from a save game
//
static inline void LoadFrictionThinker(savebuffer_t *save, actionf_p1 thinker)
{
	friction_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->friction = READINT32(save->p);
	ht->movefactor = READINT32(save->p);
	ht->affectee = READINT32(save->p);
	ht->referrer = READINT32(save->p);
	ht->roverfriction = READUINT8(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadPusherThinker
//
// Loads a pusher_t from a save game
//
static void LoadPusherThinker(savebuffer_t *save, actionf_p1 thinker)
{
	pusher_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->type = READUINT8(save->p);
	ht->x_mag = READINT32(save->p);
	ht->y_mag = READINT32(save->p);
	ht->magnitude = READINT32(save->p);
	ht->radius = READINT32(save->p);
	ht->x = READINT32(save->p);
	ht->y = READINT32(save->p);
	ht->z = READINT32(save->p);
	ht->affectee = READINT32(save->p);
	ht->roverpusher = READUINT8(save->p);
	ht->referrer = READINT32(save->p);
	ht->exclusive = READINT32(save->p);
	ht->slider = READINT32(save->p);
	ht->source = P_GetPushThing(ht->affectee);
	P_AddThinker(&ht->thinker);
}

//
// LoadLaserThinker
//
// Loads a laserthink_t from a save game
//
static inline void LoadLaserThinker(savebuffer_t *save, actionf_p1 thinker)
{
	laserthink_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ffloor_t *rover = NULL;
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->sec = LoadSector(READUINT32(save->p));
	ht->sourceline = LoadLine(READUINT32(save->p));
	for (rover = ht->sector->ffloors; rover; rover = rover->next)
		if (rover->secnum == (size_t)(ht->sec - sectors)
		&& rover->master == ht->sourceline)
			ht->ffloor = rover;
	P_AddThinker(&ht->thinker);
}

//
// LoadLightlevelThinker
//
// Loads a lightlevel_t from a save game
//
FUNCINLINE static ATTRINLINE void LoadLightlevelThinker(savebuffer_t *save, actionf_p1 thinker)
{
	lightlevel_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->destlevel = READINT32(save->p);
	ht->speed = READINT32(save->p);
	if (ht->sector)
		ht->sector->lightingdata = ht;
	P_AddThinker(&ht->thinker);
}

//
// LoadExecutorThinker
//
// Loads a executor_t from a save game
//
static inline void LoadExecutorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	executor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->line = LoadLine(READUINT32(save->p));
	ht->caller = LoadMobj(READUINT32(save->p));
	ht->sector = LoadSector(READUINT32(save->p));
	ht->timer = READINT32(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadDisappearThinker
//
// Loads a disappear_t thinker
//
static inline void LoadDisappearThinker(savebuffer_t *save, actionf_p1 thinker)
{
	disappear_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->appeartime = READUINT32(save->p);
	ht->disappeartime = READUINT32(save->p);
	ht->offset = READUINT32(save->p);
	ht->timer = READUINT32(save->p);
	ht->affectee = READINT32(save->p);
	ht->sourceline = READINT32(save->p);
	ht->exists = READINT32(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadPolyrotateThinker
//
// Loads a polyrotate_t thinker
//
static inline void LoadPolyrotatetThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyrotate_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->turnobjs = READUINT8(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadPolymoveThinker
//
// Loads a polymovet_t thinker
//
static void LoadPolymoveThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polymove_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->momx = READFIXED(save->p);
	ht->momy = READFIXED(save->p);
	ht->distance = READINT32(save->p);
	ht->angle = READANGLE(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadPolywaypointThinker
//
// Loads a polywaypoint_t thinker
//
static inline void LoadPolywaypointThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polywaypoint_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->sequence = READINT32(save->p);
	ht->pointnum = READINT32(save->p);
	ht->direction = READINT32(save->p);
	ht->comeback = READUINT8(save->p);
	ht->wrap = READUINT8(save->p);
	ht->continuous = READUINT8(save->p);
	ht->stophere = READUINT8(save->p);
	ht->diffx = READFIXED(save->p);
	ht->diffy = READFIXED(save->p);
	ht->diffz = READFIXED(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadPolyslidedoorThinker
//
// loads a polyslidedoor_t thinker
//
static inline void LoadPolyslidedoorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyslidedoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->delay = READINT32(save->p);
	ht->delayCount = READINT32(save->p);
	ht->initSpeed = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->initDistance = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->initAngle = READUINT32(save->p);
	ht->angle = READUINT32(save->p);
	ht->revAngle = READUINT32(save->p);
	ht->momx = READFIXED(save->p);
	ht->momy = READFIXED(save->p);
	ht->closing = READUINT8(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadPolyswingdoorThinker
//
// Loads a polyswingdoor_t thinker
//
static inline void LoadPolyswingdoorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyswingdoor_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->delay = READINT32(save->p);
	ht->delayCount = READINT32(save->p);
	ht->initSpeed = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->initDistance = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->closing = READUINT8(save->p);
	P_AddThinker(&ht->thinker);
}

//
// LoadPolydisplaceThinker
//
// Loads a polydisplace_t thinker
//
static inline void LoadPolydisplaceThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polydisplace_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->controlSector = LoadSector(READUINT32(save->p));
	ht->dx = READFIXED(save->p);
	ht->dy = READFIXED(save->p);
	ht->oldHeights = READFIXED(save->p);
	P_AddThinker(&ht->thinker);
}

/*
//
// LoadWhatThinker
//
// load a what_t thinker
//
static inline void LoadWhatThinker(savebuffer_t *save, actionf_p1 thinker)
{
	what_t *ht = Z_Malloc(sizeof (*ht), PU_LEVSPEC, NULL);
	ht->thinker.function.acp1 = thinker;
}
*/

//
// P_NetUnArchiveThinkers
//
static void P_NetUnArchiveThinkers(savebuffer_t *save)
{
	thinker_t *currentthinker;
	thinker_t *next;
	UINT8 tclass;
	UINT8 restoreNum = false;
	UINT32 i;
	UINT32 numloaded = 0;

	if (READUINT32(save->p) != ARCHIVEBLOCK_THINKERS)
		I_Error("Bad $$$.sav at archive block Thinkers");

	// remove all the current thinkers
	currentthinker = thinkercap.next;
	for (currentthinker = thinkercap.next; currentthinker != &thinkercap; currentthinker = next)
	{
		next = currentthinker->next;

		if (currentthinker->function.acp1 == (actionf_p1)P_MobjThinker || currentthinker->function.acp1 == (actionf_p1)P_NullPrecipThinker)
			P_RemoveSavegameMobj((mobj_t *)currentthinker); // item isn't saved, don't remove it
		else
		{
			(next->prev = currentthinker->prev)->next = next;
			R_DestroyLevelInterpolators(currentthinker);
			Z_Free(currentthinker);
		}
	}

	// we don't want the removed mobjs to come back
	iquetail = iquehead = 0;
	P_InitThinkers();

	// clear sector thinker pointers so they don't point to non-existant thinkers for all of eternity
	for (i = 0; i < numsectors; i++)
	{
		sectors[i].floordata = sectors[i].ceilingdata = sectors[i].lightingdata = NULL;
	}

	// read in saved thinkers
	for (;;)
	{
		tclass = READUINT8(save->p);

		if (tclass == tc_end)
			break; // leave the saved thinker reading loop
		numloaded++;

		switch (tclass)
		{
			case tc_mobj:
				LoadMobjThinker(save, (actionf_p1)P_MobjThinker);
				break;

			case tc_ceiling:
				LoadCeilingThinker(save, (actionf_p1)T_MoveCeiling);
				break;

			case tc_crushceiling:
				LoadCeilingThinker(save, (actionf_p1)T_CrushCeiling);
				break;

			case tc_floor:
				LoadFloormoveThinker(save, (actionf_p1)T_MoveFloor);
				break;

			case tc_flash:
				LoadLightflashThinker(save, (actionf_p1)T_LightningFlash);
				break;

			case tc_strobe:
				LoadStrobeThinker(save, (actionf_p1)T_StrobeFlash);
				break;

			case tc_glow:
				LoadGlowThinker(save, (actionf_p1)T_Glow);
				break;

			case tc_fireflicker:
				LoadFireflickerThinker(save, (actionf_p1)T_FireFlicker);
				break;

			case tc_elevator:
				LoadElevatorThinker(save, (actionf_p1)T_MoveElevator, 3);
				break;

			case tc_continuousfalling:
				LoadSpecialLevelThinker(save, (actionf_p1)T_ContinuousFalling, 3);
				break;

			case tc_thwomp:
				LoadSpecialLevelThinker(save, (actionf_p1)T_ThwompSector, 3);
				break;

			case tc_noenemies:
				LoadSpecialLevelThinker(save, (actionf_p1)T_NoEnemiesSector, 0);
				break;

			case tc_eachtime:
				LoadSpecialLevelThinker(save, (actionf_p1)T_EachTimeThinker, 0);
				break;

			case tc_raisesector:
				LoadSpecialLevelThinker(save, (actionf_p1)T_RaiseSector, 0);
				break;

			/// \todo rewrite all the code that uses an elevator_t but isn't an elevator
			/// \note working on it!
			case tc_camerascanner:
				LoadElevatorThinker(save, (actionf_p1)T_CameraScanner, 0);
				break;

			case tc_bouncecheese:
				LoadSpecialLevelThinker(save, (actionf_p1)T_BounceCheese, 2);
				break;

			case tc_startcrumble:
				LoadElevatorThinker(save, (actionf_p1)T_StartCrumble, 1);
				break;

			case tc_marioblock:
				LoadSpecialLevelThinker(save, (actionf_p1)T_MarioBlock, 3);
				break;

			case tc_marioblockchecker:
				LoadSpecialLevelThinker(save, (actionf_p1)T_MarioBlockChecker, 0);
				break;

			case tc_spikesector:
				LoadSpecialLevelThinker(save, (actionf_p1)T_SpikeSector, 0);
				break;

			case tc_floatsector:
				LoadSpecialLevelThinker(save, (actionf_p1)T_FloatSector, 0);
				break;

			case tc_bridgethinker:
				LoadSpecialLevelThinker(save, (actionf_p1)T_BridgeThinker, 3);
				break;

			case tc_laserflash:
				LoadLaserThinker(save, (actionf_p1)T_LaserFlash);
				break;

			case tc_lightfade:
				LoadLightlevelThinker(save, (actionf_p1)T_LightFade);
				break;

			case tc_executor:
				LoadExecutorThinker(save, (actionf_p1)T_ExecutorDelay);
				restoreNum = true;
				break;

			case tc_disappear:
				LoadDisappearThinker(save, (actionf_p1)T_Disappear);
				break;

			case tc_polyrotate:
				LoadPolyrotatetThinker(save, (actionf_p1)T_PolyObjRotate);
				break;

			case tc_polymove:
				LoadPolymoveThinker(save, (actionf_p1)T_PolyObjMove);
				break;

			case tc_polywaypoint:
				LoadPolywaypointThinker(save, (actionf_p1)T_PolyObjWaypoint);
				restoreNum = true;
				break;

			case tc_polyslidedoor:
				LoadPolyslidedoorThinker(save, (actionf_p1)T_PolyDoorSlide);
				break;

			case tc_polyswingdoor:
				LoadPolyswingdoorThinker(save, (actionf_p1)T_PolyDoorSwing);
				break;

			case tc_polyflag:
				LoadPolymoveThinker(save, (actionf_p1)T_PolyObjFlag);
				break;

			case tc_polydisplace:
				LoadPolydisplaceThinker(save, (actionf_p1)T_PolyObjDisplace);
				break;
			case tc_scroll:
				LoadScrollThinker(save, (actionf_p1)T_Scroll);
				break;

			case tc_friction:
				LoadFrictionThinker(save, (actionf_p1)T_Friction);
				break;

			case tc_pusher:
				LoadPusherThinker(save, (actionf_p1)T_Pusher);
				break;

			default:
				I_Error("P_UnarchiveSpecials: Unknown tclass %d in savegame", tclass);
		}
	}

	CONS_Debug(DBG_NETPLAY, "%u thinkers loaded\n", numloaded);

	if (restoreNum)
	{
		executor_t *delay = NULL;
		UINT32 mobjnum;
		for (currentthinker = thinkercap.next; currentthinker != &thinkercap;
			currentthinker = currentthinker->next)
		{
			if (currentthinker->function.acp1 != (actionf_p1)T_ExecutorDelay)
				continue;

			delay = (void *)currentthinker;

			if ((mobjnum = (UINT32)(size_t)delay->caller))
				delay->caller = P_FindNewPosition(mobjnum);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// haleyjd 03/26/06: PolyObject saving code
//
#define PD_FLAGS  0x01
#define PD_TRANS   0x02

static inline void P_ArchivePolyObj(savebuffer_t *save, polyobj_t *po)
{
	UINT8 diff = 0;
	WRITEINT32(save->p, po->id);
	WRITEANGLE(save->p, po->angle);

	WRITEFIXED(save->p, po->spawnSpot.x);
	WRITEFIXED(save->p, po->spawnSpot.y);

	if (po->flags != po->spawnflags)
		diff |= PD_FLAGS;
	if (po->translucency != 0)
		diff |= PD_TRANS;

	WRITEUINT8(save->p, diff);

	if (diff & PD_FLAGS)
		WRITEINT32(save->p, po->flags);
	if (diff & PD_TRANS)
		WRITEINT32(save->p, po->translucency);
}

static inline void P_UnArchivePolyObj(savebuffer_t *save, polyobj_t *po)
{
	INT32 id;
	UINT32 angle;
	fixed_t x, y;
	UINT8 diff;

	// nullify all polyobject thinker pointers;
	// the thinkers themselves will fight over who gets the field
	// when they first start to run.
	po->thinker = NULL;

	id = READINT32(save->p);

	angle = READANGLE(save->p);

	x = READFIXED(save->p);
	y = READFIXED(save->p);

	diff = READUINT8(save->p);

	if (diff & PD_FLAGS)
		po->flags = READINT32(save->p);
	if (diff & PD_TRANS)
		po->translucency = READINT32(save->p);

	// if the object is bad or isn't in the id hash, we can do nothing more
	// with it, so return now
	if (po->isBad || po != Polyobj_GetForNum(id))
		return;

	// rotate and translate polyobject
	Polyobj_MoveOnLoad(po, angle, x, y);
}

static inline void P_ArchivePolyObjects(savebuffer_t *save)
{
	INT32 i;

	WRITEUINT32(save->p, ARCHIVEBLOCK_POBJS);

	// save number of polyobjects
	WRITEINT32(save->p, numPolyObjects);

	for (i = 0; i < numPolyObjects; ++i)
		P_ArchivePolyObj(save, &PolyObjects[i]);
}

static inline void P_UnArchivePolyObjects(savebuffer_t *save)
{
	INT32 i, numSavedPolys;

	if (READUINT32(save->p) != ARCHIVEBLOCK_POBJS)
		I_Error("Bad $$$.sav at archive block Pobjs");

	numSavedPolys = READINT32(save->p);

	if (numSavedPolys != numPolyObjects)
		I_Error("P_UnArchivePolyObjects: polyobj count inconsistency\n");

	for (i = 0; i < numSavedPolys; ++i)
		P_UnArchivePolyObj(save, &PolyObjects[i]);
}
//
// P_FinishMobjs
//
static inline void P_FinishMobjs(void)
{
	thinker_t *currentthinker;
	mobj_t *mobj;

	// put info field there real value
	for (currentthinker = thinkercap.next; currentthinker != &thinkercap;
		currentthinker = currentthinker->next)
	{
		if (currentthinker->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mobj = (mobj_t *)currentthinker;
		mobj->info = &mobjinfo[mobj->type];
	}
}

static void P_RelinkPointers(void)
{
	thinker_t *currentthinker;
	mobj_t *mobj;
	UINT32 temp;

	// use info field (value = oldposition) to relink mobjs
	for (currentthinker = thinkercap.next; currentthinker != &thinkercap;
		currentthinker = currentthinker->next)
	{
		if (currentthinker->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mobj = (mobj_t *)currentthinker;

		if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER)
			continue;

		if (mobj->tracer)
		{
			temp = (UINT32)(size_t)mobj->tracer;
			mobj->tracer = NULL;
			if (!P_SetTarget(&mobj->tracer, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "tracer not found on %d\n", mobj->type);
		}
		if (mobj->target)
		{
			temp = (UINT32)(size_t)mobj->target;
			mobj->target = NULL;
			if (!P_SetTarget(&mobj->target, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "target not found on %d\n", mobj->type);
		}
		if (mobj->hnext)
		{
			temp = (UINT32)(size_t)mobj->hnext;
			mobj->hnext = NULL;
			if (!P_SetTarget(&mobj->hnext, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "hnext not found on %d\n", mobj->type);
		}
		if (mobj->hprev)
		{
			temp = (UINT32)(size_t)mobj->hprev;
			mobj->hprev = NULL;
			if (!P_SetTarget(&mobj->hprev, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "hprev not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->capsule)
		{
			temp = (UINT32)(size_t)mobj->player->capsule;
			mobj->player->capsule = NULL;
			if (!P_SetTarget(&mobj->player->capsule, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "capsule not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->axis1)
		{
			temp = (UINT32)(size_t)mobj->player->axis1;
			mobj->player->axis1 = NULL;
			if (!P_SetTarget(&mobj->player->axis1, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "axis1 not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->axis2)
		{
			temp = (UINT32)(size_t)mobj->player->axis2;
			mobj->player->axis2 = NULL;
			if (!P_SetTarget(&mobj->player->axis2, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "axis2 not found on %d\n", mobj->type);
		}
		if (mobj->player && mobj->player->awayviewmobj)
		{
			temp = (UINT32)(size_t)mobj->player->awayviewmobj;
			mobj->player->awayviewmobj = NULL;
			if (!P_SetTarget(&mobj->player->awayviewmobj, P_FindNewPosition(temp)))
				CONS_Debug(DBG_GAMELOGIC, "awayviewmobj not found on %d\n", mobj->type);
		}
	}
}

//
// P_NetArchiveSpecials
//
static inline void P_NetArchiveSpecials(savebuffer_t *save)
{
	size_t i, z;

	WRITEUINT32(save->p, ARCHIVEBLOCK_SPECIALS);

	// itemrespawn queue for deathmatch
	i = iquetail;
	while (iquehead != i)
	{
		for (z = 0; z < nummapthings; z++)
		{
			if (&mapthings[z] == itemrespawnque[i])
			{
				WRITEUINT32(save->p, z);
				break;
			}
		}
		WRITEUINT32(save->p, itemrespawntime[i]);
		i = (i + 1) & (ITEMQUESIZE-1);
	}

	// end delimiter
	WRITEUINT32(save->p, 0xffffffff);

	// Sky number
	WRITEINT32(save->p, globallevelskynum);

	// Current global weather type
	WRITEUINT8(save->p, globalweather);

	if (metalplayback) // Is metal sonic running?
	{
		WRITEUINT8(save->p, 0x01);
		G_SaveMetal(&save->p);
	}
	else
		WRITEUINT8(save->p, 0x00);
}

//
// P_NetUnArchiveSpecials
//
static void P_NetUnArchiveSpecials(savebuffer_t *save)
{
	size_t i;
	INT32 j;

	if (READUINT32(save->p) != ARCHIVEBLOCK_SPECIALS)
		I_Error("Bad $$$.sav at archive block Specials");

	// BP: added save itemrespawn queue for deathmatch
	iquetail = iquehead = 0;
	while ((i = READUINT32(save->p)) != 0xffffffff)
	{
		itemrespawnque[iquehead] = &mapthings[i];
		itemrespawntime[iquehead++] = READINT32(save->p);
	}

	j = READINT32(save->p);
	if (j != globallevelskynum)
		P_SetupLevelSky(j, true);

	globalweather = READUINT8(save->p);

	if (globalweather)
	{
		if (curWeather == globalweather)
			curWeather = PRECIP_NONE;

		P_SwitchWeather(globalweather);
	}
	else // PRECIP_NONE
	{
		if (curWeather != PRECIP_NONE)
			P_SwitchWeather(globalweather);
	}

	if (READUINT8(save->p) == 0x01) // metal sonic
		G_LoadMetal(&save->p);
}

// =======================================================================
//          Misc
// =======================================================================
static inline void P_ArchiveMisc(savebuffer_t *save)
{
	if (gamecomplete)
		WRITEINT16(save->p, gamemap | 8192);
	else
		WRITEINT16(save->p, gamemap);

	lastmapsaved = gamemap;

	WRITEUINT16(save->p, (botskin ? (emeralds|(1<<10)) : emeralds)+357);
	WRITESTRINGN(save->p, timeattackfolder, sizeof(timeattackfolder));
}

static inline void P_UnArchiveSPGame(savebuffer_t *save, INT16 mapoverride)
{
	char testname[sizeof(timeattackfolder)];

	gamemap = READINT16(save->p);

	if (mapoverride != 0)
	{
		gamemap = mapoverride;
		gamecomplete = true;
	}
	else
		gamecomplete = false;

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	lastmapsaved = gamemap;

	tokenlist = 0;
	token = 0;

	savedata.emeralds = READUINT16(save->p)-357;
	if (savedata.emeralds & (1<<10))
		savedata.botcolor = 0xFF;
	savedata.emeralds &= 0xff;

	READSTRINGN(save->p, testname, sizeof(testname));

	if (strcmp(testname, timeattackfolder))
	{
		if (modifiedgame)
			I_Error("Save game not for this modification.");
		else
			I_Error("This save file is for a particular mod, it cannot be used with the regular game.");
	}

	memset(playeringame, 0, sizeof(*playeringame));
	playeringame[consoleplayer] = true;
}

static void P_NetArchiveMisc(savebuffer_t *save, boolean resending)
{
	UINT32 pig = 0;
	INT32 i;

	WRITEUINT32(save->p, ARCHIVEBLOCK_MISC);

	if (resending)
		WRITEUINT32(save->p, gametic);
	WRITEINT16(save->p, gamemap);
	if (gamestate != GS_LEVEL)
		WRITEINT16(save->p, GS_WAITINGPLAYERS); // nice hack to put people back into waitingplayers
	else
		WRITEINT16(save->p, gamestate);

	WRITEINT16(save->p, gametype);

	for (i = 0; i < MAXPLAYERS; i++)
		pig |= (playeringame[i] != 0)<<i;
	WRITEUINT32(save->p, pig);

	WRITEUINT32(save->p, P_GetRandSeed());

	WRITEUINT32(save->p, tokenlist);

	WRITEUINT8(save->p, encoremode);

	WRITEUINT32(save->p, leveltime);
	WRITEUINT32(save->p, totalrings);
	WRITEINT16(save->p, lastmap);

	for (i = 0; i < 4; i++)
	{
		WRITEINT16(save->p, votelevels[i][0]);
		WRITEINT16(save->p, votelevels[i][1]);
	}

	for (i = 0; i < MAXPLAYERS; i++)
		WRITESINT8(save->p, votes[i]);

	WRITESINT8(save->p, pickedvote);

	WRITEUINT16(save->p, emeralds);
	WRITEUINT8(save->p, stagefailed);

	WRITEUINT32(save->p, token);
	WRITEINT32(save->p, sstimer);
	WRITEUINT32(save->p, bluescore);
	WRITEUINT32(save->p, redscore);

	WRITEINT16(save->p, autobalance);
	WRITEINT16(save->p, teamscramble);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save->p, scrambleplayers[i]);

	for (i = 0; i < MAXPLAYERS; i++)
		WRITEINT16(save->p, scrambleteams[i]);

	WRITEINT16(save->p, scrambletotal);
	WRITEINT16(save->p, scramblecount);

	WRITEUINT32(save->p, racecountdown);
	WRITEUINT32(save->p, exitcountdown);

	WRITEFIXED(save->p, gravity);
	WRITEFIXED(save->p, mapobjectscale);

	WRITEUINT32(save->p, countdowntimer);
	WRITEUINT8(save->p, countdowntimeup);

	WRITEUINT32(save->p, hidetime);

	// SRB2kart
	WRITEINT32(save->p, numgotboxes);

	WRITEUINT8(save->p, gamespeed);
	WRITEUINT8(save->p, franticitems);
	WRITEUINT8(save->p, comeback);

	for (i = 0; i < 4; i++)
		WRITESINT8(save->p, battlewanted[i]);

	WRITEUINT32(save->p, wantedcalcdelay);
	WRITEUINT32(save->p, indirectitemcooldown);
	WRITEUINT32(save->p, hyubgone);
	WRITEUINT32(save->p, mapreset);
	WRITEUINT8(save->p, nospectategrief);
	WRITEUINT8(save->p, thwompsactive);
	WRITESINT8(save->p, spbplace);
	WRITEUINT8(save->p, startedInFreePlay);

	// Is it paused?
	if (paused)
		WRITEUINT8(save->p, 0x2f);
	else
		WRITEUINT8(save->p, 0x2e);
}

FUNCINLINE static ATTRINLINE boolean P_NetUnArchiveMisc(savebuffer_t *save, boolean reloading)
{
	UINT32 pig;
	INT32 i;

	if (READUINT32(save->p) != ARCHIVEBLOCK_MISC)
		I_Error("Bad $$$.sav at archive block Misc");

	if (reloading)
		gametic = READUINT32(save->p);

	gamemap = READINT16(save->p);

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	// tell the sound code to reset the music since we're skipping what
	// normally sets this flag
	if (!reloading)
		mapmusflags |= MUSIC_RELOADRESET;

	G_SetGamestate(READINT16(save->p));

	if (reloading)
		gametype = READINT16(save->p);

	pig = READUINT32(save->p);
	for (i = 0; i < MAXPLAYERS; i++)
	{
		playeringame[i] = (pig & (1<<i)) != 0;
		// playerstate is set in unarchiveplayers
	}

	P_SetRandSeed(READUINT32(save->p));

	tokenlist = READUINT32(save->p);

	encoremode = (boolean)READUINT8(save->p);

	if (!P_SetupLevel(true, reloading))
	{
		CONS_Alert(CONS_ERROR, M_GetText("Can't load the level!\n"));
		return false;
	}

	// get the time
	leveltime = READUINT32(save->p);
	totalrings = READUINT32(save->p);
	lastmap = READINT16(save->p);

	for (i = 0; i < 4; i++)
	{
		votelevels[i][0] = READINT16(save->p);
		votelevels[i][1] = READINT16(save->p);
	}

	for (i = 0; i < MAXPLAYERS; i++)
		votes[i] = READSINT8(save->p);

	pickedvote = READSINT8(save->p);

	emeralds = READUINT16(save->p);
	stagefailed = READUINT8(save->p);

	token = READUINT32(save->p);
	sstimer = READINT32(save->p);
	bluescore = READUINT32(save->p);
	redscore = READUINT32(save->p);

	autobalance = READINT16(save->p);
	teamscramble = READINT16(save->p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleplayers[i] = READINT16(save->p);

	for (i = 0; i < MAXPLAYERS; i++)
		scrambleteams[i] = READINT16(save->p);

	scrambletotal = READINT16(save->p);
	scramblecount = READINT16(save->p);

	racecountdown = READUINT32(save->p);
	exitcountdown = READUINT32(save->p);

	gravity = READFIXED(save->p);
	mapobjectscale = READFIXED(save->p);

	countdowntimer = (tic_t)READUINT32(save->p);
	countdowntimeup = (boolean)READUINT8(save->p);

	hidetime = READUINT32(save->p);

	// SRB2kart
	numgotboxes = READINT32(save->p);

	gamespeed = READUINT8(save->p);
	franticitems = (boolean)READUINT8(save->p);
	comeback = (boolean)READUINT8(save->p);

	for (i = 0; i < 4; i++)
		battlewanted[i] = READSINT8(save->p);

	wantedcalcdelay = READUINT32(save->p);
	indirectitemcooldown = READUINT32(save->p);
	hyubgone = READUINT32(save->p);
	mapreset = READUINT32(save->p);
	nospectategrief = READUINT8(save->p);
	thwompsactive = (boolean)READUINT8(save->p);
	spbplace = READSINT8(save->p);
	startedInFreePlay = READUINT8(save->p);

	// Is it paused?
	if (READUINT8(save->p) == 0x2f)
		paused = true;

	return true;
}

void P_SaveGame(savebuffer_t *save)
{
	P_ArchiveMisc(save);
	P_ArchivePlayer(save);

	WRITEUINT8(save->p, 0x1d); // consistency marker
}

void P_SaveNetGame(savebuffer_t *save, boolean resending)
{
	thinker_t *th;
	mobj_t *mobj;
	INT32 i = 1; // don't start from 0, it'd be confused with a blank pointer otherwise

	CV_SaveNetVars(&save->p, false);
	P_NetArchiveMisc(save, resending);

	// Assign the mobjnumber for pointer tracking
	if (gamestate == GS_LEVEL)
	{
		for (th = thinkercap.next; th != &thinkercap; th = th->next)
		{
			if (th->function.acp1 != (actionf_p1)P_MobjThinker)
				continue;

			mobj = (mobj_t *)th;

			if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER)
				continue;

			mobj->mobjnum = i++;
		}
	}

	P_NetArchivePlayers(save, resending);
	if (gamestate == GS_LEVEL)
	{
		P_NetArchiveWorld(save);
		P_ArchivePolyObjects(save);
		P_NetArchiveThinkers(save);
		P_NetArchiveSpecials(save);
	}

	LUA_Archive(save, true);
	WRITEUINT8(save->p, 0x1d); // consistency marker
}

boolean P_LoadGame(savebuffer_t *save, INT16 mapoverride)
{
	if (gamestate == GS_INTERMISSION)
		Y_EndIntermission();
	if (gamestate == GS_VOTING)
		Y_EndVote();
	G_SetGamestate(GS_NULL); // should be changed in P_UnArchiveMisc

	P_UnArchiveSPGame(save, mapoverride);
	P_UnArchivePlayer(save);

	// Savegame end marker
	if (READUINT8(save->p) != 0x1d)
	{
		CONS_Alert(CONS_ERROR, M_GetText("Corrupt Luabanks! (Failed consistency check)\n"));
		return false;
	}

	// Only do this after confirming savegame is ok
	G_DeferedInitNew(false, G_BuildMapName(gamemap), savedata.skin, 0, true);
	COM_BufAddText("dummyconsvar 1\n"); // G_DeferedInitNew doesn't do this

	return true;
}

boolean P_LoadNetGame(savebuffer_t *save, boolean reloading)
{
	CV_LoadNetVars(&save->p);

	if (!P_NetUnArchiveMisc(save, reloading))
		return false;

	P_NetUnArchivePlayers(save, reloading);

	if (gamestate == GS_LEVEL)
	{
		P_NetUnArchiveWorld(save);
		P_UnArchivePolyObjects(save);
		P_NetUnArchiveThinkers(save);
		P_NetUnArchiveSpecials(save);
		P_RelinkPointers();
		P_FinishMobjs();
	}

	LUA_UnArchive(save, true);

	// This is stupid and hacky, but maybe it'll work!
	P_SetRandSeed(P_GetInitSeed());

	// The precipitation would normally be spawned in P_SetupLevel, which is called by
	// P_NetUnArchiveMisc above. However, that would place it up before P_NetUnArchiveThinkers,
	// so the thinkers would be deleted later. Therefore, P_SetupLevel will *not* spawn
	// precipitation when loading a netgame save. Instead, precip has to be spawned here.
	// This is done in P_NetUnArchiveSpecials now.

	return READUINT8(save->p) == 0x1d;
}
