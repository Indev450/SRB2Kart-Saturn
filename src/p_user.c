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
/// \file  p_user.c
/// \brief New stuff?
///        Player related stuff.
///        Bobbing POV/weapon, movement.
///        Pending weapon.

#include "doomdef.h"
#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "g_game.h"
#include "p_local.h"
#include "r_fps.h"
#include "r_main.h"
#include "s_sound.h"
#include "r_things.h"
#include "d_think.h"
#include "r_sky.h"
#include "p_setup.h"
#include "m_random.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_joy.h"
#include "p_slopes.h"
#include "p_spec.h"
#include "r_splats.h"
#include "z_zone.h"
#include "w_wad.h"
#include "hu_stuff.h"
// We need to affect the NiGHTS hud
#include "st_stuff.h"
#include "lua_script.h"
#include "lua_hook.h"
#include "b_bot.h"
// Objectplace
#include "m_cheat.h"
// SRB2kart
#include "m_cond.h" // M_UpdateUnlockablesAndExtraEmblems
#include "k_kart.h"
#include "console.h" // CON_LogMessage
#include "m_menu.h"

#ifdef HW3SOUND
#include "hardware/hw3sound.h"
#endif

#ifdef HWRENDER
#include "hardware/hw_light.h"
#include "hardware/hw_main.h"
#endif

#if 0
static void P_NukeAllPlayers(player_t *player);
#endif

//
// Movement.
//

// 16 pixels of bob
//#define MAXBOB (0x10 << FRACBITS)

static boolean onground;

//
// P_Thrust
// Moves the given origin along a given angle.
//
void P_Thrust(mobj_t *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	mo->momx += FixedMul(move, FINECOSINE(angle));

	if (!(twodlevel || (mo->flags2 & MF2_TWOD)))
		mo->momy += FixedMul(move, FINESINE(angle));
}

#if 0
static inline void P_ThrustEvenIn2D(mobj_t *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	mo->momx += FixedMul(move, FINECOSINE(angle));
	mo->momy += FixedMul(move, FINESINE(angle));
}

static inline void P_VectorInstaThrust(fixed_t xa, fixed_t xb, fixed_t xc, fixed_t ya, fixed_t yb, fixed_t yc,
	fixed_t za, fixed_t zb, fixed_t zc, fixed_t momentum, mobj_t *mo)
{
	fixed_t a1, b1, c1, a2, b2, c2, i, j, k;

	a1 = xb - xa;
	b1 = yb - ya;
	c1 = zb - za;
	a2 = xb - xc;
	b2 = yb - yc;
	c2 = zb - zc;
/*
	// Convert to unit vectors...
	a1 = FixedDiv(a1,FixedSqrt(FixedMul(a1,a1) + FixedMul(b1,b1) + FixedMul(c1,c1)));
	b1 = FixedDiv(b1,FixedSqrt(FixedMul(a1,a1) + FixedMul(b1,b1) + FixedMul(c1,c1)));
	c1 = FixedDiv(c1,FixedSqrt(FixedMul(c1,c1) + FixedMul(c1,c1) + FixedMul(c1,c1)));

	a2 = FixedDiv(a2,FixedSqrt(FixedMul(a2,a2) + FixedMul(c2,c2) + FixedMul(c2,c2)));
	b2 = FixedDiv(b2,FixedSqrt(FixedMul(a2,a2) + FixedMul(c2,c2) + FixedMul(c2,c2)));
	c2 = FixedDiv(c2,FixedSqrt(FixedMul(a2,a2) + FixedMul(c2,c2) + FixedMul(c2,c2)));
*/
	// Calculate the momx, momy, and momz
	i = FixedMul(momentum, FixedMul(b1, c2) - FixedMul(c1, b2));
	j = FixedMul(momentum, FixedMul(c1, a2) - FixedMul(a1, c2));
	k = FixedMul(momentum, FixedMul(a1, b2) - FixedMul(a1, c2));

	mo->momx = i;
	mo->momy = j;
	mo->momz = k;
}
#endif

//
// P_InstaThrust
// Moves the given origin along a given angle instantly.
//
// FIXTHIS: belongs in another file, not here
//
void P_InstaThrust(mobj_t *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	mo->momx = FixedMul(move, FINECOSINE(angle));

	if (!(twodlevel || (mo->flags2 & MF2_TWOD)))
		mo->momy = FixedMul(move,FINESINE(angle));
}

void P_InstaThrustEvenIn2D(mobj_t *mo, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	mo->momx = FixedMul(move, FINECOSINE(angle));
	mo->momy = FixedMul(move, FINESINE(angle));
}

// Returns a location (hard to explain - go see how it is used)
fixed_t P_ReturnThrustX(mobj_t *mo, angle_t angle, fixed_t move)
{
	(void)mo;
	angle >>= ANGLETOFINESHIFT;
	return FixedMul(move, FINECOSINE(angle));
}
fixed_t P_ReturnThrustY(mobj_t *mo, angle_t angle, fixed_t move)
{
	(void)mo;
	angle >>= ANGLETOFINESHIFT;
	return FixedMul(move, FINESINE(angle));
}

//
// P_AutoPause
// Returns true when gameplay should be halted even if the game isn't necessarily paused.
//
boolean P_AutoPause(void)
{
	// Don't pause even on menu-up or focus-lost in netgames or record attack
	if (netgame || modeattacking || demo.title)
		return false;

	return ((menuactive && !demo.playback) || ( window_notinfocus && cv_pauseifunfocused.value ));
}

//
// P_CalcHeight
// Calculate the walking / running height adjustment
//
void P_CalcHeight(player_t *player)
{
	//INT32 angle;
	//fixed_t bob;
	//fixed_t pviewheight;
	mobj_t *mo = player->mo;

	// Regular movement bobbing.
	// Should not be calculated when not on ground (FIXTHIS?)
	// OPTIMIZE: tablify angle
	// Note: a LUT allows for effects
	//  like a ramp with low health.

	/*player->bob = (FixedMul(player->rmomx,player->rmomx)
		+ FixedMul(player->rmomy,player->rmomy))>>2;

	if (player->bob > FixedMul(MAXBOB, mo->scale))
		player->bob = FixedMul(MAXBOB, mo->scale);*/

	if (!P_IsObjectOnGround(mo))
	{
		if (mo->eflags & MFE_VERTICALFLIP)
		{
			player->viewz = mo->z + mo->height - player->viewheight;
			if (player->viewz < mo->floorz + FixedMul(FRACUNIT, mo->scale))
				player->viewz = mo->floorz + FixedMul(FRACUNIT, mo->scale);
		}
		else
		{
			player->viewz = mo->z + player->viewheight;
			if (player->viewz > mo->ceilingz - FixedMul(FRACUNIT, mo->scale))
				player->viewz = mo->ceilingz - FixedMul(FRACUNIT, mo->scale);
		}
		return;
	}

	//angle = (FINEANGLES/20*localgametic)&FINEMASK;
	//bob = FixedMul(player->bob/2, FINESINE(angle));

	// move viewheight
	player->viewheight = FixedMul(32 << FRACBITS, mo->scale); // default eye view height

	/*if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > pviewheight)
		{
			player->viewheight = pviewheight;
			player->deltaviewheight = 0;
		}

		if (player->viewheight < pviewheight/2)
		{
			player->viewheight = pviewheight/2;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}

		if (player->deltaviewheight)
		{
			player->deltaviewheight += FixedMul(FRACUNIT/4, mo->scale);
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}*/

	if (player->mo->eflags & MFE_VERTICALFLIP)
		player->viewz = mo->z + mo->height - player->viewheight; //- bob
	else
		player->viewz = mo->z + player->viewheight; //+ bob

	if (player->viewz > mo->ceilingz-FixedMul(4*FRACUNIT, mo->scale))
		player->viewz = mo->ceilingz-FixedMul(4*FRACUNIT, mo->scale);
	if (player->viewz < mo->floorz+FixedMul(4*FRACUNIT, mo->scale))
		player->viewz = mo->floorz+FixedMul(4*FRACUNIT, mo->scale);
}

/** Decides if a player is moving.
  * \param pnum The player number to test.
  * \return True if the player is considered to be moving.
  * \author Graue <graue@oceanbase.org>
  */
boolean P_PlayerMoving(INT32 pnum)
{
	player_t *p = &players[pnum];

	if (!Playing())
		return false;

	if (p->jointime < 5*TICRATE || p->playerstate == PST_DEAD || p->playerstate == PST_REBORN || p->spectator)
		return false;

	return gamestate == GS_LEVEL && p->mo && p->mo->health > 0
		&& (abs(p->rmomx) >= FixedMul(FRACUNIT/2, p->mo->scale)
			|| abs(p->rmomy) >= FixedMul(FRACUNIT/2, p->mo->scale)
			|| abs(p->mo->momz) >= FixedMul(FRACUNIT/2, p->mo->scale)
			|| p->climbing || p->powers[pw_tailsfly]
			|| (p->pflags & PF_JUMPED) || (p->pflags & PF_SPINNING));
}

// P_GetNextEmerald
//
// Gets the number (0 based) of the next emerald to obtain
//
UINT8 P_GetNextEmerald(void)
{
	if (!useNightsSS) // In order
	{
		if (!(emeralds & EMERALD1)) return 0;
		if (!(emeralds & EMERALD2)) return 1;
		if (!(emeralds & EMERALD3)) return 2;
		if (!(emeralds & EMERALD4)) return 3;
		if (!(emeralds & EMERALD5)) return 4;
		if (!(emeralds & EMERALD6)) return 5;
		return 6;
	}
	else // Depends on stage
	{
		if (gamemap < sstage_start || gamemap > sstage_end)
			return 0;
		return (UINT8)(gamemap - sstage_start);
	}
}

//
// P_GiveEmerald
//
// Award an emerald upon completion
// of a special stage.
//
void P_GiveEmerald(boolean spawnObj)
{
	INT32 i;
	UINT8 em;

	S_StartSound(NULL, sfx_cgot); // Got the emerald!
	em = P_GetNextEmerald();
	emeralds |= (1 << em);

	if (spawnObj)
	{
		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i])
				P_SetMobjState(P_SpawnMobj(players[i].mo->x, players[i].mo->y, players[i].mo->z + players[i].mo->info->height, MT_GOTEMERALD),
				mobjinfo[MT_GOTEMERALD].spawnstate + em);

	}
}

//
// P_ResetScore
//
// This is called when your chain is reset.
void P_ResetScore(player_t *player)
{
	// Formally a host for Chaos mode behavior

	player->scoreadd = 0;
}

//
// P_FindLowestMare
//
// Returns the lowest open mare available
//
/*UINT8 P_FindLowestMare(void)
{
	thinker_t *th;
	mobj_t *mo2;
	UINT8 mare = UINT8_MAX;

	if (G_RaceGametype())
		return 0;

	// scan the thinkers
	// to find the egg capsule with the lowest mare
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		if (mo2->type == MT_EGGCAPSULE && mo2->health > 0)
		{
			const UINT8 threshold = (UINT8)mo2->threshold;
			if (mare == 255)
				mare = threshold;
			else if (threshold < mare)
				mare = threshold;
		}
	}

	CONS_Debug(DBG_NIGHTS, "Lowest mare found: %d\n", mare);

	return mare;
}*/

//
// P_FindLowestLap
//
// SRB2Kart, a similar function as above for finding the lowest lap
//
UINT8 P_FindLowestLap(void)
{
	INT32 i;
	UINT8 lowest = UINT8_MAX;

	if (!G_RaceGametype())
		return 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (lowest == 255)
			lowest = players[i].laps;
		else if (players[i].laps < lowest)
			lowest = players[i].laps;
	}

	CONS_Debug(DBG_GAMELOGIC, "Lowest laps found: %d\n", lowest);

	return lowest;
}

//
// P_FindHighestLap
//
UINT8 P_FindHighestLap(void)
{
	INT32 i;
	UINT8 highest = 0;

	if (!G_RaceGametype())
		return 0;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (players[i].laps > highest)
			highest = players[i].laps;
	}

	CONS_Debug(DBG_GAMELOGIC, "Highest laps found: %d\n", highest);

	return highest;
}

//
// P_TransferToNextMare
//
// Transfers the player to the next Mare.
// (Finds the lowest mare # for capsules that have not been destroyed).
// Returns true if successful, false if there is no other mare.
//
/*boolean P_TransferToNextMare(player_t *player)
{
	thinker_t *th;
	mobj_t *mo2;
	mobj_t *closestaxis = NULL;
	INT32 lowestaxisnum = -1;
	UINT8 mare = P_FindLowestMare();
	fixed_t dist1, dist2 = 0;

	if (mare == 255)
		return false;

	CONS_Debug(DBG_NIGHTS, "Mare is %d\n", mare);

	player->mare = mare;

	// scan the thinkers
	// to find the closest axis point
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		if (mo2->type == MT_AXIS)
		{
			if (mo2->threshold == mare)
			{
				if (closestaxis == NULL)
				{
					closestaxis = mo2;
					lowestaxisnum = mo2->health;
					dist2 = R_PointToDist2(player->mo->x, player->mo->y, mo2->x, mo2->y)-mo2->radius;
				}
				else if (mo2->health < lowestaxisnum)
				{
					dist1 = R_PointToDist2(player->mo->x, player->mo->y, mo2->x, mo2->y)-mo2->radius;

					if (dist1 < dist2)
					{
						closestaxis = mo2;
						lowestaxisnum = mo2->health;
						dist2 = dist1;
					}
				}
			}
		}
	}

	if (closestaxis == NULL)
		return false;

	P_SetTarget(&player->mo->target, closestaxis);
	return true;
}

//
// P_FindAxis
//
// Given a mare and axis number, returns
// the mobj for that axis point.
static mobj_t *P_FindAxis(INT32 mare, INT32 axisnum)
{
	thinker_t *th;
	mobj_t *mo2;

	// scan the thinkers
	// to find the closest axis point
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		// Axis things are only at beginning of list.
		if (!(mo2->flags2 & MF2_AXIS))
			return NULL;

		if (mo2->type == MT_AXIS)
		{
			if (mo2->health == axisnum && mo2->threshold == mare)
				return mo2;
		}
	}

	return NULL;
}

//
// P_FindAxisTransfer
//
// Given a mare and axis number, returns
// the mobj for that axis transfer point.
static mobj_t *P_FindAxisTransfer(INT32 mare, INT32 axisnum, mobjtype_t type)
{
	thinker_t *th;
	mobj_t *mo2;

	// scan the thinkers
	// to find the closest axis point
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		// Axis things are only at beginning of list.
		if (!(mo2->flags2 & MF2_AXIS))
			return NULL;

		if (mo2->type == type)
		{
			if (mo2->health == axisnum && mo2->threshold == mare)
				return mo2;
		}
	}

	return NULL;
}

//
// P_TransferToAxis
//
// Finds the CLOSEST axis with the number specified.
void P_TransferToAxis(player_t *player, INT32 axisnum)
{
	thinker_t *th;
	mobj_t *mo2;
	mobj_t *closestaxis;
	INT32 mare = player->mare;
	fixed_t dist1, dist2 = 0;

	CONS_Debug(DBG_NIGHTS, "Transferring to axis %d\nLeveltime: %u...\n", axisnum, leveltime);

	closestaxis = NULL;

	// scan the thinkers
	// to find the closest axis point
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		if (mo2->type == MT_AXIS)
		{
			if (mo2->health == axisnum && mo2->threshold == mare)
			{
				if (closestaxis == NULL)
				{
					closestaxis = mo2;
					dist2 = R_PointToDist2(player->mo->x, player->mo->y, mo2->x, mo2->y)-mo2->radius;
				}
				else
				{
					dist1 = R_PointToDist2(player->mo->x, player->mo->y, mo2->x, mo2->y)-mo2->radius;

					if (dist1 < dist2)
					{
						closestaxis = mo2;
						dist2 = dist1;
					}
				}
			}
		}
	}

	if (!closestaxis)
	{
		CONS_Debug(DBG_NIGHTS, "ERROR: Specified axis point to transfer to not found!\n%d\n", axisnum);
	}
	else
	{
		CONS_Debug(DBG_NIGHTS, "Transferred to axis %d, mare %d\n", closestaxis->health, closestaxis->threshold);
	}

	P_SetTarget(&player->mo->target, closestaxis);
}

//
// P_DeNightserizePlayer
//
// Whoops! Ran out of NiGHTS time!
//
static void P_DeNightserizePlayer(player_t *player)
{
	thinker_t *th;
	mobj_t *mo2;

	player->pflags &= ~PF_NIGHTSMODE;

	//if (player->mo->tracer)
		//P_RemoveMobj(player->mo->tracer);

	player->powers[pw_underwater] = 0;
	player->pflags &= ~(PF_USEDOWN|PF_JUMPDOWN|PF_ATTACKDOWN|PF_STARTDASH|PF_GLIDING|PF_JUMPED|PF_THOKKED|PF_SPINNING|PF_DRILLING|PF_TRANSFERTOCLOSEST);
	player->secondjump = 0;
	player->jumping = 0;
	player->homing = 0;
	player->climbing = 0;
	player->mo->fuse = 0;
	player->speed = 0;
	P_SetTarget(&player->mo->target, NULL);
	P_SetTarget(&player->axis1, P_SetTarget(&player->axis2, NULL));

	player->mo->flags &= ~MF_NOGRAVITY;

	player->mo->flags2 &= ~MF2_DONTDRAW;

	// Restore aiming angle
	if (player == &players[consoleplayer])
		localaiming[0] = 0;
	else if (player == &players[displayplayers[1]])
		localaiming[1] = 0;
	else if (player == &players[displayplayers[2]])
		localaiming[2] = 0;
	else if (player == &players[displayplayers[3]])
		localaiming[3] = 0;

	if (player->mo->tracer)
		P_RemoveMobj(player->mo->tracer);
	//P_SetPlayerMobjState(player->mo, S_PLAY_FALL1); // SRB2kart
	player->pflags |= PF_NIGHTSFALL;

	// If in a special stage, add some preliminary exit time.
	if (G_IsSpecialStage(gamemap))
	{
		INT32 i;
		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].pflags & PF_NIGHTSMODE)
				players[i].nightstime = 1; // force everyone else to fall too.
		player->exiting = raceexittime+2;
		stagefailed = true; // NIGHT OVER
	}

	// Check to see if the player should be killed.
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;
		if (!(mo2->type == MT_NIGHTSDRONE))
			continue;

		if (mo2->flags2 & MF2_AMBUSH)
			P_DamageMobj(player->mo, NULL, NULL, 10000);

		break;
	}

	// Restore from drowning music
	P_RestoreMusic(player);
}
//
// P_NightserizePlayer
//
// NiGHTS Time!
void P_NightserizePlayer(player_t *player, INT32 nighttime)
{
	INT32 oldmare;

	// Bots can't be super, silly!1 :P
	if (player->bot)
		return;

	if (!(player->pflags & PF_NIGHTSMODE))
	{
		P_SetTarget(&player->mo->tracer, P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_NIGHTSCHAR));
		player->mo->tracer->destscale = player->mo->scale;
		P_SetScale(player->mo->tracer, player->mo->scale);
		player->mo->tracer->eflags = (player->mo->tracer->eflags & ~MFE_VERTICALFLIP)|(player->mo->eflags & MFE_VERTICALFLIP);
		player->mo->height = player->mo->tracer->height;
	}

	player->pflags &= ~(PF_USEDOWN|PF_JUMPDOWN|PF_ATTACKDOWN|PF_STARTDASH|PF_GLIDING|PF_JUMPED|PF_THOKKED|PF_SPINNING|PF_DRILLING);
	player->homing = 0;
	player->mo->fuse = 0;
	player->speed = 0;
	player->climbing = 0;
	player->secondjump = 0;

	player->powers[pw_shield] = SH_NONE;

	player->mo->flags |= MF_NOGRAVITY;

	player->mo->flags2 |= MF2_DONTDRAW;

	player->nightstime = player->startedtime = nighttime*TICRATE;
	player->bonustime = false;

	P_RestoreMusic(player);
	P_SetMobjState(player->mo->tracer, S_SUPERTRANS1);

	if (G_RaceGametype())
	{
		if (player->drillmeter < 48*20)
			player->drillmeter = 48*20;
	}
	else
	{
		if (player->drillmeter < 40*20)
			player->drillmeter = 40*20;
	}

	oldmare = player->mare;

	if (P_TransferToNextMare(player) == false)
	{
		INT32 i;
		INT32 total_rings = 0;

		P_SetTarget(&player->mo->target, NULL);

		if (G_IsSpecialStage(gamemap))
		{
			for (i = 0; i < MAXPLAYERS; i++)
				if (playeringame[i])
					total_rings += players[i].health-1;
		}

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || !players[i].mo || players[i].spectator)
				continue;

			players[i].texttimer = (3 * TICRATE) - 10;
			players[i].textvar = 4; // Score and grades
			players[i].lastmare = players[i].mare;
			if (G_IsSpecialStage(gamemap))
			{
				players[i].finishedrings = (INT16)total_rings;
				P_AddPlayerScore(player, total_rings * 50);
			}
			else
			{
				players[i].finishedrings = (INT16)(players[i].health - 1);
				P_AddPlayerScore(&players[i], (players[i].health - 1) * 50);
			}

			// transfer scores anyway

			players[i].mo->health = players[i].health = 1;
			P_DoPlayerExit(&players[i]);
		}
	}
	else if (oldmare != player->mare)
	{
		/// \todo Handle multi-mare special stages.
		// Ring bonus
		P_AddPlayerScore(player, (player->health - 1) * 50);

		player->lastmare = (UINT8)oldmare;
		player->texttimer = 4*TICRATE;
		player->textvar = 4; // Score and grades
		player->finishedrings = (INT16)(player->health - 1);

		// Starting a new mare, transfer scores
		player->marebegunat = leveltime;

		player->mo->health = player->health = 1;
	}
	else
	{
		player->textvar = 5; // Nothing, just tells it to go to the GET n RINGS/SPHERES text in a bit
		player->texttimer = 40;

		// Don't show before title card
		// Not consistency safe, but this only affects drawing
		if (timeinmap + 40 < 110)
			player->texttimer = (UINT8)(110 - timeinmap);
	}

	player->pflags |= PF_NIGHTSMODE;
}*/

//
// P_PlayerInPain
//
// Is player in pain??
// Checks for painstate and pw_flashing, if both found return true
//
boolean P_PlayerInPain(player_t *player)
{
	// no silly, sliding isn't pain
	if (!(player->pflags & PF_SLIDING) && player->mo->state == &states[player->mo->info->painstate] && player->powers[pw_flashing])
		return true;

	return false;
}

