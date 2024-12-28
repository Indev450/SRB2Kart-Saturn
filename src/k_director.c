// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by AJ "Tyron" Martinez.
// Copyright (C) 2024 by James Robert Roman.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  k_director.c
/// \brief SRB2kart automatic spectator camera.

//#include "k_kart.h"
#include "doomdef.h"
#include "doomstat.h"
#include "g_game.h"
#include "v_video.h"
#include "k_director.h"
#include "d_netcmd.h"
#include "p_local.h"
#include "st_stuff.h"

#include "r_fps.h"

#define SWITCHTIME TICRATE * 5		// cooldown between unforced switches
#define BOREDOMTIME 3 * TICRATE / 2 // how long until players considered far apart?
#define TRANSFERTIME TICRATE		// how long to delay reaction shots?
#define BREAKAWAYDIST 2000			// how *far* until players considered far apart?
#define WALKBACKDIST 400			// how close should a trailing player be before we switch?
#define PINCHDIST 20000				// how close should the leader be to be considered "end of race"?

struct directorinfo
{
	boolean active;
	tic_t cooldown; // how long has it been since we last switched?
	tic_t freeze;   // when nonzero, fixed switch pending, freeze logic!
	INT32 attacker; // who to switch to when freeze delay elapses
	INT32 maxdist;  // how far is the closest player from finishing?

	INT32 sortedplayers[MAXPLAYERS]; // position-1 goes in, player index comes out.
	INT32 gap[MAXPLAYERS];           // gap between a given position and their closest pursuer
	INT32 boredom[MAXPLAYERS];       // how long has a given position had no credible attackers?
} directorinfo;

struct directorinfo directorinfosplit[MAXSPLITSCREENPLAYERS];

boolean K_DirectorIsPlayerAlone(void)
{
	UINT8 pingame = 0;

	// Gotta check how many players are active at this moment.
	for (UINT8 i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		pingame++;

		if (pingame >= 2) // we dont need to check further
			break;
	}

	return (pingame <= 1);
}

static inline boolean race_rules(void)
{
	return gametype == GT_RACE;
}

static fixed_t ScaleFromMap(fixed_t n, fixed_t scale)
{
	return FixedMul(n, FixedDiv(scale, mapobjectscale));
}

static boolean K_DirectorIsEnabled(const UINT8 viewnum)
{
	//return cv_director.value && !splitscreen && (gamestate == GS_LEVEL && (((!playeringame[consoleplayer] || players[consoleplayer].spectator)) || (demo.playback && !camera[0].freecam && (!demo.title || !modeattacking))) && !K_DirectorIsPlayerAlone());
	return directorinfosplit[viewnum].active;
}

void K_InitDirector(void)
{
	INT32 playernum;

	for (UINT8 i = 0; i < MAXSPLITSCREENPLAYERS; i++)
	{
		directorinfosplit[i].cooldown = SWITCHTIME;
		directorinfosplit[i].freeze = 0;
		directorinfosplit[i].attacker = 0;
		directorinfosplit[i].maxdist = 0;
		directorinfosplit[i].active = 0;

		for (playernum = 0; playernum < MAXPLAYERS; playernum++)
		{
			directorinfosplit[i].sortedplayers[playernum] = -1;
			directorinfosplit[i].gap[playernum] = INT32_MAX;
			directorinfosplit[i].boredom[playernum] = 0;
		}
	}
}

static fixed_t K_GetDistanceToFinish(player_t player)
{
	mobj_t *mo;
	fixed_t dist = 0;
	INT16 maxMoveCount = -1;
	INT16 maxAngle = -1;

	if (!(mapheaderinfo[gamemap - 1]->levelflags & LF_SECTIONRACE))
	{
		for (mo = waypointcap; mo != NULL; mo = mo->tracer)
		{
			if (mo->spawnpoint->angle != 0)
				continue;

			dist = P_AproxDistance(P_AproxDistance(mo->x - player.mo->x,
												mo->y - player.mo->y),
							mo->z - player.mo->z) / FRACUNIT;

			break;
		}
	}
	else // crappy optimization weeeee
	{
		for (mo = waypointcap; mo != NULL; mo = mo->tracer)
		{
			if (mo->movecount > maxMoveCount)
				maxMoveCount = mo->movecount;
			if (mo->spawnpoint->angle > maxAngle)
				maxAngle = mo->spawnpoint->angle;

			if (!(mo->movecount == maxMoveCount && mo->spawnpoint->angle == maxAngle)) // sprint maps finishline waypoint is the one with highest movecount AND angle
				continue;

			dist = P_AproxDistance(P_AproxDistance(mo->x - player.mo->x,
												   mo->y - player.mo->y),
						  mo->z - player.mo->z) / FRACUNIT;

			break;
		}
	}

	return dist;
}

