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
/// \file  p_tick.c
/// \brief Archiving: SaveGame I/O, Thinker, Ticker

#include "doomstat.h"
#include "g_game.h"
#include "g_input.h"
#include "p_local.h"
#include "z_zone.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "p_setup.h"
#include "p_polyobj.h"
#include "m_random.h"
#include "lua_script.h"
#include "lua_hook.h"
#include "k_director.h"
#include "k_kart.h"
#include "i_system.h"
#include "r_main.h"
#include "r_fps.h"
#include "i_video.h" // rendermode
#include "m_perfstats.h"

// Object place
#include "m_cheat.h"

// Dynamic slopes
#include "p_slopes.h"

tic_t leveltime;

//
// THINKERS
// All thinkers should be allocated by Z_Calloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//

// Both the head and tail of the thinker list.
thinker_t thinkercap;

void Command_Numthinkers_f(void)
{
	INT32 num;
	INT32 count = 0;
	actionf_p1 action;
	thinker_t *think;

	if (gamestate != GS_LEVEL)
	{
		CONS_Printf(M_GetText("You must be in a level to use this.\n"));
		return;
	}

	if (COM_Argc() < 2)
	{
		CONS_Printf(M_GetText("numthinkers <#>: Count number of thinkers\n"));
		CONS_Printf(
			"\t1: P_MobjThinker\n"
			"\t2: P_NullPrecipThinker\n"
			"\t3: T_Friction\n"
			"\t4: T_Pusher\n"
			"\t5: P_RemoveThinkerDelayed\n");
		return;
	}

	num = atoi(COM_Argv(1));

	switch (num)
	{
		case 1:
			action = (actionf_p1)P_MobjThinker;
			CONS_Printf(M_GetText("Number of %s: "), "P_MobjThinker");
			break;
		case 2:
			action = (actionf_p1)P_NullPrecipThinker;
			CONS_Printf(M_GetText("Number of %s: "), "P_NullPrecipThinker");
			break;
		case 3:
			action = (actionf_p1)T_Friction;
			CONS_Printf(M_GetText("Number of %s: "), "T_Friction");
			break;
		case 4:
			action = (actionf_p1)T_Pusher;
			CONS_Printf(M_GetText("Number of %s: "), "T_Pusher");
			break;
		case 5:
			action = (actionf_p1)P_RemoveThinkerDelayed;
			CONS_Printf(M_GetText("Number of %s: "), "P_RemoveThinkerDelayed");
			break;
		default:
			CONS_Printf(M_GetText("That is not a valid number.\n"));
			return;
	}

	for (think = thinkercap.next; think != &thinkercap; think = think->next)
	{
		if (think->function.acp1 != action)
			continue;

		count++;
	}

	CONS_Printf("%d\n", count);
}

void Command_CountMobjs_f(void)
{
	thinker_t *th;
	mobjtype_t i;
	INT32 count;

	if (gamestate != GS_LEVEL)
	{
		CONS_Printf(M_GetText("You must be in a level to use this.\n"));
		return;
	}

	if (COM_Argc() >= 2)
	{
		size_t j;
		for (j = 1; j < COM_Argc(); j++)
		{
			i = atoi(COM_Argv(j));
			if (i >= NUMMOBJTYPES)
			{
				CONS_Printf(M_GetText("Object number %d out of range (max %d).\n"), i, NUMMOBJTYPES-1);
				continue;
			}

			count = 0;

			for (th = thinkercap.next; th != &thinkercap; th = th->next)
			{
				if (th->function.acp1 != (actionf_p1)P_MobjThinker)
					continue;

				if (((mobj_t *)th)->type == i)
					count++;
			}

			CONS_Printf(M_GetText("There are %d objects of type %d currently in the level.\n"), count, i);
		}
		return;
	}

	CONS_Printf(M_GetText("Count of active objects in level:\n"));

	for (i = 0; i < NUMMOBJTYPES; i++)
	{
		count = 0;

		for (th = thinkercap.next; th != &thinkercap; th = th->next)
		{
			if (th->function.acp1 != (actionf_p1)P_MobjThinker)
				continue;

			if (((mobj_t *)th)->type == i)
				count++;
		}

		if (count > 0) // Don't bother displaying if there are none of this type!
			CONS_Printf(" * %d: %d\n", i, count);
	}
}