//
// P_DoPlayerPain
//
// Player was hit,
// put them in pain.
//
void P_DoPlayerPain(player_t *player, mobj_t *source, mobj_t *inflictor)
{
	angle_t ang;
	fixed_t fallbackspeed;

	if (inflictor && (inflictor->type != MT_PLAYER && inflictor->type != MT_ORBINAUT && inflictor->type != MT_ORBINAUT_SHIELD
		&& inflictor->type != MT_JAWZ && inflictor->type != MT_JAWZ_DUD && inflictor->type != MT_JAWZ_SHIELD
		&& inflictor->type != MT_SMK_THWOMP))
	{
		if (player->mo->eflags & MFE_VERTICALFLIP)
			player->mo->z--;
		else
			player->mo->z++;

		if (player->mo->eflags & MFE_UNDERWATER)
			P_SetObjectMomZ(player->mo, FixedDiv(10511*FRACUNIT,2600*FRACUNIT), false);
		else
			P_SetObjectMomZ(player->mo, FixedDiv(69*FRACUNIT,10*FRACUNIT), false);
	}

	if (inflictor)
	{
		ang = R_PointToAngle2(inflictor->x, inflictor->y, player->mo->x, player->mo->y); // SRB2kart
		//ang = R_PointToAngle2(inflictor->x-inflictor->momx, inflictor->y - inflictor->momy, player->mo->x - player->mo->momx, player->mo->y - player->mo->momy);

		// explosion and rail rings send you farther back, making it more difficult
		// to recover
		if ((inflictor->flags2 & MF2_SCATTER) && source)
		{
			fixed_t dist = P_AproxDistance(P_AproxDistance(source->x-player->mo->x, source->y-player->mo->y), source->z-player->mo->z);

			dist = FixedMul(128*FRACUNIT, inflictor->scale) - dist/4;

			if (dist < FixedMul(4*FRACUNIT, inflictor->scale))
				dist = FixedMul(4*FRACUNIT, inflictor->scale);

			fallbackspeed = dist;
		}
		else if (inflictor->flags2 & MF2_EXPLOSION)
		{
			if (inflictor->flags2 & MF2_RAILRING)
				fallbackspeed = FixedMul(38*FRACUNIT, inflictor->scale); // 7x
			else
				fallbackspeed = FixedMul(30*FRACUNIT, inflictor->scale); // 5x
		}
		else if (inflictor->flags2 & MF2_RAILRING)
			fallbackspeed = FixedMul(45*FRACUNIT, inflictor->scale); // 4x
		else
			fallbackspeed = FixedMul(4*FRACUNIT, inflictor->scale); // the usual amount of force
	}
	else
	{
		ang = R_PointToAngle2(player->mo->momx, player->mo->momy, 0, 0);
		fallbackspeed = FixedMul(4*FRACUNIT, player->mo->scale);
	}

	P_InstaThrust(player->mo, ang, fallbackspeed);

	if (player->pflags & PF_ROPEHANG)
		P_SetTarget(&player->mo->tracer, NULL);

	// Point penalty for hitting a hazard during tag.
	// Discourages players from intentionally hurting themselves to avoid being tagged.
	/*if (gametype == GT_TAG && (!(player->pflags & PF_TAGGED) && !(player->pflags & PF_TAGIT)))
	{
		if (player->score >= 50)
			player->score -= 50;
		else
			player->score = 0;
	}*/

	P_ResetPlayer(player);
	P_SetPlayerMobjState(player->mo, player->mo->info->painstate);
	player->powers[pw_flashing] = K_GetKartFlashing(player);

	if (player->timeshit != UINT8_MAX)
		++player->timeshit;
}

//
// P_ResetPlayer
//
// Useful when you want to kill everything the player is doing.
void P_ResetPlayer(player_t *player)
{
	player->pflags &= ~(PF_ROPEHANG|PF_ITEMHANG|PF_MACESPIN|PF_SPINNING|PF_JUMPED|PF_GLIDING|PF_THOKKED|PF_CARRIED);
	player->jumping = 0;
	player->secondjump = 0;
	player->glidetime = 0;
	player->homing = 0;
	player->climbing = 0;
	player->powers[pw_tailsfly] = 0;
	player->onconveyor = 0;
	player->skidtime = 0;
	/*if (player-players == consoleplayer && botingame)
		CV_SetValue(&cv_analog2, true);*/
}

//
// P_GivePlayerRings
//
// Gives rings to the player, and does any special things required.
// Call this function when you want to increment the player's health.
//
void P_GivePlayerRings(player_t *player, INT32 num_rings)
{
	if (player->bot)
		player = &players[consoleplayer];

	if (!player->mo)
		return;

	player->mo->health += num_rings;
	player->health += num_rings;

	if (!G_IsSpecialStage(gamemap) || !useNightsSS)
		player->totalring += num_rings;

	//{ SRB2kart - rings don't really do anything, but we don't want the player spilling them later.
	/*
	// Can only get up to 9999 rings, sorry!
	if (player->mo->health > 10000)
	{
		player->mo->health = 10000;
		player->health = 10000;
	}
	else if (player->mo->health < 1)*/
	{
		player->mo->health = 1;
		player->health = 1;
	}
	//}

	// Now extra life bonuses are handled here instead of in P_MovePlayer, since why not?
	if (!ultimatemode && !modeattacking && !G_IsSpecialStage(gamemap) && G_GametypeUsesLives())
	{
		INT32 gainlives = 0;

		while (player->xtralife < maxXtraLife && player->health > 100 * (player->xtralife+1))
		{
			++gainlives;
			++player->xtralife;
		}

		if (gainlives)
		{
			P_GivePlayerLives(player, gainlives);
			P_PlayLivesJingle(player);
		}
	}
}

//
// P_GivePlayerLives
//
// Gives the player an extra life.
// Call this function when you want to add lives to the player.
//
void P_GivePlayerLives(player_t *player, INT32 numlives)
{
	player->lives += numlives;

	if (player->lives > 99)
		player->lives = 99;
	else if (player->lives < 1)
		player->lives = 1;
}

//
// P_DoSuperTransformation
//
// Transform into Super Sonic!
void P_DoSuperTransformation(player_t *player, boolean giverings)
{
	return; // SRB2kart - this is not a thing we need
	player->powers[pw_super] = 1;
	if (!(mapheaderinfo[gamemap-1]->levelflags & LF_NOSSMUSIC) && P_IsLocalPlayer(player))
	{
		S_StopMusic();
		S_ChangeMusicInternal("supers", true);
	}

	S_StartSound(NULL, sfx_supert); //let all players hear it -mattw_cfi

	// Transformation animation
	//P_SetPlayerMobjState(player->mo, S_PLAY_SUPERTRANS1);

	player->mo->momx = player->mo->momy = player->mo->momz = 0;

	if (giverings)
	{
		player->mo->health = 51;
		player->health = player->mo->health;
	}

	// Just in case.
	if (!(mapheaderinfo[gamemap-1]->levelflags & LF_NOSSMUSIC))
	{
		player->powers[pw_extralife] = 0;
		player->powers[pw_invulnerability] = 0;
		player->powers[pw_sneakers] = 0;
	}

	if (gametype != GT_COOP)
	{
		HU_SetCEchoFlags(0);
		HU_SetCEchoDuration(5);
		HU_DoCEcho(va("%s\\is now super.\\\\\\\\", player_names[player-players]));
	}

	P_PlayerFlagBurst(player, false);
}
// Adds to the player's score
void P_AddPlayerScore(player_t *player, UINT32 amount)
{
	//UINT32 oldscore;

	if (!(G_BattleGametype()))
		return;

	if (player->bot)
		player = &players[consoleplayer];

	if (player->exiting) // srb2kart
		return;

	//oldscore = player->score;

	// Don't go above MAXSCORE.
	if (player->marescore + amount < MAXSCORE)
		player->marescore += amount;
	else
		player->marescore = MAXSCORE;

	// check for extra lives every 50000 pts
	/*if (!ultimatemode && !modeattacking && player->score > oldscore && player->score % 50000 < amount && (gametype == GT_COMPETITION || gametype == GT_COOP))
	{
		P_GivePlayerLives(player, (player->score/50000) - (oldscore/50000));
		P_PlayLivesJingle(player);
	}*/

	// In team match, all awarded points are incremented to the team's running score.
	if (gametype == GT_TEAMMATCH)
	{
		if (player->ctfteam == 1)
			redscore += amount;
		else if (player->ctfteam == 2)
			bluescore += amount;
	}
}

//
// P_PlayLivesJingle
//
void P_PlayLivesJingle(player_t *player)
{
	if (player && !P_IsLocalPlayer(player))
		return;

	if (use1upSound)
		S_StartSound(NULL, sfx_oneup);
	else if (mariomode)
		S_StartSound(NULL, sfx_marioa);
	else
	{
		if (player)
			player->powers[pw_extralife] = extralifetics + 1;
		S_StopMusic(); // otherwise it won't restart if this is done twice in a row
		S_ChangeMusicInternal("xtlife", false);
	}
}

void P_PlayRinglossSound(mobj_t *source)
{
	sfxenum_t key = P_RandomKey(2);
	if (cv_kartvoices.value)
		S_StartSound(source, (mariomode) ? sfx_mario8 : sfx_khurt1 + key);
	else
		S_StartSound(source, sfx_slip);
}

void P_PlayDeathSound(mobj_t *source)
{
	S_StartSound(source, sfx_s3k35);
}

void P_PlayVictorySound(mobj_t *source)
{
	if (cv_kartvoices.value)
		S_StartSound(source, sfx_kwin);
}

// Can't rely on k_position and K_IsPlayerLosing in battle smh
static UINT8 getPlayerPos(player_t *player)
{
	if (G_BattleGametype())
	{
		UINT8 pos = 1;

		for (int i = 0; i < MAXPLAYERS; ++i) {
			if (!playeringame[i] || players[i].spectator) continue;
			if (players[i].marescore > player->marescore) ++pos;
		}

		return pos;
	}

	return player->kartstuff[k_position];
}

static boolean isPlayerLosing(player_t *player)
{
	if (G_BattleGametype()) {
		UINT8 pos = 1;
		UINT8 maxpos = 1;

		for (int i = 0; i < MAXPLAYERS; ++i) {
			if (!playeringame[i] || players[i].spectator) continue;
			if (players[i].marescore > player->marescore) ++pos;
			maxpos = max(getPlayerPos(&players[i]), maxpos);
		}

		if (maxpos == 1) return false;

		if (maxpos % 2) ++maxpos;

		return pos > (maxpos / 2);
	}

	return K_IsPlayerLosing(player);
}

//
// P_EndingMusic
//
// Consistently sets ending music!
//
boolean P_EndingMusic(player_t *player)
{
	char buffer[9];
	boolean looping = true;
	INT32 bestlocalpos;
	player_t *bestlocalplayer;

	if (!P_IsLocalPlayer(player)) // Only applies to a local player
		return false;

	if (multiplayer && demo.playback) // Don't play this in multiplayer replays
		return false;

	// Event - Level Finish
	// Check for if this is valid or not
	if (splitscreen)
	{
		if (!((players[displayplayers[0]].exiting || (players[displayplayers[0]].pflags & PF_TIMEOVER))
			|| (players[displayplayers[1]].exiting || (players[displayplayers[1]].pflags & PF_TIMEOVER))
			|| ((splitscreen < 2) && (players[displayplayers[2]].exiting || (players[displayplayers[2]].pflags & PF_TIMEOVER)))
			|| ((splitscreen < 3) && (players[displayplayers[3]].exiting || (players[displayplayers[3]].pflags & PF_TIMEOVER)))))
			return false;

		bestlocalplayer = &players[displayplayers[0]];
		bestlocalpos = ((players[displayplayers[0]].pflags & PF_TIMEOVER) ? MAXPLAYERS+1 : getPlayerPos(&players[displayplayers[0]]));
#define setbests(p) \
	if (((players[p].pflags & PF_TIMEOVER) ? MAXPLAYERS+1 : getPlayerPos(&players[p])) < bestlocalpos) \
	{ \
		bestlocalplayer = &players[p]; \
		bestlocalpos = ((players[p].pflags & PF_TIMEOVER) ? MAXPLAYERS+1 : getPlayerPos(&players[p])); \
	}
		setbests(displayplayers[1]);
		if (splitscreen > 1)
			setbests(displayplayers[2]);
		if (splitscreen > 2)
			setbests(displayplayers[3]);
#undef setbests
	}
	else
	{
		if (!(player->exiting || (player->pflags & PF_TIMEOVER)))
			return false;

		bestlocalplayer = player;
		bestlocalpos = ((player->pflags & PF_TIMEOVER) ? MAXPLAYERS+1 : getPlayerPos(player));
	}

	if (G_RaceGametype() && bestlocalpos == MAXPLAYERS+1)
		sprintf(buffer, "k*fail"); // F-Zero death results theme
	else
	{
		if (bestlocalpos == 1)
			sprintf(buffer, "k*win");
		else if (isPlayerLosing(bestlocalplayer))
			sprintf(buffer, "k*lose");
		else
			sprintf(buffer, "k*ok");
	}

	S_SpeedMusic(1.0f);

	if (G_RaceGametype())
		buffer[1] = 'r';
	else if (G_BattleGametype())
	{
		buffer[1] = 'b';
		looping = false;
	}

	S_ChangeMusicInternal(buffer, looping);

	return true;
}

//
// P_RestoreMusic
//
// Restores music after some special music change
//
void P_RestoreMusic(player_t *player)
{
	UINT32 position;

	if (!P_IsLocalPlayer(player)) // Only applies to a local player
		return;

	S_SpeedMusic(1.0f);

	// Event - HERE COMES A NEW CHALLENGER
	if (mapreset)
	{
		S_ChangeMusicInternal("chalng", false); //S_StopMusic();
		return;
	}

	// Event - Level Ending
	if (P_EndingMusic(player))
		return;

	// Event - Level Start
	if (leveltime < (starttime + (TICRATE/2)))
		S_ChangeMusicInternal((encoremode ? "estart" : "kstart"), false); //S_StopMusic();
	else // see also where time overs are handled - search for "lives = 2" in this file
	{
		INT32 wantedmus = 0; // 0 is level music, 1 is invincibility, 2 is grow

		if (splitscreen)
		{
			INT32 bestlocaltimer = 1;

#define setbests(p) \
	if (players[p].playerstate == PST_LIVE) \
	{ \
		if (players[p].kartstuff[k_growshrinktimer] > bestlocaltimer) \
		{ wantedmus = 2; bestlocaltimer = players[p].kartstuff[k_growshrinktimer]; } \
		else if (players[p].kartstuff[k_invincibilitytimer] > bestlocaltimer) \
		{ wantedmus = 1; bestlocaltimer = players[p].kartstuff[k_invincibilitytimer]; } \
	}
			setbests(displayplayers[0]);
			setbests(displayplayers[1]);
			if (splitscreen > 1)
				setbests(displayplayers[2]);
			if (splitscreen > 2)
				setbests(displayplayers[3]);
#undef setbests
		}
		else
		{
			if (player->playerstate == PST_LIVE)
			{
				if (player->kartstuff[k_growshrinktimer] > 1)
					wantedmus = 2;
				else if (player->kartstuff[k_invincibilitytimer] > 1)
					wantedmus = 1;
			}
		}

		// Item - Grow
		if (wantedmus == 2 && cv_growmusic.value)
		{
			S_ChangeMusicInternal("kgrow", true);
			
			if (cv_birdmusic.value)
				S_SetRestoreMusicFadeInCvar(&cv_growmusicfade);
		}
		// Item - Invincibility
		else if (wantedmus == 1 && cv_supermusic.value)
		{
			S_ChangeMusicInternal("kinvnc", true);
			
			if (cv_birdmusic.value)
				S_SetRestoreMusicFadeInCvar(&cv_invincmusicfade);
		}
		else
		{
#if 0
			// Event - Final Lap
			// Still works for GME, but disabled for consistency
			if (G_RaceGametype() && player->laps >= (UINT8)(cv_numlaps.value - 1))
				S_SpeedMusic(1.2f);
#endif
			if (cv_birdmusic.value)
			{
				if (mapmusresume && cv_resume.value)
					position = mapmusresume;
				else
					position = mapmusposition;

				S_ChangeMusicEx(mapmusname, mapmusflags, true, position, 0,
						S_GetRestoreMusicFadeIn());
				S_ClearRestoreMusicFadeInCvar();
			}
			else
				S_ChangeMusicEx(mapmusname, mapmusflags, true, mapmusposition, 0, 0);
			
			mapmusresume = 0;
		}
	}
}

//
// P_IsObjectInGoop
//
// Returns true if the object is inside goop water.
// (Spectators and objects otherwise without gravity cannot have goop gravity!)
//
boolean P_IsObjectInGoop(mobj_t *mo)
{
	if (mo->player && mo->player->spectator)
		return false;

	if (mo->flags & MF_NOGRAVITY)
		return false;

	return ((mo->eflags & (MFE_UNDERWATER|MFE_GOOWATER)) == (MFE_UNDERWATER|MFE_GOOWATER));
}

//
// P_IsObjectOnGround
//
// Returns true if the player is
// on the ground. Takes reverse
// gravity and FOFs into account.
//
boolean P_IsObjectOnGround(mobj_t *mo)
{
	if (P_IsObjectInGoop(mo))
	{
/*
		// It's a crazy hack that checking if you're on the ground
		// would actually CHANGE your position and momentum,
		if (mo->z < mo->floorz)
		{
			mo->z = mo->floorz;
			mo->momz = 0;
		}
		else if (mo->z + mo->height > mo->ceilingz)
		{
			mo->z = mo->ceilingz - mo->height;
			mo->momz = 0;
		}
*/
		// but I don't want you to ever 'stand' while submerged in goo.
		// You're in constant vertical momentum, even if you get stuck on something.
		// No exceptions.
		return false;
	}

	if (mo->eflags & MFE_VERTICALFLIP)
	{
		if (mo->z+mo->height >= mo->ceilingz)
			return true;
	}
	else
	{
		if (mo->z <= mo->floorz)
			return true;
	}

	return false;
}

//
// P_IsObjectOnGroundIn
//
// Returns true if the player is
// on the ground in a specific sector. Takes reverse
// gravity and FOFs into account.
//
boolean P_IsObjectOnGroundIn(mobj_t *mo, sector_t *sec)
{
	ffloor_t *rover;

	// Is the object in reverse gravity?
	if (mo->eflags & MFE_VERTICALFLIP)
	{
		// Detect if the player is on the ceiling.
		if (mo->z+mo->height >= P_GetSpecialTopZ(mo, sec, sec))
			return true;
		// Otherwise, detect if the player is on the bottom of a FOF.
		else
		{
			for (rover = sec->ffloors; rover; rover = rover->next)
			{
				// If the FOF doesn't exist, continue.
				if (!(rover->flags & FF_EXISTS))
					continue;

				// If the FOF is configured to let the object through, continue.
				if (!((rover->flags & FF_BLOCKPLAYER && mo->player)
					|| (rover->flags & FF_BLOCKOTHERS && !mo->player)))
					continue;

				// If the the platform is intangible from below, continue.
				if (rover->flags & FF_PLATFORM)
					continue;

				// If the FOF is a water block, continue. (Unnecessary check?)
				if (rover->flags & FF_SWIMMABLE)
					continue;

				// Actually check if the player is on the suitable FOF.
				if (mo->z+mo->height == P_GetSpecialBottomZ(mo, sectors + rover->secnum, sec))
					return true;
			}
		}
	}
	// Nope!
	else
	{
		// Detect if the player is on the floor.
		if (mo->z <= P_GetSpecialBottomZ(mo, sec, sec))
			return true;
		// Otherwise, detect if the player is on the top of a FOF.
		else
		{
			for (rover = sec->ffloors; rover; rover = rover->next)
			{
				// If the FOF doesn't exist, continue.
				if (!(rover->flags & FF_EXISTS))
					continue;

				// If the FOF is configured to let the object through, continue.
				if (!((rover->flags & FF_BLOCKPLAYER && mo->player)
					|| (rover->flags & FF_BLOCKOTHERS && !mo->player)))
					continue;

				// If the the platform is intangible from above, continue.
				if (rover->flags & FF_REVERSEPLATFORM)
					continue;

				// If the FOF is a water block, continue. (Unnecessary check?)
				if (rover->flags & FF_SWIMMABLE)
					continue;

				// Actually check if the player is on the suitable FOF.
				if (mo->z == P_GetSpecialTopZ(mo, sectors + rover->secnum, sec))
					return true;
			}
		}
	}

	return false;
}

//
// P_IsObjectOnRealGround
//
// Helper function for T_EachTimeThinker
// Like P_IsObjectOnGroundIn, except ONLY THE REAL GROUND IS CONSIDERED, NOT FOFS
// I'll consider whether to make this a more globally accessible function or whatever in future
// -- Monster Iestyn
//
// Really simple, but personally I think it's also incredibly helpful. I think this is fine in p_user.c
// -- Sal

boolean P_IsObjectOnRealGround(mobj_t *mo, sector_t *sec)
{
	// Is the object in reverse gravity?
	if (mo->eflags & MFE_VERTICALFLIP)
	{
		// Detect if the player is on the ceiling.
		if (mo->z+mo->height >= P_GetSpecialTopZ(mo, sec, sec))
			return true;
	}
	// Nope!
	else
	{
		// Detect if the player is on the floor.
		if (mo->z <= P_GetSpecialBottomZ(mo, sec, sec))
			return true;
	}
	return false;
}

//
// P_SetObjectMomZ
//
// Sets the player momz appropriately.
// Takes reverse gravity into account.
//
void P_SetObjectMomZ(mobj_t *mo, fixed_t value, boolean relative)
{
	if (mo->eflags & MFE_VERTICALFLIP)
		value = -value;

	if (mo->scale != FRACUNIT)
		value = FixedMul(value, mo->scale);

	if (relative)
		mo->momz += value;
	else
		mo->momz = value;
}

//
// P_GetPlayerHeight
//
// Returns the height
// of the player.
//
fixed_t P_GetPlayerHeight(player_t *player)
{
	return FixedMul(player->mo->info->height, player->mo->scale);
}

//
// P_GetPlayerSpinHeight
//
// Returns the 'spin height'
// of the player.
//
fixed_t P_GetPlayerSpinHeight(player_t *player)
{
	return FixedMul(FixedMul(player->mo->info->height, player->mo->scale),2*FRACUNIT/3);
}

