// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2012-2016 by John "JTE" Muniz.
// Copyright (C) 2012-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_playerlib.c
/// \brief player object library for Lua scripting

#include "doomdef.h"
#include "fastcmp.h"
#include "r_main.h"
#include "r_skins.h"
#include "r_things.h"
#include "p_mobj.h"
#include "d_player.h"
#include "g_game.h"
#include "p_local.h"
#include "d_clisrv.h"

#include "lua_script.h"
#include "lua_libs.h"
#include "lua_hud.h" // hud_running errors
#include "lua_hook.h"	// hook_cmd_running

#define PLAYER_FIELDS(X) \
	X(valid) \
    X(name) \
    X(mo) \
    X(cmd) \
    X(playerstate) \
    X(viewz) \
    X(viewheight) \
    X(aiming) \
    X(health) \
    X(pity) \
    X(currentweapon) \
    X(ringweapons) \
    X(powers) \
    X(kartstuff) \
    X(frameangle) \
    X(pflags) \
    X(panim) \
    X(flashcount) \
    X(flashpal) \
    X(skincolor) \
    X(localskin) \
    X(score) \
    X(dashspeed) \
    X(dashtime) \
    X(kartspeed) \
    X(kartweight) \
    X(charflags) \
    X(lives) \
    X(continues) \
    X(xtralife) \
    X(gotcontinue) \
    X(speed) \
    X(jumping) \
    X(secondjump) \
    X(fly1) \
    X(scoreadd) \
    X(glidetime) \
    X(climbing) \
    X(deadtimer) \
    X(exiting) \
    X(homing) \
    X(skidtime) \
    X(cmomx) \
    X(cmomy) \
    X(rmomx) \
    X(rmomy) \
    X(numboxes) \
    X(totalring) \
    X(realtime) \
    X(laps) \
    X(ctfteam) \
    X(gotflag) \
    X(weapondelay) \
    X(tossdelay) \
    X(starpostx) \
    X(starposty) \
    X(starpostz) \
    X(starpostnum) \
    X(starposttime) \
    X(starpostangle) \
    X(angle_pos) \
    X(old_angle_pos) \
    X(axis1) \
    X(axis2) \
    X(bumpertime) \
    X(flyangle) \
    X(drilltimer) \
    X(linkcount) \
    X(linktimer) \
    X(anotherflyangle) \
    X(nightstime) \
    X(drillmeter) \
    X(drilldelay) \
    X(bonustime) \
    X(capsule) \
    X(mare) \
    X(marebegunat) \
    X(startedtime) \
    X(finishedtime) \
    X(finishedrings) \
    X(marescore) \
    X(lastmarescore) \
    X(lastmare) \
    X(maxlink) \
    X(texttimer) \
    X(textvar) \
    X(lastsidehit) \
    X(lastlinehit) \
    X(losstime) \
    X(timeshit) \
    X(onconveyor) \
    X(awayviewmobj) \
    X(awayviewtics) \
    X(awayviewaiming) \
    X(spectator) \
    X(bot) \
    X(jointime) \
    X(spectatorreentry) \
    X(grieftime) \
    X(griefstrikes) \
    X(splitscreenindex) \
    X(fovadd) \
	X(sliproll) \
	X(viewrollangle) \
    X(ping)

enum player_e {
#define ENUMITEM(name) player_##name,
	PLAYER_FIELDS(ENUMITEM)
	NUMPLAYERFIELDS,
#undef ENUMITEM
};

static const char *player_opt[] = {
#define STRITEM(name) (#name),
	PLAYER_FIELDS(STRITEM)
	NULL,
};

static int player_fields_ref = LUA_NOREF;

static int lib_iteratePlayers(lua_State *L)
{
	INT32 i = -1;
	if (lua_gettop(L) < 2)
	{
		//return luaL_error(L, "Don't call players.iterate() directly, use it as 'for player in players.iterate do <block> end'.");
		lua_pushcfunction(L, lib_iteratePlayers);
		return 1;
	}
	lua_settop(L, 2);
	lua_remove(L, 1); // state is unused.
	if (!lua_isnil(L, 1))
		i = (INT32)(*((player_t **)luaL_checkudata(L, 1, META_PLAYER)) - players);
	for (i++; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		if (!players[i].mo)
			continue;
		LUA_PushUserdata(L, &players[i], META_PLAYER);
		return 1;
	}
	return 0;
}