//
// P_InitThinkers
//
void P_InitThinkers(void)
{
	thinkercap.prev = thinkercap.next = &thinkercap;
	waypointcap = NULL;
}

//
// P_AddThinker
// Adds a new thinker at the end of the list.
//
void P_AddThinker(thinker_t *thinker)
{
	thinkercap.prev->next = thinker;
	thinker->next = &thinkercap;
	thinker->prev = thinkercap.prev;
	thinkercap.prev = thinker;

	thinker->references = 0;    // killough 11/98: init reference counter to 0
}

//
// killough 11/98:
//
// Make currentthinker external, so that P_RemoveThinkerDelayed
// can adjust currentthinker when thinkers self-remove.

static thinker_t *currentthinker;

//
// P_RemoveThinkerDelayed()
//
// Called automatically as part of the thinker loop in P_RunThinkers(),
// on nodes which are pending deletion.
//
// If this thinker has no more pointers referencing it indirectly,
// remove it, and set currentthinker to one node preceeding it, so
// that the next step in P_RunThinkers() will get its successor.
//
void P_RemoveThinkerDelayed(thinker_t *thinker)
{
#ifdef PARANOIA
	if (thinker->next)
		thinker->next = NULL;
	else if (thinker->references) // Usually gets cleared up in one frame; what's going on here, then?
		CONS_Printf("Number of potentially faulty references: %d\n", thinker->references);
#endif
	if (thinker->references)
		return;

	/* Remove from main thinker list */
	thinker_t *next = thinker->next;
	/* Note that currentthinker is guaranteed to point to us,
	* and since we're freeing our memory, we had better change that. So
	* point it to thinker->prev, so the iterator will correctly move on to
	* thinker->prev->next = thinker->next */
	(next->prev = currentthinker = thinker->prev)->next = next;
	R_DestroyLevelInterpolators(thinker);
	Z_Free(thinker);
}

//
// P_UnlinkThinker()
//
// Actually removes thinker from the list and frees its memory.
//
void P_UnlinkThinker(thinker_t *thinker)
{
	thinker_t *next = thinker->next;

	I_Assert(thinker->references == 0);

	(next->prev = thinker->prev)->next = next;
	Z_Free(thinker);
}

//
// P_RemoveThinker
//
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
// killough 4/25/98:
//
// Instead of marking the function with -1 value cast to a function pointer,
// set the function to P_RemoveThinkerDelayed(), so that later, it will be
// removed automatically as part of the thinker process.
//
void P_RemoveThinker(thinker_t *thinker)
{
	LUA_InvalidateUserdata(thinker);
	thinker->function.acp1 = (actionf_p1)P_RemoveThinkerDelayed;
}

/*
 * P_SetTarget
 *
 * This function is used to keep track of pointer references to mobj thinkers.
 * In Doom, objects such as lost souls could sometimes be removed despite
 * their still being referenced. In Boom, 'target' mobj fields were tested
 * during each gametic, and any objects pointed to by them would be prevented
 * from being removed. But this was incomplete, and was slow (every mobj was
 * checked during every gametic). Now, we keep a count of the number of
 * references, and delay removal until the count is 0.
 */

mobj_t *P_SetTarget(mobj_t **mop, mobj_t *targ)
{
	if (*mop)              // If there was a target already, decrease its refcount
		(*mop)->thinker.references--;
	if ((*mop = targ) != NULL) // Set new target and if non-NULL, increase its counter
		targ->thinker.references++;
	return targ;
}