//
// P_IsLocalPlayer
//
// Returns true if player is
// on the local machine.
//
boolean P_IsLocalPlayer(player_t *player)
{
	UINT8 i;

	if (player == &players[consoleplayer])
		return true;
	else if (splitscreen)
	{
		for (i = 1; i <= splitscreen; i++) // Skip P1
		{
			if (player == &players[displayplayers[i]])
				return true;
		}
	}

	return false;
}

//
// P_IsDisplayPlayer
//
// Returns true if player is
// currently being watched.
//
boolean P_IsDisplayPlayer(player_t *player)
{
	UINT8 i;

	for (i = 0; i <= splitscreen; i++) // DON'T skip P1
	{
		if (player == &players[displayplayers[i]])
			return true;
	}

	return false;
}

//
// P_SpawnShieldOrb
//
// Spawns the shield orb on the player
// depending on which shield they are
// supposed to have.
//
void P_SpawnShieldOrb(player_t *player)
{
	mobjtype_t orbtype;
	thinker_t *th;
	mobj_t *shieldobj, *ov;

#ifdef PARANOIA
	if (!player->mo)
		I_Error("P_SpawnShieldOrb: player->mo is NULL!\n");
#endif

	if (player->powers[pw_shield] & SH_FORCE)
		orbtype = MT_BLUEORB;
	else switch (player->powers[pw_shield] & SH_NOSTACK)
	{
	case SH_JUMP:
		orbtype = MT_WHITEORB;
		break;
	case SH_ATTRACT:
		orbtype = MT_YELLOWORB;
		break;
	case SH_ELEMENTAL:
		orbtype = MT_GREENORB;
		break;
	case SH_BOMB:
		orbtype = MT_BLACKORB;
		break;
	case SH_PITY:
		orbtype = MT_PITYORB;
		break;
	default:
		return;
	}

	// blaze through the thinkers to see if an orb already exists!
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		shieldobj = (mobj_t *)th;

		if (shieldobj->type == orbtype && shieldobj->target == player->mo)
			P_RemoveMobj(shieldobj); //kill the old one(s)
	}

	shieldobj = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, orbtype);
	P_SetTarget(&shieldobj->target, player->mo);
	shieldobj->color = (UINT8)shieldobj->info->painchance;

	if (shieldobj->info->seestate)
	{
		ov = P_SpawnMobj(shieldobj->x, shieldobj->y, shieldobj->z, MT_OVERLAY);
		P_SetTarget(&ov->target, shieldobj);
		P_SetMobjState(ov, shieldobj->info->seestate);
	}
	if (shieldobj->info->meleestate)
	{
		ov = P_SpawnMobj(shieldobj->x, shieldobj->y, shieldobj->z, MT_OVERLAY);
		P_SetTarget(&ov->target, shieldobj);
		P_SetMobjState(ov, shieldobj->info->meleestate);
	}
	if (shieldobj->info->missilestate)
	{
		ov = P_SpawnMobj(shieldobj->x, shieldobj->y, shieldobj->z, MT_OVERLAY);
		P_SetTarget(&ov->target, shieldobj);
		P_SetMobjState(ov, shieldobj->info->missilestate);
	}
	if (player->powers[pw_shield] & SH_FORCE)
	{
		//Copy and pasted from P_ShieldLook in p_mobj.c
		shieldobj->movecount = (player->powers[pw_shield] & 0xFF);
		if (shieldobj->movecount < 1)
		{
			if (shieldobj->info->painstate)
				P_SetMobjState(shieldobj,shieldobj->info->painstate);
			else
				shieldobj->flags2 |= MF2_SHADOW;
		}
	}
}

//
// P_SpawnGhostMobj
//
// Spawns a ghost object on the player
//
mobj_t *P_SpawnGhostMobj(mobj_t *mobj)
{
	mobj_t *ghost;

	ghost = P_SpawnMobj(mobj->x, mobj->y, mobj->z, MT_GHOST);

	P_SetScale(ghost, mobj->scale);
	ghost->destscale = mobj->scale;

	if (mobj->eflags & MFE_VERTICALFLIP)
	{
		ghost->eflags |= MFE_VERTICALFLIP;
		ghost->z += mobj->height - ghost->height;
	}

	ghost->color = mobj->color;
	ghost->colorized = mobj->colorized; // Kart: they should also be colorized if their origin is

	if (mobj->player)
		ghost->angle = mobj->player->frameangle;
	else
		ghost->angle = mobj->angle;

	ghost->pitch = mobj->pitch;
	ghost->roll = mobj->roll;

	ghost->sprite = mobj->sprite;
	ghost->frame = mobj->frame;
	ghost->tics = -1;
	ghost->frame &= ~FF_TRANSMASK;
	ghost->frame |= tr_trans50<<FF_TRANSSHIFT;
	ghost->slopepitch = mobj->slopepitch; 
	ghost->sloperoll = mobj->sloperoll;
	
	ghost->fuse = ghost->info->damage;
	ghost->skin = mobj->skin;
	ghost->localskin = mobj->localskin;
	ghost->skinlocal = mobj->skinlocal;
	ghost->spritexscale = mobj->spritexscale;
	ghost->spriteyscale = mobj->spriteyscale;
	ghost->spritexoffset = mobj->spritexoffset;
	ghost->spriteyoffset = mobj->spriteyoffset;

	if (mobj->flags2 & MF2_OBJECTFLIP)
		ghost->flags |= MF2_OBJECTFLIP;

	if (!(mobj->flags & MF_DONTENCOREMAP))
		mobj->flags &= ~MF_DONTENCOREMAP;

	// Copy interpolation data :)
	ghost->old_x = mobj->old_x2;
	ghost->old_y = mobj->old_y2;
	ghost->old_z = mobj->old_z2;
	ghost->old_angle = (mobj->player ? mobj->player->old_frameangle2 : mobj->old_angle2);
	ghost->old_pitch = mobj->old_pitch2;
	ghost->old_roll = mobj->old_roll2;
	ghost->old_sloperoll = mobj->old_sloperoll2;
	ghost->old_slopepitch = mobj->old_slopepitch2;

	return ghost;
}

//
// P_DoPlayerExit
//
// Player exits the map via sector trigger
void P_DoPlayerExit(player_t *player)
{
	if (player->exiting || mapreset)
		return;

	if (P_IsLocalPlayer(player) && (!player->spectator && !demo.playback))
		legitimateexit = true;

	if (player->griefstrikes > 0)
		player->griefstrikes--; // Remove a strike for finishing a race normally

	if (G_RaceGametype()) // If in Race Mode, allow
	{
		player->exiting = raceexittime+2;
		K_KartUpdatePosition(player);

		if (cv_kartvoices.value)
		{
			if (P_IsLocalPlayer(player))
			{
				sfxenum_t sfx_id;
				// fix godjjsa win sounds
				if (K_IsPlayerLosing(player)) {
					if (player->mo->localskin)
						sfx_id = ((skin_t *)player->mo->localskin)->soundsid[S_sfx[sfx_klose].skinsound];
					else
						sfx_id = ((skin_t *)player->mo->skin)->soundsid[S_sfx[sfx_klose].skinsound];
				}
				else {
					if (player->mo->localskin)
						sfx_id = ((skin_t *)player->mo->localskin)->soundsid[S_sfx[sfx_kwin].skinsound];
					else
						sfx_id = ((skin_t *)player->mo->skin)->soundsid[S_sfx[sfx_kwin].skinsound];
				}
				S_StartSound(NULL, sfx_id);
			}
			else
			{
				if (K_IsPlayerLosing(player))
					S_StartSound(player->mo, sfx_klose);
				else
					S_StartSound(player->mo, sfx_kwin);
			}
		}

		if (cv_inttime.value > 0)
			P_EndingMusic(player);

		// SRB2kart 120217
		//if (!exitcountdown)
			//exitcountdown = racecountdown + 8*TICRATE;

		if (P_CheckRacers())
			player->exiting = raceexittime+1;
	}
	else if (G_BattleGametype()) // Battle Mode exiting
	{
		player->exiting = battleexittime+1;
		P_EndingMusic(player);
	}
	else
		player->exiting = raceexittime+2; // Accidental death safeguard???

	player->powers[pw_underwater] = 0;
	player->powers[pw_spacetime] = 0;
	player->kartstuff[k_cardanimation] = 0; // srb2kart: reset battle animation

	if (player == &players[consoleplayer])
		demo.savebutton = leveltime;

	/*if (playeringame[player-players] && netgame && !circuitmap)
		CONS_Printf(M_GetText("%s has completed the level.\n"), player_names[player-players]);*/
}

#define SPACESPECIAL 12
boolean P_InSpaceSector(mobj_t *mo) // Returns true if you are in space
{
	sector_t *sector = mo->subsector->sector;
	fixed_t topheight, bottomheight;

	if (GETSECSPECIAL(sector->special, 1) == SPACESPECIAL)
		return true;

	if (sector->ffloors)
	{
		ffloor_t *rover;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS))
				continue;

			if (GETSECSPECIAL(rover->master->frontsector->special, 1) != SPACESPECIAL)
				continue;
			topheight = *rover->t_slope ? P_GetZAt(*rover->t_slope, mo->x, mo->y) : *rover->topheight;
			bottomheight = *rover->b_slope ? P_GetZAt(*rover->b_slope, mo->x, mo->y) : *rover->bottomheight;

			if (mo->z + (mo->height/2) > topheight)
				continue;

			if (mo->z + (mo->height/2) < bottomheight)
				continue;

			return true;
		}
	}

	return false; // No vacuum here, Captain!
}

boolean P_InQuicksand(mobj_t *mo) // Returns true if you are in quicksand
{
	sector_t *sector = mo->subsector->sector;
	fixed_t topheight, bottomheight;

	fixed_t flipoffset = ((mo->eflags & MFE_VERTICALFLIP) ? (mo->height/2) : 0);

	if (sector->ffloors)
	{
		ffloor_t *rover;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS))
				continue;

			if (!(rover->flags & FF_QUICKSAND))
				continue;

			topheight = *rover->t_slope ? P_GetZAt(*rover->t_slope, mo->x, mo->y) : *rover->topheight;
			bottomheight = *rover->b_slope ? P_GetZAt(*rover->b_slope, mo->x, mo->y) : *rover->bottomheight;

			if (mo->z + flipoffset > topheight)
				continue;

			if (mo->z + (mo->height/2) + flipoffset < bottomheight)
				continue;

			return true;
		}
	}

	return false; // No sand here, Captain!
}

static void P_CheckBustableBlocks(player_t *player)
{
	msecnode_t *node;
	fixed_t oldx;
	fixed_t oldy;

	if ((netgame || multiplayer) && player->spectator)
		return;

	oldx = player->mo->x;
	oldy = player->mo->y;

	P_UnsetThingPosition(player->mo);
	player->mo->x += player->mo->momx;
	player->mo->y += player->mo->momy;
	P_SetThingPosition(player->mo);

	for (node = player->mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		if (!node->m_sector)
			break;

		if (node->m_sector->ffloors)
		{
			ffloor_t *rover;
			fixed_t topheight, bottomheight;

			for (rover = node->m_sector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS)) continue;

				if ((rover->flags & FF_BUSTUP)/* && !rover->master->frontsector->crumblestate*/)
				{
					// If it's an FF_SPINBUST, you have to either be jumping, or coming down
					// onto the top from a spin.
					if (rover->flags & FF_SPINBUST && ((!(player->pflags & PF_JUMPED) && !(player->pflags & PF_SPINNING)) || (player->pflags & PF_STARTDASH)))
						continue;

					// if it's not an FF_SHATTER, you must be spinning (and not jumping)
					// or have Knuckles's abilities (or Super Sonic)
					// ...or are drilling in NiGHTS (or Metal Sonic)
					if (!(rover->flags & FF_SHATTER) && !(rover->flags & FF_SPINBUST)
						&& !((player->pflags & PF_SPINNING) && !(player->pflags & PF_JUMPED))
						&& (!player->powers[pw_super])
						&& !(player->pflags & PF_DRILLING) && !metalrecording)
						continue;

					topheight = P_GetFOFTopZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);
					bottomheight = P_GetFOFBottomZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);

					// Height checks
					if (rover->flags & FF_SHATTERBOTTOM)
					{
						if (player->mo->z+player->mo->momz + player->mo->height < bottomheight)
							continue;

						if (player->mo->z+player->mo->height > bottomheight)
							continue;
					}
					else if (rover->flags & FF_SPINBUST)
					{
						if (player->mo->z+player->mo->momz > topheight)
							continue;

						if (player->mo->z + player->mo->height < bottomheight)
							continue;
					}
					else if (rover->flags & FF_SHATTER)
					{
						if (player->mo->z + player->mo->momz > topheight)
							continue;

						if (player->mo->z+player->mo->momz + player->mo->height < bottomheight)
							continue;
					}
					else
					{
						if (player->mo->z >= topheight)
							continue;

						if (player->mo->z + player->mo->height < bottomheight)
							continue;
					}

					// Impede the player's fall a bit
					if (((rover->flags & FF_SPINBUST) || (rover->flags & FF_SHATTER)) && player->mo->z >= topheight)
						player->mo->momz >>= 1;
					else if (rover->flags & FF_SHATTER)
					{
						player->mo->momx >>= 1;
						player->mo->momy >>= 1;
					}

					//if (metalrecording)
					//	G_RecordBustup(rover);

					EV_CrumbleChain(node->m_sector, rover);

					// Run a linedef executor??
					if (rover->master->flags & ML_EFFECT5)
						P_LinedefExecute((INT16)(P_AproxDistance(rover->master->dx, rover->master->dy)>>FRACBITS), player->mo, node->m_sector);

					goto bustupdone;
				}
			}
		}
	}
bustupdone:
	P_UnsetThingPosition(player->mo);
	player->mo->x = oldx;
	player->mo->y = oldy;
	P_SetThingPosition(player->mo);
}

static void P_CheckBouncySectors(player_t *player)
{
	msecnode_t *node;
	fixed_t oldx;
	fixed_t oldy;
	fixed_t oldz;
	vector3_t momentum;

	oldx = player->mo->x;
	oldy = player->mo->y;
	oldz = player->mo->z;

	P_UnsetThingPosition(player->mo);
	player->mo->x += player->mo->momx;
	player->mo->y += player->mo->momy;
	player->mo->z += player->mo->momz;
	P_SetThingPosition(player->mo);

	for (node = player->mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		if (!node->m_sector)
			break;

		if (node->m_sector->ffloors)
		{
			ffloor_t *rover;
			boolean top = true;
			fixed_t topheight, bottomheight;

			for (rover = node->m_sector->ffloors; rover; rover = rover->next)
			{
				if (!(rover->flags & FF_EXISTS))
					continue; // FOFs should not be bouncy if they don't even "exist"

				if (GETSECSPECIAL(rover->master->frontsector->special, 1) != 15)
					continue; // this sector type is required for FOFs to be bouncy

				topheight = P_GetFOFTopZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);
				bottomheight = P_GetFOFBottomZ(player->mo, node->m_sector, rover, player->mo->x, player->mo->y, NULL);

				if (player->mo->z > topheight)
					continue;

				if (player->mo->z + player->mo->height < bottomheight)
					continue;

				if (oldz < P_GetFOFTopZ(player->mo, node->m_sector, rover, oldx, oldy, NULL)
						&& oldz + player->mo->height > P_GetFOFBottomZ(player->mo, node->m_sector, rover, oldx, oldy, NULL))
					top = false;

				{
					fixed_t linedist;

					linedist = P_AproxDistance(rover->master->v1->x-rover->master->v2->x, rover->master->v1->y-rover->master->v2->y);

					linedist = FixedDiv(linedist,100*FRACUNIT);

					if (top)
					{
						fixed_t newmom;

						pslope_t *slope;
						if (abs(oldz - topheight) < abs(oldz + player->mo->height - bottomheight)) { // Hit top
							slope = *rover->t_slope;
						} else { // Hit bottom
							slope = *rover->b_slope;
						}

						momentum.x = player->mo->momx;
						momentum.y = player->mo->momy;
						momentum.z = player->mo->momz*2;

						if (slope)
							P_ReverseQuantizeMomentumToSlope(&momentum, slope);

						newmom = momentum.z = -FixedMul(momentum.z,linedist)/2;

						if (abs(newmom) < (linedist*2))
						{
							goto bouncydone;
						}

						if (!(rover->master->flags & ML_BOUNCY))
						{
							if (newmom > 0)
							{
								if (newmom < 8*FRACUNIT)
									newmom = 8*FRACUNIT;
							}
							else if (newmom > -8*FRACUNIT && newmom != 0)
								newmom = -8*FRACUNIT;
						}

						if (newmom > P_GetPlayerHeight(player)/2)
							newmom = P_GetPlayerHeight(player)/2;
						else if (newmom < -P_GetPlayerHeight(player)/2)
							newmom = -P_GetPlayerHeight(player)/2;

						momentum.z = newmom*2;

						if (slope)
							P_QuantizeMomentumToSlope(&momentum, slope);

						player->mo->momx = momentum.x;
						player->mo->momy = momentum.y;
						player->mo->momz = momentum.z/2;

						if (player->pflags & PF_SPINNING)
						{
							player->pflags &= ~PF_SPINNING;
							player->pflags |= PF_JUMPED;
							player->pflags |= PF_THOKKED;
						}
					}
					else
					{
						player->mo->momx = -FixedMul(player->mo->momx,linedist);
						player->mo->momy = -FixedMul(player->mo->momy,linedist);

						if (player->pflags & PF_SPINNING)
						{
							player->pflags &= ~PF_SPINNING;
							player->pflags |= PF_JUMPED;
							player->pflags |= PF_THOKKED;
						}
					}

					if ((player->pflags & PF_SPINNING) && player->speed < FixedMul(1<<FRACBITS, player->mo->scale) && player->mo->momz)
					{
						player->pflags &= ~PF_SPINNING;
						player->pflags |= PF_JUMPED;
					}

					goto bouncydone;
				}
			}
		}
	}
bouncydone:
	P_UnsetThingPosition(player->mo);
	player->mo->x = oldx;
	player->mo->y = oldy;
	player->mo->z = oldz;
	P_SetThingPosition(player->mo);
}

static void P_CheckQuicksand(player_t *player)
{
	ffloor_t *rover;
	fixed_t sinkspeed, friction;
	fixed_t topheight, bottomheight;

	if (!(player->mo->subsector->sector->ffloors && player->mo->momz <= 0))
		return;

	for (rover = player->mo->subsector->sector->ffloors; rover; rover = rover->next)
	{
		if (!(rover->flags & FF_EXISTS)) continue;

		if (!(rover->flags & FF_QUICKSAND))
			continue;

		topheight = *rover->t_slope ? P_GetZAt(*rover->t_slope, player->mo->x, player->mo->y) : *rover->topheight;
		bottomheight = *rover->b_slope ? P_GetZAt(*rover->b_slope, player->mo->x, player->mo->y) : *rover->bottomheight;

		if (topheight >= player->mo->z && bottomheight < player->mo->z + player->mo->height)
		{
			sinkspeed = abs(rover->master->v1->x - rover->master->v2->x)>>1;

			sinkspeed = FixedDiv(sinkspeed,TICRATE*FRACUNIT);

			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				fixed_t ceilingheight = P_GetCeilingZ(player->mo, player->mo->subsector->sector, player->mo->x, player->mo->y, NULL);

				player->mo->z += sinkspeed;

				if (player->mo->z + player->mo->height >= ceilingheight)
					player->mo->z = ceilingheight - player->mo->height;
			}
			else
			{
				fixed_t floorheight = P_GetFloorZ(player->mo, player->mo->subsector->sector, player->mo->x, player->mo->y, NULL);

				player->mo->z -= sinkspeed;

				if (player->mo->z <= floorheight)
					player->mo->z = floorheight;
			}

			friction = abs(rover->master->v1->y - rover->master->v2->y)>>6;

			player->mo->momx = FixedMul(player->mo->momx, friction);
			player->mo->momy = FixedMul(player->mo->momy, friction);
		}
	}
}

//
// P_CheckInvincibilityTimer
//
// Restores music from invincibility, and handles invincibility sparkles
//
static void P_CheckInvincibilityTimer(player_t *player)
{
	if (!player->powers[pw_invulnerability] && !player->kartstuff[k_invincibilitytimer])
		return;

	player->mo->color = (UINT8)(1 + (leveltime % (MAXSKINCOLORS-1)));

	// Resume normal music stuff.
	if (player->powers[pw_invulnerability] == 1 || player->kartstuff[k_invincibilitytimer] == 1)
	{
		if (!player->powers[pw_super])
		{
			{
				player->mo->color = player->skincolor;
				G_GhostAddColor((INT32) (player - players), GHC_NORMAL);
			}

			// If you had a shield, restore its visual significance
			P_SpawnShieldOrb(player);
		}

		P_RestoreMusic(player);
	}
}


//
// P_DoBubbleBreath
//
// Handles bubbles spawned by the player
//
static void P_DoBubbleBreath(player_t *player)
{
	fixed_t zh;
	mobj_t *bubble = NULL;

	if (player->mo->eflags & MFE_VERTICALFLIP)
		zh = player->mo->z + player->mo->height - FixedDiv(player->mo->height,5*(FRACUNIT/4));
	else
		zh = player->mo->z + FixedDiv(player->mo->height,5*(FRACUNIT/4));

	if (!(player->mo->eflags & MFE_UNDERWATER) || ((player->powers[pw_shield] & SH_NOSTACK) == SH_ELEMENTAL && !(player->pflags & PF_NIGHTSMODE)) || player->spectator)
		return;

	if (P_RandomChance(FRACUNIT/16))
		bubble = P_SpawnMobj(player->mo->x, player->mo->y, zh, MT_SMALLBUBBLE);
	else if (P_RandomChance(3*FRACUNIT/256))
		bubble = P_SpawnMobj(player->mo->x, player->mo->y, zh, MT_MEDIUMBUBBLE);
	if (bubble)
	{
		bubble->threshold = 42;
		bubble->destscale = player->mo->scale;
		P_SetScale(bubble, bubble->destscale);
	}

	if (player->pflags & PF_NIGHTSMODE) // NiGHTS Super doesn't spawn flight bubbles
		return;
}