static int lib_getPlayer(lua_State *L)
{
	const char *field;
	// i -> players[i]
	if (lua_type(L, 2) == LUA_TNUMBER)
	{
		lua_Integer i = luaL_checkinteger(L, 2);
		if (i < 0 || i >= MAXPLAYERS)
			return luaL_error(L, "players[] index %d out of range (0 - %d)", i, MAXPLAYERS-1);
		if (!playeringame[i])
			return 0;
		if (!players[i].mo)
			return 0;
		LUA_PushUserdata(L, &players[i], META_PLAYER);
		return 1;
	}

	field = luaL_checkstring(L, 2);
	if (fastcmp(field,"iterate"))
	{
		lua_pushcfunction(L, lib_iteratePlayers);
		return 1;
	}
	return 0;
}

// #players -> MAXPLAYERS
FUNCINLINE static ATTRINLINE int lib_lenPlayer(lua_State *L)
{
	lua_pushinteger(L, MAXPLAYERS);
	return 1;
}

// Same deal as the three functions above but for displayplayers

static int lib_iterateDisplayplayers(lua_State *L)
{
	INT32 i = lua_tonumber(L, lua_upvalueindex(1));

	if (lua_gettop(L) < 2)
	{
		lua_pushcclosure(L, lib_iterateDisplayplayers, 1);
		return 1;
	}

	if (i <= splitscreen)
	{
		if (!playeringame[displayplayers[i]])
			return 0;

		// Return player and splitscreen index.
		LUA_PushUserdata(L, &players[displayplayers[i]], META_PLAYER);
		lua_pushnumber(L, i);

		// Update splitscreen index value for next iteration.
		lua_pushnumber(L, i + 1);
		lua_pushvalue(L, -1);
		lua_replace(L, lua_upvalueindex(1));
		lua_pop(L, 1);

		return 2;
	}
	return 0;
}

static int lib_getDisplayplayers(lua_State *L)
{
	const char *field;
	// i -> players[i]
	if (lua_type(L, 2) == LUA_TNUMBER)
	{
		lua_Integer i = luaL_checkinteger(L, 2);
		if (i < 0 || i >= MAXSPLITSCREENPLAYERS)
			return luaL_error(L, "displayplayers[] index %d out of range (0 - %d)", i, MAXSPLITSCREENPLAYERS-1);
		if (i > splitscreen)
			return 0;
		if (!playeringame[displayplayers[i]])
			return 0;
		if (!players[displayplayers[i]].mo)
			return 0;
		LUA_PushUserdata(L, &players[displayplayers[i]], META_PLAYER);
		return 1;
	}

	field = luaL_checkstring(L, 2);
	if (fastcmp(field,"iterate"))
	{
		lua_pushcclosure(L, lib_iterateDisplayplayers, 1);
		return 1;
	}
	return 0;
}

// #displayplayers -> MAXSPLITSCREENPLAYERS
FUNCINLINE static ATTRINLINE int lib_lenDisplayplayers(lua_State *L)
{
	lua_pushinteger(L, MAXSPLITSCREENPLAYERS);
	return 1;
}

