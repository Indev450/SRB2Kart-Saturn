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

#include "lua_udatalib.h"

int player_name_getter(lua_State *L);
int player_name_noset(lua_State *L);
int player_mo_getter(lua_State *L);
int player_mo_setter(lua_State *L);
int player_cmd_noset(lua_State *L);
int player_aiming_setter(lua_State *L);
int player_powers_noset(lua_State *L);
int player_kartstuff_noset(lua_State *L);
int player_skincolor_setter(lua_State *L);
int player_axis_setter(lua_State *L);
int player_capsule_setter(lua_State *L);
int player_awayviewmobj_setter(lua_State *L);
int player_awayviewtics_setter(lua_State *L);
int player_bot_noset(lua_State *L);
int player_splitscreenindex_noset(lua_State *L);
int player_ping_getter(lua_State *L);
int player_ping_noset(lua_State *L);
int player_localskin_getter(lua_State *L);
int player_localskin_setter(lua_State *L);

// Non synch safe!
int player_sliproll_getter(lua_State *L);
int player_sliproll_noset(lua_State *L);

#define FIELD(type, field_name, getter, setter) { #field_name, offsetof(type, field_name), getter, setter }
static const udata_field_t player_fields[] = {
    // Player doesn't actually have field "name" so macro fails. Need to declare field manually.
    { "name", 0, player_name_getter, player_name_noset },
    FIELD(player_t, mo,               player_mo_getter,            player_mo_setter),
    FIELD(player_t, cmd,              udatalib_getter_cmd,         player_cmd_noset),
    FIELD(player_t, playerstate,      udatalib_getter_playerstate, udatalib_setter_playerstate),
    FIELD(player_t, viewz,            udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, viewheight,       udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, aiming,           udatalib_getter_angle,       player_aiming_setter),
    FIELD(player_t, health,           udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, pity,             udatalib_getter_sint8,       udatalib_setter_sint8),
    FIELD(player_t, currentweapon,    udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, ringweapons,      udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, powers,           udatalib_getter_powers,      player_powers_noset),
    FIELD(player_t, kartstuff,        udatalib_getter_kartstuff,   player_kartstuff_noset),
    FIELD(player_t, frameangle,       udatalib_getter_angle,       udatalib_setter_angle),
    FIELD(player_t, pflags,           udatalib_getter_pflags,      udatalib_setter_pflags),
    FIELD(player_t, panim,            udatalib_getter_panim,       udatalib_setter_panim),
    FIELD(player_t, flashcount,       udatalib_getter_uint16,      udatalib_setter_uint16),
    FIELD(player_t, flashpal,         udatalib_getter_uint16,      udatalib_setter_uint16),
    FIELD(player_t, skincolor,        udatalib_getter_uint8,       player_skincolor_setter),
    FIELD(player_t, localskin,        player_localskin_getter,     player_localskin_setter),
    FIELD(player_t, score,            udatalib_getter_uint32,      udatalib_setter_uint32),
    FIELD(player_t, dashspeed,        udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, dashtime,         udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, kartspeed,        udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, kartweight,       udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, charflags,        udatalib_getter_uint32,      udatalib_setter_uint32),
    FIELD(player_t, lives,            udatalib_getter_sint8,       udatalib_setter_sint8),
    FIELD(player_t, continues,        udatalib_getter_sint8,       udatalib_setter_sint8),
    FIELD(player_t, xtralife,         udatalib_getter_sint8,       udatalib_setter_sint8),
    FIELD(player_t, gotcontinue,      udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, speed,            udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, jumping,          udatalib_getter_boolean,     udatalib_setter_boolean),
    FIELD(player_t, secondjump,       udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, fly1,             udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, scoreadd,         udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, glidetime,        udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, climbing,         udatalib_getter_uint8,       udatalib_setter_sint8), // For whatever reason, original setter casted to INT32...
    FIELD(player_t, deadtimer,        udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, exiting,          udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, homing,           udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, skidtime,         udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, cmomx,            udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, cmomy,            udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, rmomx,            udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, rmomy,            udatalib_getter_fixed,       udatalib_setter_fixed),
    FIELD(player_t, numboxes,         udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, totalring,        udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, realtime,         udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, laps,             udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, ctfteam,          udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, gotflag,          udatalib_getter_uint16,      udatalib_setter_uint16),
    FIELD(player_t, weapondelay,      udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, tossdelay,        udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, starpostx,        udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, starposty,        udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, starpostz,        udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, starpostnum,      udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, starposttime,     udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, starpostangle,    udatalib_getter_angle,       udatalib_setter_angle),
    FIELD(player_t, angle_pos,        udatalib_getter_angle,       udatalib_setter_angle),
    FIELD(player_t, old_angle_pos,    udatalib_getter_angle,       udatalib_setter_angle),
    FIELD(player_t, axis1,            udatalib_getter_mobj,        player_axis_setter),
    FIELD(player_t, axis2,            udatalib_getter_mobj,        player_axis_setter),
    FIELD(player_t, bumpertime,       udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, flyangle,         udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, drilltimer,       udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, linkcount,        udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, linktimer,        udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, anotherflyangle,  udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, nightstime,       udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, drillmeter,       udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, drilldelay,       udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, bonustime,        udatalib_getter_boolean,     udatalib_setter_boolean),
    FIELD(player_t, capsule,          udatalib_getter_mobj,        player_capsule_setter),
    FIELD(player_t, mare,             udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, marebegunat,      udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, startedtime,      udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, finishedtime,     udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, finishedrings,    udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, marescore,        udatalib_getter_uint32,      udatalib_setter_uint32),
    FIELD(player_t, lastmarescore,    udatalib_getter_uint32,      udatalib_setter_uint32),
    FIELD(player_t, lastmare,         udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, maxlink,          udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, texttimer,        udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, textvar,          udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, lastsidehit,      udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, lastlinehit,      udatalib_getter_int16,       udatalib_setter_int16),
    FIELD(player_t, losstime,         udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, timeshit,         udatalib_getter_uint8,       udatalib_setter_uint8), // Haha, time shit, funni
    FIELD(player_t, onconveyor,       udatalib_getter_int32,       udatalib_setter_int32),
    FIELD(player_t, awayviewmobj,     udatalib_getter_mobj,        player_awayviewmobj_setter),
    FIELD(player_t, awayviewtics,     udatalib_getter_int32,       player_awayviewtics_setter),
    FIELD(player_t, awayviewaiming,   udatalib_getter_angle,       udatalib_setter_angle),
    FIELD(player_t, spectator,        udatalib_getter_boolean,     udatalib_setter_boolean_nocheck), // ffs
    FIELD(player_t, bot,              udatalib_getter_uint8,       player_bot_noset),
    FIELD(player_t, jointime,         udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, spectatorreentry, udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, grieftime,        udatalib_getter_tic,         udatalib_setter_tic),
    FIELD(player_t, griefstrikes,     udatalib_getter_uint8,       udatalib_setter_uint8),
    FIELD(player_t, splitscreenindex, udatalib_getter_uint8,       player_splitscreenindex_noset),
#ifdef HWRENDER
    FIELD(player_t, fovadd,           udatalib_getter_fixed,       udatalib_setter_fixed), // Mmm yeah thats definitely synch safe
#endif
    // Same as player.name
	{ "sliproll", 0, player_sliproll_getter, player_sliproll_noset },
    { "ping", 0, player_ping_getter, player_ping_noset }, // Hmm originally setter doesn't exist so data is written as unreachable custom field...
    { NULL, 0, NULL, NULL },
};
#undef FIELD