//
// P_DoPlayerHeadSigns
//
// Spawns "IT" and "GOT FLAG" signs for Tag and CTF respectively
//
static void P_DoPlayerHeadSigns(player_t *player)
{
	if (G_TagGametype())
	{
		// If you're "IT", show a big "IT" over your head for others to see.
		if (player->pflags & PF_TAGIT)
		{
			if (!P_IsDisplayPlayer(player)) // Don't display it on your own view.
			{
				if (!(player->mo->eflags & MFE_VERTICALFLIP))
					P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height, MT_TAG);
				else
					P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z - mobjinfo[MT_TAG].height, MT_TAG)->eflags |= MFE_VERTICALFLIP;
			}
		}
	}
	else if (gametype == GT_CTF)
	{
		if (player->gotflag & (GF_REDFLAG|GF_BLUEFLAG)) // If you have the flag (duh).
		{
			// Spawn a got-flag message over the head of the player that
			// has it (but not on your own screen if you have the flag).
			if (splitscreen || player != &players[consoleplayer])
			{
				if (player->gotflag & GF_REDFLAG)
				{
					if (!(player->mo->eflags & MFE_VERTICALFLIP))
						P_SpawnMobj(player->mo->x+player->mo->momx, player->mo->y+player->mo->momy,
							player->mo->z+P_GetPlayerHeight(player)+FixedMul(16*FRACUNIT, player->mo->scale)+player->mo->momz, MT_GOTFLAG);
					else
						P_SpawnMobj(player->mo->x+player->mo->momx, player->mo->y+player->mo->momy,
							player->mo->z+player->mo->height-P_GetPlayerHeight(player)-mobjinfo[MT_GOTFLAG].height-FixedMul(16*FRACUNIT, player->mo->scale)+player->mo->momz, MT_GOTFLAG)->eflags |= MFE_VERTICALFLIP;
				}
				if (player->gotflag & GF_BLUEFLAG)
				{
					if (!(player->mo->eflags & MFE_VERTICALFLIP))
						P_SpawnMobj(player->mo->x+player->mo->momx, player->mo->y+player->mo->momy,
							player->mo->z+P_GetPlayerHeight(player)+FixedMul(16*FRACUNIT, player->mo->scale)+player->mo->momz, MT_GOTFLAG2);
					else
						P_SpawnMobj(player->mo->x+player->mo->momx, player->mo->y+player->mo->momy,
							player->mo->z+player->mo->height-P_GetPlayerHeight(player)-mobjinfo[MT_GOTFLAG2].height-FixedMul(16*FRACUNIT, player->mo->scale)+player->mo->momz, MT_GOTFLAG2)->eflags |= MFE_VERTICALFLIP;
				}
			}
		}
	}
}


//
// P_DoJumpShield
//
// Jump Shield Activation
//
void P_DoJumpShield(player_t *player)
{
	if (player->pflags & PF_THOKKED)
		return;

	player->pflags &= ~PF_JUMPED;
	//P_DoJump(player, false);
	player->pflags &= ~PF_JUMPED;
	player->secondjump = 0;
	player->jumping = 0;
	player->pflags |= PF_THOKKED;
	player->pflags &= ~PF_SPINNING;
	//P_SetPlayerMobjState(player->mo, S_PLAY_FALL1);
	S_StartSound(player->mo, sfx_wdjump);
}

//
// P_Telekinesis
//
// Morph's fancy stuff-moving character ability
// +ve thrust pushes away, -ve thrust pulls in
//
void P_Telekinesis(player_t *player, fixed_t thrust, fixed_t range)
{
	thinker_t *th;
	mobj_t *mo2;
	fixed_t dist = 0;
	angle_t an;

	if (player->powers[pw_super]) // increase range when super
		range *= 2;

	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;

		if (mo2 == player->mo)
			continue;

		if (!((mo2->flags & MF_SHOOTABLE && mo2->flags & MF_ENEMY) || mo2->type == MT_EGGGUARD || mo2->player))
			continue;

		dist = P_AproxDistance(P_AproxDistance(player->mo->x-mo2->x, player->mo->y-mo2->y), player->mo->z-mo2->z);

		if (range < dist)
			continue;

		if (!P_CheckSight(player->mo, mo2))
			continue; // if your psychic powers can't "see" it don't bother

		an = R_PointToAngle2(player->mo->x, player->mo->y, mo2->x, mo2->y);

		if (mo2->health > 0)
		{
			P_Thrust(mo2, an, thrust);

			if (mo2->type == MT_GOLDBUZZ || mo2->type == MT_REDBUZZ)
				mo2->tics += 8;
		}
	}

	//P_SpawnThokMobj(player);
	player->pflags |= PF_THOKKED;
}

boolean P_AnalogMove(player_t *player)
{
	return player->pflags & PF_ANALOGMODE;
}


//#define OLD_MOVEMENT_CODE 1
static void P_3dMovement(player_t *player)
{
	ticcmd_t *cmd;
	angle_t movepushangle, movepushsideangle; // Analog
	//INT32 topspeed, acceleration, thrustfactor;
	fixed_t movepushforward = 0, movepushside = 0;
	angle_t dangle; // replaces old quadrants bits
	//boolean dangleflip = false; // SRB2kart - toaster
	//fixed_t normalspd = FixedMul(player->normalspeed, player->mo->scale);
	boolean analogmove = false;
	fixed_t oldMagnitude, newMagnitude;
	vector3_t totalthrust;

	totalthrust.x = totalthrust.y = 0; // I forget if this is needed
	totalthrust.z = FRACUNIT*P_MobjFlip(player->mo)/3; // A bit of extra push-back on slopes

	// Get the old momentum; this will be needed at the end of the function! -SH
	oldMagnitude = R_PointToDist2(player->mo->momx - player->cmomx, player->mo->momy - player->cmomy, 0, 0);

	analogmove = P_AnalogMove(player);

	cmd = &player->cmd;

	if ((player->exiting || mapreset) || player->pflags & PF_STASIS || player->kartstuff[k_spinouttimer]) // pw_introcam?
	{
		cmd->forwardmove = cmd->sidemove = 0;
		if (player->kartstuff[k_sneakertimer])
			cmd->forwardmove = 50;
	}

	if (!(player->pflags & PF_FORCESTRAFE) && !player->kartstuff[k_pogospring])
		cmd->sidemove = 0;

	if (analogmove)
	{
		movepushangle = (cmd->angleturn<<16 /* not FRACBITS */);
	}
	else
	{
		if (player->kartstuff[k_drift] != 0)
			movepushangle = player->mo->angle-(ANGLE_45/5)*player->kartstuff[k_drift];
		else if (player->kartstuff[k_spinouttimer] || player->kartstuff[k_wipeoutslow])	// if spun out, use the boost angle
			movepushangle = (angle_t)player->kartstuff[k_boostangle];
		else
			movepushangle = player->mo->angle;
	}
	movepushsideangle = movepushangle-ANGLE_90;

	// cmomx/cmomy stands for the conveyor belt speed.
	if (player->onconveyor == 2) // Wind/Current
	{
		//if (player->mo->z > player->mo->watertop || player->mo->z + player->mo->height < player->mo->waterbottom)
		if (!(player->mo->eflags & (MFE_UNDERWATER|MFE_TOUCHWATER)))
			player->cmomx = player->cmomy = 0;
	}
	else if (player->onconveyor == 4 && !P_IsObjectOnGround(player->mo)) // Actual conveyor belt
		player->cmomx = player->cmomy = 0;
	else if (player->onconveyor != 2 && player->onconveyor != 4
				&& player->onconveyor != 1
	)
		player->cmomx = player->cmomy = 0;

	player->rmomx = player->mo->momx - player->cmomx;
	player->rmomy = player->mo->momy - player->cmomy;

	// Calculates player's speed based on distance-of-a-line formula
	player->speed = R_PointToDist2(0, 0, player->rmomx, player->rmomy);

	// Monster Iestyn - 04-11-13
	// Quadrants are stupid, excessive and broken, let's do this a much simpler way!
	// Get delta angle from rmom angle and player angle first
	dangle = R_PointToAngle2(0,0, player->rmomx, player->rmomy) - player->mo->angle;
	if (dangle > ANGLE_180) //flip to keep to one side
	{
		dangle = InvAngle(dangle);
		//dangleflip = true;
	}

	// anything else will leave both at 0, so no need to do anything else

	//{ SRB2kart 220217 - Toaster Code for misplaced thrust
	/*
	if (!player->kartstuff[k_drift]) // Not Drifting
	{
		angle_t difference = dangle/2;
		boolean reverse = (dangle >= ANGLE_90);

		if (dangleflip)
			difference = InvAngle(difference);

		if (reverse)
			difference += ANGLE_180;

		P_InstaThrust(player->mo, player->mo->angle + difference, player->speed);
	}
	*/
	//}

	// When sliding, don't allow forward/back
	if (player->pflags & PF_SLIDING)
		cmd->forwardmove = 0;

	// Do not let the player control movement if not onground.
	// SRB2Kart: pogo spring and speed bumps are supposed to control like you're on the ground
	onground = (P_IsObjectOnGround(player->mo) || (player->kartstuff[k_pogospring]));

	player->aiming = cmd->aiming<<FRACBITS;

	// Forward movement
	if (!((player->exiting || mapreset) || (P_PlayerInPain(player) && !onground)))
	{
		//movepushforward = cmd->forwardmove * (thrustfactor * acceleration);
		movepushforward = K_3dKartMovement(player, onground, cmd->forwardmove);

		// allow very small movement while in air for gameplay
		if (!onground)
			movepushforward >>= 2; // proper air movement

		// don't need to account for scale here with kart accel code
		//movepushforward = FixedMul(movepushforward, player->mo->scale);

		if (player->mo->movefactor != FRACUNIT) // Friction-scaled acceleration...
			movepushforward = FixedMul(movepushforward, player->mo->movefactor);

		if (cmd->buttons & BT_BRAKE && !cmd->forwardmove) // SRB2kart - braking isn't instant
			movepushforward /= 64;

		if (cmd->forwardmove > 0)
			player->kartstuff[k_brakestop] = 0;
		else if (player->kartstuff[k_brakestop] < 6) // Don't start reversing with brakes until you've made a stop first
		{
			if (player->speed < 8*FRACUNIT)
				player->kartstuff[k_brakestop]++;
			movepushforward = 0;
		}

		totalthrust.x += P_ReturnThrustX(player->mo, movepushangle, movepushforward);
		totalthrust.y += P_ReturnThrustY(player->mo, movepushangle, movepushforward);
	}
	else if (!(player->kartstuff[k_spinouttimer]))
	{
		K_MomentumToFacing(player);
	}

	// Sideways movement
	if (cmd->sidemove != 0 && !((player->exiting || mapreset) || player->kartstuff[k_spinouttimer]))
	{
		if (cmd->sidemove > 0)
			movepushside = (cmd->sidemove * FRACUNIT/128) + FixedDiv(player->speed, K_GetKartSpeed(player, true));
		else
			movepushside = (cmd->sidemove * FRACUNIT/128) - FixedDiv(player->speed, K_GetKartSpeed(player, true));

		totalthrust.x += P_ReturnThrustX(player->mo, movepushsideangle, movepushside);
		totalthrust.y += P_ReturnThrustY(player->mo, movepushsideangle, movepushside);
	}

	if ((totalthrust.x || totalthrust.y)
		&& player->mo->standingslope && (!(player->mo->standingslope->flags & SL_NOPHYSICS)) && abs(player->mo->standingslope->zdelta) > FRACUNIT/2) {
		// Factor thrust to slope, but only for the part pushing up it!
		// The rest is unaffected.
		angle_t thrustangle = R_PointToAngle2(0, 0, totalthrust.x, totalthrust.y)-player->mo->standingslope->xydirection;

		if (player->mo->standingslope->zdelta < 0) { // Direction goes down, so thrustangle needs to face toward
			if (thrustangle < ANGLE_90 || thrustangle > ANGLE_270) {
				P_QuantizeMomentumToSlope(&totalthrust, player->mo->standingslope);
			}
		} else { // Direction goes up, so thrustangle needs to face away
			if (thrustangle > ANGLE_90 && thrustangle < ANGLE_270) {
				P_QuantizeMomentumToSlope(&totalthrust, player->mo->standingslope);
			}
		}
	}

	player->mo->momx += totalthrust.x;
	player->mo->momy += totalthrust.y;

	// Time to ask three questions:
	// 1) Are we over topspeed?
	// 2) If "yes" to 1, were we moving over topspeed to begin with?
	// 3) If "yes" to 2, are we now going faster?

	// If "yes" to 3, normalize to our initial momentum; this will allow thoks to stay as fast as they normally are.
	// If "no" to 3, ignore it; the player might be going too fast, but they're slowing down, so let them.
	// If "no" to 2, normalize to topspeed, so we can't suddenly run faster than it of our own accord.
	// If "no" to 1, we're not reaching any limits yet, so ignore this entirely!
	// -Shadow Hog
	newMagnitude = R_PointToDist2(player->mo->momx - player->cmomx, player->mo->momy - player->cmomy, 0, 0);
	if (newMagnitude > K_GetKartSpeed(player, true)) //topspeed)
	{
		fixed_t tempmomx, tempmomy;
		if (oldMagnitude > K_GetKartSpeed(player, true) && onground) // SRB2Kart: onground check for air speed cap
		{
			if (newMagnitude > oldMagnitude)
			{
				tempmomx = FixedMul(FixedDiv(player->mo->momx - player->cmomx, newMagnitude), oldMagnitude);
				tempmomy = FixedMul(FixedDiv(player->mo->momy - player->cmomy, newMagnitude), oldMagnitude);
				player->mo->momx = tempmomx + player->cmomx;
				player->mo->momy = tempmomy + player->cmomy;
			}
			// else do nothing
		}
		else
		{
			tempmomx = FixedMul(FixedDiv(player->mo->momx - player->cmomx, newMagnitude), K_GetKartSpeed(player, true)); //topspeed)
			tempmomy = FixedMul(FixedDiv(player->mo->momy - player->cmomy, newMagnitude), K_GetKartSpeed(player, true)); //topspeed)
			player->mo->momx = tempmomx + player->cmomx;
			player->mo->momy = tempmomy + player->cmomy;
		}
	}
}

//
// P_SpectatorMovement
//
// Control for spectators in multiplayer
//
static void P_SpectatorMovement(player_t *player)
{
	ticcmd_t *cmd = &player->cmd;

	player->mo->angle = (cmd->angleturn<<16 /* not FRACBITS */);

	ticruned++;
	if (!(cmd->angleturn & TICCMD_RECEIVED))
		ticmiss++;

	if (player->mo->z > player->mo->ceilingz - player->mo->height)
		player->mo->z = player->mo->ceilingz - player->mo->height;
	if (player->mo->z < player->mo->floorz)
		player->mo->z = player->mo->floorz;

	if (cmd->buttons & BT_ACCELERATE)
		player->mo->z += 32*mapobjectscale;
	else if (cmd->buttons & BT_BRAKE)
		player->mo->z -= 32*mapobjectscale;

	// Aiming needed for SEENAMES, etc.
	// We may not need to fire as a spectator, but this is still handy!
	player->aiming = cmd->aiming<<FRACBITS;

	player->mo->momx = player->mo->momy = player->mo->momz = 0;
	if (cmd->forwardmove != 0)
	{
		P_Thrust(player->mo, player->mo->angle, cmd->forwardmove*mapobjectscale);

		// Quake-style flying spectators :D
		player->mo->momz += FixedMul(cmd->forwardmove*mapobjectscale, AIMINGTOSLOPE(player->aiming));
	}
	/*if (cmd->sidemove != 0) -- was disabled in practice anyways, since sidemove was suppressed
	{
		P_Thrust(player->mo, player->mo->angle-ANGLE_90, cmd->sidemove*mapobjectscale);
	}*/
}

void P_BlackOw(player_t *player)
{
	INT32 i;
	S_StartSound (player->mo, sfx_bkpoof); // Sound the BANG!

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i] && P_AproxDistance(player->mo->x - players[i].mo->x,
			player->mo->y - players[i].mo->y) < 1536*FRACUNIT)
			P_FlashPal(&players[i], PAL_NUKE, 10);

	P_NukeEnemies(player->mo, player->mo, 1536*FRACUNIT); // Search for all nearby enemies and nuke their pants off!
	player->powers[pw_shield] = player->powers[pw_shield] & SH_STACK;
}

void P_ElementalFireTrail(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	fixed_t ground;
	mobj_t *flame;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (player->mo->eflags & MFE_VERTICALFLIP)
		ground = player->mo->ceilingz - FixedMul(mobjinfo[MT_SPINFIRE].height, player->mo->scale);
	else
		ground = player->mo->floorz;

	travelangle = R_PointToAngle2(0, 0, player->rmomx, player->rmomy);

	for (i = 0; i < 2; i++)
	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
		if (player->mo->standingslope)
		{
			ground = P_GetZAt(player->mo->standingslope, newx, newy);
			if (player->mo->eflags & MFE_VERTICALFLIP)
				ground -= FixedMul(mobjinfo[MT_SPINFIRE].height, player->mo->scale);
		}
		flame = P_SpawnMobj(newx, newy, ground, MT_SPINFIRE);
		P_SetTarget(&flame->target, player->mo);
		flame->angle = travelangle;
		flame->fuse = TICRATE*6;
		flame->destscale = player->mo->scale;
		P_SetScale(flame, player->mo->scale);
		flame->eflags = (flame->eflags & ~MFE_VERTICALFLIP)|(player->mo->eflags & MFE_VERTICALFLIP);

		flame->momx = 8;
		P_XYMovement(flame);
		if (P_MobjWasRemoved(flame))
			continue;

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			if (flame->z + flame->height < flame->ceilingz)
				P_RemoveMobj(flame);
		}
		else if (flame->z > flame->floorz)
			P_RemoveMobj(flame);
	}
}