static int player_get(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	enum player_e field = Lua_optoption(L, 2, -1, player_fields_ref);

	if (!plr) {
		if (field == player_valid) {
			lua_pushboolean(L, false);
			return 1;
		}
		return LUA_ErrInvalid(L, "player_t");
	} else if (field == player_valid) {
		lua_pushboolean(L, true);
        return 1;
    }

	switch (field)
	{
	case player_valid:
		lua_pushboolean(L, true);
		break;
	case player_name:
		lua_pushstring(L, player_names[plr-players]);
		break;
	case player_mo:
	{
		if (plr->spectator)
			lua_pushnil(L);
		else
			LUA_PushUserdata(L, plr->mo, META_MOBJ);
	}
		break;
	case player_cmd:
		LUA_PushUserdata(L, &plr->cmd, META_TICCMD);
		break;
	case player_playerstate:
		lua_pushinteger(L, plr->playerstate);
		break;
	case player_viewz:
		lua_pushfixed(L, plr->viewz);
		break;
	case player_viewheight:
		lua_pushfixed(L, plr->viewheight);
	/*	break;
	case player_deltaviewheight:
		lua_pushfixed(L, plr->deltaviewheight);
		break;
	case player_bob:
		lua_pushfixed(L, plr->bob);*/
		break;
	case player_aiming:
		lua_pushangle(L, plr->aiming);
		break;
	case player_health:
		lua_pushinteger(L, plr->health);
		break;
	case player_pity:
		lua_pushinteger(L, plr->pity);
		break;
	case player_currentweapon:
		lua_pushinteger(L, plr->currentweapon);
		break;
	case player_ringweapons:
		lua_pushinteger(L, plr->ringweapons);
		break;
	case player_powers:
		LUA_PushUserdata(L, plr->powers, META_POWERS);
		break;
	case player_kartstuff:
		LUA_PushUserdata(L, plr->kartstuff, META_KARTSTUFF);
		break;
	case player_frameangle:
		lua_pushangle(L, plr->frameangle);
		break;
	case player_pflags:
		lua_pushinteger(L, plr->pflags);
		break;
	case player_panim:
		lua_pushinteger(L, plr->panim);
		break;
	case player_flashcount:
		lua_pushinteger(L, plr->flashcount);
		break;
	case player_flashpal:
		lua_pushinteger(L, plr->flashpal);
		break;
	case player_skincolor:
		lua_pushinteger(L, plr->skincolor);
		break;
	case player_score:
		lua_pushinteger(L, plr->score);
		break;
	case player_dashspeed:
		lua_pushfixed(L, plr->dashspeed);
		break;
	case player_dashtime:
		lua_pushinteger(L, plr->dashtime);
	// SRB2kart
		break;
	case player_kartspeed:
		lua_pushinteger(L, plr->kartspeed);
		break;
	case player_kartweight:
		lua_pushinteger(L, plr->kartweight);
	//
		break;
	case player_charflags:
		lua_pushinteger(L, plr->charflags);
		break;
	case player_lives:
		lua_pushinteger(L, plr->lives);
		break;
	case player_continues:
		lua_pushinteger(L, plr->continues);
		break;
	case player_xtralife:
		lua_pushinteger(L, plr->xtralife);
		break;
	case player_gotcontinue:
		lua_pushinteger(L, plr->gotcontinue);
		break;
	case player_speed:
		lua_pushfixed(L, plr->speed);
		break;
	case player_jumping:
		lua_pushboolean(L, plr->jumping);
		break;
	case player_secondjump:
		lua_pushinteger(L, plr->secondjump);
		break;
	case player_fly1:
		lua_pushinteger(L, plr->fly1);
		break;
	case player_scoreadd:
		lua_pushinteger(L, plr->scoreadd);
		break;
	case player_glidetime:
		lua_pushinteger(L, plr->glidetime);
		break;
	case player_climbing:
		lua_pushinteger(L, plr->climbing);
		break;
	case player_deadtimer:
		lua_pushinteger(L, plr->deadtimer);
		break;
	case player_exiting:
		lua_pushinteger(L, plr->exiting);
		break;
	case player_homing:
		lua_pushinteger(L, plr->homing);
		break;
	case player_skidtime:
		lua_pushinteger(L, plr->skidtime);
		break;
	case player_cmomx:
		lua_pushfixed(L, plr->cmomx);
		break;
	case player_cmomy:
		lua_pushfixed(L, plr->cmomy);
		break;
	case player_rmomx:
		lua_pushfixed(L, plr->rmomx);
		break;
	case player_rmomy:
		lua_pushfixed(L, plr->rmomy);
		break;
	case player_numboxes:
		lua_pushinteger(L, plr->numboxes);
		break;
	case player_totalring:
		lua_pushinteger(L, plr->totalring);
		break;
	case player_realtime:
		lua_pushinteger(L, plr->realtime);
		break;
	case player_laps:
		lua_pushinteger(L, plr->laps);
		break;
	case player_ctfteam:
		lua_pushinteger(L, plr->ctfteam);
		break;
	case player_gotflag:
		lua_pushinteger(L, plr->gotflag);
		break;
	case player_weapondelay:
		lua_pushinteger(L, plr->weapondelay);
		break;
	case player_tossdelay:
		lua_pushinteger(L, plr->tossdelay);
		break;
	case player_starpostx:
		lua_pushinteger(L, plr->starpostx);
		break;
	case player_starposty:
		lua_pushinteger(L, plr->starposty);
		break;
	case player_starpostz:
		lua_pushinteger(L, plr->starpostz);
		break;
	case player_starpostnum:
		lua_pushinteger(L, plr->starpostnum);
		break;
	case player_starposttime:
		lua_pushinteger(L, plr->starposttime);
		break;
	case player_starpostangle:
		lua_pushangle(L, plr->starpostangle);
		break;
	case player_angle_pos:
		lua_pushangle(L, plr->angle_pos);
		break;
	case player_old_angle_pos:
		lua_pushangle(L, plr->old_angle_pos);
		break;
	case player_axis1:
		LUA_PushUserdata(L, plr->axis1, META_MOBJ);
		break;
	case player_axis2:
		LUA_PushUserdata(L, plr->axis2, META_MOBJ);
		break;
	case player_bumpertime:
		lua_pushinteger(L, plr->bumpertime);
		break;
	case player_flyangle:
		lua_pushinteger(L, plr->flyangle);
		break;
	case player_drilltimer:
		lua_pushinteger(L, plr->drilltimer);
		break;
	case player_linkcount:
		lua_pushinteger(L, plr->linkcount);
		break;
	case player_linktimer:
		lua_pushinteger(L, plr->linktimer);
		break;
	case player_anotherflyangle:
		lua_pushinteger(L, plr->anotherflyangle);
		break;
	case player_nightstime:
		lua_pushinteger(L, plr->nightstime);
		break;
	case player_drillmeter:
		lua_pushinteger(L, plr->drillmeter);
		break;
	case player_drilldelay:
		lua_pushinteger(L, plr->drilldelay);
		break;
	case player_bonustime:
		lua_pushboolean(L, plr->bonustime);
		break;
	case player_capsule:
		LUA_PushUserdata(L, plr->capsule, META_MOBJ);
		break;
	case player_mare:
		lua_pushinteger(L, plr->mare);
		break;
	case player_marebegunat:
		lua_pushinteger(L, plr->marebegunat);
		break;
	case player_startedtime:
		lua_pushinteger(L, plr->startedtime);
		break;
	case player_finishedtime:
		lua_pushinteger(L, plr->finishedtime);
		break;
	case player_finishedrings:
		lua_pushinteger(L, plr->finishedrings);
		break;
	case player_marescore:
		lua_pushinteger(L, plr->marescore);
		break;
	case player_lastmarescore:
		lua_pushinteger(L, plr->lastmarescore);
		break;
	case player_lastmare:
		lua_pushinteger(L, plr->lastmare);
		break;
	case player_maxlink:
		lua_pushinteger(L, plr->maxlink);
		break;
	case player_texttimer:
		lua_pushinteger(L, plr->texttimer);
		break;
	case player_textvar:
		lua_pushinteger(L, plr->textvar);
		break;
	case player_lastsidehit:
		lua_pushinteger(L, plr->lastsidehit);
		break;
	case player_lastlinehit:
		lua_pushinteger(L, plr->lastlinehit);
		break;
	case player_losstime:
		lua_pushinteger(L, plr->losstime);
		break;
	case player_timeshit:
		lua_pushinteger(L, plr->timeshit);
		break;
	case player_onconveyor:
		lua_pushinteger(L, plr->onconveyor);
		break;
	case player_awayviewmobj:
		LUA_PushUserdata(L, plr->awayviewmobj, META_MOBJ);
		break;
	case player_awayviewtics:
		lua_pushinteger(L, plr->awayviewtics);
		break;
	case player_awayviewaiming:
		lua_pushangle(L, plr->awayviewaiming);
		break;
	case player_spectator:
		lua_pushboolean(L, plr->spectator);
		break;
	case player_bot:
		lua_pushinteger(L, plr->bot);
		break;
	case player_jointime:
		lua_pushinteger(L, plr->jointime);
		break;
	case player_spectatorreentry:
		lua_pushinteger(L, plr->spectatorreentry);
		break;
	case player_grieftime:
		lua_pushinteger(L, plr->grieftime);
		break;
	case player_griefstrikes:
		lua_pushinteger(L, plr->griefstrikes);
		break;
	case player_splitscreenindex:
		lua_pushinteger(L, plr->splitscreenindex);
		break;
#ifdef HWRENDER
	case player_fovadd:
		lua_pushfixed(L, plr->fovadd);
		break;
#endif
	case player_ping:
		lua_pushinteger(L, playerpingtable[( plr - players )]);
		break;
	// Saturn
	case player_localskin:
		if (plr->localskin)
			lua_pushstring(L, R_GetPlayerSkin(plr)->name);
		else
			lua_pushnil(L);
		break;
	case player_sliproll:
		lua_pushangle(L, R_PlayerSliptideAngle(plr));
		break;
	case player_viewrollangle:
		lua_pushangle(L, R_ViewRollAngle(plr));
		break;
	default:
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
		I_Assert(lua_istable(L, -1));
		lua_pushlightuserdata(L, plr);
		lua_rawget(L, -2);
		if (!lua_istable(L, -1)) { // no extra values table
			CONS_Debug(DBG_LUA, M_GetText("'%s' has no extvars table or field named '%s'; returning nil.\n"), "player_t", lua_tostring(L, 2));
			return 0;
		}
		lua_pushvalue(L, 2); // field name
		lua_gettable(L, -2);
		if (lua_isnil(L, -1)) // no value for this field
			CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; returning nil.\n"), "player_t", lua_tostring(L, 2));
		break;
	}

	return 1;
}

