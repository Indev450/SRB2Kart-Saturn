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

#include "d_think.h"
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
#include "r_skins.h"
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
FUNCINLINE static ATTRINLINE void P_ArchivePlayer(savebuffer_t *save)
{
	const player_t *player = &players[consoleplayer];
	INT32 pllives = player->lives;
	if (pllives < 3) // Bump up to 3 lives if the player
		pllives = 3; // has less than that.

	WRITEUINT8(save->p, player->skincolor);
	WRITEUINT8(save->p, player->skin);

	WRITEUINT32(save->p, player->score);
	WRITEINT32(save->p, pllives);
	WRITEINT32(save->p, player->continues);

	if (botskin)
	{
		WRITEUINT8(save->p, botskin);
		WRITEUINT8(save->p, botcolor);
	}
}

//
// P_UnArchivePlayer
//
FUNCINLINE static ATTRINLINE void P_UnArchivePlayer(savebuffer_t *save)
{
	savedata.skincolor = READUINT8(save->p);
	savedata.skin = READUINT8(save->p);

	savedata.score = READINT32(save->p);
	savedata.lives = READINT32(save->p);
	savedata.continues = READINT32(save->p);

	if (savedata.botcolor)
	{
		savedata.botskin = READUINT8(save->p);
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

		const player_t *player = &players[i];

		WRITEANGLE(save->p, player->aiming);
		WRITEANGLE(save->p, player->awayviewaiming);
		WRITEINT32(save->p, player->awayviewtics);
		WRITEINT32(save->p, player->health);

		WRITESINT8(save->p, player->pity);
		WRITEINT32(save->p, player->currentweapon);
		WRITEINT32(save->p, player->ringweapons);

		for (j = 0; j < NUMPOWERS; j++)
			WRITEUINT16(save->p, player->powers[j]);
		for (j = 0; j < NUMKARTSTUFF; j++)
			WRITEINT32(save->p, player->kartstuff[j]);

		WRITEANGLE(save->p, player->frameangle);

		WRITEUINT8(save->p, player->playerstate);
		WRITEUINT32(save->p, player->pflags);
		WRITEUINT8(save->p, player->panim);
		WRITEUINT8(save->p, player->spectator);

		WRITEUINT16(save->p, player->flashpal);
		WRITEUINT16(save->p, player->flashcount);

		if (resending)
		{
			WRITEUINT8(save->p, player->skincolor);
			WRITEINT32(save->p, player->skin);
		}

		WRITEUINT32(save->p, player->score);
		WRITEFIXED(save->p, player->dashspeed);
		WRITEINT32(save->p, player->dashtime);
		WRITESINT8(save->p, player->lives);
		WRITESINT8(save->p, player->continues);
		WRITESINT8(save->p, player->xtralife);
		WRITEUINT8(save->p, player->gotcontinue);
		WRITEFIXED(save->p, player->speed);
		WRITEUINT8(save->p, player->jumping);
		WRITEUINT8(save->p, player->secondjump);
		WRITEUINT8(save->p, player->fly1);
		WRITEUINT8(save->p, player->scoreadd);
		WRITEUINT32(save->p, player->glidetime);
		WRITEUINT8(save->p, player->climbing);
		WRITEINT32(save->p, player->deadtimer);
		WRITEUINT32(save->p, player->exiting);
		WRITEUINT8(save->p, player->homing);
		WRITEUINT32(save->p, player->skidtime);

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		WRITEFIXED(save->p, player->cmomx); // Conveyor momx
		WRITEFIXED(save->p, player->cmomy); // Conveyor momy
		WRITEFIXED(save->p, player->rmomx); // "Real" momx (momx - cmomx)
		WRITEFIXED(save->p, player->rmomy); // "Real" momy (momy - cmomy)

		/////////////////////
		// Race Mode Stuff //
		/////////////////////
		WRITEINT16(save->p, player->numboxes);
		WRITEINT16(save->p, player->totalring);
		WRITEUINT32(save->p, player->realtime);
		WRITEUINT8(save->p, player->laps);

		////////////////////
		// CTF Mode Stuff //
		////////////////////
		WRITEINT32(save->p, player->ctfteam);
		WRITEUINT16(save->p, player->gotflag);

		WRITEINT32(save->p, player->weapondelay);
		WRITEINT32(save->p, player->tossdelay);

		WRITEUINT32(save->p, player->starposttime);
		WRITEINT16(save->p, player->starpostx);
		WRITEINT16(save->p, player->starposty);
		WRITEINT16(save->p, player->starpostz);
		WRITEINT32(save->p, player->starpostnum);
		WRITEANGLE(save->p, player->starpostangle);

		WRITEANGLE(save->p, player->angle_pos);
		WRITEANGLE(save->p, player->old_angle_pos);

		WRITEINT32(save->p, player->flyangle);
		WRITEUINT32(save->p, player->drilltimer);
		WRITEINT32(save->p, player->linkcount);
		WRITEUINT32(save->p, player->linktimer);
		WRITEINT32(save->p, player->anotherflyangle);
		WRITEUINT32(save->p, player->nightstime);
		WRITEUINT32(save->p, player->bumpertime);
		WRITEINT32(save->p, player->drillmeter);
		WRITEUINT8(save->p, player->drilldelay);
		WRITEUINT8(save->p, player->bonustime);
		WRITEUINT8(save->p, player->mare);

		WRITEUINT32(save->p, player->marebegunat);
		WRITEUINT32(save->p, player->startedtime);
		WRITEUINT32(save->p, player->finishedtime);
		WRITEINT16(save->p, player->finishedrings);
		WRITEUINT32(save->p, player->marescore);
		WRITEUINT32(save->p, player->lastmarescore);
		WRITEUINT8(save->p, player->lastmare);
		WRITEINT32(save->p, player->maxlink);
		WRITEUINT8(save->p, player->texttimer);
		WRITEUINT8(save->p, player->textvar);

		if (player->capsule)
			flags |= CAPSULE;

		if (player->awayviewmobj)
			flags |= AWAYVIEW;

		if (player->axis1)
			flags |= FIRSTAXIS;

		if (player->axis2)
			flags |= SECONDAXIS;

		WRITEINT16(save->p, player->lastsidehit);
		WRITEINT16(save->p, player->lastlinehit);

		WRITEUINT32(save->p, player->losstime);

		WRITEUINT8(save->p, player->timeshit);

		WRITEINT32(save->p, player->onconveyor);

		WRITEUINT32(save->p, player->jointime);
		WRITEUINT32(save->p, player->spectatorreentry);

		WRITEUINT32(save->p, player->grieftime);
		WRITEUINT8(save->p, player->griefstrikes);

		WRITEUINT8(save->p, player->splitscreenindex);

		WRITEUINT16(save->p, flags);

		if (flags & CAPSULE)
			WRITEUINT32(save->p, player->capsule->mobjnum);

		if (flags & FIRSTAXIS)
			WRITEUINT32(save->p, player->axis1->mobjnum);

		if (flags & SECONDAXIS)
			WRITEUINT32(save->p, player->axis2->mobjnum);

		if (flags & AWAYVIEW)
			WRITEUINT32(save->p, player->awayviewmobj->mobjnum);

		WRITEUINT32(save->p, player->charflags);
		// SRB2kart
		WRITEUINT8(save->p, player->kartspeed);
		WRITEUINT8(save->p, player->kartweight);
		//

		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			WRITEINT16(save->p, player->lturn_max[j]);
			WRITEINT16(save->p, player->rturn_max[j]);
		}
	}
}