//
// P_MovePlayer
static void P_MovePlayer(player_t *player)
{
	ticcmd_t *cmd;
	//INT32 i;

	fixed_t runspd;

	if (countdowntimeup)
		return;

	cmd = &player->cmd;
	runspd = 14*player->mo->scale; //srb2kart

	// Let's have some movement speed fun on low-friction surfaces, JUST for players...
	// (high friction surfaces shouldn't have any adjustment, since the acceleration in
	// this game is super high and that ends up cheesing high-friction surfaces.)
	runspd = FixedMul(runspd, player->mo->movefactor);

	// Control relinquishing stuff!
	if (player->powers[pw_ingoop])
		player->pflags |= PF_FULLSTASIS;
	else if (player->pflags & PF_GLIDING && player->skidtime)
		player->pflags |= PF_FULLSTASIS;
	else if (player->powers[pw_nocontrol])
	{
		player->pflags |= PF_STASIS;
		if (!(player->powers[pw_nocontrol] & (1<<15)))
			player->pflags |= PF_JUMPSTASIS;
	}
	// note: don't unset stasis here

	if (!player->spectator && G_TagGametype())
	{
		// If we have stasis already here, it's because it's forced on us
		// by a linedef executor or what have you
		boolean forcestasis = false;

		//During hide time, taggers cannot move.
		if (leveltime < hidetime * TICRATE)
		{
			if (player->pflags & PF_TAGIT)
				forcestasis = true;
		}
		else if (gametype == GT_HIDEANDSEEK)
		{
			if (!(player->pflags & PF_TAGIT))
			{
				forcestasis = true;
				if (player->pflags & PF_TAGGED) // Already hit.
					player->powers[pw_flashing] = 5;
			}
		}

		if (forcestasis)
		{
			player->pflags |= PF_FULLSTASIS;
			// If you're in stasis in tag, you don't drown.
			/*if (player->powers[pw_underwater] <= 12*TICRATE + 1)
				P_RestoreMusic(player);*/
			player->powers[pw_underwater] = player->powers[pw_spacetime] = 0;
		}
	}

	if (player->spectator)
	{
		P_SpectatorMovement(player);
		return;
	}

	//////////////////////
	// MOVEMENT CODE	//
	//////////////////////

	{
		INT16 angle_diff, max_left_turn, max_right_turn;
		boolean add_delta = true;

		// Kart: store the current turn range for later use
		if (((player->mo && player->speed > 0) // Moving
			|| (leveltime > starttime && (cmd->buttons & BT_ACCELERATE && cmd->buttons & BT_BRAKE)) // Rubber-burn turn
			|| (player->kartstuff[k_respawn]) // Respawning
			|| (player->spectator || objectplacing)) // Not a physical player
			) // ~~Spinning and boosting cancels out turning~~ Not anymore given spinout is more slippery and more prone to get you killed because of boosters.
		{
			player->lturn_max[leveltime%MAXPREDICTTICS] = K_GetKartTurnValue(player, KART_FULLTURN)+1;
			player->rturn_max[leveltime%MAXPREDICTTICS] = K_GetKartTurnValue(player, -KART_FULLTURN)-1;
		} else {
			player->lturn_max[leveltime%MAXPREDICTTICS] = player->rturn_max[leveltime%MAXPREDICTTICS] = 0;
		}

		if (leveltime >= starttime)
		{
			// KART: Don't directly apply angleturn! It may have been either A) forged by a malicious client, or B) not be a smooth turn due to a player dropping frames.
			// Instead, turn the player only up to the amount they're supposed to turn accounting for latency. Allow exactly 1 extra turn unit to try to keep old replays synced.
			angle_diff = cmd->angleturn - (player->mo->angle>>16);
			max_left_turn = player->lturn_max[(leveltime + MAXPREDICTTICS - cmd->latency) % MAXPREDICTTICS];
			max_right_turn = player->rturn_max[(leveltime + MAXPREDICTTICS - cmd->latency) % MAXPREDICTTICS];

			//CONS_Printf("----------------\nangle diff: %d - turning options: %d to %d - ", angle_diff, max_left_turn, max_right_turn);

			if (angle_diff > max_left_turn)
				angle_diff = max_left_turn;
			else if (angle_diff < max_right_turn)
				angle_diff = max_right_turn;
			else
			{
				// Try to keep normal turning as accurate to 1.0.1 as possible to reduce replay desyncs.
				player->mo->angle = cmd->angleturn<<16;
				add_delta = false;
			}
			//CONS_Printf("applied turn: %d\n", angle_diff);

			if (add_delta) {
				player->mo->angle += angle_diff<<16;
				player->mo->angle &= ~0xFFFF; // Try to keep the turning somewhat similar to how it was before?
				//CONS_Printf("leftover turn (%s): %5d or %4d%%\n",
				//				player_names[player-players],
				//				(INT16) (cmd->angleturn - (player->mo->angle>>16)),
				//				(INT16) (cmd->angleturn - (player->mo->angle>>16)) * 100 / (angle_diff ? angle_diff : 1));
			}
		}

		ticruned++;
		if ((cmd->angleturn & TICCMD_RECEIVED) == 0)
			ticmiss++;

		P_3dMovement(player);
	}

	if (maptol & TOL_2D)
		runspd = FixedMul(runspd, 2*FRACUNIT/3);

	//P_SkidStuff(player);

	/////////////////////////
	// MOVEMENT ANIMATIONS //
	/////////////////////////

	// Kart frames
	if (player->kartstuff[k_squishedtimer] > 0)
	{
		if (player->mo->state != &states[S_KART_SQUISH])
			P_SetPlayerMobjState(player->mo, S_KART_SQUISH);
	}
	else if (player->pflags & PF_SLIDING)
	{
		if (player->mo->state != &states[S_KART_SPIN])
			P_SetPlayerMobjState(player->mo, S_KART_SPIN);
		player->frameangle -= ANGLE_22h;
	}
	else if (player->kartstuff[k_spinouttimer] > 0)
	{
		INT32 speed = max(1, min(8, player->kartstuff[k_spinouttimer]/8));

		if (player->mo->state != &states[S_KART_SPIN])
			P_SetPlayerMobjState(player->mo, S_KART_SPIN);

		if (speed == 1 && abs((signed)(player->mo->angle - player->frameangle)) < ANGLE_22h)
			player->frameangle = player->mo->angle; // Face forward at the end of the animation
		else
			player->frameangle -= (ANGLE_11hh * speed);
	}
	else if (player->powers[pw_nocontrol] && player->pflags & PF_SKIDDOWN)
	{
		if (player->mo->state != &states[S_KART_SPIN])
			P_SetPlayerMobjState(player->mo, S_KART_SPIN);

		if (((player->powers[pw_nocontrol] + 5) % 20) < 10)
			player->frameangle += ANGLE_11hh;
		else
			player->frameangle -= ANGLE_11hh;
	}
	else
	{
		K_KartMoveAnimation(player);

		if (player->kartstuff[k_pogospring])
			player->frameangle += ANGLE_22h;
		else
			player->frameangle = player->mo->angle;
	}

	player->mo->movefactor = FRACUNIT; // We're not going to do any more with this, so let's change it back for the next frame.

	// If you are stopped and are still walking, stand still!
	if (!player->mo->momx && !player->mo->momy && !player->mo->momz && player->panim == PA_WALK)
		P_SetPlayerMobjState(player->mo, S_KART_STND1); // SRB2kart - was S_PLAY_STND

	//{ SRB2kart

	// Drifting sound
	// Start looping the sound now.
	if (leveltime % 50 == 0 && onground && player->kartstuff[k_drift] != 0)
		S_StartSound(player->mo, sfx_drift);
	// Leveltime being 50 might take a while at times. We'll start it up once, isntantly.
	else if (!S_SoundPlaying(player->mo, sfx_drift) && onground && player->kartstuff[k_drift] != 0)
		S_StartSound(player->mo, sfx_drift);
	// Ok, we'll stop now.
	else if (player->kartstuff[k_drift] == 0)
		S_StopSoundByID(player->mo, sfx_drift);

	K_MoveKartPlayer(player, onground);
	//}


//////////////////
//GAMEPLAY STUFF//
//////////////////

	// Make sure you're not "jumping" on the ground
	if (onground && player->pflags & PF_JUMPED && !(player->pflags & PF_GLIDING)
	&& P_MobjFlip(player->mo)*player->mo->momz < 0)
	{
		player->pflags &= ~PF_JUMPED;
		player->jumping = 0;
		player->secondjump = 0;
		player->pflags &= ~PF_THOKKED;
		P_SetPlayerMobjState(player->mo, S_KART_STND1); // SRB2kart - was S_PLAY_STND
	}

	if (player->gotflag) // If you can't glide, then why the heck would you be gliding?
	{
		player->pflags &= ~PF_GLIDING;
		player->glidetime = 0;
		player->climbing = 0;
	}

	// If you're running fast enough, you can create splashes as you run in shallow water.
	if (!player->climbing
	&& ((!(player->mo->eflags & MFE_VERTICALFLIP) && player->mo->z + player->mo->height >= player->mo->watertop && player->mo->z <= player->mo->watertop)
		|| (player->mo->eflags & MFE_VERTICALFLIP && player->mo->z + player->mo->height >= player->mo->waterbottom && player->mo->z <= player->mo->waterbottom))
	&& (player->speed > runspd || (player->pflags & PF_STARTDASH))
	&& leveltime % (TICRATE/7) == 0 && player->mo->momz == 0 && !(player->pflags & PF_SLIDING) && !player->spectator)
	{
		mobj_t *water = P_SpawnMobj(player->mo->x, player->mo->y,
			((player->mo->eflags & MFE_VERTICALFLIP) ? player->mo->waterbottom - FixedMul(mobjinfo[MT_SPLISH].height, player->mo->scale) : player->mo->watertop), MT_SPLISH);
		if (player->mo->eflags & MFE_GOOWATER)
			S_StartSound(water, sfx_ghit);
		else
			S_StartSound(water, sfx_wslap);
		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			water->flags2 |= MF2_OBJECTFLIP;
			water->eflags |= MFE_VERTICALFLIP;
		}
		water->destscale = player->mo->scale;
		P_SetScale(water, player->mo->scale);
	}

	// Little water sound while touching water - just a nicety.
	if ((player->mo->eflags & MFE_TOUCHWATER) && !(player->mo->eflags & MFE_UNDERWATER) && !player->spectator)
	{
		if (P_RandomChance(FRACUNIT/2) && leveltime % TICRATE == 0)
			S_StartSound(player->mo, sfx_floush);
	}

	////////////////////////////
	//SPINNING AND SPINDASHING//
	////////////////////////////

	// SRB2kart - Drifting smoke and fire
	if (player->kartstuff[k_sneakertimer] > 0 && onground && (leveltime & 1))
		K_SpawnBoostTrail(player);

	if (player->kartstuff[k_invincibilitytimer] > 0)
		K_SpawnSparkleTrail(player->mo);

	if (player->kartstuff[k_wipeoutslow] > 1 && (leveltime & 1))
		K_SpawnWipeoutTrail(player->mo, false);

	K_DriftDustHandling(player->mo);


	// Crush test...
	if ((player->mo->ceilingz - player->mo->floorz < player->mo->height)
		&& !(player->mo->flags & MF_NOCLIP))
	{
		if (player->mo->ceilingz - player->mo->floorz < player->mo->height)
		{
			if ((netgame || multiplayer) && player->spectator)
				P_DamageMobj(player->mo, NULL, NULL, 42000); // Respawn crushed spectators
			else
			{
				K_SquishPlayer(player, NULL, NULL); // SRB2kart - we don't kill when squished, we squish when squished.
			}

			if (player->playerstate == PST_DEAD)
				return;
		}
	}

#ifdef HWRENDER
	if (rendermode != render_soft && rendermode != render_none && cv_grfovchange.value)
	{
		fixed_t speed;
		const fixed_t runnyspeed = 20*FRACUNIT;

		speed = R_PointToDist2(player->rmomx, player->rmomy, 0, 0);

		if (speed > K_GetKartSpeed(player, false)-(5<<FRACBITS))
			speed = K_GetKartSpeed(player, false)-(5<<FRACBITS);

		if (speed >= runnyspeed)
			player->fovadd = speed-runnyspeed;
		else
			player->fovadd = 0;

		if (player->fovadd < 0)
			player->fovadd = 0;
	}
	else
		player->fovadd = 0;
#endif

#ifdef FLOORSPLATS
	if (cv_shadow.value && rendermode == render_soft)
		R_AddFloorSplat(player->mo->subsector, player->mo, "SHADOW", player->mo->x,
			player->mo->y, player->mo->floorz, SPLATDRAWMODE_OPAQUE);
#endif

	// Look for blocks to bust up
	// Because of FF_SHATTER, we should look for blocks constantly,
	// not just when spinning or playing as Knuckles
	if (CheckForBustableBlocks)
		P_CheckBustableBlocks(player);

	// Check for a BOUNCY sector!
	if (CheckForBouncySector)
		P_CheckBouncySectors(player);

	// Look for Quicksand!
	if (CheckForQuicksand)
		P_CheckQuicksand(player);
}

static void P_DoZoomTube(player_t *player)
{
	INT32 sequence;
	fixed_t speed;
	thinker_t *th;
	mobj_t *mo2;
	mobj_t *waypoint = NULL;
	fixed_t dist;
	boolean reverse;

	//player->mo->height = P_GetPlayerSpinHeight(player);

	if (player->speed > 0)
		reverse = false;
	else
		reverse = true;

	player->powers[pw_flashing] = 1;

	speed = abs(player->speed);

	sequence = player->mo->tracer->threshold;

	// change slope
	dist = P_AproxDistance(P_AproxDistance(player->mo->tracer->x - player->mo->x, player->mo->tracer->y - player->mo->y), player->mo->tracer->z - player->mo->z);

	if (dist < 1)
		dist = 1;

	player->mo->momx = FixedMul(FixedDiv(player->mo->tracer->x - player->mo->x, dist), (speed));
	player->mo->momy = FixedMul(FixedDiv(player->mo->tracer->y - player->mo->y, dist), (speed));
	player->mo->momz = FixedMul(FixedDiv(player->mo->tracer->z - player->mo->z, dist), (speed));

	// Calculate the distance between the player and the waypoint
	// 'dist' already equals this.

	// Will the player go past the waypoint?
	if (speed > dist)
	{
		speed -= dist;
		// If further away, set XYZ of player to waypoint location
		P_UnsetThingPosition(player->mo);
		player->mo->x = player->mo->tracer->x;
		player->mo->y = player->mo->tracer->y;
		player->mo->z = player->mo->tracer->z;
		P_SetThingPosition(player->mo);

		// ugh, duh!!
		player->mo->floorz = player->mo->subsector->sector->floorheight;
		player->mo->ceilingz = player->mo->subsector->sector->ceilingheight;

		CONS_Debug(DBG_GAMELOGIC, "Looking for next waypoint...\n");

		// Find next waypoint
		for (th = thinkercap.next; th != &thinkercap; th = th->next)
		{
			if (th->function.acp1 != (actionf_p1)P_MobjThinker) // Not a mobj thinker
				continue;

			mo2 = (mobj_t *)th;

			if (mo2->type != MT_TUBEWAYPOINT)
				continue;

			if (mo2->threshold == sequence)
			{
				if ((reverse && mo2->health == player->mo->tracer->health - 1)
					|| (!reverse && mo2->health == player->mo->tracer->health + 1))
				{
					waypoint = mo2;
					break;
				}
			}
		}

		if (waypoint)
		{
			CONS_Debug(DBG_GAMELOGIC, "Found waypoint (sequence %d, number %d).\n", waypoint->threshold, waypoint->health);

			P_SetTarget(&player->mo->tracer, waypoint);

			// calculate MOMX/MOMY/MOMZ for next waypoint

			// change slope
			dist = P_AproxDistance(P_AproxDistance(player->mo->tracer->x - player->mo->x, player->mo->tracer->y - player->mo->y), player->mo->tracer->z - player->mo->z);

			if (dist < 1)
				dist = 1;

			player->mo->momx = FixedMul(FixedDiv(player->mo->tracer->x - player->mo->x, dist), (speed));
			player->mo->momy = FixedMul(FixedDiv(player->mo->tracer->y - player->mo->y, dist), (speed));
			player->mo->momz = FixedMul(FixedDiv(player->mo->tracer->z - player->mo->z, dist), (speed));
		}
		else
		{
			P_SetTarget(&player->mo->tracer, NULL); // Else, we just let them fly.

			CONS_Debug(DBG_GAMELOGIC, "Next waypoint not found, releasing from track...\n");
		}
	}

	// change angle
	if (player->mo->tracer)
	{
		player->mo->angle = R_PointToAngle2(player->mo->x, player->mo->y, player->mo->tracer->x, player->mo->tracer->y);

		if (player == &players[consoleplayer])
			localangle[0] = player->mo->angle;
		else if (player == &players[displayplayers[1]])
			localangle[1] = player->mo->angle;
		else if (player == &players[displayplayers[2]])
			localangle[2] = player->mo->angle;
		else if (player == &players[displayplayers[3]])
			localangle[3] = player->mo->angle;
	}
#if 0
	if (player->mo->state != &states[S_KART_SPIN])
		P_SetPlayerMobjState(player->mo, S_KART_SPIN);
	player->frameangle -= ANGLE_22h;
#endif
}

//
// P_NukeEnemies
// Looks for something you can hit - Used for bomb shield
//
void P_NukeEnemies(mobj_t *inflictor, mobj_t *source, fixed_t radius)
{
	const fixed_t ns = 60 * mapobjectscale;
	mobj_t *mo;
	angle_t fa;
	thinker_t *think;
	INT32 i;

	radius = FixedMul(radius, mapobjectscale);

	for (i = 0; i < 16; i++)
	{
		fa = (i*(FINEANGLES/16));
		mo = P_SpawnMobj(inflictor->x, inflictor->y, inflictor->z, MT_SUPERSPARK);
		if (!P_MobjWasRemoved(mo))
		{
			mo->momx = FixedMul(FINESINE(fa),ns);
			mo->momy = FixedMul(FINECOSINE(fa),ns);
		}
	}

	for (think = thinkercap.next; think != &thinkercap; think = think->next)
	{
		if (think->function.acp1 != (actionf_p1)P_MobjThinker)
			continue; // not a mobj thinker

		mo = (mobj_t *)think;

		if (!(mo->flags & MF_SHOOTABLE) && !(mo->type == MT_EGGGUARD || mo->type == MT_MINUS
			|| mo->type == MT_SPB)) // Don't want to give SPB MF_SHOOTABLE, to ensure it's undamagable through other means
			continue;

		if (mo->flags & MF_MONITOR)
			continue; // Monitors cannot be 'nuked'.

		//if (!G_BattleGametype() && mo->type == MT_PLAYER)
		//	continue; // Don't hurt players in Co-Op!

		if (abs(inflictor->x - mo->x) > radius || abs(inflictor->y - mo->y) > radius || abs(inflictor->z - mo->z) > radius)
			continue; // Workaround for possible integer overflow in the below -Red

		if (P_AproxDistance(P_AproxDistance(inflictor->x - mo->x, inflictor->y - mo->y), inflictor->z - mo->z) > radius)
			continue;

		if (mo->type == MT_MINUS && !(mo->flags & (MF_SPECIAL|MF_SHOOTABLE)))
			mo->flags |= MF_SPECIAL|MF_SHOOTABLE;

		if (mo->type == MT_EGGGUARD && mo->tracer) //nuke Egg Guard's shield!
			P_KillMobj(mo->tracer, inflictor, source);

		if (mo->flags & MF_BOSS) //don't OHKO bosses!
			P_DamageMobj(mo, inflictor, source, 1);

		//{ SRB2kart
		if (mo->type == MT_ORBINAUT || mo->type == MT_JAWZ || mo->type == MT_JAWZ_DUD
			|| mo->type == MT_ORBINAUT_SHIELD || mo->type == MT_JAWZ_SHIELD
			|| mo->type == MT_BANANA || mo->type == MT_BANANA_SHIELD
			|| mo->type == MT_EGGMANITEM || mo->type == MT_EGGMANITEM_SHIELD
			|| mo->type == MT_BALLHOG || mo->type == MT_SPB)
		{
			if (mo->eflags & MFE_VERTICALFLIP)
				mo->z -= mo->height;
			else
				mo->z += mo->height;

			S_StartSound(mo, mo->info->deathsound);
			P_KillMobj(mo, inflictor, source);

			P_SetObjectMomZ(mo, 8*FRACUNIT, false);
			P_InstaThrust(mo, R_PointToAngle2(inflictor->x, inflictor->y, mo->x, mo->y)+ANGLE_90, 16*FRACUNIT);
		}

		if (mo->type == MT_SPB) // If you destroy a SPB, you don't get the luxury of a cooldown.
		{
			spbplace = -1;
			indirectitemcooldown = 0;
		}

		if (mo == inflictor) // Don't nuke yourself, dummy!
			continue;

		if (mo->type == MT_PLAYER) // Players wipe out in Kart
			K_SpinPlayer(mo->player, source, 0, inflictor, false);
		//}
		else
			P_DamageMobj(mo, inflictor, source, 1000);
	}
}

//
// P_LookForEnemies
// Looks for something you can hit - Used for homing attack
// Includes monitors and springs!
//
boolean P_LookForEnemies(player_t *player)
{
	mobj_t *mo;
	thinker_t *think;
	mobj_t *closestmo = NULL;
	angle_t an;

	for (think = thinkercap.next; think != &thinkercap; think = think->next)
	{
		if (think->function.acp1 != (actionf_p1)P_MobjThinker)
			continue; // not a mobj thinker

		mo = (mobj_t *)think;
		if (!(mo->flags & (MF_ENEMY|MF_BOSS|MF_MONITOR|MF_SPRING)))
			continue; // not a valid enemy

		if (mo->health <= 0) // dead
			continue;

		if (mo == player->mo)
			continue;

		if (mo->flags2 & MF2_FRET)
			continue;

		if ((mo->flags & (MF_ENEMY|MF_BOSS)) && !(mo->flags & MF_SHOOTABLE)) // don't aim at something you can't shoot at anyway (see Egg Guard or Minus)
			continue;

		if (mo->type == MT_DETON) // Don't be STUPID, Sonic!
			continue;

		if (((mo->z > player->mo->z+FixedMul(MAXSTEPMOVE, mapobjectscale)) && !(player->mo->eflags & MFE_VERTICALFLIP))
		|| ((mo->z+mo->height < player->mo->z+player->mo->height-FixedMul(MAXSTEPMOVE, mapobjectscale)) && (player->mo->eflags & MFE_VERTICALFLIP))) // Reverse gravity check - Flame.
			continue; // Don't home upwards!

		if (P_AproxDistance(P_AproxDistance(player->mo->x-mo->x, player->mo->y-mo->y),
			player->mo->z-mo->z) > FixedMul(RING_DIST, player->mo->scale))
			continue; // out of range

		if ((twodlevel || player->mo->flags2 & MF2_TWOD)
		&& abs(player->mo->y-mo->y) > player->mo->radius)
			continue; // not in your 2d plane

		if (mo->type == MT_PLAYER) // Don't chase after other players!
			continue;

		if (closestmo && P_AproxDistance(P_AproxDistance(player->mo->x-mo->x, player->mo->y-mo->y),
			player->mo->z-mo->z) > P_AproxDistance(P_AproxDistance(player->mo->x-closestmo->x,
			player->mo->y-closestmo->y), player->mo->z-closestmo->z))
			continue;

		an = R_PointToAngle2(player->mo->x, player->mo->y, mo->x, mo->y) - player->mo->angle;

		if (an > ANGLE_90 && an < ANGLE_270)
			continue; // behind back

		if (!P_CheckSight(player->mo, mo))
			continue; // out of sight

		closestmo = mo;
	}

	if (closestmo)
	{
		// Found a target monster
		P_SetTarget(&player->mo->target, P_SetTarget(&player->mo->tracer, closestmo));
		player->mo->angle = R_PointToAngle2(player->mo->x, player->mo->y, closestmo->x, closestmo->y);
		return true;
	}

	return false;
}

void P_HomingAttack(mobj_t *source, mobj_t *enemy) // Home in on your target
{
	fixed_t dist;
	fixed_t ns = 0;

	if (!enemy)
		return;

	if (!(enemy->health))
		return;

	// change angle
	source->angle = R_PointToAngle2(source->x, source->y, enemy->x, enemy->y);
	if (source->player)
	{
		if (source->player == &players[consoleplayer])
			localangle[0] = source->angle;
		else if (source->player == &players[displayplayers[1]])
			localangle[1] = source->angle;
		else if (source->player == &players[displayplayers[2]])
			localangle[2] = source->angle;
		else if (source->player == &players[displayplayers[3]])
			localangle[3] = source->angle;
	}

	// change slope
	dist = P_AproxDistance(P_AproxDistance(enemy->x - source->x, enemy->y - source->y),
		enemy->z - source->z);

	if (dist < 1)
		dist = 1;

	if (source->type == MT_DETON && enemy->player) // For Deton Chase (Unused)
		ns = FixedDiv(FixedMul(K_GetKartSpeed(enemy->player, false), enemy->scale), FixedDiv(20*FRACUNIT,17*FRACUNIT));
	else if (source->type != MT_PLAYER)
	{
		if (source->threshold == 32000)
			ns = FixedMul(source->info->speed/2, source->scale);
		else
			ns = FixedMul(source->info->speed, source->scale);
	}
	else if (source->player)
		ns = FixedDiv(FixedMul(K_GetKartSpeed(source->player, false), source->scale), 3*FRACUNIT/2);

	source->momx = FixedMul(FixedDiv(enemy->x - source->x, dist), ns);
	source->momy = FixedMul(FixedDiv(enemy->y - source->y, dist), ns);
	source->momz = FixedMul(FixedDiv(enemy->z - source->z, dist), ns);
}

// Search for emeralds
void P_FindEmerald(void)
{
	thinker_t *th;
	mobj_t *mo2;

	hunt1 = hunt2 = hunt3 = NULL;

	// scan the remaining thinkers
	// to find all emeralds
	for (th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		if (th->function.acp1 != (actionf_p1)P_MobjThinker)
			continue;

		mo2 = (mobj_t *)th;
		if (mo2->type == MT_EMERHUNT)
		{
			if (!hunt1)
				hunt1 = mo2;
			else if (!hunt2)
				hunt2 = mo2;
			else if (!hunt3)
				hunt3 = mo2;
		}
	}
	return;
}