#define NOSET luaL_error(L, LUA_QL("player_t") " field " LUA_QS " should not be set directly.", lua_tostring(L, 2))
static int player_set(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	enum player_e field = Lua_optoption(L, 2, player_cmd, player_fields_ref);

	if (!plr)
		return LUA_ErrInvalid(L, "player_t");

	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");

	if (hook_cmd_running)
		return luaL_error(L, "Do not alter player_t in BuildCMD code!");

	switch (field)
	{
	case player_name:
	case player_valid:
	case player_ping:
		//return NOSET;
		return 0; // In vanilla those are not handled and are added into player fields table, but they aren't accessible anyway so we can just ignore them
		break;
	case player_mo:
	{
		mobj_t *newmo = *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ));
		plr->mo->player = NULL; // remove player pointer from old mobj
		(newmo->player = plr)->mo = newmo; // set player pointer for new mobj, and set new mobj as the player's mobj
	}
		break;
	case player_cmd:
		return NOSET;
		break;
	case player_playerstate:
		plr->playerstate = luaL_checkinteger(L, 3);
		break;
	case player_viewz:
		plr->viewz = luaL_checkfixed(L, 3);
		break;
	case player_viewheight:
		plr->viewheight = luaL_checkfixed(L, 3);
	/*	break;
	case player_deltaviewheight:
		plr->deltaviewheight = luaL_checkfixed(L, 3);
		break;
	case player_bob:
		plr->bob = luaL_checkfixed(L, 3);*/
		break;
	case player_aiming: {
		plr->aiming = luaL_checkangle(L, 3);
		if (plr == &players[consoleplayer])
			localaiming[0] = plr->aiming;
		else if (plr == &players[displayplayers[1]])
			localaiming[1] = plr->aiming;
		else if (plr == &players[displayplayers[2]])
			localaiming[2] = plr->aiming;
		else if (plr == &players[displayplayers[3]])
			localaiming[3] = plr->aiming;
	}
		break;
	case player_health:
		plr->health = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_pity:
		plr->pity = (SINT8)luaL_checkinteger(L, 3);
		break;
	case player_currentweapon:
		plr->currentweapon = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_ringweapons:
		plr->ringweapons = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_powers:
		return NOSET;
		break;
	case player_pflags:
		plr->pflags = luaL_checkinteger(L, 3);
		break;
	case player_panim:
		plr->panim = luaL_checkinteger(L, 3);
		break;
	case player_flashcount:
		plr->flashcount = (UINT16)luaL_checkinteger(L, 3);
		break;
	case player_flashpal:
		plr->flashpal = (UINT16)luaL_checkinteger(L, 3);
		break;
	case player_skincolor:
	{
		UINT8 newcolor = (UINT8)luaL_checkinteger(L,3);
		if (newcolor >= MAXSKINCOLORS)
			return luaL_error(L, "player.skincolor %d out of range (0 - %d).", newcolor, MAXSKINCOLORS-1);
		plr->skincolor = newcolor;
	}
		break;
	case player_score:
		plr->score = (UINT32)luaL_checkinteger(L, 3);
		break;
	case player_dashspeed:
		plr->dashspeed = luaL_checkfixed(L, 3);
		break;
	case player_dashtime:
		plr->dashtime = (INT32)luaL_checkinteger(L, 3);
	// SRB2kart
		break;
	case player_kartstuff:
		return NOSET;
		break;
	case player_frameangle:
		plr->frameangle = luaL_checkangle(L, 3);
		break;
	case player_kartspeed:
		plr->kartspeed = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_kartweight:
		plr->kartweight = (UINT8)luaL_checkinteger(L, 3);
	//
		break;
	case player_charflags:
		plr->charflags = (UINT32)luaL_checkinteger(L, 3);
		break;
	case player_lives:
		plr->lives = (SINT8)luaL_checkinteger(L, 3);
		break;
	case player_continues:
		plr->continues = (SINT8)luaL_checkinteger(L, 3);
		break;
	case player_xtralife:
		plr->xtralife = (SINT8)luaL_checkinteger(L, 3);
		break;
	case player_gotcontinue:
		plr->gotcontinue = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_speed:
		plr->speed = luaL_checkfixed(L, 3);
		break;
	case player_jumping:
		plr->jumping = luaL_checkboolean(L, 3);
		break;
	case player_secondjump:
		plr->secondjump = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_fly1:
		plr->fly1 = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_scoreadd:
		plr->scoreadd = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_glidetime:
		plr->glidetime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_climbing:
		plr->climbing = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_deadtimer:
		plr->deadtimer = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_exiting:
		plr->exiting = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_homing:
		plr->homing = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_skidtime:
		plr->skidtime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_cmomx:
		plr->cmomx = luaL_checkfixed(L, 3);
		break;
	case player_cmomy:
		plr->cmomy = luaL_checkfixed(L, 3);
		break;
	case player_rmomx:
		plr->rmomx = luaL_checkfixed(L, 3);
		break;
	case player_rmomy:
		plr->rmomy = luaL_checkfixed(L, 3);
		break;
	case player_numboxes:
		plr->numboxes = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_totalring:
		plr->totalring = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_realtime:
		plr->realtime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_laps:
		plr->laps = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_ctfteam:
		plr->ctfteam = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_gotflag:
		plr->gotflag = (UINT16)luaL_checkinteger(L, 3);
		break;
	case player_weapondelay:
		plr->weapondelay = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_tossdelay:
		plr->tossdelay = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_starpostx:
		plr->starpostx = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_starposty:
		plr->starposty = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_starpostz:
		plr->starpostz = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_starpostnum:
		plr->starpostnum = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_starposttime:
		plr->starposttime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_starpostangle:
		plr->starpostangle = luaL_checkangle(L, 3);
		break;
	case player_angle_pos:
		plr->angle_pos = luaL_checkangle(L, 3);
		break;
	case player_old_angle_pos:
		plr->old_angle_pos = luaL_checkangle(L, 3);
		break;
	case player_axis1:
		P_SetTarget(&plr->axis1, *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ)));
		break;
	case player_axis2:
		P_SetTarget(&plr->axis2, *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ)));
		break;
	case player_bumpertime:
		plr->bumpertime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_flyangle:
		plr->flyangle = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_drilltimer:
		plr->drilltimer = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_linkcount:
		plr->linkcount = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_linktimer:
		plr->linktimer = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_anotherflyangle:
		plr->anotherflyangle = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_nightstime:
		plr->nightstime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_drillmeter:
		plr->drillmeter = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_drilldelay:
		plr->drilldelay = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_bonustime:
		plr->bonustime = luaL_checkboolean(L, 3);
		break;
	case player_capsule:
	{
		mobj_t *mo = NULL;
		if (!lua_isnil(L, 3))
			mo = *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ));
		P_SetTarget(&plr->capsule, mo);
	}
		break;
	case player_mare:
		plr->mare = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_marebegunat:
		plr->marebegunat = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_startedtime:
		plr->startedtime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_finishedtime:
		plr->finishedtime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_finishedrings:
		plr->finishedrings = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_marescore:
		plr->marescore = (UINT32)luaL_checkinteger(L, 3);
		break;
	case player_lastmarescore:
		plr->lastmarescore = (UINT32)luaL_checkinteger(L, 3);
		break;
	case player_lastmare:
		plr->lastmare = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_maxlink:
		plr->maxlink = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_texttimer:
		plr->texttimer = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_textvar:
		plr->textvar = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_lastsidehit:
		plr->lastsidehit = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_lastlinehit:
		plr->lastlinehit = (INT16)luaL_checkinteger(L, 3);
		break;
	case player_losstime:
		plr->losstime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_timeshit:
		plr->timeshit = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_onconveyor:
		plr->onconveyor = (INT32)luaL_checkinteger(L, 3);
		break;
	case player_awayviewmobj:
	{
		mobj_t *mo = NULL;
		if (!lua_isnil(L, 3))
			mo = *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ));
		P_SetTarget(&plr->awayviewmobj, mo);
	}
		break;
	case player_awayviewtics:
	{
		plr->awayviewtics = (INT32)luaL_checkinteger(L, 3);
		if (plr->awayviewtics && !plr->awayviewmobj) // awayviewtics must ALWAYS have an awayviewmobj set!!
			P_SetTarget(&plr->awayviewmobj, plr->mo); // but since the script might set awayviewmobj immediately AFTER setting awayviewtics, use player mobj as filler for now.
	}
		break;
	case player_awayviewaiming:
		plr->awayviewaiming = luaL_checkangle(L, 3);
		break;
	case player_spectator:
		plr->spectator = lua_toboolean(L, 3);
		break;
	case player_bot:
		return NOSET;
		break;
	case player_jointime:
		plr->jointime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_spectatorreentry:
		plr->spectatorreentry = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_grieftime:
		plr->grieftime = (tic_t)luaL_checkinteger(L, 3);
		break;
	case player_griefstrikes:
		plr->griefstrikes = (UINT8)luaL_checkinteger(L, 3);
		break;
	case player_splitscreenindex:
		return NOSET;
		break;