static fixed_t K_GetFinishGap(INT32 leader, INT32 follower)
{
	fixed_t dista = K_GetDistanceToFinish(players[follower]);
	fixed_t distb = K_GetDistanceToFinish(players[leader]);

	if (players[follower].kartstuff[k_position] < players[leader].kartstuff[k_position])
	{
		return distb - dista;
	}
	else
	{
		return dista - distb;
	}
}

static void K_UpdateDirectorPositions(const UINT8 viewnum)
{
	INT32 playernum;
	INT32 position;
	player_t* target;

	memset(directorinfosplit[viewnum].sortedplayers, -1, sizeof(directorinfo.sortedplayers));

	for (playernum = 0; playernum < MAXPLAYERS; playernum++)
	{
		target = &players[playernum];

		if (playeringame[playernum] && !target->spectator && target->kartstuff[k_position] > 0)
		{
			directorinfosplit[viewnum].sortedplayers[target->kartstuff[k_position] - 1] = playernum;
		}
	}

	for (position = 0; position < MAXPLAYERS - 1; position++)
	{
		directorinfosplit[viewnum].gap[position] = INT32_MAX;

		if (directorinfosplit[viewnum].sortedplayers[position] == -1 || directorinfosplit[viewnum].sortedplayers[position + 1] == -1)
		{
			continue;
		}

		directorinfosplit[viewnum].gap[position] = ScaleFromMap(K_GetFinishGap(directorinfosplit[viewnum].sortedplayers[position], directorinfosplit[viewnum].sortedplayers[position + 1]), FRACUNIT);

		if (directorinfosplit[viewnum].gap[position] >= BREAKAWAYDIST)
		{
			directorinfosplit[viewnum].boredom[position] = (INT32)(min(BOREDOMTIME * 2, directorinfosplit[viewnum].boredom[position] + 1));
		}
		else if (directorinfosplit[viewnum].boredom[position] > 0)
		{
			directorinfosplit[viewnum].boredom[position]--;
		}
	}

	if (directorinfosplit[viewnum].sortedplayers[0] == -1)
	{
		directorinfosplit[viewnum].maxdist = -1;
		return;
	}

	directorinfosplit[viewnum].maxdist = ScaleFromMap(K_GetDistanceToFinish(players[directorinfosplit[viewnum].sortedplayers[0]]), FRACUNIT);
}

static boolean K_CanSwitchDirector(const UINT8 viewnum)
{
	if (directorinfosplit[viewnum].cooldown > 0)
	{
		return false;
	}

	return true;
}

static void K_DirectorSwitch(INT32 player, boolean force, const UINT8 viewnum)
{
	if (!K_DirectorIsEnabled(viewnum))
	{
		return;
	}

	if (P_IsDisplayPlayer(&players[player]))
	{
		return;
	}

	if (players[player].exiting)
	{
		return;
	}

	if (!force && !K_CanSwitchDirector(viewnum))
	{
		return;
	}

	G_ResetView(viewnum+1, player, true);
	directorinfosplit[viewnum].cooldown = SWITCHTIME;
}

static void K_DirectorForceSwitch(INT32 player, INT32 time, const UINT8 viewnum)
{
	if (players[player].exiting)
	{
		return;
	}

	directorinfosplit[viewnum].attacker = player;
	directorinfosplit[viewnum].freeze = time;
}

static UINT8 curview = 0;

void K_DirectorFollowAttack(player_t *player, mobj_t *inflictor, mobj_t *source)
{
	if (player != &players[displayplayers[curview]])
		return;

	if (!K_DirectorIsEnabled(curview))
	{
		return;
	}

	if (!P_IsDisplayPlayer(player))
	{
		return;
	}

	if (inflictor && inflictor->player)
	{
		K_DirectorForceSwitch(inflictor->player - players, TRANSFERTIME, curview);
	}
	else if (source && source->player)
	{
		K_DirectorForceSwitch(source->player - players, TRANSFERTIME, curview);
	}
}