//
// P_DeathThink
// Fall on your face when dying.
// Decrease POV height to floor height.
//
static void P_DeathThink(player_t *player)
{
	//ticcmd_t *cmd = &player->cmd;
	//player->deltaviewheight = 0;

	if (player->deadtimer < INT32_MAX)
		player->deadtimer++;

	if (player->bot) // don't allow bots to do any of the below, B_CheckRespawn does all they need for respawning already
		goto notrealplayer;

	if (player->pflags & PF_TIMEOVER)
	{
		player->kartstuff[k_timeovercam]++;
		if (player->mo)
		{
			player->mo->flags |= (MF_NOGRAVITY|MF_NOCLIP);
			player->mo->flags2 |= MF2_DONTDRAW;
		}
	}
	else
		player->kartstuff[k_timeovercam] = 0;

	K_KartPlayerHUDUpdate(player);

	// Force respawn if idle for more than 30 seconds in shooter modes.
	if (player->lives > 0 /*&& leveltime >= starttime*/) // *could* you respawn?
	{
		// SRB2kart - spawn automatically after 1 second
		if (player->deadtimer > ((netgame || multiplayer)
			? cv_respawntime.value*TICRATE
			: TICRATE)) // don't let them change it in record attack
				player->playerstate = PST_REBORN;
	}

	// Keep time rolling
	if (!(exitcountdown && !racecountdown) && !(player->exiting || mapreset) && !(player->pflags & PF_TIMEOVER))
	{
		if (leveltime >= starttime)
		{
			player->realtime = leveltime - starttime;
			if (player == &players[consoleplayer])
			{
				if (player->spectator || !circuitmap)
					curlap = 0;
				else
					curlap++; // This is too complicated to sync to realtime, just sorta hope for the best :V
			}
		}
		else
		{
			player->realtime = 0;
			if (player == &players[consoleplayer])
				curlap = 0;
		}
	}

notrealplayer:

	if (!player->mo)
		return;

	player->mo->colorized = false;
	player->mo->color = player->skincolor;

	P_CalcHeight(player);
}

//
// P_MoveCamera: make sure the camera is not outside the world and looks at the player avatar
//

camera_t camera[MAXSPLITSCREENPLAYERS]; // Four cameras, three for splitscreen

static void CV_CamRotate_OnChange(void)
{
	if (cv_cam_rotate.value < 0)
		CV_SetValue(&cv_cam_rotate, cv_cam_rotate.value + 360);
	else if (cv_cam_rotate.value > 359)
		CV_SetValue(&cv_cam_rotate, cv_cam_rotate.value % 360);
}

static void CV_CamRotate2_OnChange(void)
{
	if (cv_cam2_rotate.value < 0)
		CV_SetValue(&cv_cam2_rotate, cv_cam2_rotate.value + 360);
	else if (cv_cam2_rotate.value > 359)
		CV_SetValue(&cv_cam2_rotate, cv_cam2_rotate.value % 360);
}

static void CV_CamRotate3_OnChange(void)
{
	if (cv_cam3_rotate.value < 0)
		CV_SetValue(&cv_cam3_rotate, cv_cam3_rotate.value + 360);
	else if (cv_cam3_rotate.value > 359)
		CV_SetValue(&cv_cam3_rotate, cv_cam3_rotate.value % 360);
}

static void CV_CamRotate4_OnChange(void)
{
	if (cv_cam4_rotate.value < 0)
		CV_SetValue(&cv_cam4_rotate, cv_cam4_rotate.value + 360);
	else if (cv_cam4_rotate.value > 359)
		CV_SetValue(&cv_cam4_rotate, cv_cam4_rotate.value % 360);
}

static CV_PossibleValue_t CV_CamSpeed[] = {{0, "MIN"}, {1*FRACUNIT, "MAX"}, {0, NULL}};
static CV_PossibleValue_t rotation_cons_t[] = {{1, "MIN"}, {45, "MAX"}, {0, NULL}};
static CV_PossibleValue_t CV_CamRotate[] = {{-720, "MIN"}, {720, "MAX"}, {0, NULL}};