#ifdef HWRENDER
	case player_fovadd:
		plr->fovadd = luaL_checkfixed(L, 3);
		break;
#endif
	// Saturn
	case player_localskin:
		SetLocalPlayerSkin(plr - players, luaL_optstring(L, 2, "none"), NULL);
		break;
	case player_sliproll:
	case player_viewrollangle:
		return NOSET;
		break;
	default:
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
		I_Assert(lua_istable(L, -1));
		lua_pushlightuserdata(L, plr);
		lua_rawget(L, -2);
		if (lua_isnil(L, -1)) {
			// This index doesn't have a table for extra values yet, let's make one.
			lua_pop(L, 1);
			CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; adding it as Lua data.\n"), "player_t", lua_tostring(L, 2));
			lua_newtable(L);
			lua_pushlightuserdata(L, plr);
			lua_pushvalue(L, -2); // ext value table
			lua_rawset(L, -4); // LREG_EXTVARS table
		}
		lua_pushvalue(L, 3); // value to store
		lua_setfield(L, -2, lua_tostring(L, 2));
		lua_pop(L, 2);
	}

	return 0;
}

#undef NOSET

FUNCINLINE static ATTRINLINE int player_num(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	if (!plr)
		return luaL_error(L, "accessed player_t doesn't exist anymore.");
	lua_pushinteger(L, plr-players);
	return 1;
}