void K_DrawDirectorDebugger(void)
{
	INT32 position;
	INT32 leader;
	INT32 follower;
	INT32 ytxt;

	if (!cv_kartdebugdirector.value)
	{
		return;
	}

	V_DrawThinString(10, 0, V_70TRANS, va("PLACE"));
	V_DrawThinString(40, 0, V_70TRANS, va("CONF?"));
	V_DrawThinString(80, 0, V_70TRANS, va("GAP"));
	V_DrawThinString(120, 0, V_70TRANS, va("BORED"));
	V_DrawThinString(150, 0, V_70TRANS, va("COOLDOWN: %d", directorinfo.cooldown));
	V_DrawThinString(230, 0, V_70TRANS, va("MAXDIST: %d", directorinfo.maxdist));

	for (position = 0; position < MAXPLAYERS - 1; position++)
	{
		ytxt = 10 * (position + 1);
		leader = directorinfo.sortedplayers[position];
		follower = directorinfo.sortedplayers[position + 1];

		if (leader == -1 || follower == -1)
			break;

		V_DrawThinString(10, ytxt, V_70TRANS, va("%d", position));
		V_DrawThinString(20, ytxt, V_70TRANS, va("%d", position + 1));

		if (players[leader].kartstuff[k_positiondelay])
		{
			V_DrawThinString(40, ytxt, V_70TRANS, va("NG"));
		}

		V_DrawThinString(80, ytxt, V_70TRANS, va("%d", directorinfo.gap[position]));

		if (directorinfo.boredom[position] >= BOREDOMTIME)
		{
			V_DrawThinString(120, ytxt, V_70TRANS, va("BORED"));
		}
		else
		{
			V_DrawThinString(120, ytxt, V_70TRANS, va("%d", directorinfo.boredom[position]));
		}

		V_DrawThinString(150, ytxt, V_70TRANS, va("%s", player_names[leader]));
		V_DrawThinString(230, ytxt, V_70TRANS, va("%s", player_names[follower]));
	}
}

void K_UpdateDirector(const UINT8 viewnum)
{
	INT32 *displayplayerp = &displayplayers[viewnum];
	INT32 targetposition;

	curview = viewnum;

	if (!K_DirectorIsEnabled(viewnum))
	{
		return;
	}

	K_UpdateDirectorPositions(viewnum);

	if (directorinfosplit[viewnum].cooldown > 0) {
		directorinfosplit[viewnum].cooldown--;
	}

	// handle pending forced switches
	if (directorinfosplit[viewnum].freeze > 0)
	{
		if (!(--directorinfosplit[viewnum].freeze))
			K_DirectorSwitch(directorinfosplit[viewnum].attacker, true, viewnum);

		return;
	}

	// if there's only one player left in the list, just switch to that player
	if (directorinfosplit[viewnum].sortedplayers[0] != -1 && (directorinfosplit[viewnum].sortedplayers[1] == -1 ||
		// TODO: Battle; I just threw this together quick. Focus on leader.
		!race_rules()))
	{
		K_DirectorSwitch(directorinfosplit[viewnum].sortedplayers[0], false, viewnum);
		return;
	}

	// aaight, time to walk through the standings to find the first interesting pair
	// NB: targetposition/sortedplayers is 0-indexed, aiming at the "back half" of a given pair by default.
	// we adjust for this when comparing to player->position or when looking at the leading player, Don't Freak Out
	for (targetposition = 1; targetposition < MAXPLAYERS; targetposition++)
	{
		INT32 target;

		// you are out of players, try again
		if (directorinfosplit[viewnum].sortedplayers[targetposition] == -1)
		{
			break;
		}

		// pair too far apart? try the next one
		if (directorinfosplit[viewnum].boredom[targetposition - 1] >= BOREDOMTIME)
		{
			continue;
		}

		// pair finished? try the next one
		if (players[directorinfosplit[viewnum].sortedplayers[targetposition]].exiting)
		{
			continue;
		}

		// don't risk switching away from forward pairs at race end, might miss something!
		if (directorinfosplit[viewnum].maxdist > PINCHDIST)
		{
			// if the "next" player is close enough, they should be able to see everyone fine!
			// walk back through the standings to find a vantage that gets everyone in frame.
			// (also creates a pretty cool effect w/ overtakes at speed)
			while (targetposition < MAXPLAYERS && directorinfosplit[viewnum].gap[targetposition] < WALKBACKDIST)
			{
				targetposition++;
			}
		}

		target = directorinfosplit[viewnum].sortedplayers[targetposition];

		// stop here since we're already viewing this player
		if (*displayplayerp == target)
		{
			break;
		}

		// if this is a splitscreen player, try next pair
		if (P_IsDisplayPlayer(&players[target]))
		{
			continue;
		}

		// if we're certain the back half of the pair is actually in this position, try to switch
		if (!players[target].kartstuff[k_positiondelay])
		{
			K_DirectorSwitch(target, false, viewnum);
		}

		// even if we're not certain, if we're certain we're watching the WRONG player, try to switch
		if (players[*displayplayerp].kartstuff[k_position] != targetposition+1 && !players[*displayplayerp].kartstuff[k_positiondelay])
		{
			K_DirectorSwitch(target, false, viewnum);
		}

		break;
	}
}

void K_ToggleDirector(const UINT8 viewnum)
{
	//if (!directortextactive)
		//return;

	if (!K_DirectorIsEnabled(viewnum))
	{
		directorinfosplit[viewnum].cooldown = 0; // switch immediately
	}

	directortoggletimer = 0;

	if (directorinfosplit[viewnum].active == false)
		directorinfosplit[viewnum].active = true;
	else
		directorinfosplit[viewnum].active = false;

	CONS_Printf("director active %d for %d\n", directorinfosplit[viewnum].active, viewnum);

	//COM_ImmedExecute("add director 1");
}