// Implement udatalib simple getters
#define pushplayer(L, player) LUA_PushUserdata(L, player, META_PLAYER)
int udatalib_getter_player(lua_State *L)
UDATALIB_SIMPLE_GETTER(player_t*, pushplayer)
#undef pushplayer

#define pushcmd(L, cmd) LUA_PushUserdata(L, &cmd, META_TICCMD)
int udatalib_getter_cmd(lua_State *L)
UDATALIB_SIMPLE_GETTER(ticcmd_t, pushcmd)
#undef pushcmd

int udatalib_getter_playerstate(lua_State *L)
UDATALIB_SIMPLE_GETTER(playerstate_t, lua_pushinteger)

int udatalib_setter_playerstate(lua_State *L)
UDATALIB_SIMPLE_SETTER(playerstate_t, luaL_checkinteger)

#define pushpowers(L, powers) LUA_PushUserdata(L, &powers, META_POWERS)
int udatalib_getter_powers(lua_State *L)
UDATALIB_SIMPLE_GETTER(UINT16, pushpowers)
#undef pushpowers

#define pushkartstuff(L, kartstuff) LUA_PushUserdata(L, &kartstuff, META_KARTSTUFF)
int udatalib_getter_kartstuff(lua_State *L)
UDATALIB_SIMPLE_GETTER(INT32, pushkartstuff)
#undef pushkartstuff