//
// P_NetUnArchivePlayers
//
static void P_NetUnArchivePlayers(savebuffer_t *save, boolean reloading)
{
	INT32 i, j;
	UINT16 flags;

	if (READUINT32(save->p) != ARCHIVEBLOCK_PLAYERS)
		I_Error("Bad $$$.sav at archive block Players");

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (reloading)
			adminplayers[i] = (INT32)READSINT8(save->p);

		// Do NOT memset player struct to 0
		// other areas may initialize data elsewhere
		//memset(&players[i], 0, sizeof (player_t));
		if (!playeringame[i])
			continue;

		// NOTE: sending tics should (hopefully) no longer be necessary

		if (reloading)
			READSTRINGN(save->p, player_names[i], MAXPLAYERNAME);

		player_t *player = &players[i];

		player->aiming = READANGLE(save->p);
		player->awayviewaiming = READANGLE(save->p);
		player->awayviewtics = READINT32(save->p);
		player->health = READINT32(save->p);

		player->pity = READSINT8(save->p);
		player->currentweapon = READINT32(save->p);
		player->ringweapons = READINT32(save->p);

		for (j = 0; j < NUMPOWERS; j++)
			player->powers[j] = READUINT16(save->p);
		for (j = 0; j < NUMKARTSTUFF; j++)
			player->kartstuff[j] = READINT32(save->p);

		player->frameangle = READANGLE(save->p);

		player->playerstate = READUINT8(save->p);
		player->pflags = READUINT32(save->p);
		player->panim = READUINT8(save->p);
		player->spectator = READUINT8(save->p);

		player->flashpal = READUINT16(save->p);
		player->flashcount = READUINT16(save->p);

		if (reloading)
		{
			player->skincolor = READUINT8(save->p);
			player->skin = READINT32(save->p);
		}

		player->score = READUINT32(save->p);
		player->dashspeed = READFIXED(save->p); // dashing speed
		player->dashtime = READINT32(save->p); // dashing speed
		player->lives = READSINT8(save->p);
		player->continues = READSINT8(save->p); // continues that player has acquired
		player->xtralife = READSINT8(save->p); // Ring Extra Life counter
		player->gotcontinue = READUINT8(save->p); // got continue from stage
		player->speed = READFIXED(save->p); // Player's speed (distance formula of MOMX and MOMY values)
		player->jumping = READUINT8(save->p); // Jump counter
		player->secondjump = READUINT8(save->p);
		player->fly1 = READUINT8(save->p); // Tails flying
		player->scoreadd = READUINT8(save->p); // Used for multiple enemy attack bonus
		player->glidetime = READUINT32(save->p); // Glide counter for thrust
		player->climbing = READUINT8(save->p); // Climbing on the wall
		player->deadtimer = READINT32(save->p); // End game if game over lasts too long
		player->exiting = READUINT32(save->p); // Exitlevel timer
		player->homing = READUINT8(save->p); // Are you homing?
		player->skidtime = READUINT32(save->p); // Skid timer

		////////////////////////////
		// Conveyor Belt Movement //
		////////////////////////////
		player->cmomx = READFIXED(save->p); // Conveyor momx
		player->cmomy = READFIXED(save->p); // Conveyor momy
		player->rmomx = READFIXED(save->p); // "Real" momx (momx - cmomx)
		player->rmomy = READFIXED(save->p); // "Real" momy (momy - cmomy)

		/////////////////////
		// Race Mode Stuff //
		/////////////////////
		player->numboxes = READINT16(save->p); // Number of item boxes obtained for Race Mode
		player->totalring = READINT16(save->p); // Total number of rings obtained for Race Mode
		player->realtime = READUINT32(save->p); // integer replacement for leveltime
		player->laps = READUINT8(save->p); // Number of laps (optional)

		////////////////////
		// CTF Mode Stuff //
		////////////////////
		player->ctfteam = READINT32(save->p); // 1 == Red, 2 == Blue
		player->gotflag = READUINT16(save->p); // 1 == Red, 2 == Blue Do you have the flag?

		player->weapondelay = READINT32(save->p);
		player->tossdelay = READINT32(save->p);

		player->starposttime = READUINT32(save->p);
		player->starpostx = READINT16(save->p);
		player->starposty = READINT16(save->p);
		player->starpostz = READINT16(save->p);
		player->starpostnum = READINT32(save->p);
		player->starpostangle = READANGLE(save->p);

		player->angle_pos = READANGLE(save->p);
		player->old_angle_pos = READANGLE(save->p);

		player->flyangle = READINT32(save->p);
		player->drilltimer = READUINT32(save->p);
		player->linkcount = READINT32(save->p);
		player->linktimer = READUINT32(save->p);
		player->anotherflyangle = READINT32(save->p);
		player->nightstime = READUINT32(save->p);
		player->bumpertime = READUINT32(save->p);
		player->drillmeter = READINT32(save->p);
		player->drilldelay = READUINT8(save->p);
		player->bonustime = (boolean)READUINT8(save->p);
		player->mare = READUINT8(save->p);

		player->marebegunat = READUINT32(save->p);
		player->startedtime = READUINT32(save->p);
		player->finishedtime = READUINT32(save->p);
		player->finishedrings = READINT16(save->p);
		player->marescore = READUINT32(save->p);
		player->lastmarescore = READUINT32(save->p);
		player->lastmare = READUINT8(save->p);
		player->maxlink = READINT32(save->p);
		player->texttimer = READUINT8(save->p);
		player->textvar = READUINT8(save->p);

		player->lastsidehit = READINT16(save->p);
		player->lastlinehit = READINT16(save->p);

		player->losstime = READUINT32(save->p);

		player->timeshit = READUINT8(save->p);

		player->onconveyor = READINT32(save->p);

		player->jointime = READUINT32(save->p);
		player->spectatorreentry = READUINT32(save->p);

		player->grieftime = READUINT32(save->p);
		player->griefstrikes = READUINT8(save->p);

		player->splitscreenindex = READUINT8(save->p);

		flags = READUINT16(save->p);

		if (flags & CAPSULE)
			player->capsule = (mobj_t *)(size_t)READUINT32(save->p);

		if (flags & FIRSTAXIS)
			player->axis1 = (mobj_t *)(size_t)READUINT32(save->p);

		if (flags & SECONDAXIS)
			player->axis2 = (mobj_t *)(size_t)READUINT32(save->p);

		if (flags & AWAYVIEW)
			player->awayviewmobj = (mobj_t *)(size_t)READUINT32(save->p);

		player->viewheight = 32<<FRACBITS;

		//SetPlayerSkinByNum(i, player->skin);
		player->charflags = READUINT32(save->p);
		// SRB2kart
		player->kartspeed = READUINT8(save->p);
		player->kartweight = READUINT8(save->p);
		//

		for (j = 0; j < MAXPREDICTTICS; j++)
		{
			player->lturn_max[j] = READINT16(save->p);
			player->rturn_max[j] = READINT16(save->p);
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

#define FD_FLAGS 0x01
#define FD_ALPHA 0x02

// Check if any of the sector's FOFs differ from how they spawned
static boolean CheckFFloorDiff(const sector_t *ss)
{
	ffloor_t *rover;

	for (rover = ss->ffloors; rover; rover = rover->next)
	{
		if (rover->flags != rover->spawnflags
			|| rover->alpha != rover->spawnalpha)
		{
			return true; // we found an FOF that changed!
			// don't bother checking for more, we do that later
		}
	}

	return false;
}

// Special case: save the stats of all modified ffloors along with their ffloor "number"s
// we don't bother with ffloors that haven't changed, that would just add to savegame even more than is really needed
static void ArchiveFFloors(savebuffer_t *save, const sector_t *ss)
{
	size_t j = 0; // ss->ffloors is saved as ffloor #0, ss->ffloors->next is #1, etc
	ffloor_t *rover;
	UINT8 fflr_diff;

	for (rover = ss->ffloors; rover; rover = rover->next)
	{
		fflr_diff = 0; // reset diff flags

		if (rover->flags != rover->spawnflags)
			fflr_diff |= FD_FLAGS;
		if (rover->alpha != rover->spawnalpha)
			fflr_diff |= FD_ALPHA;

		if (fflr_diff)
		{
			WRITEUINT16(save->p, j); // save ffloor "number"
			WRITEUINT8(save->p, fflr_diff);

			if (fflr_diff & FD_FLAGS)
				WRITEUINT32(save->p, rover->flags);
			if (fflr_diff & FD_ALPHA)
				WRITEINT16(save->p, rover->alpha);
		}

		j++;
	}

	WRITEUINT16(save->p, 0xffff);
}

static void ArchiveSectors(savebuffer_t *save)
{
	size_t i;
	const sector_t *ss = sectors;
	const sector_t *spawnss = spawnsectors;
	UINT8 diff, diff2;

	for (i = 0; i < numsectors; i++, ss++, spawnss++)
	{
		diff = diff2 = 0;
		if (ss->floorheight != spawnss->floorheight)
			diff |= SD_FLOORHT;
		if (ss->ceilingheight != spawnss->ceilingheight)
			diff |= SD_CEILHT;

		//
		// flats
		//
		if (ss->floorpic != spawnss->floorpic)
			diff |= SD_FLOORPIC;
		if (ss->ceilingpic != spawnss->ceilingpic)
			diff |= SD_CEILPIC;

		if (ss->lightlevel != spawnss->lightlevel)
			diff |= SD_LIGHT;
		if (ss->special != spawnss->special)
			diff |= SD_SPECIAL;

		if (ss->floor_xoffs != spawnss->floor_xoffs)
			diff2 |= SD_FXOFFS;
		if (ss->floor_yoffs != spawnss->floor_yoffs)
			diff2 |= SD_FYOFFS;
		if (ss->ceiling_xoffs != spawnss->ceiling_xoffs)
			diff2 |= SD_CXOFFS;
		if (ss->ceiling_yoffs != spawnss->ceiling_yoffs)
			diff2 |= SD_CYOFFS;
		if (ss->floorpic_angle != spawnss->floorpic_angle)
			diff2 |= SD_FLOORANG;
		if (ss->ceilingpic_angle != spawnss->ceilingpic_angle)
			diff2 |= SD_CEILANG;

		if (ss->tag != spawnss->tag)
			diff2 |= SD_TAG;
		if (ss->nexttag != spawnss->nexttag || ss->firsttag != spawnss->firsttag)
			diff2 |= SD_TAGLIST;

		if (ss->ffloors && CheckFFloorDiff(ss))
			diff |= SD_FFLOORS;

		if (diff2)
			diff |= SD_DIFF2;

		if (diff)
		{
			WRITEUINT16(save->p, i);
			WRITEUINT8(save->p, diff);

			if (diff & SD_DIFF2)
				WRITEUINT8(save->p, diff2);
			if (diff & SD_FLOORHT)
				WRITEFIXED(save->p, ss->floorheight);
			if (diff & SD_CEILHT)
				WRITEFIXED(save->p, ss->ceilingheight);
			if (diff & SD_FLOORPIC)
				WRITEMEM(save->p, levelflats[ss->floorpic].name, 8);
			if (diff & SD_CEILPIC)
				WRITEMEM(save->p, levelflats[ss->ceilingpic].name, 8);
			if (diff & SD_LIGHT)
				WRITEINT16(save->p, ss->lightlevel);
			if (diff & SD_SPECIAL)
				WRITEINT16(save->p, ss->special);
			if (diff2 & SD_FXOFFS)
				WRITEFIXED(save->p, ss->floor_xoffs);
			if (diff2 & SD_FYOFFS)
				WRITEFIXED(save->p, ss->floor_yoffs);
			if (diff2 & SD_CXOFFS)
				WRITEFIXED(save->p, ss->ceiling_xoffs);
			if (diff2 & SD_CYOFFS)
				WRITEFIXED(save->p, ss->ceiling_yoffs);
			if (diff2 & SD_TAG) // save only the tag
				WRITEINT16(save->p, ss->tag);
			if (diff2 & SD_FLOORANG)
				WRITEANGLE(save->p, ss->floorpic_angle);
			if (diff2 & SD_CEILANG)
				WRITEANGLE(save->p, ss->ceilingpic_angle);
			if (diff2 & SD_TAGLIST) // save both firsttag and nexttag
			{ // either of these could be changed even if tag isn't
				WRITEINT32(save->p, ss->firsttag);
				WRITEINT32(save->p, ss->nexttag);
			}

			if (diff & SD_FFLOORS)
				ArchiveFFloors(save, ss);
		}
	}

	WRITEUINT16(save->p, 0xffff);
}

static void ArchiveLines(savebuffer_t *save)
{
	size_t i;
	const line_t *li = lines;
	const line_t *spawnli = spawnlines;
	const side_t *si;
	const side_t *spawnsi;
	UINT8 diff, diff2;

	for (i = 0; i < numlines; i++, spawnli++, li++)
	{
		diff = diff2 = 0;

		if (li->special != spawnli->special)
			diff |= LD_SPECIAL;

		if (spawnli->special == 321 || spawnli->special == 322) // only reason li->callcount would be non-zero is if either of these are involved
			diff |= LD_CLLCOUNT;

		if (li->sidenum[0] != 0xffff)
		{
			si = &sides[li->sidenum[0]];
			spawnsi = &spawnsides[li->sidenum[0]];

			if (si->textureoffset != spawnsi->textureoffset)
				diff |= LD_S1TEXOFF;
			//SoM: 4/1/2000: Some textures are colormaps. Don't worry about invalid textures.
			if (si->toptexture != spawnsi->toptexture)
				diff |= LD_S1TOPTEX;
			if (si->bottomtexture != spawnsi->bottomtexture)
				diff |= LD_S1BOTTEX;
			if (si->midtexture != spawnsi->midtexture)
				diff |= LD_S1MIDTEX;
		}
		if (li->sidenum[1] != 0xffff)
		{
			si = &sides[li->sidenum[1]];
			spawnsi = &spawnsides[li->sidenum[1]];

			if (si->textureoffset != spawnsi->textureoffset)
				diff2 |= LD_S2TEXOFF;
			if (si->toptexture != spawnsi->toptexture)
				diff2 |= LD_S2TOPTEX;
			if (si->bottomtexture != spawnsi->bottomtexture)
				diff2 |= LD_S2BOTTEX;
			if (si->midtexture != spawnsi->midtexture)
				diff2 |= LD_S2MIDTEX;

			if (diff2)
				diff |= LD_DIFF2;
		}

		if (diff)
		{
			WRITEINT16(save->p, i);
			WRITEUINT8(save->p, diff);
			if (diff & LD_DIFF2)
				WRITEUINT8(save->p, diff2);
			if (diff & LD_FLAG)
				WRITEINT16(save->p, li->flags);
			if (diff & LD_SPECIAL)
				WRITEINT16(save->p, li->special);
			if (diff & LD_CLLCOUNT)
				WRITEINT16(save->p, li->callcount);

			si = &sides[li->sidenum[0]];
			if (diff & LD_S1TEXOFF)
				WRITEFIXED(save->p, si->textureoffset);
			if (diff & LD_S1TOPTEX)
				WRITEINT32(save->p, si->toptexture);
			if (diff & LD_S1BOTTEX)
				WRITEINT32(save->p, si->bottomtexture);
			if (diff & LD_S1MIDTEX)
				WRITEINT32(save->p, si->midtexture);

			si = &sides[li->sidenum[1]];
			if (diff2 & LD_S2TEXOFF)
				WRITEFIXED(save->p, si->textureoffset);
			if (diff2 & LD_S2TOPTEX)
				WRITEINT32(save->p, si->toptexture);
			if (diff2 & LD_S2BOTTEX)
				WRITEINT32(save->p, si->bottomtexture);
			if (diff2 & LD_S2MIDTEX)
				WRITEINT32(save->p, si->midtexture);
		}
	}

	WRITEUINT16(save->p, 0xffff);
}

//
// P_NetArchiveWorld
//
static void P_NetArchiveWorld(savebuffer_t *save)
{
	WRITEUINT32(save->p, ARCHIVEBLOCK_WORLD);

	ArchiveSectors(save);
	ArchiveLines(save);
	R_ClearTextureNumCache(false);
}

static void UnArchiveFFloors(savebuffer_t *save, const sector_t *ss)
{
	UINT16 j = 0; // number of current ffloor in loop
	UINT16 fflr_i; // saved ffloor "number" of next modified ffloor
	UINT16 fflr_diff; // saved ffloor diff
	ffloor_t *rover;

	rover = ss->ffloors;
	if (!rover) // it is assumed sectors[i].ffloors actually exists, but just in case...
		I_Error("Sector does not have any ffloors!");

	fflr_i = READUINT16(save->p); // get first modified ffloor's number ready
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

		fflr_diff = READUINT8(save->p);

		if (fflr_diff & FD_FLAGS)
			rover->flags = (ffloortype_e)READUINT32(save->p);
		if (fflr_diff & FD_ALPHA)
			rover->alpha = READINT16(save->p);

		fflr_i = READUINT16(save->p); // get next ffloor "number" ready

		j++;
		rover = rover->next;
	}
}

static void UnArchiveSectors(savebuffer_t *save)
{
	sector_t *ss;
	UINT16 i;
	UINT8 diff, diff2;

	for (;;)
	{
		i = READUINT16(save->p);

		if (i == 0xffff)
			break;

		if (i > numsectors)
			I_Error("Invalid sector number %u from server (expected end at %s)", i, sizeu1(numsectors));

		diff = READUINT8(save->p);
		if (diff & SD_DIFF2)
			diff2 = READUINT8(save->p);
		else
			diff2 = 0;

		ss = &sectors[i];

		if (diff & SD_FLOORHT)
			ss->floorheight = READFIXED(save->p);
		if (diff & SD_CEILHT)
			ss->ceilingheight = READFIXED(save->p);
		if (diff & SD_FLOORPIC)
		{
			ss->floorpic = P_AddLevelFlatRuntime((char *)save->p);
			save->p += 8;
		}
		if (diff & SD_CEILPIC)
		{
			ss->ceilingpic = P_AddLevelFlatRuntime((char *)save->p);
			save->p += 8;
		}
		if (diff & SD_LIGHT)
			ss->lightlevel = READINT16(save->p);
		if (diff & SD_SPECIAL)
			ss->special = READINT16(save->p);

		if (diff2 & SD_FXOFFS)
			ss->floor_xoffs = READFIXED(save->p);
		if (diff2 & SD_FYOFFS)
			ss->floor_yoffs = READFIXED(save->p);
		if (diff2 & SD_CXOFFS)
			ss->ceiling_xoffs = READFIXED(save->p);
		if (diff2 & SD_CYOFFS)
			ss->ceiling_yoffs = READFIXED(save->p);
		if (diff2 & SD_TAG)
			ss->tag = READINT16(save->p); // DON'T use P_ChangeSectorTag
		if (diff2 & SD_TAGLIST)
		{
			ss->firsttag = READINT32(save->p);
			ss->nexttag = READINT32(save->p);
		}
		if (diff2 & SD_FLOORANG)
			ss->floorpic_angle  = READANGLE(save->p);
		if (diff2 & SD_CEILANG)
			ss->ceilingpic_angle = READANGLE(save->p);

		if (diff & SD_FFLOORS)
			UnArchiveFFloors(save, ss);
	}
}

static void UnArchiveLines(savebuffer_t *save)
{
	UINT16 i;
	line_t *li;
	side_t *si;
	UINT8 diff, diff2;

	for (;;)
	{
		i = READUINT16(save->p);

		if (i == 0xffff)
			break;
		if (i > numlines)
			I_Error("Invalid line number %u from server", i);

		diff = READUINT8(save->p);
		li = &lines[i];

		if (diff & LD_DIFF2)
			diff2 = READUINT8(save->p);
		else
			diff2 = 0;
		if (diff & LD_FLAG)
			li->flags = READINT16(save->p);
		if (diff & LD_SPECIAL)
			li->special = READINT16(save->p);
		if (diff & LD_CLLCOUNT)
			li->callcount = READINT16(save->p);

		si = &sides[li->sidenum[0]];
		if (diff & LD_S1TEXOFF)
			si->textureoffset = READFIXED(save->p);
		if (diff & LD_S1TOPTEX)
			si->toptexture = READINT32(save->p);
		if (diff & LD_S1BOTTEX)
			si->bottomtexture = READINT32(save->p);
		if (diff & LD_S1MIDTEX)
			si->midtexture = READINT32(save->p);

		si = &sides[li->sidenum[1]];
		if (diff2 & LD_S2TEXOFF)
			si->textureoffset = READFIXED(save->p);
		if (diff2 & LD_S2TOPTEX)
			si->toptexture = READINT32(save->p);
		if (diff2 & LD_S2BOTTEX)
			si->bottomtexture = READINT32(save->p);
		if (diff2 & LD_S2MIDTEX)
			si->midtexture = READINT32(save->p);
	}
}

//
// P_NetUnArchiveWorld
//
static void P_NetUnArchiveWorld(savebuffer_t *save)
{
	if (READUINT32(save->p) != ARCHIVEBLOCK_WORLD)
		I_Error("Bad $$$.sav at archive block World");

	UnArchiveSectors(save);
	UnArchiveLines(save);
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

FUNCINLINE static ATTRINLINE UINT32 SaveMobjnum(const mobj_t *mobj)
{
	if (mobj) return mobj->mobjnum;
	return 0;
}

FUNCINLINE static ATTRINLINE UINT32 SaveSector(const sector_t *sector)
{
	if (sector) return (UINT32)(sector - sectors);
	return 0xFFFFFFFF;
}

FUNCINLINE static ATTRINLINE UINT32 SaveLine(const line_t *line)
{
	if (line) return (UINT32)(line - lines);
	return 0xFFFFFFFF;
}

FUNCINLINE static ATTRINLINE UINT32 SavePlayer(const player_t *player)
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
	if (mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE) // These are NEVER saved.
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

	// keep here for vanilla compat
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
		WRITEUINT8(save->p, (UINT8)((skin_t *)mobj->skin - skins));
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
static void SaveFireflickerThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
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
FUNCINLINE static ATTRINLINE void SaveScrollThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
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
FUNCINLINE static ATTRINLINE void SaveFrictionThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
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
FUNCINLINE static ATTRINLINE void SavePusherThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
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
FUNCINLINE static ATTRINLINE void SavePolyrotatetThinker(savebuffer_t *save, const thinker_t *th, const UINT8 type)
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
		if (th->function == (actionf_p1)P_RemoveThinkerDelayed)
			continue;

		numsaved++;

		if (th->function == (actionf_p1)P_MobjThinker)
		{
			SaveMobjThinker(save, th, tc_mobj);
			continue;
		}
		else if (th->function == (actionf_p1)T_MoveCeiling)
		{
			SaveCeilingThinker(save, th, tc_ceiling);
			continue;
		}
		else if (th->function == (actionf_p1)T_CrushCeiling)
		{
			SaveCeilingThinker(save, th, tc_crushceiling);
			continue;
		}
		else if (th->function == (actionf_p1)T_MoveFloor)
		{
			SaveFloormoveThinker(save, th, tc_floor);
			continue;
		}
		else if (th->function == (actionf_p1)T_LightningFlash)
		{
			SaveLightflashThinker(save, th, tc_flash);
			continue;
		}
		else if (th->function == (actionf_p1)T_StrobeFlash)
		{
			SaveStrobeThinker(save, th, tc_strobe);
			continue;
		}
		else if (th->function == (actionf_p1)T_Glow)
		{
			SaveGlowThinker(save, th, tc_glow);
			continue;
		}
		else if (th->function == (actionf_p1)T_FireFlicker)
		{
			SaveFireflickerThinker(save, th, tc_fireflicker);
			continue;
		}
		else if (th->function == (actionf_p1)T_MoveElevator)
		{
			SaveElevatorThinker(save, th, tc_elevator);
			continue;
		}
		else if (th->function == (actionf_p1)T_ContinuousFalling)
		{
			SaveSpecialLevelThinker(save, th, tc_continuousfalling);
			continue;
		}
		else if (th->function == (actionf_p1)T_ThwompSector)
		{
			SaveSpecialLevelThinker(save, th, tc_thwomp);
			continue;
		}
		else if (th->function == (actionf_p1)T_NoEnemiesSector)
		{
			SaveSpecialLevelThinker(save, th, tc_noenemies);
			continue;
		}
		else if (th->function == (actionf_p1)T_EachTimeThinker)
		{
			SaveSpecialLevelThinker(save, th, tc_eachtime);
			continue;
		}
		else if (th->function == (actionf_p1)T_RaiseSector)
		{
			SaveSpecialLevelThinker(save, th, tc_raisesector);
			continue;
		}
		else if (th->function == (actionf_p1)T_CameraScanner)
		{
			SaveElevatorThinker(save, th, tc_camerascanner);
			continue;
		}
		else if (th->function == (actionf_p1)T_Scroll)
		{
			SaveScrollThinker(save, th, tc_scroll);
			continue;
		}
		else if (th->function == (actionf_p1)T_Friction)
		{
			SaveFrictionThinker(save, th, tc_friction);
			continue;
		}
		else if (th->function == (actionf_p1)T_Pusher)
		{
			SavePusherThinker(save, th, tc_pusher);
			continue;
		}
		else if (th->function == (actionf_p1)T_BounceCheese)
		{
			SaveSpecialLevelThinker(save, th, tc_bouncecheese);
			continue;
		}
		else if (th->function == (actionf_p1)T_StartCrumble)
		{
			SaveElevatorThinker(save, th, tc_startcrumble);
			continue;
		}
		else if (th->function == (actionf_p1)T_MarioBlock)
		{
			SaveSpecialLevelThinker(save, th, tc_marioblock);
			continue;
		}
		else if (th->function == (actionf_p1)T_MarioBlockChecker)
		{
			SaveSpecialLevelThinker(save, th, tc_marioblockchecker);
			continue;
		}
		else if (th->function == (actionf_p1)T_SpikeSector)
		{
			SaveSpecialLevelThinker(save, th, tc_spikesector);
			continue;
		}
		else if (th->function == (actionf_p1)T_FloatSector)
		{
			SaveSpecialLevelThinker(save, th, tc_floatsector);
			continue;
		}
		else if (th->function == (actionf_p1)T_BridgeThinker)
		{
			SaveSpecialLevelThinker(save, th, tc_bridgethinker);
			continue;
		}
		else if (th->function == (actionf_p1)T_LaserFlash)
		{
			SaveLaserThinker(save, th, tc_laserflash);
			continue;
		}
		else if (th->function == (actionf_p1)T_LightFade)
		{
			SaveLightlevelThinker(save, th, tc_lightfade);
			continue;
		}
		else if (th->function == (actionf_p1)T_ExecutorDelay)
		{
			SaveExecutorThinker(save, th, tc_executor);
			continue;
		}
		else if (th->function == (actionf_p1)T_Disappear)
		{
			SaveDisappearThinker(save, th, tc_disappear);
			continue;
		}
		else if (th->function == (actionf_p1)T_PolyObjRotate)
		{
			SavePolyrotatetThinker(save, th, tc_polyrotate);
			continue;
		}
		else if (th->function == (actionf_p1)T_PolyObjMove)
		{
			SavePolymoveThinker(save, th, tc_polymove);
			continue;
		}
		else if (th->function == (actionf_p1)T_PolyObjWaypoint)
		{
			SavePolywaypointThinker(save, th, tc_polywaypoint);
			continue;
		}
		else if (th->function == (actionf_p1)T_PolyDoorSlide)
		{
			SavePolyslidedoorThinker(save, th, tc_polyslidedoor);
			continue;
		}
		else if (th->function == (actionf_p1)T_PolyDoorSwing)
		{
			SavePolyswingdoorThinker(save, th, tc_polyswingdoor);
			continue;
		}
		else if (th->function == (actionf_p1)T_PolyObjFlag)
		{
			SavePolymoveThinker(save, th, tc_polyflag);
			continue;
		}
		else if (th->function == (actionf_p1)T_PolyObjDisplace)
		{
			SavePolydisplaceThinker(save, th, tc_polydisplace);
			continue;
		}
#ifdef PARANOIA
		else if (th->function != (actionf_p1)P_RemoveThinkerDelayed) // wait garbage collection
			I_Error("unknown thinker type %p", th->function);
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
		if (th->function != (actionf_p1)P_MobjThinker)
			continue;

		mobj = (mobj_t *)th;

		if (mobj->mobjnum == oldposition)
			return mobj;
	}
	CONS_Debug(DBG_GAMELOGIC, "mobj %d not found\n", oldposition);
	return NULL;
}