//
// P_RunThinkers
//
// killough 4/25/98:
//
// Fix deallocator to stop using "next" pointer after node has been freed
// (a Doom bug).
//
// Process each thinker. For thinkers which are marked deleted, we must
// load the "next" pointer prior to freeing the node. In Doom, the "next"
// pointer was loaded AFTER the thinker was freed, which could have caused
// crashes.
//
// But if we are not deleting the thinker, we should reload the "next"
// pointer after calling the function, in case additional thinkers are
// added at the end of the list.
//
// killough 11/98:
//
// Rewritten to delete nodes implicitly, by making currentthinker
// external and using P_RemoveThinkerDelayed() implicitly.
//
static inline void P_RunThinkers(void)
{
	for (currentthinker = thinkercap.next; currentthinker != &thinkercap; currentthinker = currentthinker->next)
	{
		if (currentthinker->function.acp1 == (actionf_p1)P_NullPrecipThinker)
			continue;
#ifdef PARANOIA
		I_Assert(currentthinker->function.acp1 != NULL)
#endif
		currentthinker->function.acp1(currentthinker);
	}
}

static inline void P_DeviceRumbleTick(void)
{
	UINT8 i;

	if (I_NumJoys() == 0 || (cv_rumble[0].value == 0 && cv_rumble[1].value == 0 && cv_rumble[2].value == 0 && cv_rumble[3].value == 0))
	{
		return;
	}

	for (i = 0; i <= splitscreen; i++)
	{
		player_t *player = &players[displayplayers[i]];
		UINT16 low = 0;
		UINT16 high = 0;

		if (!P_IsLocalPlayer(player))
			continue;

		if (G_GetDeviceForPlayer(i) == 0)
			continue;

		if (!playeringame[displayplayers[i]] || player->spectator)
			continue;

		if (player->mo == NULL)
			continue;

		if (player->exiting)
		{
			G_PlayerDeviceRumble(i, low, high, 0);
			continue;
		}

		if (player->kartstuff[k_spinouttimer])
		{
			low = high = 65536 / 4;
		}
		else if (player->kartstuff[k_sneakertimer] > (sneakertime-(TICRATE/2)))
		{
			low = high = 65536 / 8;
		}
		else if ((player->kartstuff[k_offroad] && !player->kartstuff[k_hyudorotimer])
			&& P_IsObjectOnGround(player->mo) && player->speed != 0)
		{
			low = high = 65536 / 64;
		}
		else if (player->kartstuff[k_brakedrift])
		{
			low = 0;
			high = 65536 / 256;
		}

		 if (low == 0 && high == 0)
			continue;

		G_PlayerDeviceRumble(i, low, high, 57); // hack alert! i just dont want this think constantly resetting the rumble lol
	}
}

void P_RunChaseCameras(void)
{
	UINT8 i;

	for (i = 0; i <= splitscreen; i++)
	{
		if (camera[i].chase)
			P_MoveChaseCamera(&players[displayplayers[i]], &camera[i], false);
	}
}

static inline void P_RunQuakes(void)
{
	fixed_t ir;

	if (quake.time <= 0)
	{
		quake.x = quake.y = quake.z = quake.roll = 0;
		return;
	}

	ir = quake.intensity>>1;

	/// \todo Calculate distance from epicenter if set and modulate the intensity accordingly based on radius.
	quake.x = M_RandomRange(-ir,ir);
	quake.y = M_RandomRange(-ir,ir);
	quake.z = M_RandomRange(-ir,ir);

	ir >>= 2;
	ir = M_RandomRange(-ir,ir);

	if (ir < 0)
		ir = ANGLE_MAX - FixedAngle(-ir);
	else
		ir = FixedAngle(ir);

	quake.roll = ir;

	--quake.time;
}