consvar_t cv_cam_dist = {"cam_dist", "160", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam_height = {"cam_height", "50", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam_still = {"cam_still", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam_speed = {"cam_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam_rotate = {"cam_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam_rotspeed = {"cam_rotspeed", "10", CV_SAVE, rotation_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_cam2_dist = {"cam2_dist", "160", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam2_height = {"cam2_height", "50", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam2_still = {"cam2_still", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam2_speed = {"cam2_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam2_rotate = {"cam2_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate2_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam2_rotspeed = {"cam2_rotspeed", "10", CV_SAVE, rotation_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_cam3_dist = {"cam3_dist", "160", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam3_height = {"cam3_height", "50", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam3_still = {"cam3_still", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam3_speed = {"cam3_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam3_rotate = {"cam3_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate3_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam3_rotspeed = {"cam3_rotspeed", "10", CV_SAVE, rotation_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_cam4_dist = {"cam4_dist", "160", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam4_height = {"cam4_height", "50", CV_FLOAT|CV_SAVE, NULL, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam4_still = {"cam4_still", "Off", 0, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam4_speed = {"cam4_speed", "0.4", CV_FLOAT|CV_SAVE, CV_CamSpeed, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam4_rotate = {"cam4_rotate", "0", CV_CALL|CV_NOINIT, CV_CamRotate, CV_CamRotate4_OnChange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_cam4_rotspeed = {"cam4_rotspeed", "10", CV_SAVE, rotation_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_tilting = {"tilting", "Off", CV_SAVE|CV_CALL, CV_OnOff, Bird_menu_Onchange, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_quaketilt = {"quaketilt", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
consvar_t cv_tiltsmoothing = {"tiltsmoothing", "32", CV_SAVE, CV_Natural, NULL, 0, NULL, NULL, 0, 0, NULL};

consvar_t cv_actionmovie = {"actionmovie", "Off", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};

fixed_t t_cam_dist = -42;
fixed_t t_cam_height = -42;
fixed_t t_cam_rotate = -42;
fixed_t t_cam2_dist = -42;
fixed_t t_cam2_height = -42;
fixed_t t_cam2_rotate = -42;
fixed_t t_cam3_dist = -42;
fixed_t t_cam3_height = -42;
fixed_t t_cam3_rotate = -42;
fixed_t t_cam4_dist = -42;
fixed_t t_cam4_height = -42;
fixed_t t_cam4_rotate = -42;

#define MAXCAMERADIST 140*FRACUNIT // Max distance the camera can be in front of the player (2D mode)

// Heavily simplified version of G_BuildTicCmd that only takes the local first player's control input and converts it to readable ticcmd_t
// we then throw that ticcmd garbage in the camera and make it move

// redefine this
static fixed_t forwardmove[2] = {25<<FRACBITS>>16, 50<<FRACBITS>>16};
static fixed_t sidemove[2] = {2<<FRACBITS>>16, 4<<FRACBITS>>16};
static fixed_t angleturn[3] = {KART_FULLTURN/2, KART_FULLTURN, KART_FULLTURN/4}; // + slow turn

static ticcmd_t cameracmd;

struct demofreecam_s democam;

// called by m_menu to reinit cam input every time it's toggled
void P_InitCameraCmd(void)
{
	memset(&cameracmd, 0, sizeof(ticcmd_t));	// initialize cmd
}

static ticcmd_t *P_CameraCmd(camera_t *cam)
{
	INT32 laim, th, tspeed, forward, side, axis; //i
	const INT32 speed = 1;
	// these ones used for multiple conditions
	boolean turnleft, turnright, mouseaiming;
	boolean invertmouse, lookaxis, usejoystick, kbl;
	angle_t lang;
	INT32 player_invert;
	INT32 screen_invert;

	ticcmd_t *cmd = &cameracmd;

	(void)cam;

	if (!demo.playback)
		return cmd;	// empty cmd, no.

	lang = democam.localangle;
	laim = democam.localaiming;
	th = democam.turnheld;
	kbl = democam.keyboardlook;

	G_CopyTiccmd(cmd, I_BaseTiccmd(), 1); // empty, or external driver

	cmd->angleturn = (INT16)(lang >> 16);
	cmd->aiming = G_ClipAimingPitch(&laim);

	mouseaiming = true;
	invertmouse = cv_invertmouse.value;
	lookaxis = cv_lookaxis.value;

	usejoystick = true;
	turnright = InputDown(gc_turnright, 1);
	turnleft = InputDown(gc_turnleft, 1);

	axis = JoyAxis(AXISTURN, 1);

	if (encoremode)
	{
		turnright ^= turnleft; // swap these using three XORs
		turnleft ^= turnright;
		turnright ^= turnleft;
		axis = -axis;
	}

	if (axis != 0)
	{
		turnright = turnright || (axis > 0);
		turnleft = turnleft || (axis < 0);
	}
	forward = side = 0;

	// use two stage accelerative turning
	// on the keyboard and joystick
	if (turnleft || turnright)
		th += 1;
	else
		th = 0;

	if (th < SLOWTURNTICS)
		tspeed = 2; // slow turn
	else
		tspeed = speed;

	// let movement keys cancel each other out
	if (turnright && !(turnleft))
	{
		cmd->angleturn = (INT16)(cmd->angleturn - (angleturn[tspeed]));
		side += sidemove[1];
	}
	else if (turnleft && !(turnright))
	{
		cmd->angleturn = (INT16)(cmd->angleturn + (angleturn[tspeed]));
		side -= sidemove[1];
	}

	cmd->angleturn = (INT16)(cmd->angleturn - ((mousex*(encoremode ? -1 : 1)*8)));

	axis = JoyAxis(AXISMOVE, 1);
	if (InputDown(gc_accelerate, 1) || (usejoystick && axis > 0))
		cmd->buttons |= BT_ACCELERATE;
	axis = JoyAxis(AXISBRAKE, 1);
	if (InputDown(gc_brake, 1) || (usejoystick && axis > 0))
		cmd->buttons |= BT_BRAKE;
	axis = JoyAxis(AXISAIM, 1);
	if (InputDown(gc_aimforward, 1) || (usejoystick && axis < 0))
		forward += forwardmove[1];
	if (InputDown(gc_aimbackward, 1) || (usejoystick && axis > 0))
		forward -= forwardmove[1];

	// fire with any button/key
	axis = JoyAxis(AXISFIRE, 1);
	if (InputDown(gc_fire, 1) || (usejoystick && axis > 0))
		cmd->buttons |= BT_ATTACK;

	// spectator aiming shit, ahhhh...
	player_invert = invertmouse ? -1 : 1;
	screen_invert = 1;	// nope

	// mouse look stuff (mouse look is not the same as mouse aim)
	kbl = false;

	// looking up/down
	laim += (mlooky<<19)*player_invert*screen_invert;

	axis = JoyAxis(AXISLOOK, 1);

	// spring back if not using keyboard neither mouselookin'
	if (!kbl && !lookaxis && !mouseaiming)
		laim = 0;

	if (InputDown(gc_lookup, 1) || (axis < 0))
	{
		laim += KB_LOOKSPEED * screen_invert;
		kbl = true;
	}
	else if (InputDown(gc_lookdown, 1) || (axis > 0))
	{
		laim -= KB_LOOKSPEED * screen_invert;
		kbl = true;
	}

	if (InputDown(gc_centerview, 1)) // No need to put a spectator limit on this one though :V
		laim = 0;

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

	lang += (cmd->angleturn<<16);

	democam.localangle = lang;
	democam.localaiming = laim;
	democam.turnheld = th;
	democam.keyboardlook = kbl;

	return cmd;
}

void P_DemoCameraMovement(camera_t *cam)
{
	ticcmd_t *cmd;
	angle_t thrustangle;
	mobj_t *awayviewmobj_hack;
	player_t *lastp;

	// update democam stuff with what we got here:
	democam.cam = cam;
	democam.localangle = cam->angle;
	democam.localaiming = cam->aiming;

	// first off we need to get button input
	cmd = P_CameraCmd(cam);

	cam->aiming = cmd->aiming<<FRACBITS;
	cam->angle = cmd->angleturn<<16;

	// camera movement:

	if (cmd->buttons & BT_ACCELERATE)
		cam->z += 32*mapobjectscale;
	else if (cmd->buttons & BT_BRAKE)
		cam->z -= 32*mapobjectscale;

	// if you hold item, you will lock on to displayplayer. (The last player you were ""f12-ing"")
	if (cmd->buttons & BT_ATTACK)
	{
		lastp = &players[displayplayers[0]];	// Fun fact, I was trying displayplayers[0]->mo as if it was Lua like an absolute idiot.
		cam->angle = R_PointToAngle2(cam->x, cam->y, lastp->mo->x, lastp->mo->y);
		cam->aiming = R_PointToAngle2(0, cam->z, R_PointToDist2(cam->x, cam->y, lastp->mo->x, lastp->mo->y), lastp->mo->z + lastp->mo->scale*128*P_MobjFlip(lastp->mo));	// This is still unholy. Aim a bit above their heads.
	}


	cam->momx = cam->momy = cam->momz = 0;
	if (cmd->forwardmove != 0)
	{

		thrustangle = cam->angle >> ANGLETOFINESHIFT;

		cam->x += FixedMul(cmd->forwardmove*mapobjectscale, FINECOSINE(thrustangle));
		cam->y += FixedMul(cmd->forwardmove*mapobjectscale, FINESINE(thrustangle));
		cam->z += FixedMul(cmd->forwardmove*mapobjectscale, AIMINGTOSLOPE(cam->aiming));
		// momentums are useless here, directly add to the coordinates

		// this.......... doesn't actually check for floors and walls and whatnot but the function to do that is a pure mess so fuck that.
		// besides freecam going inside walls sounds pretty cool on paper.
	}

	// awayviewmobj hack; this is to prevent us from hearing sounds from the player's perspective

	awayviewmobj_hack = P_SpawnMobj(cam->x, cam->y, cam->z, MT_THOK);
	awayviewmobj_hack->tics = 2;
	awayviewmobj_hack->flags2 |= MF2_DONTDRAW;

	democam.soundmobj = awayviewmobj_hack;

	// update subsector to avoid crashes;
	cam->subsector = R_PointInSubsector(cam->x, cam->y);
}

void P_ResetCamera(player_t *player, camera_t *thiscam)
{
	tic_t tries = 0;
	fixed_t x, y, z;

	if (demo.freecam)
		return;	// do not reset the camera there.

	if (!player->mo)
		return;

	if (thiscam->chase && player->mo->health <= 0)
		return;

	thiscam->chase = true;
	x = player->mo->x - P_ReturnThrustX(player->mo, thiscam->angle, player->mo->radius);
	y = player->mo->y - P_ReturnThrustY(player->mo, thiscam->angle, player->mo->radius);
	if (player->mo->eflags & MFE_VERTICALFLIP)
		z = player->mo->z + player->mo->height - (32<<FRACBITS) - 16*FRACUNIT;
	else
		z = player->mo->z + (32<<FRACBITS);

	// set bits for the camera
	thiscam->x = x;
	thiscam->y = y;
	thiscam->z = z;
	thiscam->reset = true;

	if (!(thiscam == &camera[0] && (cv_cam_still.value || cv_analog.value))
		&& !(thiscam == &camera[1] && (cv_cam2_still.value || cv_analog2.value))
		&& !(thiscam == &camera[2] && (cv_cam3_still.value || cv_analog3.value))
		&& !(thiscam == &camera[3] && (cv_cam4_still.value || cv_analog4.value)))
	{
		thiscam->angle = player->mo->angle;
		thiscam->aiming = 0;
	}
	thiscam->relativex = 0;

	thiscam->subsector = R_PointInSubsector(thiscam->x,thiscam->y);

	thiscam->radius = 20*FRACUNIT;
	thiscam->height = 16*FRACUNIT;

	while (!P_MoveChaseCamera(player,thiscam,true) && ++tries < 2*TICRATE);
}

boolean P_MoveChaseCamera(player_t *player, camera_t *thiscam, boolean resetcalled)
{
	static boolean lookbackactive[MAXSPLITSCREENPLAYERS];
	static UINT8 lookbackdelay[MAXSPLITSCREENPLAYERS];
	UINT8 num;
	angle_t angle = 0, focusangle = 0, focusaiming = 0;
	fixed_t x, y, z, dist, viewpointx, viewpointy, camspeed, camdist, camheight, pviewheight;
	fixed_t pan, xpan, ypan;
	INT32 camrotate;
	boolean camstill, lookback, lookbackdown;
	UINT8 timeover;
	mobj_t *mo;
	fixed_t f1, f2;
#ifndef NOCLIPCAM
	boolean cameranoclip;
	subsector_t *newsubsec;
#endif

	democam.soundmobj = NULL;	// reset this each frame, we don't want the game crashing for stupid reasons now do we

	// We probably shouldn't move the camera if there is no player or player mobj somehow
	if (!player || !player->mo)
		return true;

	// This can happen when joining
	if (thiscam->subsector == NULL || thiscam->subsector->sector == NULL)
		return true;

	if (demo.freecam)
	{
		P_DemoCameraMovement(thiscam);
		return true;
	}

	mo = player->mo;

#ifndef NOCLIPCAM
	cameranoclip = ((player->pflags & (PF_NOCLIP|PF_NIGHTSMODE))
		|| (mo->flags & (MF_NOCLIP|MF_NOCLIPHEIGHT)) // Noclipping player camera noclips too!!
		|| (leveltime < introtime)); // Kart intro cam
#endif

	if (player->pflags & PF_TIMEOVER) // 1 for momentum keep, 2 for turnaround
		timeover = (player->kartstuff[k_timeovercam] > 2*TICRATE ? 2 : 1);
	else
		timeover = 0;

	if (!(player->playerstate == PST_DEAD || player->exiting))
	{
		if (player->spectator) // force cam off for spectators
			return true;

		if (!cv_chasecam.value && thiscam == &camera[0])
			return true;

		if (!cv_chasecam2.value && thiscam == &camera[1])
			return true;

		if (!cv_chasecam3.value && thiscam == &camera[2])
			return true;

		if (!cv_chasecam4.value && thiscam == &camera[3])
			return true;
	}

	if (!thiscam->chase && !resetcalled)
	{
		if (player == &players[consoleplayer])
			focusangle = localangle[0];
		else if (player == &players[displayplayers[1]])
			focusangle = localangle[1];
		else if (player == &players[displayplayers[2]])
			focusangle = localangle[2];
		else if (player == &players[displayplayers[3]])
			focusangle = localangle[3];
		else
			focusangle = mo->angle;

		if (thiscam == &camera[0])
			camrotate = cv_cam_rotate.value;
		else if (thiscam == &camera[1])
			camrotate = cv_cam2_rotate.value;
		else if (thiscam == &camera[2])
			camrotate = cv_cam3_rotate.value;
		else if (thiscam == &camera[3])
			camrotate = cv_cam4_rotate.value;
		else
			camrotate = 0;

		if (leveltime < introtime) // Whoooshy camera!
		{
			const INT32 introcam = (introtime - leveltime);
			camrotate += introcam*5;
		}

		thiscam->angle = focusangle + FixedAngle(camrotate*FRACUNIT);
		P_ResetCamera(player, thiscam);
		return true;
	}

	thiscam->radius = 20*mapobjectscale;
	thiscam->height = 16*mapobjectscale;

	// Don't run while respawning from a starpost
	// Inu 4/8/13 Why not?!
//	if (leveltime > 0 && timeinmap <= 0)
//		return true;

	if (demo.playback)
	{
		focusangle = mo->angle;
		focusaiming = 0;
	}
	else if (player == &players[consoleplayer])
	{
		focusangle = localangle[0];
		focusaiming = localaiming[0];
	}
	else if (player == &players[displayplayers[1]])
	{
		focusangle = localangle[1];
		focusaiming = localaiming[1];
	}
	else if (player == &players[displayplayers[2]])
	{
		focusangle = localangle[2];
		focusaiming = localaiming[2];
	}
	else if (player == &players[displayplayers[3]])
	{
		focusangle = localangle[3];
		focusaiming = localaiming[3];
	}
	else
	{
		focusangle = mo->angle;
		focusaiming = player->aiming;
	}

	if (P_CameraThinker(player, thiscam, resetcalled))
		return true;


	if (thiscam == &camera[1]) // Camera 2
	{
		num = 1;
		camspeed = cv_cam2_speed.value;
		camstill = cv_cam2_still.value;
		camrotate = cv_cam2_rotate.value;
		camdist = FixedMul(cv_cam2_dist.value, mapobjectscale);
		camheight = FixedMul(cv_cam2_height.value, mapobjectscale);
		lookback = camspin[1];
	}
	else if (thiscam == &camera[2]) // Camera 3
	{
		num = 2;
		camspeed = cv_cam3_speed.value;
		camstill = cv_cam3_still.value;
		camrotate = cv_cam3_rotate.value;
		camdist = FixedMul(cv_cam3_dist.value, mapobjectscale);
		camheight = FixedMul(cv_cam3_height.value, mapobjectscale);
		lookback = camspin[2];
	}
	else if (thiscam == &camera[3]) // Camera 4
	{
		num = 3;
		camspeed = cv_cam4_speed.value;
		camstill = cv_cam4_still.value;
		camrotate = cv_cam4_rotate.value;
		camdist = FixedMul(cv_cam4_dist.value, mapobjectscale);
		camheight = FixedMul(cv_cam4_height.value, mapobjectscale);
		lookback = camspin[3];
	}
	else // Camera 1
	{
		num = 0;
		camspeed = cv_cam_speed.value;
		camstill = cv_cam_still.value;
		camrotate = cv_cam_rotate.value;
		camdist = FixedMul(cv_cam_dist.value, mapobjectscale);
		camheight = FixedMul(cv_cam_height.value, mapobjectscale);
		lookback = camspin[0];
	}

	if (timeover)
	{
		const INT32 timeovercam = max(0, min(180, (player->kartstuff[k_timeovercam] - 2*TICRATE)*15));
		camrotate += timeovercam;
	}
	else if (leveltime < introtime && !(modeattacking && !demo.playback)) // Whoooshy camera! (don't do this in RA when we PLAY, still do it in replays however~)
	{
		const INT32 introcam = (introtime - leveltime);
		camrotate += introcam*5;
		camdist += (introcam * mapobjectscale)*3;
		camheight += (introcam * mapobjectscale)*2;
	}
	else if (player->exiting) // SRB2Kart: Leave the camera behind while exiting, for dramatic effect!
		camstill = true;
	else if (lookback || lookbackdelay[num]) // SRB2kart - Camera flipper
	{
#define MAXLOOKBACKDELAY 2
		camspeed = FRACUNIT;
		if (lookback)
		{
			camrotate += 180;
			lookbackdelay[num] = MAXLOOKBACKDELAY;
		}
		else
			lookbackdelay[num]--;
	}
	lookbackdown = (lookbackdelay[num] == MAXLOOKBACKDELAY) != lookbackactive[num];
	lookbackactive[num] = (lookbackdelay[num] == MAXLOOKBACKDELAY);
#undef MAXLOOKBACKDELAY

	if (mo->eflags & MFE_VERTICALFLIP)
		camheight += thiscam->height;

	if (camspeed > FRACUNIT)
		camspeed = FRACUNIT;

	if (timeover)
		angle = mo->angle + FixedAngle(camrotate*FRACUNIT);
	else if (leveltime < starttime)
		angle = focusangle + FixedAngle(camrotate*FRACUNIT);
	else if (camstill || resetcalled || player->playerstate == PST_DEAD)
		angle = thiscam->angle;
	else
	{
		if (camspeed == FRACUNIT)
			angle = focusangle + FixedAngle(camrotate<<FRACBITS);
		else
		{
			angle_t input = focusangle + FixedAngle(camrotate<<FRACBITS) - thiscam->angle;
			boolean invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			input = FixedAngle(FixedMul(AngleFixed(input), camspeed));
			if (invert)
				input = InvAngle(input);

			angle = thiscam->angle + input;
		}
	}

	if (!resetcalled && (leveltime > starttime && timeover != 2)
		&& ((thiscam == &camera[0] && t_cam_rotate != -42)
		|| (thiscam == &camera[1] && t_cam2_rotate != -42)
		|| (thiscam == &camera[2] && t_cam3_rotate != -42)
		|| (thiscam == &camera[3] && t_cam4_rotate != -42)))
	{
		angle = FixedAngle(camrotate*FRACUNIT);
		thiscam->angle = angle;
	}

	// sets ideal cam pos
	dist = camdist;

	if (player->speed > K_GetKartSpeed(player, false))
		dist += 4*(player->speed - K_GetKartSpeed(player, false));
	dist += abs(thiscam->momz)/4;

	if (player->kartstuff[k_boostcam])
		dist -= FixedMul(11*dist/16, player->kartstuff[k_boostcam]);

	x = mo->x - FixedMul(FINECOSINE((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);
	y = mo->y - FixedMul(FINESINE((angle>>ANGLETOFINESHIFT) & FINEMASK), dist);

	// SRB2Kart: set camera panning
	if (camstill || resetcalled || player->playerstate == PST_DEAD)
		pan = xpan = ypan = 0;
	else
	{
		if (player->kartstuff[k_drift] != 0)
		{
			fixed_t panmax = (dist/5);
			pan = FixedDiv(FixedMul(min((fixed_t)player->kartstuff[k_driftcharge], K_GetKartDriftSparkValue(player)), panmax), K_GetKartDriftSparkValue(player));
			if (pan > panmax)
				pan = panmax;
			if (player->kartstuff[k_drift] < 0)
				pan *= -1;
		}
		else
			pan = 0;

		pan = thiscam->pan + FixedMul(pan - thiscam->pan, camspeed/4);

		xpan = FixedMul(FINECOSINE(((angle+ANGLE_90)>>ANGLETOFINESHIFT) & FINEMASK), pan);
		ypan = FixedMul(FINESINE(((angle+ANGLE_90)>>ANGLETOFINESHIFT) & FINEMASK), pan);

		x += xpan;
		y += ypan;
	}

	pviewheight = FixedMul(32<<FRACBITS, mo->scale);

	if (mo->eflags & MFE_VERTICALFLIP)
		z = mo->z + mo->height - pviewheight - camheight;
	else
		z = mo->z + pviewheight + camheight;

#ifndef NOCLIPCAM // Disable all z-clipping for noclip cam
	// move camera down to move under lower ceilings
	newsubsec = R_IsPointInSubsector(((mo->x>>FRACBITS) + (thiscam->x>>FRACBITS))<<(FRACBITS-1), ((mo->y>>FRACBITS) + (thiscam->y>>FRACBITS))<<(FRACBITS-1));

	if (!newsubsec)
		newsubsec = thiscam->subsector;

	if (newsubsec)
	{
		fixed_t myfloorz, myceilingz;
		fixed_t midz = thiscam->z + (thiscam->z - mo->z)/2;
		fixed_t midx = ((mo->x>>FRACBITS) + (thiscam->x>>FRACBITS))<<(FRACBITS-1);
		fixed_t midy = ((mo->y>>FRACBITS) + (thiscam->y>>FRACBITS))<<(FRACBITS-1);

		// Cameras use the heightsec's heights rather then the actual sector heights.
		// If you can see through it, why not move the camera through it too?
		if (newsubsec->sector->camsec >= 0)
		{
			myfloorz = sectors[newsubsec->sector->camsec].floorheight;
			myceilingz = sectors[newsubsec->sector->camsec].ceilingheight;
		}
		else if (newsubsec->sector->heightsec >= 0)
		{
			myfloorz = sectors[newsubsec->sector->heightsec].floorheight;
			myceilingz = sectors[newsubsec->sector->heightsec].ceilingheight;
		}
		else
		{
			myfloorz = P_CameraGetFloorZ(thiscam, newsubsec->sector, midx, midy, NULL);
			myceilingz = P_CameraGetCeilingZ(thiscam, newsubsec->sector, midx, midy, NULL);
		}

		// Check list of fake floors and see if floorz/ceilingz need to be altered.
		if (newsubsec->sector->ffloors)
		{
			ffloor_t *rover;
			fixed_t delta1, delta2;
			INT32 thingtop = midz + thiscam->height;

			for (rover = newsubsec->sector->ffloors; rover; rover = rover->next)
			{
				fixed_t topheight, bottomheight;
				if (!(rover->flags & FF_BLOCKOTHERS) || !(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERALL) || GETSECSPECIAL(rover->master->frontsector->special, 4) == 12)
					continue;

				topheight = P_CameraGetFOFTopZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);
				bottomheight = P_CameraGetFOFBottomZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);

				delta1 = midz - (bottomheight
					+ ((topheight - bottomheight)/2));
				delta2 = thingtop - (bottomheight
					+ ((topheight - bottomheight)/2));
				if (topheight > myfloorz && abs(delta1) < abs(delta2))
					myfloorz = topheight;
				if (bottomheight < myceilingz && abs(delta1) >= abs(delta2))
					myceilingz = bottomheight;
			}
		}

	// Check polyobjects and see if floorz/ceilingz need to be altered
	{
		INT32 xl, xh, yl, yh, bx, by;
		validcount++;

		xl = (unsigned)(tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
		xh = (unsigned)(tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
		yl = (unsigned)(tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
		yh = (unsigned)(tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

		BMBOUNDFIX(xl, xh, yl, yh);

		for (by = yl; by <= yh; by++)
			for (bx = xl; bx <= xh; bx++)
			{
				INT32 offset;
				polymaplink_t *plink; // haleyjd 02/22/06

				if (bx < 0 || by < 0 || bx >= bmapwidth || by >= bmapheight)
					continue;

				offset = by*bmapwidth + bx;

				// haleyjd 02/22/06: consider polyobject lines
				plink = polyblocklinks[offset];

				while (plink)
				{
					polyobj_t *po = plink->po;

					if (po->validcount != validcount) // if polyobj hasn't been checked
					{
						sector_t *polysec;
						fixed_t delta1, delta2, thingtop;
						fixed_t polytop, polybottom;

						po->validcount = validcount;

						if (!P_PointInsidePolyobj(po, x, y) || !(po->flags & POF_SOLID))
						{
							plink = (polymaplink_t *)(plink->link.next);
							continue;
						}

						// We're inside it! Yess...
						polysec = po->lines[0]->backsector;

						if (GETSECSPECIAL(polysec->special, 4) == 12)
						{ // Camera noclip polyobj.
							plink = (polymaplink_t *)(plink->link.next);
							continue;
						}

						if (po->flags & POF_CLIPPLANES)
						{
							polytop = polysec->ceilingheight;
							polybottom = polysec->floorheight;
						}
						else
						{
							polytop = INT32_MAX;
							polybottom = INT32_MIN;
						}

						thingtop = midz + thiscam->height;
						delta1 = midz - (polybottom + ((polytop - polybottom)/2));
						delta2 = thingtop - (polybottom + ((polytop - polybottom)/2));

						if (polytop > myfloorz && abs(delta1) < abs(delta2))
							myfloorz = polytop;

						if (polybottom < myceilingz && abs(delta1) >= abs(delta2))
							myceilingz = polybottom;
					}
					plink = (polymaplink_t *)(plink->link.next);
				}
			}
	}

		// crushed camera
		if (myceilingz <= myfloorz + thiscam->height && !resetcalled && !cameranoclip)
		{
			P_ResetCamera(player, thiscam);
			return true;
		}

		// camera fit?
		if (myceilingz != myfloorz
			&& myceilingz - thiscam->height < z)
		{
			z = myceilingz - thiscam->height-FixedMul(11*FRACUNIT, mo->scale);
			// is the camera fit is there own sector
		}

		// Make the camera a tad smarter with 3d floors
		if (newsubsec->sector->ffloors && !cameranoclip)
		{
			ffloor_t *rover;

			for (rover = newsubsec->sector->ffloors; rover; rover = rover->next)
			{
				fixed_t topheight, bottomheight;
				if ((rover->flags & FF_BLOCKOTHERS) && (rover->flags & FF_RENDERALL) && (rover->flags & FF_EXISTS) && GETSECSPECIAL(rover->master->frontsector->special, 4) == 12)
				{
					topheight = P_CameraGetFOFTopZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);
					bottomheight = P_CameraGetFOFBottomZ(thiscam, newsubsec->sector, rover, midx, midy, NULL);

					if (bottomheight - thiscam->height < z
						&& midz < bottomheight)
						z = bottomheight - thiscam->height-FixedMul(11*FRACUNIT, mo->scale);

					else if (topheight + thiscam->height > z
						&& midz > topheight)
						z = topheight;

					if ((mo->z >= topheight && midz < bottomheight)
						|| ((mo->z < bottomheight && mo->z+mo->height < topheight) && midz >= topheight))
					{
						// Can't see
						if (!resetcalled)
							P_ResetCamera(player, thiscam);
						return true;
					}
				}
			}
		}
	}

	if (thiscam->z < thiscam->floorz && !cameranoclip)
		thiscam->z = thiscam->floorz;
#endif // NOCLIPCAM

	// point viewed by the camera
	// this point is just 64 unit forward the player
	dist = 64*mapobjectscale;
	viewpointx = mo->x + FixedMul(FINECOSINE((angle>>ANGLETOFINESHIFT) & FINEMASK), dist) + xpan;
	viewpointy = mo->y + FixedMul(FINESINE((angle>>ANGLETOFINESHIFT) & FINEMASK), dist) + ypan;

	if (timeover)
		thiscam->angle = angle;
	else if (!camstill && !resetcalled && !paused && timeover != 1)
		thiscam->angle = R_PointToAngle2(thiscam->x, thiscam->y, viewpointx, viewpointy);

	if (timeover == 1)
	{
		thiscam->momx = P_ReturnThrustX(NULL, mo->angle, 32*mo->scale); // Push forward
		thiscam->momy = P_ReturnThrustY(NULL, mo->angle, 32*mo->scale);
		thiscam->momz = 0;
	}
	else if (player->exiting || timeover == 2)
		thiscam->momx = thiscam->momy = thiscam->momz = 0;
	else if (leveltime < starttime)
	{
		thiscam->momx = FixedMul(x - thiscam->x, camspeed);
		thiscam->momy = FixedMul(y - thiscam->y, camspeed);
		thiscam->momz = FixedMul(z - thiscam->z, camspeed);
	}
	else
	{
		thiscam->momx = x - thiscam->x;
		thiscam->momy = y - thiscam->y;
		thiscam->momz = FixedMul(z - thiscam->z, camspeed/2);
	}

	thiscam->pan = pan;

	// compute aming to look the viewed point
	f1 = viewpointx-thiscam->x;
	f2 = viewpointy-thiscam->y;
	dist = FixedHypot(f1, f2);

	if (mo->eflags & MFE_VERTICALFLIP)
		angle = R_PointToAngle2(0, thiscam->z + thiscam->height, dist, mo->z + mo->height - P_GetPlayerHeight(player));
	else
		angle = R_PointToAngle2(0, thiscam->z, dist, mo->z + P_GetPlayerHeight(player));

	if (player->playerstate != PST_DEAD && !((player->pflags & PF_NIGHTSMODE) && player->exiting))
		angle += (focusaiming < ANGLE_180 ? focusaiming/2 : InvAngle(InvAngle(focusaiming)/2)); // overcomplicated version of '((signed)focusaiming)/2;'

	if (twodlevel || (mo->flags2 & MF2_TWOD) || (!camstill && !timeover)) // Keep the view still...
	{
		G_ClipAimingPitch((INT32 *)&angle);

		if (camspeed == FRACUNIT)
			thiscam->aiming = angle;
		else
		{
			angle_t input;
			boolean invert;

			input = thiscam->aiming - angle;
			invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			input = FixedAngle(FixedMul(AngleFixed(input), (5*camspeed)/16));
			if (invert)
				input = InvAngle(input);

			thiscam->aiming -= input;
		}
	}

	if (!resetcalled && (player->playerstate == PST_DEAD || player->playerstate == PST_REBORN))
	{
		// Don't let the camera match your movement.
		thiscam->momz = 0;
		if (player->spectator)
			thiscam->aiming = 0;
		// Only let the camera go a little bit downwards.
		else if (!(mo->eflags & MFE_VERTICALFLIP) && thiscam->aiming < ANGLE_337h && thiscam->aiming > ANGLE_180)
			thiscam->aiming = ANGLE_337h;
		else if (mo->eflags & MFE_VERTICALFLIP && thiscam->aiming > ANGLE_22h && thiscam->aiming < ANGLE_180)
			thiscam->aiming = ANGLE_22h;
	}

	if (lookbackdown)
	{
		P_MoveChaseCamera(player, thiscam, false);
		R_ResetViewInterpolation(num + 1);
		R_ResetViewInterpolation(num + 1);
	}

	return (x == thiscam->x && y == thiscam->y && z == thiscam->z && angle == thiscam->aiming);
}

boolean P_SpectatorJoinGame(player_t *player)
{
	// Team changing isn't allowed.
	if (!cv_allowteamchange.value)
	{
		if (P_IsLocalPlayer(player))
			CONS_Printf(M_GetText("Server does not allow team change.\n"));
		//player->powers[pw_flashing] = TICRATE + 1; //to prevent message spam.
	}
	// Team changing in Team Match and CTF
	// Pressing fire assigns you to a team that needs players if allowed.
	// Partial code reproduction from p_tick.c autobalance code.
	else if (G_GametypeHasTeams())
	{		
		INT32 changeto = 0;
		INT32 z, numplayersred = 0, numplayersblue = 0;

		//find a team by num players, score, or random if all else fails.
		for (z = 0; z < MAXPLAYERS; ++z)
			if (playeringame[z])
			{
				if (players[z].ctfteam == 1)
					++numplayersred;
				else if (players[z].ctfteam == 2)
					++numplayersblue;
			}
		// for z

		if (numplayersblue > numplayersred)
			changeto = 1;
		else if (numplayersred > numplayersblue)
			changeto = 2;
		else if (bluescore > redscore)
			changeto = 1;
		else if (redscore > bluescore)
			changeto = 2;
		else
			changeto = (P_RandomFixed() & 1) + 1;

		if (player->mo)
		{
			P_RemoveMobj(player->mo);
			player->mo = NULL;
		}
		player->spectator = false;
		player->pflags &= ~PF_WANTSTOJOIN;
		player->kartstuff[k_spectatewait] = 0;
		player->ctfteam = changeto;
		player->playerstate = PST_REBORN;
		
		//center camera if its not already
		if ((P_IsLocalPlayer(player)) && localaiming[0] != 0)
			localaiming[0] = 0;

		//Reset away view
		if (P_IsLocalPlayer(player) && displayplayers[0] != consoleplayer)
			displayplayers[0] = consoleplayer;

		if (changeto == 1)
			CONS_Printf(M_GetText("%s switched to the %c%s%c.\n"), player_names[player-players], '\x85', M_GetText("Red team"), '\x80');
		else if (changeto == 2)
			CONS_Printf(M_GetText("%s switched to the %c%s%c.\n"), player_names[player-players], '\x84', M_GetText("Blue team"), '\x80');

		return true; // no more player->mo, cannot continue.
	}
	// Joining in game from firing.
	else
	{		
		if (player->mo)
		{
			P_RemoveMobj(player->mo);
			player->mo = NULL;
		}
		player->spectator = false;
		player->pflags &= ~PF_WANTSTOJOIN;
		player->kartstuff[k_spectatewait] = 0;
		player->playerstate = PST_REBORN;

		//center camera if its not already
		if ((P_IsLocalPlayer(player)) && localaiming[0] != 0)
			localaiming[0] = 0;

		//Reset away view
		if (P_IsLocalPlayer(player) && displayplayers[0] != consoleplayer)
			displayplayers[0] = consoleplayer;

		HU_AddChatText(va(M_GetText("\x82*%s entered the game."), player_names[player-players]), false);
		return true; // no more player->mo, cannot continue.
	}
	return false;
}

static void P_CalcPostImg(player_t *player)
{
	sector_t *sector = player->mo->subsector->sector;
	postimg_t *type = NULL;
	INT32 *param;
	fixed_t pviewheight;
	UINT8 i;

	if (player->mo->eflags & MFE_VERTICALFLIP)
		pviewheight = player->mo->z + player->mo->height - player->viewheight;
	else
		pviewheight = player->mo->z + player->viewheight;

	if (player->awayviewtics && player->awayviewmobj && !P_MobjWasRemoved(player->awayviewmobj))
	{
		sector = player->awayviewmobj->subsector->sector;
		pviewheight = player->awayviewmobj->z + 20*FRACUNIT;
	}

	for (i = 0; i <= splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			type = &postimgtype[i];
			param = &postimgparam[i];
			break;
		}
	}

	// see if we are in heat (no, not THAT kind of heat...)

	if (P_FindSpecialLineFromTag(13, sector->tag, -1) != -1)
		*type = postimg_heat;
	else if (sector->ffloors)
	{
		ffloor_t *rover;
		fixed_t topheight;
		fixed_t bottomheight;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS))
				continue;

			topheight = *rover->t_slope ? P_GetZAt(*rover->t_slope, player->mo->x, player->mo->y) : *rover->topheight;
			bottomheight = *rover->b_slope ? P_GetZAt(*rover->b_slope, player->mo->x, player->mo->y) : *rover->bottomheight;

			if (pviewheight >= topheight || pviewheight <= bottomheight)
				continue;

			if (P_FindSpecialLineFromTag(13, rover->master->frontsector->tag, -1) != -1)
				*type = postimg_heat;
		}
	}

	// see if we are in water (water trumps heat)
	if (sector->ffloors)
	{
		ffloor_t *rover;
		fixed_t topheight;
		fixed_t bottomheight;

		for (rover = sector->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS) || !(rover->flags & FF_SWIMMABLE) || rover->flags & FF_BLOCKPLAYER)
				continue;

			topheight = *rover->t_slope ? P_GetZAt(*rover->t_slope, player->mo->x, player->mo->y) : *rover->topheight;
			bottomheight = *rover->b_slope ? P_GetZAt(*rover->b_slope, player->mo->x, player->mo->y) : *rover->bottomheight;

			if (pviewheight >= topheight || pviewheight <= bottomheight)
				continue;

			*type = postimg_water;
		}
	}

	if (player->mo->eflags & MFE_VERTICALFLIP)
		*type = postimg_flip;

#if 1
	(void)param;
#else
	// Motion blur
	if (player->speed > (35<<FRACBITS))
	{
		*type = postimg_motion;
		*param = (player->speed - 32)/4;

		if (*param > 5)
			*param = 5;
	}
#endif

	if (encoremode) // srb2kart
		*type = postimg_mirror;
}

void P_DoTimeOver(player_t *player)
{
	if (netgame && player->health > 0)
		CON_LogMessage(va(M_GetText("%s ran out of time.\n"), player_names[player-players]));

	player->pflags |= PF_TIMEOVER;

	if (P_IsLocalPlayer(player) && !demo.playback)
		legitimateexit = true; // SRB2kart: losing a race is still seeing it through to the end :p

	if (player->mo)
	{
		S_StopSound(player->mo);
		P_DamageMobj(player->mo, NULL, NULL, 10000);
	}

	player->lives = 0;

	P_EndingMusic(player);

	if (!exitcountdown)
		exitcountdown = 5*TICRATE;
}

	/* gaysed script from me, based on Golden's sprite slope roll */

// holy SHIT
static INT32
Quaketilt (player_t *player)
{
	angle_t tilt;
	fixed_t lowb; // this threshold for speed
	angle_t moma = R_PointToAngle2(0, 0, player->mo->momx, player->mo->momy);
	INT32 delta = (INT32)( player->mo->angle - moma );
	fixed_t speed;

	boolean sliptiding =
		(
				player->kartstuff[k_aizdriftstrat] != 0 &&
				player->kartstuff[k_drift]         == 0
		);

	if (delta == (INT32)ANGLE_180)/* FUCK YOU HAVE A HACK */
	{
		return 0;
	}

	// Hi! I'm "not a math guy"!
	if (abs(delta) > ANGLE_90)
		delta = (INT32)(( moma + ANGLE_180 ) - player->mo->angle );
	if (P_IsObjectOnGround(player->mo))
	{
		if (sliptiding)
		{
			tilt = ANGLE_45;
			lowb = 5*FRACUNIT;
		}
		else
		{
			tilt = ANGLE_11hh/2;
			lowb = 15*FRACUNIT;
		}
	}
	else
	{
		tilt = ANGLE_22h;
		lowb = 10*FRACUNIT;
	}
	moma = FixedMul(FixedDiv(delta, ANGLE_90), tilt);
	speed = abs( player->mo->momx + player->mo->momy );
	if (speed < lowb)
	{
		// ease out tilt as we slow...
		moma = FixedMul(moma, FixedDiv(speed, lowb));
	}
	return moma;
}

static void
DoABarrelRoll (player_t *player)
{
	angle_t slope;
	angle_t delta;

	if (player->mo->standingslope)
	{
		slope = player->mo->standingslope->real_zangle;
	}
	else
	{
		slope = 0;
	}

	if (abs((INT32)slope) > ANGLE_11hh)
	{
		delta = ( player->mo->angle - player->mo->standingslope->real_xydirection );
		slope = -(FixedMul(FINESINE (delta>>ANGLETOFINESHIFT), slope));
	}
	else
	{
		slope = 0;
	}

	if (cv_quaketilt.value)
	{
		slope -= Quaketilt(player);
	}

	delta = (INT32)( slope - player->tilt )/ cv_tiltsmoothing.value;

	if (delta)
		player->tilt += delta;
	else
		player->tilt  = slope;

	if (cv_tilting.value)
	{
		player->viewrollangle = player->tilt;

		if (cv_actionmovie.value)
		{
			player->viewrollangle += quake.roll;
		}
	}
	else
	{
		player->viewrollangle = 0;
	}
}

//
// P_PlayerThink
//

void P_PlayerThink(player_t *player)
{
	ticcmd_t *cmd;
	const size_t playeri = (size_t)(player - players);

	player->lerp.aiming = player->aiming;
	player->lerp.awayviewaiming = player->awayviewaiming;
	player->lerp.frameangle = player->frameangle;
	player->lerp.viewrollangle = player->viewrollangle;

#ifdef PARANOIA
	if (!player->mo)
		I_Error("p_playerthink: players[%s].mo == NULL", sizeu1(playeri));
#endif

	// todo: Figure out what is actually causing these problems in the first place...
	if ((player->health <= 0 || player->mo->health <= 0) && player->playerstate == PST_LIVE) //you should be DEAD!
	{
		CONS_Debug(DBG_GAMELOGIC, "P_PlayerThink: Player %s in PST_LIVE with 0 health. (\"Zombie bug\")\n", sizeu1(playeri));
		player->playerstate = PST_DEAD;
	}

	if (player->bot)
	{
		if (player->playerstate == PST_LIVE || player->playerstate == PST_DEAD)
		{
			if (B_CheckRespawn(player))
				player->playerstate = PST_REBORN;
		}
		if (player->playerstate == PST_REBORN)
		{
#ifdef HAVE_BLUA
			LUAh_PlayerThink(player);
#endif
			return;
		}
	}

#ifdef SEENAMES
	if (netgame && player == &players[displayplayers[0]] && !(leveltime % (TICRATE/5)) && !splitscreen)
	{
		seenplayer = NULL;

		if (cv_seenames.value && cv_allowseenames.value &&
			!(G_TagGametype() && (player->pflags & PF_TAGIT)))
		{
			mobj_t *mo = P_SpawnNameFinder(player->mo, MT_NAMECHECK);

			if (mo)
			{
				short int i;
				mo->flags |= MF_NOCLIPHEIGHT;
				for (i = 0; i < 32; i++)
				{
					// Debug drawing
//					if (i&1)
//						P_SpawnMobj(mo->x, mo->y, mo->z, MT_SPARK);
					if (P_RailThinker(mo))
						break; // mobj was removed (missile hit a wall) or couldn't move
				}
			}
		}
	}
#endif

	if (player->awayviewmobj && P_MobjWasRemoved(player->awayviewmobj))
	{
		P_SetTarget(&player->awayviewmobj, NULL); // remove awayviewmobj asap if invalid
		player->awayviewtics = 0; // reset to zero
	}

	if (player->flashcount)
		player->flashcount--;

	// Re-fixed by Jimita (11-12-2018)
	if (player->awayviewtics)
	{
		player->awayviewtics--;
		if (!player->awayviewtics)
			player->awayviewtics = -1;
		// The timer might've reached zero, but we'll run the remote view camera anyway by setting it to -1.
	}

	/// \note do this in the cheat code
	if (player->pflags & PF_NOCLIP)
		player->mo->flags |= MF_NOCLIP;
	else
		player->mo->flags &= ~MF_NOCLIP;

	cmd = &player->cmd;

	//@TODO This fixes a one-tic latency on direction handling, AND makes behavior consistent while paused, but is not BC with 1.0.4. Do this for 1.1!
#if 0
	// SRB2kart
	// Save the dir the player is holding
	//  to allow items to be thrown forward or backward.
	if (cmd->buttons & BT_FORWARD)
		player->kartstuff[k_throwdir] = 1;
	else if (cmd->buttons & BT_BACKWARD)
		player->kartstuff[k_throwdir] = -1;
	else
		player->kartstuff[k_throwdir] = 0;
#endif

	// Add some extra randomization.
	if (cmd->forwardmove)
		P_RandomFixed();

#ifdef PARANOIA
	if (player->playerstate == PST_REBORN)
		I_Error("player %s is in PST_REBORN\n", sizeu1(playeri));
#endif

	if (!mapreset)
	{
		if (G_RaceGametype())
		{
			INT32 i;

			// Check if all the players in the race have finished. If so, end the level.
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (playeringame[i] && !players[i].spectator)
				{
					if (!players[i].exiting && players[i].lives > 0)
						break;
				}
			}

			if (i == MAXPLAYERS && player->exiting == raceexittime+2) // finished
				player->exiting = raceexittime+1;

			// If you've hit the countdown and you haven't made
			//  it to the exit, you're a goner!
			else if (racecountdown == 1 && !player->exiting && !player->spectator && player->lives > 0)
			{
				P_DoTimeOver(player);

				if (player->playerstate == PST_DEAD)
				{
#ifdef HAVE_BLUA
					LUAh_PlayerThink(player);
#endif
					return;
				}
			}
		}

		// If it is set, start subtracting
		// Don't allow it to go back to 0
		if (player->exiting > 1 && (player->exiting < raceexittime+2 || !G_RaceGametype())) // SRB2kart - "&& player->exiting > 1"
			player->exiting--;

		if (player->exiting && exitcountdown)
			player->exiting = 99; // SRB2kart

		if (player->exiting == 2 || exitcountdown == 2)
		{
			if (cv_playersforexit.value) // Count to be sure everyone's exited
			{
				INT32 i;

				for (i = 0; i < MAXPLAYERS; i++)
				{
					if (!playeringame[i] || players[i].spectator || players[i].bot)
						continue;
					if (players[i].lives <= 0)
						continue;

					if (!players[i].exiting || players[i].exiting > 3)
						break;
				}

				if (i == MAXPLAYERS)
				{
					if (server)
						SendNetXCmd(XD_EXITLEVEL, NULL, 0);
				}
				else
					player->exiting = 3;
			}
			else
			{
				if (server)
					SendNetXCmd(XD_EXITLEVEL, NULL, 0);
			}
		}
	}

	// check water content, set stuff in mobj
	P_MobjCheckWater(player->mo);

#ifndef SECTORSPECIALSAFTERTHINK
	if (player->onconveyor != 1 || !P_IsObjectOnGround(player->mo))
	player->onconveyor = 0;
	// check special sectors : damage & secrets

	if (!player->spectator)
		P_PlayerInSpecialSector(player);
#endif

	if (player->playerstate == PST_DEAD)
	{
		if (player->spectator)
			player->mo->flags2 |= MF2_SHADOW;
		else
			player->mo->flags2 &= ~MF2_SHADOW;
		P_DeathThink(player);
#ifdef HAVE_BLUA
		LUAh_PlayerThink(player);
#endif
		return;
	}

	// Make sure spectators always have a score and ring count of 0.
	if (player->spectator)
	{
		//player->score = 0;
		player->mo->health = 1;
		player->health = 1;
	}

	player->lives = 1; // SRB2Kart

	// SRB2kart 010217
	if (leveltime < starttime)
		player->powers[pw_nocontrol] = 2;

	// Synchronizes the "real" amount of time spent in the level.
	if (!player->exiting)
	{
		if (leveltime >= starttime)
		{
			player->realtime = leveltime - starttime;
			if (player == &players[consoleplayer])
			{
				if (player->spectator || !circuitmap)
					curlap = 0;
				else
					curlap++; // This is too complicated to sync to realtime, just sorta hope for the best :V
			}
		}
		else
		{
			player->realtime = 0;
			if (player == &players[consoleplayer])
				curlap = 0;
		}
	}

	if (netgame && cv_antigrief.value != 0 && G_RaceGametype())
	{
		INT32 i;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator)
				continue;
			if (&players[i] == player)
				continue;
			break;
		}
		
		if (i < MAXPLAYERS && !player->spectator && !player->exiting && !(player->pflags & PF_TIMEOVER))
		{
			const tic_t griefval = cv_antigrief.value * TICRATE;
			const UINT8 n = player - players;

			{
				if (player->grieftime > griefval)
				{
					player->griefstrikes++;
					player->grieftime = 0;

					if (server)
					{
						if ((player->griefstrikes > 2)
#ifndef DEVELOP
							&& !IsPlayerAdmin(n)
#endif
							&& !P_IsLocalPlayer(player)) // P_IsMachineLocalPlayer for DRRR
						{
							// Send kick
							UINT8 buf[2];

							buf[0] = n;
							buf[1] = KICK_MSG_GRIEF;
							SendNetXCmd(XD_KICK, &buf, 2);
						}
						else
						{
							// Send spectate
							changeteam_union NetPacket;
							UINT16 usvalue;

							NetPacket.value.l = NetPacket.value.b = 0;
							NetPacket.packet.newteam = 0;
							NetPacket.packet.playernum = n;
							NetPacket.packet.verification = true;

							usvalue = SHORT(NetPacket.value.l|NetPacket.value.b);
							SendNetXCmd(XD_TEAMCHANGE, &usvalue, sizeof(usvalue));
						}
					}
				}
				else if (player->powers[pw_flashing] == 0)
				{
					player->grieftime++;
				}
			}
		}
	}

	if ((netgame || multiplayer) && player->spectator && cmd->buttons & BT_ATTACK && !player->powers[pw_flashing])
	{
		player->pflags ^= PF_WANTSTOJOIN;
		player->powers[pw_flashing] = TICRATE/2 + 1;
		/*if (P_SpectatorJoinGame(player))
			return; // player->mo was removed.*/
	}

	// Even if not NiGHTS, pull in nearby objects when walking around as John Q. Elliot.
	if (!objectplacing && !((netgame || multiplayer) && player->spectator)
	&& maptol & TOL_NIGHTS && (!(player->pflags & PF_NIGHTSMODE) || player->powers[pw_nights_helper]))
	{
		thinker_t *th;
		mobj_t *mo2;
		fixed_t x = player->mo->x;
		fixed_t y = player->mo->y;
		fixed_t z = player->mo->z;

		for (th = thinkercap.next; th != &thinkercap; th = th->next)
		{
			if (th->function.acp1 != (actionf_p1)P_MobjThinker)
				continue;

			mo2 = (mobj_t *)th;

			if (!(mo2->type == MT_NIGHTSWING || mo2->type == MT_RING || mo2->type == MT_COIN
			   || mo2->type == MT_BLUEBALL))
				continue;

			if (P_AproxDistance(P_AproxDistance(mo2->x - x, mo2->y - y), mo2->z - z) > FixedMul(128*FRACUNIT, player->mo->scale))
				continue;

			// Yay! The thing's in reach! Pull it in!
			mo2->flags |= MF_NOCLIP|MF_NOCLIPHEIGHT;
			mo2->flags2 |= MF2_NIGHTSPULL;
			P_SetTarget(&mo2->tracer, player->mo);
		}
	}

	if (player->linktimer && !player->powers[pw_nights_linkfreeze])
	{
		if (--player->linktimer <= 0) // Link timer
			player->linkcount = 0;
	}

	// Move around.
	// Reactiontime is used to prevent movement
	//  for a bit after a teleport.
	if (player->mo->reactiontime)
		player->mo->reactiontime--;
	else if (player->mo->tracer && player->mo->tracer->type == MT_TUBEWAYPOINT)
	{
		{
			P_DoZoomTube(player);
		}
		player->rmomx = player->rmomy = 0; // no actual momentum from your controls
		P_ResetScore(player);
	}
	else
		P_MovePlayer(player);

	if (!player->mo)
	{
#ifdef HAVE_BLUA
		LUAh_PlayerThink(player);
#endif
		return; // P_MovePlayer removed player->mo.
	}

	// Unset statis flags after moving.
	// In other words, if you manually set stasis via code,
	// it lasts for one tic.
	player->pflags &= ~PF_FULLSTASIS;

	if (player->onconveyor == 1)
			player->cmomy = player->cmomx = 0;

	//P_DoSuperStuff(player);
	//P_CheckSneakerAndLivesTimer(player);
	P_DoBubbleBreath(player); // Spawn Sonic's bubbles
	//P_CheckUnderwaterAndSpaceTimer(player); // Display the countdown drown numbers!
	P_CheckInvincibilityTimer(player); // Spawn Invincibility Sparkles
	P_DoPlayerHeadSigns(player); // Spawn Tag/CTF signs over player's head

#if 1
	// "Blur" a bit when you have speed shoes and are going fast enough
	if ((player->powers[pw_super] || player->powers[pw_sneakers]
		|| player->kartstuff[k_driftboost] || player->kartstuff[k_sneakertimer] || player->kartstuff[k_startboost]) && !player->kartstuff[k_invincibilitytimer] // SRB2kart
		&& (player->speed + abs(player->mo->momz)) > FixedMul(20*FRACUNIT,player->mo->scale))
	{
		UINT8 i;
		mobj_t *gmobj = P_SpawnGhostMobj(player->mo);

		gmobj->fuse = 2;
		if (leveltime & 1)
		{
			gmobj->frame &= ~FF_TRANSMASK;
			gmobj->frame |= tr_trans70<<FF_TRANSSHIFT;
		}

		// Hide the mobj from our sights if we're the displayplayer and chasecam is off.
		// Why not just not spawn the mobj?  Well, I'd rather only flirt with
		// consistency so much...
		for (i = 0; i <= splitscreen; i++)
		{
			if (player == &players[displayplayers[i]] && !camera[i].chase)
			{
				gmobj->flags2 |= MF2_DONTDRAW;
				break;
			}
		}
	}
#endif

	// check for use
	if (!(player->pflags & PF_NIGHTSMODE))
	{
		if (cmd->buttons & BT_BRAKE)
			player->pflags |= PF_USEDOWN;
		else
			player->pflags &= ~PF_USEDOWN;
	}
	else if (player->mo->tracer) // match tracer's position with yours when NiGHTS
	{
		P_UnsetThingPosition(player->mo->tracer);
		player->mo->tracer->x = player->mo->x;
		player->mo->tracer->y = player->mo->y;
		if (player->mo->eflags & MFE_VERTICALFLIP)
			player->mo->tracer->z = player->mo->z + player->mo->height - player->mo->tracer->height;
		else
			player->mo->tracer->z = player->mo->z;
		player->mo->tracer->floorz = player->mo->floorz;
		player->mo->tracer->ceilingz = player->mo->ceilingz;
		P_SetThingPosition(player->mo->tracer);
	}

	// Counters, time dependent power ups.
	// Time Bonus & Ring Bonus count settings

	// Strength counts up to diminish fade.
	if (player->powers[pw_sneakers] && player->powers[pw_sneakers] < UINT16_MAX)
		player->powers[pw_sneakers]--;

	if (player->powers[pw_invulnerability] && player->powers[pw_invulnerability] < UINT16_MAX)
		player->powers[pw_invulnerability]--;

	if (player->powers[pw_flashing] && player->powers[pw_flashing] < UINT16_MAX &&
		(player->spectator || player->powers[pw_flashing] < K_GetKartFlashing(player)))
		player->powers[pw_flashing]--;

	if (player->powers[pw_tailsfly] && player->powers[pw_tailsfly] < UINT16_MAX /*&& player->charability != CA_SWIM*/ && !(player->powers[pw_super] && ALL7EMERALDS(player->powers[pw_emeralds]))) // tails fly counter
		player->powers[pw_tailsfly]--;

	if (player->powers[pw_gravityboots] && player->powers[pw_gravityboots] < UINT16_MAX)
		player->powers[pw_gravityboots]--;

	if (player->powers[pw_extralife] && player->powers[pw_extralife] < UINT16_MAX)
		player->powers[pw_extralife]--;

	if (player->powers[pw_nights_linkfreeze] && player->powers[pw_nights_linkfreeze] < UINT16_MAX)
		player->powers[pw_nights_linkfreeze]--;

	if (player->powers[pw_nights_superloop] && player->powers[pw_nights_superloop] < UINT16_MAX)
		player->powers[pw_nights_superloop]--;

	if (player->powers[pw_nights_helper] && player->powers[pw_nights_helper] < UINT16_MAX)
		player->powers[pw_nights_helper]--;

	if (player->powers[pw_nocontrol] & ((1<<15)-1) && player->powers[pw_nocontrol] < UINT16_MAX)
	{
		if (!(--player->powers[pw_nocontrol]))
			player->pflags &= ~PF_SKIDDOWN;
	}
	else
		player->powers[pw_nocontrol] = 0;

	//pw_super acts as a timer now
	if (player->powers[pw_super])
		player->powers[pw_super]++;

	if (player->powers[pw_ingoop])
	{
		if (player->mo->state == &states[S_KART_STND1]) // SRB2kart - was S_PLAY_STND
			player->mo->tics = 2;

		player->powers[pw_ingoop]--;
	}

	if (player->bumpertime)
		player->bumpertime--;

	if (player->skidtime)
		player->skidtime--;

	if (player->weapondelay)
		player->weapondelay--;

	if (player->tossdelay)
		player->tossdelay--;

	if (player->homing)
		player->homing--;

	if (player->texttimer)
	{
		--player->texttimer;
		if (!player->texttimer && !player->exiting && player->textvar >= 4)
		{
			player->texttimer = 4*TICRATE;
			player->textvar = 2; // GET n RINGS!

			if (player->capsule && player->capsule->health != player->capsule->spawnpoint->angle)
				player->textvar++; // GET n MORE RINGS!
		}
	}

	if (player->losstime && !player->powers[pw_flashing])
		player->losstime--;

	// Flash player after being hit.
	if (!(//player->pflags & PF_NIGHTSMODE ||
		player->kartstuff[k_hyudorotimer] // SRB2kart - fixes Hyudoro not flashing when it should.
		|| player->kartstuff[k_growshrinktimer] > 0 // Grow doesn't flash either.
		|| player->kartstuff[k_respawn] // Respawn timer (for drop dash effect)
		|| (player->pflags & PF_TIMEOVER) // NO CONTEST explosion
		|| (G_BattleGametype() && player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer])
		|| leveltime < starttime)) // Level intro
	{
		if (player->powers[pw_flashing] > 0 && player->powers[pw_flashing] < K_GetKartFlashing(player)
			&& (leveltime & 1))
			player->mo->flags2 |= MF2_DONTDRAW;
		else
			player->mo->flags2 &= ~MF2_DONTDRAW;
	}
	/*else if (player->mo->tracer)
	{
		if (player->powers[pw_flashing] & 1)
			player->mo->tracer->flags2 |= MF2_DONTDRAW;
		else
			player->mo->tracer->flags2 &= ~MF2_DONTDRAW;
	}*/

	player->pflags &= ~PF_SLIDING;

	K_KartPlayerThink(player, cmd); // SRB2kart

	if (rendermode != render_none)
		DoABarrelRoll(player);

#ifdef HAVE_BLUA
	LUAh_PlayerThink(player);
#endif
}

//
// P_PlayerAfterThink
//
// Thinker for player after all other thinkers have run
//
void P_PlayerAfterThink(player_t *player)
{
	ticcmd_t *cmd;
	//INT32 oldweapon = player->currentweapon; // SRB2kart - unused
	camera_t *thiscam = NULL; // if not one of the displayed players, just don't bother
	UINT8 i;

#ifdef PARANOIA
	if (!player->mo)
	{
		const size_t playeri = (size_t)(player - players);
		I_Error("P_PlayerAfterThink: players[%s].mo == NULL", sizeu1(playeri));
	}
#endif

	cmd = &player->cmd;

#ifdef SECTORSPECIALSAFTERTHINK
	if (player->onconveyor != 1 || !P_IsObjectOnGround(player->mo))
	player->onconveyor = 0;
	// check special sectors : damage & secrets

	if (!player->spectator)
		P_PlayerInSpecialSector(player);
#endif

	for (i = 0; i <= splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			thiscam = &camera[i];
			break;
		}
	}

	if (player->playerstate == PST_DEAD)
	{
		return;
	}

	if (player->pflags & PF_NIGHTSMODE)
	{
		player->powers[pw_gravityboots] = 0;
		//player->mo->eflags &= ~MFE_VERTICALFLIP;
	}

	if (player->pflags & PF_SLIDING)
		P_SetPlayerMobjState(player->mo, player->mo->info->painstate);

	if (player->pflags & PF_MACESPIN && player->mo->tracer && player->mo->tracer->target)
	{
		player->mo->height = P_GetPlayerSpinHeight(player);
		// tracer is what you're hanging onto....
		player->mo->momx = (player->mo->tracer->x - player->mo->x)*2;
		player->mo->momy = (player->mo->tracer->y - player->mo->y)*2;
		player->mo->momz = (player->mo->tracer->z - (player->mo->height-player->mo->tracer->height/2) - player->mo->z)*2;
		P_MoveOrigin(player->mo, player->mo->tracer->x, player->mo->tracer->y, player->mo->tracer->z - (player->mo->height-player->mo->tracer->height/2));
		player->pflags |= PF_JUMPED;
		player->secondjump = 0;
		player->pflags &= ~PF_THOKKED;

		if (cmd->forwardmove > 0)
			player->mo->tracer->target->lastlook += 2;
		else if (cmd->forwardmove < 0 && player->mo->tracer->target->lastlook > player->mo->tracer->target->movecount)
			player->mo->tracer->target->lastlook -= 2;

		if (!(player->mo->tracer->target->flags & MF_SLIDEME) // Noclimb on chain parameters gives this
		&& !(twodlevel || player->mo->flags2 & MF2_TWOD)) // why on earth would you want to turn them in 2D mode?
		{
			player->mo->tracer->target->health += cmd->sidemove;
			player->mo->angle += cmd->sidemove<<ANGLETOFINESHIFT; // 2048 --> ANGLE_MAX

			if (player == &players[consoleplayer])
				localangle[0] = player->mo->angle; // Adjust the local control angle.
			else if (player == &players[displayplayers[1]])
				localangle[1] = player->mo->angle;
			else if (player == &players[displayplayers[2]])
				localangle[2] = player->mo->angle;
			else if (player == &players[displayplayers[3]])
				localangle[3] = player->mo->angle;
		}
	}

	if (thiscam)
	{
		if (!thiscam->chase) // bob view only if looking through the player's eyes
		{
			P_CalcHeight(player);
			P_CalcPostImg(player);
		}
		else
		{
			// defaults to make sure 1st person cam doesn't do anything weird on startup
			//player->deltaviewheight = 0;
			player->viewheight = FixedMul(32 << FRACBITS, player->mo->scale);
			if (player->mo->eflags & MFE_VERTICALFLIP)
				player->viewz = player->mo->z + player->mo->height - player->viewheight;
			else
				player->viewz = player->mo->z + player->viewheight;
		}
	}

	if (player->awayviewtics < 0)
		player->awayviewtics = 0;

	// spectator invisibility and nogravity.
	if ((netgame || multiplayer) && player->spectator)
	{
		player->mo->flags2 |= MF2_DONTDRAW;
		player->mo->flags |= MF_NOGRAVITY;
	}

	if (P_IsObjectOnGround(player->mo))
		player->mo->pmomz = 0;

	K_KartPlayerAfterThink(player);
}