// powers, p -> powers[p]
static int power_get(lua_State *L)
{
	UINT16 *powers = *((UINT16 **)luaL_checkudata(L, 1, META_POWERS));
	powertype_t p = luaL_checkinteger(L, 2);
	if (p >= NUMPOWERS)
		return luaL_error(L, LUA_QL("powertype_t") " cannot be %u", p);
	lua_pushinteger(L, powers[p]);
	return 1;
}

// powers, p, value -> powers[p] = value
static int power_set(lua_State *L)
{
	UINT16 *powers = *((UINT16 **)luaL_checkudata(L, 1, META_POWERS));
	powertype_t p = luaL_checkinteger(L, 2);
	UINT16 i = (UINT16)luaL_checkinteger(L, 3);
	if (p >= NUMPOWERS)
		return luaL_error(L, LUA_QL("powertype_t") " cannot be %u", p);
	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");
	if (hook_cmd_running)
		return luaL_error(L, "Do not alter player_t in BuildCMD code!");
	powers[p] = i;
	return 0;
}

// #powers -> NUMPOWERS
FUNCINLINE static ATTRINLINE int power_len(lua_State *L)
{
	lua_pushinteger(L, NUMPOWERS);
	return 1;
}

// kartstuff, ks -> kartstuff[ks]
static int kartstuff_get(lua_State *L)
{
	INT32 *kartstuff = *((INT32 **)luaL_checkudata(L, 1, META_KARTSTUFF));
	kartstufftype_t ks = luaL_checkinteger(L, 2);
	if (ks >= NUMKARTSTUFF)
		return luaL_error(L, LUA_QL("kartstufftype_t") " cannot be %u", ks);
	lua_pushinteger(L, kartstuff[ks]);
	return 1;
}