FUNCINLINE static ATTRINLINE mobj_t *LoadMobj(UINT32 mobjnum)
{
	if (mobjnum == 0) return NULL;
	return (mobj_t *)(size_t)mobjnum;
}

FUNCINLINE static ATTRINLINE sector_t *LoadSector(UINT32 sector)
{
	if (sector >= numsectors) return NULL;
	return &sectors[sector];
}

FUNCINLINE static ATTRINLINE line_t *LoadLine(UINT32 line)
{
	if (line >= numlines) return NULL;
	return &lines[line];
}

FUNCINLINE static ATTRINLINE player_t *LoadPlayer(UINT32 player)
{
	if (player >= MAXPLAYERS) return NULL;
	return &players[player];
}

//
// LoadMobjThinker
//
// Loads a mobj_t from a save game
//

static mobjtype_t g_doomednum_to_mobjtype[UINT16_MAX];

static void CalculateDoomednumToMobjtype(void)
{
	memset(g_doomednum_to_mobjtype, MT_NULL, sizeof(g_doomednum_to_mobjtype));

	for (size_t i = MT_NULL+1; i < NUMMOBJTYPES; i++)
	{
		if (mobjinfo[i].doomednum > 0 && mobjinfo[i].doomednum <= UINT16_MAX)
		{
			g_doomednum_to_mobjtype[ mobjinfo[i].doomednum ] = i;
		}
	}
}