int udatalib_getter_pflags(lua_State *L)
UDATALIB_SIMPLE_GETTER(pflags_t, lua_pushinteger)

int udatalib_setter_pflags(lua_State *L)
UDATALIB_SIMPLE_SETTER(pflags_t, luaL_checkinteger)

int udatalib_getter_panim(lua_State *L)
UDATALIB_SIMPLE_GETTER(panim_t, lua_pushinteger)

int udatalib_setter_panim(lua_State *L)
UDATALIB_SIMPLE_SETTER(panim_t, luaL_checkinteger)
// Fields that cannot be set directly
#define NOSET(field) \
int player_ ## field ## _noset(lua_State *L) \
{ \
    return luaL_error(L, LUA_QL("player_t") " field " LUA_QS " should not be set directly.", #field); \
}

NOSET(name)
NOSET(cmd)
NOSET(powers)
NOSET(kartstuff)
NOSET(bot)
NOSET(splitscreenindex)
NOSET(ping)
NOSET(sliproll)

#undef NOSET

// Rest of getters/setters with arbitary logic


#define GETPLAYER() (player_t*)lua_touserdata(L, 1)

int player_name_getter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    lua_pushstring(L, player_names[plr-players]);

    return 1;
}

int player_mo_getter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    if (plr->spectator)
        lua_pushnil(L);
    else
        LUA_PushUserdata(L, plr->mo, META_MOBJ);

    return 1;
}

int player_mo_setter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    mobj_t *newmo = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
    plr->mo->player = NULL; // remove player pointer from old mobj
    (newmo->player = plr)->mo = newmo; // set player pointer for new mobj, and set new mobj as the player's mobj

    return 0;
}

int player_aiming_setter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    plr->aiming = luaL_checkangle(L, 2);
    if (plr == &players[consoleplayer])
        localaiming[0] = plr->aiming;
    else if (plr == &players[displayplayers[1]])
        localaiming[1] = plr->aiming;
    else if (plr == &players[displayplayers[2]])
        localaiming[2] = plr->aiming;
    else if (plr == &players[displayplayers[3]])
        localaiming[3] = plr->aiming;

    return 0;
}

int player_skincolor_setter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    UINT8 newcolor = (UINT8)luaL_checkinteger(L,2);
    if (newcolor >= MAXSKINCOLORS)
        return luaL_error(L, "player.skincolor %d out of range (0 - %d).", newcolor, MAXSKINCOLORS-1);
    plr->skincolor = newcolor;

    return 0;
}

int player_localskin_getter(lua_State *L)
{
	player_t *plr = GETPLAYER();

	if (plr->localskin)
		lua_pushstring(L, (plr->skinlocal ? localskins : skins)[plr->localskin - 1].name);
	else
		lua_pushnil(L);

	return 1;
}