//
// P_Ticker
//
void P_Ticker(boolean run)
{
	INT32 i;

	//Increment jointime even if paused.
	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			++players[i].jointime;

	if (objectplacing)
	{
		if (OP_FreezeObjectplace())
		{
			P_MapStart();
			R_UpdateMobjInterpolators();
			OP_ObjectplaceMovement(&players[0]);
			P_MoveChaseCamera(&players[0], &camera[0], false);
			R_UpdateViewInterpolation();
			P_MapEnd();
			return;
		}
	}

	// Check for pause or menu up in single player
	if (paused || P_AutoPause())
	{
		if (demo.rewinding && leveltime > 0)
		{
			leveltime = (leveltime-1) & ~3;
			if (timeinmap > 0)
				timeinmap = (timeinmap-1) & ~3;
			G_PreviewRewind(leveltime);
		}
		else if (demo.freecam && democam.cam)	// special case: allow freecam to MOVE during pause!
			P_DemoCameraMovement(democam.cam);

		return;
	}

	P_MapStart();

	if (run)
	{
		R_UpdateMobjInterpolators();

		if (demo.recording)
		{
			G_WriteDemoExtraData();
			for (i = 0; i < MAXPLAYERS; i++)
				if (playeringame[i])
					G_WriteDemoTiccmd(&players[i].cmd, i);
		}
		if (demo.playback)
		{

#ifdef DEMO_COMPAT_100
			if (demo.version == 0x0001)
			{
				G_ReadDemoTiccmd(&players[consoleplayer].cmd, 0);
			}
			else
			{
#endif
				G_ReadDemoExtraData();
				for (i = 0; i < MAXPLAYERS; i++)
					if (playeringame[i])
					{
						//@TODO all this throwdir stuff shouldn't be here! But it's added to maintain 1.0.4 compat for now...
						// Remove for 1.1!
						if (players[i].cmd.buttons & BT_FORWARD)
							players[i].kartstuff[k_throwdir] = 1;
						else if (players[i].cmd.buttons & BT_BACKWARD)
							players[i].kartstuff[k_throwdir] = -1;
						else
							players[i].kartstuff[k_throwdir] = 0;

						G_ReadDemoTiccmd(&players[i].cmd, i);
					}
#ifdef DEMO_COMPAT_100
			}
#endif
		}
		
		ps_lua_mobjhooks.value.i = 0;
		ps_checkposition_calls.value.i = 0;

		PS_START_TIMING(ps_lua_prethinkframe_time);
		LUAh_PreThinkFrame();
		PS_STOP_TIMING(ps_lua_prethinkframe_time);

		PS_START_TIMING(ps_playerthink_time);
		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo))
				P_PlayerThink(&players[i]);
		PS_STOP_TIMING(ps_playerthink_time);
	}

	// Keep track of how long they've been playing!
	if (!demo.playback) // Don't increment if a demo is playing.
		totalplaytime++;

	if (run)
	{
		// Dynamic slopeness
		if (midgamejoin) // only run here if we joined midgame to fix some desynchs
			P_RunDynamicSlopes();

		PS_START_TIMING(ps_thinkertime);
		P_RunThinkers();
		PS_STOP_TIMING(ps_thinkertime);

		// Run any "after all the other thinkers" stuff
		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo))
				P_PlayerAfterThink(&players[i]);

		// Apply rumble to local players
		if (!demo.playback)
		{
			P_DeviceRumbleTick();
		}

		PS_START_TIMING(ps_lua_thinkframe_time);
		LUAh_ThinkFrame();
		PS_STOP_TIMING(ps_lua_thinkframe_time);
	}

	// Run shield positioning
	//P_RunShields();
	P_RunOverlays();

	P_RunShadows();

	P_UpdateSpecials();
	P_RespawnSpecials();

	// Lightning, rain sounds, etc.
	P_PrecipitationEffects();

	if (run)
		leveltime++;

	// as this is mostly used for HUD stuff, add the record attack specific hack to it as well!
	if (!(modeattacking && !demo.playback) || leveltime >= starttime - TICRATE*4)
		timeinmap++;

	if (run)
	{
		if (countdowntimer && --countdowntimer <= 0)
		{
			countdowntimer = 0;
			countdowntimeup = true;
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i] || players[i].spectator)
					continue;

				if (!players[i].mo)
					continue;

				P_DamageMobj(players[i].mo, NULL, NULL, 10000);
			}
		}

		if (racecountdown > 1)
			racecountdown--;

		if (exitcountdown > 1)
			exitcountdown--;

		if (indirectitemcooldown > 0)
			indirectitemcooldown--;
		if (hyubgone > 0)
			hyubgone--;

		if (G_RaceGametype())
		{
			K_UpdateSpectateGrief();
		}

		if (G_BattleGametype())
		{
			if (wantedcalcdelay && --wantedcalcdelay <= 0)
				K_CalculateBattleWanted();
		}

		P_RunQuakes();

		if (metalplayback)
			G_ReadMetalTic(metalplayback);
		if (metalrecording)
			G_WriteMetalTic(players[consoleplayer].mo);

		if (demo.recording)
		{
			INT32 axis = JoyAxis(AXISLOOKBACK, 1);

			G_WriteAllGhostTics();

			if (cv_recordmultiplayerdemos.value && (demo.savemode == DSM_NOTSAVING || demo.savemode == DSM_WILLAUTOSAVE))
				if (demo.savebutton && demo.savebutton + 3*TICRATE < leveltime && (InputDown(gc_lookback, 1) || (cv_usejoystick.value && axis > 0)))
					demo.savemode = DSM_TITLEENTRY;

			//if there are no players left at all, stop demo recording
			//Demos that that dont have any players crash during playback, which can happen with dedicated servers
			if (cv_recordmultiplayerdemos.value && demo.savemode == DSM_WILLAUTOSAVE && !D_NumPlayers())
				G_SaveDemo();
		}
		else if (demo.playback) // Use Ghost data for consistency checks.
		{
#ifdef DEMO_COMPAT_100
			if (demo.version == 0x0001)
				G_ConsGhostTic(0);
			else
#endif
			G_ConsAllGhostTics();
		}

		if (modeattacking)
			G_GhostTicker();

		if (mapreset > 1
			&& --mapreset <= 1
			&& server) // Remember: server uses it for mapchange, but EVERYONE ticks down for the animation
				D_MapChange(gamemap, gametype, encoremode, true, 0, false, false);

		PS_START_TIMING(ps_lua_postthinkframe_time);
		LUAh_PostThinkFrame();
		PS_STOP_TIMING(ps_lua_postthinkframe_time);
	}

	K_UpdateDirector();

	// Always move the camera.
	P_RunChaseCameras();

	if (run)
	{
		R_UpdateLevelInterpolators();
		R_UpdateViewInterpolation();

		// Hack: ensure newview is assigned every tic.
		// Ensures view interpolation is T-1 to T in poor network conditions
		// We need a better way to assign view state decoupled from game logic
		if (rendermode != render_none)
		{
			for (i = 0; i <= splitscreen; i++)
			{
				player_t *player = &players[displayplayers[i]];
				boolean isSkyVisibleForPlayer = skyVisiblePerPlayer[i];

				if (!player->mo)
					continue;

				if (isSkyVisibleForPlayer && skyboxmo[0] && cv_skybox.value)
				{
					R_SkyboxFrame(player);
				}
				R_SetupFrame(player, (skyboxmo[0] && cv_skybox.value));
			}
		}
	}

	P_MapEnd();

	if (demo.playback)
		G_StoreRewindInfo();
}