// kartstuff, ks, value -> kartstuff[ks] = value
static int kartstuff_set(lua_State *L)
{
	INT32 *kartstuff = *((INT32 **)luaL_checkudata(L, 1, META_KARTSTUFF));
	kartstufftype_t ks = luaL_checkinteger(L, 2);
	INT32 i = (INT32)luaL_checkinteger(L, 3);
	if (ks >= NUMKARTSTUFF)
		return luaL_error(L, LUA_QL("kartstufftype_t") " cannot be %u", ks);
	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");
	if (hook_cmd_running)
		return luaL_error(L, "Do not alter player_t in BuildCMD code!");
	kartstuff[ks] = i;
	return 0;
}

// #kartstuff -> NUMKARTSTUFF
FUNCINLINE static ATTRINLINE int kartstuff_len(lua_State *L)
{
	lua_pushinteger(L, NUMKARTSTUFF);
	return 1;
}

#define NOFIELD luaL_error(L, LUA_QL("ticcmd_t") " has no field named " LUA_QS, lua_tostring(L, 2))
#define NOSET luaL_error(L, LUA_QL("ticcmd_t") " field " LUA_QS " should not be set directly.", lua_tostring(L, 2))

enum ticcmd_e
{
	ticcmd_forwardmove,
	ticcmd_sidemove,
	ticcmd_angleturn,
	ticcmd_aiming,
	ticcmd_buttons,
	ticcmd_driftturn,
	ticcmd_latency,
};