int player_localskin_setter(lua_State *L)
{
	player_t *plr = GETPLAYER();

	SetLocalPlayerSkin(plr - players, luaL_optstring(L, 2, "none"), NULL);

	return 0;
}

int player_axis_setter(lua_State *L)
{
    mobj_t **axis;

    UDATALIB_GETFIELD(mobj_t*, axis);

	P_SetTarget(axis, *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ)));

    return 0;
}

int player_capsule_setter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    mobj_t *mo = NULL;
    if (!lua_isnil(L, 2))
        mo = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
    P_SetTarget(&plr->capsule, mo);

    return 0;
}

// Probably can do same thing as with axis1 and axis2
int player_awayviewmobj_setter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    mobj_t *mo = NULL;
    if (!lua_isnil(L, 2))
        mo = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
    P_SetTarget(&plr->awayviewmobj, mo);

    return 0;
}

int player_awayviewtics_setter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    plr->awayviewtics = (INT32)luaL_checkinteger(L, 2);
    if (plr->awayviewtics && !plr->awayviewmobj) // awayviewtics must ALWAYS have an awayviewmobj set!!
        P_SetTarget(&plr->awayviewmobj, plr->mo); // but since the script might set awayviewmobj immediately AFTER setting awayviewtics, use player mobj as filler for now.

    return 0;
}

int player_ping_getter(lua_State *L)
{
    player_t *plr = GETPLAYER();

    lua_pushinteger(L, playerpingtable[( plr - players )]);

    return 1;
}

int player_sliproll_getter(lua_State *L)
{
	player_t *plr = GETPLAYER();

	lua_pushangle(L, R_PlayerSliptideAngle(plr));

	return 1;
}

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
static int lib_lenPlayer(lua_State *L)
{
	lua_pushinteger(L, MAXPLAYERS);
	return 1;
}

// Same deal as the three functions above but for displayplayers

static int lib_iterateDisplayplayers(lua_State *L)
{
	INT32 i = -1;
	INT32 temp = -1;
	INT32 iter = 0;

	if (lua_gettop(L) < 2)
	{
		//return luaL_error(L, "Don't call displayplayers.iterate() directly, use it as 'for player in displayplayers.iterate do <block> end'.");
		lua_pushcfunction(L, lib_iterateDisplayplayers);
		return 1;
	}
	lua_settop(L, 2);
	lua_remove(L, 1); // state is unused.
	if (!lua_isnil(L, 1))
	{
		temp = (INT32)(*((player_t **)luaL_checkudata(L, 1, META_PLAYER)) - players);	// get the player # of the last iterated player.

		// @FIXME:
		// I didn't quite find a better way for this; Here, we go back to which player in displayplayers we last iterated to resume the for loop below for this new function call
		// I don't understand enough about how the Lua stacks work to get this to work in possibly a single line.
		// So anyone feel free to correct this!

		for (; iter < MAXSPLITSCREENPLAYERS; iter++)
		{
			if (displayplayers[iter] == temp)
			{
				i = iter;
				break;
			}
		}
	}

	for (i++; i < MAXSPLITSCREENPLAYERS; i++)
	{
		if (i > splitscreen || !playeringame[displayplayers[i]])
			return 0;	// Stop! There are no more players for us to go through. There will never be a player gap in displayplayers.

		if (!players[displayplayers[i]].mo)
			continue;
		LUA_PushUserdata(L, &players[displayplayers[i]], META_PLAYER);
		lua_pushinteger(L, i);	// push this to recall what number we were on for the next function call. I suppose this also means you can retrieve the splitscreen player number with 'for p, n in displayplayers.iterate'!
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
		lua_pushcfunction(L, lib_iterateDisplayplayers);
		return 1;
	}
	return 0;
}

// #displayplayers -> MAXSPLITSCREENPLAYERS
static int lib_lenDisplayplayers(lua_State *L)
{
	lua_pushinteger(L, MAXSPLITSCREENPLAYERS);
	return 1;
}