// Abbreviated ticker for pre-loading, calls thinkers and assorted things
void P_PreTicker(INT32 frames)
{
	INT32 i;
	ticcmd_t temptic;

	hook_defrosting = frames;

	while (hook_defrosting)
	{
		P_MapStart();

		R_UpdateMobjInterpolators();

		LUAh_PreThinkFrame();

		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo))
			{
				// stupid fucking cmd hack
				// if it isn't for this, players can move in preticker time
				// (and disrupt demo recording and other things !!)
				memcpy(&temptic, &players[i].cmd, sizeof(ticcmd_t));
				memset(&players[i].cmd, 0, sizeof(ticcmd_t));
				// correct angle on spawn...
				players[i].cmd.angleturn = temptic.angleturn;

				P_PlayerThink(&players[i]);

				memcpy(&players[i].cmd, &temptic, sizeof(ticcmd_t));
			}

		// Dynamic slopeness
		if (midgamejoin)
			P_RunDynamicSlopes();

		P_RunThinkers();

		// Run any "after all the other thinkers" stuff
		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo))
				P_PlayerAfterThink(&players[i]);

		LUAh_ThinkFrame();

		P_RunOverlays();
		P_RunShadows();

		P_UpdateSpecials();
		P_RespawnSpecials();

		LUAh_PostThinkFrame();

		R_UpdateLevelInterpolators();
		R_UpdateViewInterpolation();
		R_ResetViewInterpolation(0);

		P_MapEnd();

		hook_defrosting--;
	}
}