static void LoadMobjThinker(savebuffer_t *save, actionf_p1 thinker)
{
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

	// here for vanilla compat
	save->p += sizeof(UINT32);

	z = READFIXED(save->p); // Force this so 3dfloor problems don't arise.
	floorz = READFIXED(save->p);
	ceilingz = READFIXED(save->p);

	if (diff & MD_SPAWNPOINT)
	{
		UINT16 spawnpointnum = READUINT16(save->p);

		if (mapthings[spawnpointnum].type == 1705 || mapthings[spawnpointnum].type == 1713) // NiGHTS Hoop special case
		{
			P_SpawnHoops(&mapthings[spawnpointnum]);
			return;
		}

		mobj = P_AllocateMobj();
		mobj->spawnpoint = &mapthings[spawnpointnum];
		mapthings[spawnpointnum].mobj = mobj;
	}
	else
		mobj = P_AllocateMobj();

	// declare this as a valid mobj as soon as possible.
	mobj->thinker.function = thinker;

	mobj->z = z;
	mobj->floorz = floorz;
	mobj->ceilingz = ceilingz;

	if (diff & MD_TYPE)
		mobj->type = READUINT32(save->p);
	else
	{
		mobjtype_t new_type = MT_NULL;
		if (mobj->spawnpoint)
		{
			new_type = g_doomednum_to_mobjtype[mobj->spawnpoint->type];
		}

		if (new_type <= MT_NULL || new_type >= NUMMOBJTYPES)
		{
			if (mobj->spawnpoint)
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing doomednum %d\n", mobj->spawnpoint->type);
			else
				CONS_Alert(CONS_ERROR, "Found mobj with unknown map thing doomednum NULL\n");

			I_Error("Savegame corrupted");
		}

		mobj->type = new_type;
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
		if (i == P_GetLocalPlayerNumForNum(i))
		{
			localangle[i] = mobj->angle;
		}
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
		mobj->skin = &skins[READUINT8(save->p)];
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
	// Sprite Rendering stuff
	mobj->blendmode = AST_TRANSLUCENT;
	mobj->spritexscale = mobj->realxscale = FRACUNIT;
	mobj->spriteyscale = mobj->realyscale = FRACUNIT;
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
	levelspecthink_t *ht = (levelspecthink_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);

	size_t i;
	ht->thinker.function = thinker;

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

	// interpolation
	if (ht->sector)
	{
		if (floorOrCeiling & 2)
			R_CreateInterpolator_SectorPlane(&ht->thinker, ht->sector, true);
		if (floorOrCeiling & 1)
			R_CreateInterpolator_SectorPlane(&ht->thinker, ht->sector, false);
	}
}

//
// LoadCeilingThinker
//
// Loads a ceiling_t from a save game
//
static void LoadCeilingThinker(savebuffer_t *save, actionf_p1 thinker)
{
	ceiling_t *ht = (ceiling_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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

	// interpolation
	if (ht->sector)
		R_CreateInterpolator_SectorPlane(&ht->thinker, ht->sector, true);
}

//
// LoadFloormoveThinker
//
// Loads a floormove_t from a save game
//
static void LoadFloormoveThinker(savebuffer_t *save, actionf_p1 thinker)
{
	floormove_t *ht = (floormove_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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

	// interpolation
	if (ht->sector)
		R_CreateInterpolator_SectorPlane(&ht->thinker, ht->sector, false);
}

//
// LoadLightflashThinker
//
// Loads a lightflash_t from a save game
//
static void LoadLightflashThinker(savebuffer_t *save, actionf_p1 thinker)
{
	lightflash_t *ht = (lightflash_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
	strobe_t *ht = (strobe_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
	glow_t *ht = (glow_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
	fireflicker_t *ht = (fireflicker_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
	elevator_t *ht = (elevator_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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

	// interpolation
	if (ht->sector)
	{
		if (floorOrCeiling & 2)
			R_CreateInterpolator_SectorPlane(&ht->thinker, ht->sector, true);
		if (floorOrCeiling & 1)
			R_CreateInterpolator_SectorPlane(&ht->thinker, ht->sector, false);
	}
}

//
// LoadScrollThinker
//
// Loads a scroll_t from a save game
//
static void LoadScrollThinker(savebuffer_t *save, actionf_p1 thinker)
{
	scroll_t *ht = (scroll_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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

	// interpolation
	switch (ht->type)
	{
		case sc_side:
			R_CreateInterpolator_SideScroll(&ht->thinker, &sides[ht->affectee]);
			break;
		case sc_floor:
			R_CreateInterpolator_SectorScroll(&ht->thinker, &sectors[ht->affectee], false);
			break;
		case sc_ceiling:
			R_CreateInterpolator_SectorScroll(&ht->thinker, &sectors[ht->affectee], true);
			break;
		default:
			break;
	}
}

//
// LoadFrictionThinker
//
// Loads a friction_t from a save game
//
FUNCINLINE static ATTRINLINE void LoadFrictionThinker(savebuffer_t *save, actionf_p1 thinker)
{
	friction_t *ht = (friction_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
	pusher_t *ht = (pusher_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
FUNCINLINE static ATTRINLINE void LoadLaserThinker(savebuffer_t *save, actionf_p1 thinker)
{
	laserthink_t *ht = (laserthink_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ffloor_t *rover = NULL;
	ht->thinker.function = thinker;
	ht->sector = LoadSector(READUINT32(save->p));
	ht->sec = LoadSector(READUINT32(save->p));
	ht->sourceline = LoadLine(READUINT32(save->p));

	for (rover = ht->sector->ffloors; rover; rover = rover->next)
	{
		if (rover->secnum == (size_t)(ht->sec - sectors)
		&& rover->master == ht->sourceline)
			ht->ffloor = rover;
	}

	P_AddThinker(&ht->thinker);
}

//
// LoadLightlevelThinker
//
// Loads a lightlevel_t from a save game
//
FUNCINLINE static ATTRINLINE void LoadLightlevelThinker(savebuffer_t *save, actionf_p1 thinker)
{
	lightlevel_t *ht = (lightlevel_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
FUNCINLINE static ATTRINLINE void LoadExecutorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	executor_t *ht = (executor_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
FUNCINLINE static ATTRINLINE void LoadDisappearThinker(savebuffer_t *save, actionf_p1 thinker)
{
	disappear_t *ht = (disappear_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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
FUNCINLINE static ATTRINLINE void LoadPolyrotatetThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyrotate_t *ht = (polyrotate_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->turnobjs = READUINT8(save->p);
	P_AddThinker(&ht->thinker);

	// interpolation
	polyobj_t *po;

	if (!(po = Polyobj_GetForNum(ht->polyObjNum)))
	{
		CONS_Debug(DBG_POLYOBJ, "LoadPolyrotatetThinker: bad polyobj %d\n", ht->polyObjNum);
		return;
	}

	R_CreateInterpolator_Polyobj(&ht->thinker, po);
}

//
// LoadPolymoveThinker
//
// Loads a polymovet_t thinker
//
static void LoadPolymoveThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polymove_t *ht = (polymove_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->momx = READFIXED(save->p);
	ht->momy = READFIXED(save->p);
	ht->distance = READINT32(save->p);
	ht->angle = READANGLE(save->p);
	P_AddThinker(&ht->thinker);

	// interpolation
	polyobj_t *po;

	if (!(po = Polyobj_GetForNum(ht->polyObjNum)))
	{
		CONS_Debug(DBG_POLYOBJ, "LoadPolymoveThinker: bad polyobj %d\n", ht->polyObjNum);
		return;
	}

	R_CreateInterpolator_Polyobj(&ht->thinker, po);
}

//
// LoadPolywaypointThinker
//
// Loads a polywaypoint_t thinker
//
FUNCINLINE static ATTRINLINE void LoadPolywaypointThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polywaypoint_t *ht = (polywaypoint_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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

	// interpolation
	polyobj_t *po;
	polyobj_t *oldpo;
	INT32 start;

	if (!(po = Polyobj_GetForNum(ht->polyObjNum)))
	{
		CONS_Debug(DBG_POLYOBJ, "LoadPolywaypointThinker: bad polyobj %d\n", ht->polyObjNum);
		return;
	}

	R_CreateInterpolator_Polyobj(&ht->thinker, po);
	// T_PolyObjWaypoint is the only polyobject movement
	// that can adjust z, so we add these ones too.
	R_CreateInterpolator_SectorPlane(&ht->thinker, po->lines[0]->backsector, false);
	R_CreateInterpolator_SectorPlane(&ht->thinker, po->lines[0]->backsector, true);

	// Most other polyobject functions handle children by recursively
	// giving each child another thinker. T_PolyObjWaypoint handles
	// it manually though, which means we need to manually give them
	// interpolation here instead.
	start = 0;
	oldpo = po;
	while ((po = Polyobj_GetChild(oldpo, &start)))
	{
		R_CreateInterpolator_Polyobj(&ht->thinker, po);
		R_CreateInterpolator_SectorPlane(&ht->thinker, po->lines[0]->backsector, false);
		R_CreateInterpolator_SectorPlane(&ht->thinker, po->lines[0]->backsector, true);
	}
}

//
// LoadPolyslidedoorThinker
//
// loads a polyslidedoor_t thinker
//
FUNCINLINE static ATTRINLINE void LoadPolyslidedoorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyslidedoor_t *ht = (polyslidedoor_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
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

	// interpolation
	polyobj_t *po;

	if (!(po = Polyobj_GetForNum(ht->polyObjNum)))
	{
		CONS_Debug(DBG_POLYOBJ, "LoadPolyslidedoorThinker: bad polyobj %d\n", ht->polyObjNum);
		return;
	}

	R_CreateInterpolator_Polyobj(&ht->thinker, po);
}

//
// LoadPolyswingdoorThinker
//
// Loads a polyswingdoor_t thinker
//
FUNCINLINE static ATTRINLINE void LoadPolyswingdoorThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polyswingdoor_t *ht = (polyswingdoor_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->delay = READINT32(save->p);
	ht->delayCount = READINT32(save->p);
	ht->initSpeed = READINT32(save->p);
	ht->speed = READINT32(save->p);
	ht->initDistance = READINT32(save->p);
	ht->distance = READINT32(save->p);
	ht->closing = READUINT8(save->p);
	P_AddThinker(&ht->thinker);

	// interpolation
	polyobj_t *po;

	if (!(po = Polyobj_GetForNum(ht->polyObjNum)))
	{
		CONS_Debug(DBG_POLYOBJ, "LoadPolyswingdoorThinker: bad polyobj %d\n", ht->polyObjNum);
		return;
	}

	R_CreateInterpolator_Polyobj(&ht->thinker, po);
}

//
// LoadPolydisplaceThinker
//
// Loads a polydisplace_t thinker
//
FUNCINLINE static ATTRINLINE void LoadPolydisplaceThinker(savebuffer_t *save, actionf_p1 thinker)
{
	polydisplace_t *ht = (polydisplace_t*)Z_LevelPoolMalloc(sizeof (*ht));
	ht->thinker.alloctype = TAT_LEVELPOOL;
	ht->thinker.size = sizeof (*ht);
	ht->thinker.function = thinker;
	ht->polyObjNum = READINT32(save->p);
	ht->controlSector = LoadSector(READUINT32(save->p));
	ht->dx = READFIXED(save->p);
	ht->dy = READFIXED(save->p);
	ht->oldHeights = READFIXED(save->p);
	P_AddThinker(&ht->thinker);

	// interpolation
	polyobj_t *po;

	if (!(po = Polyobj_GetForNum(ht->polyObjNum)))
	{
		CONS_Debug(DBG_POLYOBJ, "LoadPolydisplaceThinker: bad polyobj %d\n", ht->polyObjNum);
		return;
	}

	R_CreateInterpolator_Polyobj(&ht->thinker, po);
}

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

	// Pre-calculate this lookup, because it was wasting
	// a shit ton of time loading mobj thinkers.
	CalculateDoomednumToMobjtype();

	// remove all the current thinkers
	for (currentthinker = thinkercap.next; currentthinker != &thinkercap; currentthinker = next)
	{
		next = currentthinker->next;

		if (currentthinker->function == (actionf_p1)P_MobjThinker)
			P_RemoveSavegameMobj((mobj_t *)currentthinker); // item isn't saved, don't remove it
		else
		{
			(next->prev = currentthinker->prev)->next = next;
			R_DestroyLevelInterpolators(currentthinker);
			if (currentthinker->alloctype == TAT_LEVELPOOL)
			{
				Z_LevelPoolFree(currentthinker, currentthinker->size);
			}
			else
			{
				Z_Free(currentthinker);
			}
		}
	}

	// remove all the current precip thinkers
	P_PurgePrecipitation();

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
			if (currentthinker->function != (actionf_p1)T_ExecutorDelay)
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

FUNCINLINE static ATTRINLINE void P_ArchivePolyObj(savebuffer_t *save, polyobj_t *po)
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

FUNCINLINE static ATTRINLINE void P_UnArchivePolyObj(savebuffer_t *save, polyobj_t *po)
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

FUNCINLINE static ATTRINLINE void P_ArchivePolyObjects(savebuffer_t *save)
{
	INT32 i;

	WRITEUINT32(save->p, ARCHIVEBLOCK_POBJS);

	// save number of polyobjects
	WRITEINT32(save->p, numPolyObjects);

	for (i = 0; i < numPolyObjects; ++i)
		P_ArchivePolyObj(save, &PolyObjects[i]);
}

FUNCINLINE static ATTRINLINE void P_UnArchivePolyObjects(savebuffer_t *save)
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

FUNCINLINE static ATTRINLINE mobj_t *RelinkMobj(mobj_t **ptr)
{
	UINT32 temp = (UINT32)(size_t)*ptr;
	*ptr = NULL;
	return P_SetTarget(ptr, P_FindNewPosition(temp));
}

static void P_RelinkPointers(void)
{
	thinker_t *currentthinker;
	mobj_t *mobj;
	player_t *player;

	// use info field (value = oldposition) to relink mobjs
	for (currentthinker = thinkercap.next; currentthinker != &thinkercap;
		currentthinker = currentthinker->next)
	{
		if (currentthinker->function != (actionf_p1)P_MobjThinker)
			continue;

		mobj = (mobj_t *)currentthinker;

		if (UNLIKELY(mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER))
			continue;

#define RELINK(obj, name) if ((obj) && !RelinkMobj(&(obj))) \
		CONS_Debug(DBG_GAMELOGIC, name " not found on %d\n", obj->type);

		RELINK(mobj->tracer, "tracer");
		RELINK(mobj->target, "target");
		RELINK(mobj->hnext, "hnext");
		RELINK(mobj->hprev, "hprev");

		player = mobj->player;

		if (player)
		{
			RELINK(player->capsule, "capsule");
			RELINK(player->axis1, "axis1");
			RELINK(player->axis2, "axis2");
			RELINK(player->awayviewmobj, "awayviewmobj");
		}
#undef RELINK
	}
}

//
// P_NetArchiveSpecials
//
FUNCINLINE static ATTRINLINE void P_NetArchiveSpecials(savebuffer_t *save)
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

	WRITEUINT8(save->p, 0x00); // metal sonic
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

	READUINT8(save->p); // metal sonic
}

// =======================================================================
//          Misc
// =======================================================================
FUNCINLINE static ATTRINLINE void P_ArchiveMisc(savebuffer_t *save)
{
	if (gamecomplete)
		WRITEINT16(save->p, gamemap | 8192);
	else
		WRITEINT16(save->p, gamemap);

	lastmapsaved = gamemap;

	WRITEUINT16(save->p, (botskin ? (emeralds|(1<<10)) : emeralds)+357);
	WRITESTRINGN(save->p, timeattackfolder, sizeof(timeattackfolder));
}

FUNCINLINE static ATTRINLINE void P_UnArchiveSPGame(savebuffer_t *save, INT16 mapoverride)
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
	if (!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	lastmapsaved = gamemap;

	tokenlist = 0;
	token = 0;

	savedata.emeralds = READUINT16(save->p)-357;
	if (savedata.emeralds & (1<<10))
		savedata.botcolor = 0xFF;
	savedata.emeralds &= 0xff;

	READSTRINGN(save->p, testname, sizeof(testname));

	if (!fastcmp(testname, timeattackfolder))
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

	if (resending)
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

static void P_ReloadSaveLevelData(void)
{
	size_t i;

	// Only reload stuff that can we modify in the save states themselves.
	// This is still orders of magnitude faster than a full level reload.
	// Considered memcpy, but it's complicated -- save that for local saves.

	sector_t *ss = sectors;
	sector_t *spawnss = spawnsectors;

	for (i = 0; i < numsectors; i++, ss++, spawnss++)
	{
		ss->floorheight = spawnss->floorheight;
		ss->ceilingheight = spawnss->ceilingheight;
		ss->floorpic = spawnss->floorpic;
		ss->ceilingpic = spawnss->ceilingpic;
		ss->lightlevel = spawnss->lightlevel;
		ss->special = spawnss->special;
		ss->floor_xoffs = spawnss->floor_xoffs;
		ss->floor_yoffs = spawnss->floor_yoffs;
		ss->ceiling_xoffs = spawnss->ceiling_xoffs;
		ss->ceiling_yoffs = spawnss->ceiling_yoffs;
		ss->floorpic_angle = spawnss->floorpic_angle;
		ss->ceilingpic_angle = spawnss->ceilingpic_angle;
		ss->tag = spawnss->tag;
		ss->firsttag = spawnss->firsttag;
		ss->nexttag  = spawnss->nexttag;

		if (ss->ffloors)
		{
			ffloor_t *rover;

			for (rover = ss->ffloors; rover; rover = rover->next)
			{
				rover->flags = rover->spawnflags;
				rover->alpha = rover->spawnalpha;
			}
		}
	}

	line_t *li = lines;
	line_t *spawnli = spawnlines;
	side_t *si = NULL;
	side_t *spawnsi = NULL;

	for (i = 0; i < numlines; i++, spawnli++, li++)
	{
		li->special = spawnli->special;
		li->callcount = 0;

		li->tag = spawnli->tag;
		li->firsttag = spawnli->firsttag;
		li->nexttag  = spawnli->nexttag;

		if (li->sidenum[0] != 0xffff)
		{
			si = &sides[li->sidenum[0]];
			spawnsi = &spawnsides[li->sidenum[0]];

			si->textureoffset = spawnsi->textureoffset;
			si->toptexture = spawnsi->toptexture;
			si->bottomtexture = spawnsi->bottomtexture;
			si->midtexture = spawnsi->midtexture;
		}

		if (li->sidenum[1] != 0xffff)
		{
			si = &sides[li->sidenum[1]];
			spawnsi = &spawnsides[li->sidenum[1]];

			si->textureoffset = spawnsi->textureoffset;
			si->toptexture = spawnsi->toptexture;
			si->bottomtexture = spawnsi->bottomtexture;
			si->midtexture = spawnsi->midtexture;
		}
	}
}

FUNCINLINE static ATTRINLINE boolean P_NetUnArchiveMisc(savebuffer_t *save, boolean reloading)
{
	UINT32 pig;
	INT32 i;

	const INT16 prevgamemap = gamemap;

	if (READUINT32(save->p) != ARCHIVEBLOCK_MISC)
		I_Error("Bad $$$.sav at archive block Misc");

	if (reloading)
		gametic = READUINT32(save->p);

	gamemap = READINT16(save->p);

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if (!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	// tell the sound code to reset the music since we're skipping what
	// normally sets this flag
	if (!reloading)
		mapmusic.flags |= MUSIC_RELOADRESET;

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

	// Only reload the level during a gamestate reload
	// if the map is horribly mismatched somehow. Minor
	// differences in level state are already handled
	// by other parts of the reload, so doing this
	// on *every* reload wastes lots of time that we
	// will need for rollback down the road.
	if (!reloading || prevgamemap != gamemap)
	{
		if (!P_SetupLevel(true, reloading))
		{
			CONS_Alert(CONS_ERROR, M_GetText("Can't load the level!\n"));
			return false;
		}
	}
	else
	{
		P_ReloadSaveLevelData();
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
	UINT32 i = 1; // don't start from 0, it'd be confused with a blank pointer otherwise

	CV_SaveNetVars(&save->p, false);
	P_NetArchiveMisc(save, resending);

	// Assign the mobjnumber for pointer tracking
	if (gamestate == GS_LEVEL)
	{
		for (th = thinkercap.next; th != &thinkercap; th = th->next)
		{
			if (th->function != (actionf_p1)P_MobjThinker)
				continue;

			mobj = (mobj_t *)th;

			if (UNLIKELY(mobj->type == MT_HOOP || mobj->type == MT_HOOPCOLLIDE || mobj->type == MT_HOOPCENTER))
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
	save->p += CV_LoadNetVars(save->p);

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