static const char *const ticcmd_opt[] = {
	"forwardmove",
	"sidemove",
	"angleturn",
	"aiming",
	"buttons",
	"driftturn",
	"latency",
	NULL,
};

static int ticcmd_fields_ref = LUA_NOREF;

static int ticcmd_get(lua_State *L)
{
	ticcmd_t *cmd = *((ticcmd_t **)luaL_checkudata(L, 1, META_TICCMD));
	enum ticcmd_e field = Lua_optoption(L, 2, -1, ticcmd_fields_ref);
	if (!cmd)
		return LUA_ErrInvalid(L, "player_t");

	switch (field)
	{
	case ticcmd_forwardmove:
		lua_pushinteger(L, cmd->forwardmove);
		break;
	case ticcmd_sidemove:
		lua_pushinteger(L, cmd->sidemove);
		break;
	case ticcmd_angleturn:
		lua_pushinteger(L, cmd->angleturn);
		break;
	case ticcmd_aiming:
		lua_pushinteger(L, cmd->aiming);
		break;
	case ticcmd_buttons:
		lua_pushinteger(L, cmd->buttons);
		break;
	case ticcmd_driftturn:
		lua_pushinteger(L, cmd->driftturn);
		break;
	case ticcmd_latency:
		lua_pushinteger(L, cmd->latency);
		break;
	default:
		return NOFIELD;
	}

	return 1;
}

static int ticcmd_set(lua_State *L)
{
	ticcmd_t *cmd = *((ticcmd_t **)luaL_checkudata(L, 1, META_TICCMD));
	enum ticcmd_e field = Lua_optoption(L, 2, -1, ticcmd_fields_ref);
	if (!cmd)
		return LUA_ErrInvalid(L, "ticcmd_t");

	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");

	switch (field)
	{
	case ticcmd_forwardmove:
		cmd->forwardmove = (SINT8)luaL_checkinteger(L, 3);
		break;
	case ticcmd_sidemove:
		cmd->sidemove = (SINT8)luaL_checkinteger(L, 3);
		break;
	case ticcmd_angleturn:
		cmd->angleturn = (INT16)luaL_checkinteger(L, 3);
		break;
	case ticcmd_aiming:
		cmd->aiming = (INT16)luaL_checkinteger(L, 3);
		break;
	case ticcmd_buttons:
		cmd->buttons = (UINT16)luaL_checkinteger(L, 3);
		break;
	case ticcmd_driftturn:
		cmd->driftturn = (INT16)luaL_checkinteger(L, 3);
		break;
	case ticcmd_latency:
		return NOSET;
	default:
		return NOFIELD;
	}

	return 0;
}

#undef NOFIELD
#undef NOSET

int LUA_PlayerLib(lua_State *L)
{
	player_fields_ref = Lua_CreateFieldTable(L, player_opt);

	luaL_newmetatable(L, META_PLAYER);
		lua_pushcfunction(L, player_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, player_set);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, player_num);
		lua_setfield(L, -2, "__len");
	lua_pop(L,1);

	luaL_newmetatable(L, META_POWERS);
		lua_pushcfunction(L, power_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, power_set);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, power_len);
		lua_setfield(L, -2, "__len");
	lua_pop(L,1);

	luaL_newmetatable(L, META_KARTSTUFF);
		lua_pushcfunction(L, kartstuff_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, kartstuff_set);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, kartstuff_len);
		lua_setfield(L, -2, "__len");
	lua_pop(L,1);

	luaL_newmetatable(L, META_TICCMD);
		lua_pushcfunction(L, ticcmd_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, ticcmd_set);
		lua_setfield(L, -2, "__newindex");
	lua_pop(L,1);

	ticcmd_fields_ref = Lua_CreateFieldTable(L, ticcmd_opt);

	lua_newuserdata(L, 0);
		lua_createtable(L, 0, 2);
			lua_pushcfunction(L, lib_getPlayer);
			lua_setfield(L, -2, "__index");

			lua_pushcfunction(L, lib_lenPlayer);
			lua_setfield(L, -2, "__len");
		lua_setmetatable(L, -2);
	lua_setglobal(L, "players");

	// push displayplayers in the same fashion
	lua_newuserdata(L, 0);
		lua_createtable(L, 0, 2);
			lua_pushcfunction(L, lib_getDisplayplayers);
			lua_setfield(L, -2, "__index");

			lua_pushcfunction(L, lib_lenDisplayplayers);
			lua_setfield(L, -2, "__len");
		lua_setmetatable(L, -2);
	lua_setglobal(L, "displayplayers");

	return 0;
}