static int player_get(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	const char *field = luaL_checkstring(L, 2); // Still gotta have 2 strcmp's

	if (!plr) {
		if (fastcmp(field,"valid")) {
			lua_pushboolean(L, false);
			return 1;
		}
		return LUA_ErrInvalid(L, "player_t");
	} else if (fastcmp(field,"valid")) {
		lua_pushboolean(L, true);
        return 1;
    }

    lua_getmetatable(L, 1);

    lua_pushvalue(L, 2); // Push field name
    lua_rawget(L, -2); // Get getter/setter table from metatable

    // If field exists, run getter for it
    if (!lua_isnil(L, -1)) {
        //CONS_Printf("Running getter for field %s\n", field);
        lua_rawgeti(L, -1, UDATALIB_GETTER);
        lua_pushlightuserdata(L, plr);
        lua_call(L, 1, 1);
        //CONS_Printf("Getter returned %s\n", lua_typename(L, lua_type(L, -1)));
        return 1;
    }

    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
    I_Assert(lua_istable(L, -1));
    lua_pushlightuserdata(L, plr);
    lua_rawget(L, -2);
    if (!lua_istable(L, -1)) { // no extra values table
        CONS_Debug(DBG_LUA, M_GetText("'%s' has no extvars table or field named '%s'; returning nil.\n"), "player_t", field);
        return 0;
    }
    lua_getfield(L, -1, field);
    if (lua_isnil(L, -1)) // no value for this field
	{
        CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; returning nil.\n"), "player_t", field);
	}

	return 1;
}

static int player_set(lua_State *L)
{
	player_t *plr = *((player_t **)luaL_checkudata(L, 1, META_PLAYER));
	const char *field = luaL_checkstring(L, 2);
	if (!plr)
		return LUA_ErrInvalid(L, "player_t");

	if (hud_running)
		return luaL_error(L, "Do not alter player_t in HUD rendering code!");

	if (hook_cmd_running)
		return luaL_error(L, "Do not alter player_t in BuildCMD code!");

    lua_getmetatable(L, 1); // Push metatable

    lua_pushvalue(L, 2); // Push field name
    lua_rawget(L, -2); // Get getter/setter table from metatable

    // If field exists, run setter for it
    if (!lua_isnil(L, -1)) {
        //CONS_Printf("Running setter for field %s\n", luaL_checkstring(L, 2));
        lua_rawgeti(L, -1, UDATALIB_SETTER);
        lua_pushlightuserdata(L, plr);
        lua_pushvalue(L, 3);
        lua_call(L, 2, 0);
        return 0;
    }

    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
    I_Assert(lua_istable(L, -1));
    lua_pushlightuserdata(L, plr);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) {
        // This index doesn't have a table for extra values yet, let's make one.
        lua_pop(L, 1);
        CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; adding it as Lua data.\n"), "player_t", field);
        lua_newtable(L);
        lua_pushlightuserdata(L, plr);
        lua_pushvalue(L, -2); // ext value table
        lua_rawset(L, -4); // LREG_EXTVARS table
    }
    lua_pushvalue(L, 3); // value to store
    lua_setfield(L, -2, field);
    lua_pop(L, 2);

	return 0;
}

#undef NOSET

static int player_num(lua_State *L)
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
static int power_len(lua_State *L)
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
static int kartstuff_len(lua_State *L)
{
	lua_pushinteger(L, NUMKARTSTUFF);
	return 1;
}

#define NOFIELD luaL_error(L, LUA_QL("ticcmd_t") " has no field named " LUA_QS, field)
#define NOSET luaL_error(L, LUA_QL("ticcmd_t") " field " LUA_QS " should not be set directly.", field)

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
	luaL_newmetatable(L, META_PLAYER);
		lua_pushcfunction(L, player_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, player_set);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, player_num);
		lua_setfield(L, -2, "__len");

        udatalib_addfields(L, -1, player_fields);
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