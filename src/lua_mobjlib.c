// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2012-2016 by John "JTE" Muniz.
// Copyright (C) 2012-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_mobjlib.c
/// \brief mobj/thing library for Lua scripting

#include <stddef.h>

#include "doomdef.h"
#include "fastcmp.h"
#include "r_things.h"
#include "r_main.h"
#include "p_local.h"
#include "g_game.h"
#include "p_setup.h"
#include "doomdef.h"
#include "d_netcmd.h"

#include "lua_script.h"
#include "lua_libs.h"
#include "lua_hud.h" // hud_running errors
#include "lua_hook.h"	// cmd errors

#include "lua_udatalib.h"

int mobj_nosetpos_x(lua_State *L);
int mobj_nosetpos_y(lua_State *L);
int mobj_nosetpos_subsector(lua_State *L);
int mobj_nosetpos_floorz(lua_State *L);
int mobj_nosetpos_ceilingz(lua_State *L);
int mobj_snext_noset(lua_State *L);
int mobj_z_setter(lua_State *L);
int mobj_sprev_unimplemented(lua_State *L);
int mobj_angle_setter(lua_State *L);
int mobj_sloperoll_noop(lua_State *L);
int mobj_spritescale_setter(lua_State *L);
int mobj_touching_sectorlist_unimplemented(lua_State *L);
int mobj_radius_setter(lua_State *L);
int mobj_height_setter(lua_State *L);
int mobj_pmomz_setter(lua_State *L);
int mobj_state_getter(lua_State *L);
int mobj_state_setter(lua_State *L);
int mobj_flags_setter(lua_State *L);
int mobj_skin_getter(lua_State *L);
int mobj_skin_setter(lua_State *L);
int mobj_localskin_getter(lua_State *L);
int mobj_localskin_setter(lua_State *L);
int mobj_color_setter(lua_State *L);
int mobj_bnext_noset(lua_State *L);
int mobj_bprev_unimplemented(lua_State *L);
int mobj_hnext_setter(lua_State *L);
int mobj_hprev_setter(lua_State *L);
int mobj_type_setter(lua_State *L);
int mobj_info_getter(lua_State *L);
int mobj_info_noset(lua_State *L);
int mobj_target_getter(lua_State *L);
int mobj_target_setter(lua_State *L);
int mobj_player_noset(lua_State *L);
int mobj_spawnpoint_setter(lua_State *L);
int mobj_tracer_getter(lua_State *L);
int mobj_tracer_setter(lua_State *L);
int mobj_mobjnum_unimplemented(lua_State *L);
int mobj_scale_setter(lua_State *L);
int mobj_destscale_setter(lua_State *L);
int mobj_standingslope_noset(lua_State *L);
int mobj_rollsum_getter(lua_State *L);
int mobj_rollsum_noset(lua_State *L);

static const char *const array_opt[] ={"iterate",NULL};

#define FIELD(type, field_name, getter, setter) { #field_name, offsetof(type, field_name), getter, setter }
static const udata_field_t mobj_fields[] = {
    FIELD(mobj_t, x,                   udatalib_getter_fixed,      mobj_nosetpos_x),
    FIELD(mobj_t, y,                   udatalib_getter_fixed,      mobj_nosetpos_y),
    FIELD(mobj_t, z,                   udatalib_getter_fixed,      mobj_z_setter),
    FIELD(mobj_t, snext,               udatalib_getter_mobj,       mobj_snext_noset),
    FIELD(mobj_t, sprev,               mobj_sprev_unimplemented,   mobj_sprev_unimplemented),
    FIELD(mobj_t, angle,               udatalib_getter_angle,      mobj_angle_setter),
    FIELD(mobj_t, pitch,               udatalib_getter_angle,      udatalib_setter_angle),
    FIELD(mobj_t, roll,                udatalib_getter_angle,      udatalib_setter_angle),
    FIELD(mobj_t, rollangle,           udatalib_getter_angle,      udatalib_setter_angle),
    FIELD(mobj_t, sloperoll,           udatalib_getter_angle,      mobj_sloperoll_noop),
    FIELD(mobj_t, slopepitch,          udatalib_getter_angle,      mobj_sloperoll_noop),
	// Macro fails here
	{ "rollsum", 0, mobj_rollsum_getter, mobj_rollsum_noset },
    FIELD(mobj_t, sprite,              udatalib_getter_spritenum,  udatalib_setter_spritenum),
    FIELD(mobj_t, frame,               udatalib_getter_uint32,     udatalib_setter_uint32),
    FIELD(mobj_t, anim_duration,       udatalib_getter_uint16,     udatalib_setter_uint16),
    FIELD(mobj_t, spritexscale,        udatalib_getter_fixed,      mobj_spritescale_setter),
    FIELD(mobj_t, spriteyscale,        udatalib_getter_fixed,      mobj_spritescale_setter),
    FIELD(mobj_t, spritexoffset,       udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, spriteyoffset,       udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, touching_sectorlist, mobj_touching_sectorlist_unimplemented, mobj_touching_sectorlist_unimplemented),
    FIELD(mobj_t, subsector,           udatalib_getter_subsector,  mobj_nosetpos_subsector),
    FIELD(mobj_t, floorz,              udatalib_getter_fixed,      mobj_nosetpos_floorz),
    FIELD(mobj_t, ceilingz,            udatalib_getter_fixed,      mobj_nosetpos_ceilingz),
    FIELD(mobj_t, radius,              udatalib_getter_fixed,      mobj_radius_setter),
    FIELD(mobj_t, height,              udatalib_getter_fixed,      mobj_height_setter),
    FIELD(mobj_t, momx,                udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, momy,                udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, momz,                udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, pmomz,               udatalib_getter_fixed,      mobj_pmomz_setter),
    FIELD(mobj_t, tics,                udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, state,               mobj_state_getter,          mobj_state_setter),
    FIELD(mobj_t, flags,               udatalib_getter_uint32,     mobj_flags_setter),
    FIELD(mobj_t, flags2,              udatalib_getter_uint32,     udatalib_setter_uint32),
    FIELD(mobj_t, eflags,              udatalib_getter_uint32,     udatalib_setter_uint32),
    FIELD(mobj_t, skin,                mobj_skin_getter,           mobj_skin_setter),
    FIELD(mobj_t, localskin,           mobj_localskin_getter,      mobj_localskin_setter),
    FIELD(mobj_t, color,               udatalib_getter_uint8,      mobj_color_setter),
    FIELD(mobj_t, bnext,               udatalib_getter_mobj,       mobj_bnext_noset),
    FIELD(mobj_t, bprev,               mobj_bprev_unimplemented,   mobj_bprev_unimplemented),
    FIELD(mobj_t, hnext,               udatalib_getter_mobj,       mobj_hnext_setter),
    FIELD(mobj_t, hprev,               udatalib_getter_mobj,       mobj_hprev_setter),
    FIELD(mobj_t, type,                udatalib_getter_mobjtype,   mobj_type_setter),
    FIELD(mobj_t, info,                mobj_info_getter,           mobj_info_noset),
    FIELD(mobj_t, health,              udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, movedir,             udatalib_getter_angle,      udatalib_setter_angle), // Differs a bit from original setter and getter
    FIELD(mobj_t, movecount,           udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, target,              mobj_target_getter,         mobj_target_setter),
    FIELD(mobj_t, reactiontime,        udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, threshold,           udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, player,              udatalib_getter_player,     mobj_player_noset),
    FIELD(mobj_t, lastlook,            udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, spawnpoint,          udatalib_getter_mapthing,   mobj_spawnpoint_setter),
    FIELD(mobj_t, tracer,              mobj_tracer_getter,         mobj_tracer_setter),
    FIELD(mobj_t, friction,            udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, movefactor,          udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, fuse,                udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, watertop,            udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, waterbottom,         udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, mobjnum,             mobj_mobjnum_unimplemented, mobj_mobjnum_unimplemented),
    FIELD(mobj_t, scale,               udatalib_getter_fixed,      mobj_scale_setter),
    FIELD(mobj_t, destscale,           udatalib_getter_fixed,      mobj_destscale_setter),
    FIELD(mobj_t, scalespeed,          udatalib_getter_fixed,      udatalib_setter_fixed),
    FIELD(mobj_t, extravalue1,         udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, extravalue2,         udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, cusval,              udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, cvmem,               udatalib_getter_int32,      udatalib_setter_int32),
    FIELD(mobj_t, standingslope,       udatalib_getter_slope,      mobj_standingslope_noset),
    FIELD(mobj_t, colorized,           udatalib_getter_boolean,    udatalib_setter_boolean),
	FIELD(mobj_t, mirrored,           udatalib_getter_boolean,    udatalib_setter_boolean),
    FIELD(mobj_t, rollmodel,           udatalib_getter_boolean,    udatalib_setter_boolean),
    { NULL, 0, NULL, NULL },
};
#undef FIELD

// First, implement udatalib simple getters
#define pushmobj(L, mobj) LUA_PushUserdata(L, mobj, META_MOBJ)
int udatalib_getter_mobj(lua_State *L)
UDATALIB_SIMPLE_GETTER(mobj_t*, pushmobj)
#undef pushmobj

int udatalib_getter_mobjtype(lua_State *L)
UDATALIB_SIMPLE_GETTER(mobjtype_t, lua_pushinteger)

#define pushmapthing(L, mapthing) LUA_PushUserdata(L, mapthing, META_MAPTHING)
int udatalib_getter_mapthing(lua_State *L)
UDATALIB_SIMPLE_GETTER(mapthing_t*, pushmapthing)
#undef pushmapthing

// Now specific fields related to mobj positions that cannot be set directly
#define NOSETPOS(field) \
int mobj_nosetpos_ ## field(lua_State *L) \
{ \
    return luaL_error(L, LUA_QL("mobj_t") " field " LUA_QS " should not be set directly. Use " LUA_QL("P_Move") ", " LUA_QL("P_TryMove") ", or " LUA_QL("P_SetOrigin") ", or " LUA_QL("P_MoveOrigin") " instead.", #field); \
}

NOSETPOS(x)
NOSETPOS(y)
NOSETPOS(subsector)
NOSETPOS(floorz)
NOSETPOS(ceilingz)

#undef NOSETPOS

// Other fields that cannot be set directly
#define NOSET(field) \
int mobj_ ## field ## _noset(lua_State *L) \
{ \
    return luaL_error(L, LUA_QL("mobj_t") " field " LUA_QS " should not be set directly.", #field); \
}

#define NOSET_USE(field, use) \
int mobj_ ## field ## _noset(lua_State *L) \
{ \
    return luaL_error(L, LUA_QL("mobj_t") " field " LUA_QS " should not be set directly. Use " LUA_QL(use) " instead.", #field); \
}

NOSET(snext)
NOSET(bnext)
NOSET(info)
NOSET(player)
NOSET(standingslope)
NOSET_USE(rollsum, "rollangle")

#undef NOSET
#undef NOSET_USE

// Unimplemented fields (why would you need to set them like that explicitly?
// No idea, i'm keeping it for synch reasons)
#define UNIMPLEMENTED(field) \
int mobj_ ## field ## _unimplemented(lua_State *L) \
{ \
    return luaL_error(L, LUA_QL("mobj_t") " field " LUA_QS " is not implemented for Lua and cannot be accessed.", #field); \
}

UNIMPLEMENTED(sprev)
UNIMPLEMENTED(touching_sectorlist)
UNIMPLEMENTED(bprev)
UNIMPLEMENTED(mobjnum)

#undef UNIMPLEMENTED

// For some dumb reason it is valid to set sloperoll, even though it is read
// only
int mobj_sloperoll_noop(lua_State *L) { (void)L; return 0; }

// Now other getters/setters with arbitary logic

// Getters get light userdata, which in this case is just mobj_t pointer
#define GETMO() (mobj_t*)lua_touserdata(L, 1)

int mobj_z_setter(lua_State *L)
{
    mobj_t *mo = GETMO();
    // z doesn't cross sector bounds so it's okay.
    mobj_t *ptmthing = tmthing;
    mo->z = luaL_checkfixed(L, 2);
    P_CheckPosition(mo, mo->x, mo->y);
    mo->floorz = tmfloorz;
    mo->ceilingz = tmceilingz;
    P_SetTarget(&tmthing, ptmthing);

    return 0;
}

int mobj_angle_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    mo->angle = luaL_checkangle(L, 2);
    if (mo->player == &players[consoleplayer])
        localangle[0] = mo->angle;
    else if (mo->player == &players[displayplayers[1]])
        localangle[1] = mo->angle;
    else if (mo->player == &players[displayplayers[2]])
        localangle[2] = mo->angle;
    else if (mo->player == &players[displayplayers[3]])
        localangle[3] = mo->angle;
    return 0;
}

int mobj_spritescale_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    fixed_t *spritescale;
    UDATALIB_GETFIELD(fixed_t, spritescale);

    if (!mo->player)
        *spritescale = luaL_checkfixed(L, 2);
    else
    {
        // Mmm yea
        if (spritescale == &mo->spritexscale)
            mo->realxscale = luaL_checkfixed(L, 2);
        else
            mo->realyscale = luaL_checkfixed(L, 2);
    }
    return 0;
}

int mobj_radius_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    mobj_t *ptmthing = tmthing;
    mo->radius = luaL_checkfixed(L, 2);
    if (mo->radius < 0)
        mo->radius = 0;
    P_CheckPosition(mo, mo->x, mo->y);
    mo->floorz = tmfloorz;
    mo->ceilingz = tmceilingz;
    P_SetTarget(&tmthing, ptmthing);

    return 0;
}

int mobj_height_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    mobj_t *ptmthing = tmthing;
    mo->height = luaL_checkfixed(L, 2);
    if (mo->height < 0)
        mo->height = 0;
    P_CheckPosition(mo, mo->x, mo->y);
    mo->floorz = tmfloorz;
    mo->ceilingz = tmceilingz;
    P_SetTarget(&tmthing, ptmthing);

    return 0;
}

int mobj_pmomz_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    mo->pmomz = luaL_checkfixed(L, 2);
	mo->eflags |= MFE_APPLYPMOMZ;

    return 0;
}

int mobj_state_getter(lua_State *L)
{
    mobj_t *mo = GETMO();

    lua_pushinteger(L, mo->state-states);

    return 1;
}

int mobj_state_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (mo->player)
        P_SetPlayerMobjState(mo, luaL_checkinteger(L, 2));
    else
        P_SetMobjState(mo, luaL_checkinteger(L, 2));

    return 0;
}

int mobj_flags_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    UINT32 flags = luaL_checkinteger(L, 2);
    if ((flags & (MF_NOBLOCKMAP|MF_NOSECTOR)) != (mo->flags & (MF_NOBLOCKMAP|MF_NOSECTOR)))
    {
        P_UnsetThingPosition(mo);
        mo->flags = flags;
        if (flags & MF_NOSECTOR && sector_list)
        {
            P_DelSeclist(sector_list);
            sector_list = NULL;
        }
        mo->snext = NULL, mo->sprev = NULL;
        mo->bnext = NULL, mo->bprev = NULL;
        P_SetThingPosition(mo);
    }
    else
        mo->flags = flags;

    return 0;
}

int mobj_skin_getter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (!mo->skin)
		return 0;

	if (hud_running && cv_luaimmersion.value) {
			if (mo->localskin) // HUD ONLY!!!!!!!!!!
				lua_pushstring(L, ((skin_t *)mo->localskin)->name);
			else
				lua_pushstring(L, ((skin_t *)mo->skin)->name);
		} else {
			lua_pushstring(L, ((skin_t *)mo->skin)->name);
		}
    return 1;
}

int mobj_skin_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    INT32 i;
    char skin[SKINNAMESIZE+1]; // all skin names are limited to this length
    strlcpy(skin, luaL_checkstring(L, 2), sizeof skin);
    strlwr(skin); // all skin names are lowercase
    for (i = 0; i < numskins; i++)
    {
        if (fastcmp(skins[i].name, skin))
        {
            mo->skin = &skins[i];
            return 0;
        }
    }

	return luaL_error(L, "mobj.skin '%s' not found!", skin);
}

int mobj_localskin_getter(lua_State *L)
{
	mobj_t *mo = GETMO();

	if (mo->localskin)
		lua_pushstring(L, ((skin_t *)mo->localskin)->name);
	else
		lua_pushnil(L);

	return 1;
}

int mobj_localskin_setter(lua_State *L)
{
	mobj_t *mo = GETMO();

	if (mo->player)
	{
		SetLocalPlayerSkin(mo->player - players, luaL_optstring(L, 2, "none"), NULL);
	}
	else
	{
		INT32 i;
		char skin[SKINNAMESIZE+1]; // all skin names are limited to this length
		strlcpy(skin, luaL_optstring(L, 2, "none"), sizeof skin);
		strlwr(skin); // all skin names are lowercase

		if (strcasecmp(skin, "none"))
		{
			// Try localskins
			for (i = 0; i < numlocalskins; i++)
			{
				if (stricmp(localskins[i].name, skin) == 0)
				{
					mo->localskin = &localskins[i];
					mo->skinlocal = true;
					return 0;
				}
			}

			// Try other skins
			for (i = 0; i < numskins; i++)
			{
				if (fastcmp(skins[i].name, skin))
				{
					mo->localskin = &skins[i];
					mo->skinlocal = false;
					return 0;
				}
			}
		}
		else
		{
			mo->localskin = 0;
			mo->skinlocal = false;
		}
	}


	return 0;
}

int mobj_color_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    UINT8 newcolor = (UINT8)luaL_checkinteger(L, 2);
    if (newcolor >= MAXTRANSLATIONS)
        return luaL_error(L, "mobj.color %d out of range (0 - %d).", newcolor, MAXTRANSLATIONS-1);
    mo->color = newcolor;

    return 0;
}

int mobj_hnext_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (lua_isnil(L, 2))
        P_SetTarget(&mo->hnext, NULL);
    else
    {
        mobj_t *hnext = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
        P_SetTarget(&mo->hnext, hnext);
    }

    return 0;
}

int mobj_hprev_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (lua_isnil(L, 2))
        P_SetTarget(&mo->hprev, NULL);
    else
    {
        mobj_t *hprev = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
        P_SetTarget(&mo->hprev, hprev);
    }

    return 0;
}

int mobj_type_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    mobjtype_t newtype = luaL_checkinteger(L, 2);
    if (newtype >= NUMMOBJTYPES)
        return luaL_error(L, "mobj.type %d out of range (0 - %d).", newtype, NUMMOBJTYPES-1);
    mo->type = newtype;
    mo->info = &mobjinfo[newtype];
    P_SetScale(mo, mo->scale);

    return 0;
}

int mobj_info_getter(lua_State *L)
{
    mobj_t *mo = GETMO();

    LUA_PushUserdata(L, &mobjinfo[mo->type], META_MOBJINFO);

    return 1;
}

int mobj_target_getter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (mo->target && P_MobjWasRemoved(mo->target))
    { // don't put invalid mobj back into Lua.
        P_SetTarget(&mo->target, NULL);
        return 0;
    }
    LUA_PushUserdata(L, mo->target, META_MOBJ);

    return 1;
}

int mobj_target_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (lua_isnil(L, 2))
        P_SetTarget(&mo->target, NULL);
    else
    {
        mobj_t *target = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
        P_SetTarget(&mo->target, target);
    }

    return 0;
}

int mobj_spawnpoint_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (lua_isnil(L, 2))
        mo->spawnpoint = NULL;
    else
    {
        mapthing_t *spawnpoint = *((mapthing_t **)luaL_checkudata(L, 2, META_MAPTHING));
        mo->spawnpoint = spawnpoint;
    }

    return 0;
}

int mobj_tracer_getter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (mo->tracer && P_MobjWasRemoved(mo->tracer))
    { // don't put invalid mobj back into Lua.
        P_SetTarget(&mo->tracer, NULL);
        return 0;
    }
    LUA_PushUserdata(L, mo->tracer, META_MOBJ);

    return 1;
}

int mobj_tracer_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    if (lua_isnil(L, 2))
        P_SetTarget(&mo->tracer, NULL);
    else
    {
        mobj_t *tracer = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
        P_SetTarget(&mo->tracer, tracer);
    }

    return 0;
}

int mobj_scale_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    fixed_t scale = luaL_checkfixed(L, 2);
    if (scale < FRACUNIT/100)
        scale = FRACUNIT/100;
    mo->destscale = scale;
    P_SetScale(mo, scale);
    mo->old_scale = scale;

    return 0;
}

int mobj_destscale_setter(lua_State *L)
{
    mobj_t *mo = GETMO();

    fixed_t scale = luaL_checkfixed(L, 2);
    if (scale < FRACUNIT/100)
        scale = FRACUNIT/100;
    mo->destscale = scale;

    return 0;
}

// WARNING: Not synch safe!
// Don't use this field in game logic code!
int mobj_rollsum_getter(lua_State *L)
{
	mobj_t *mo = GETMO();

    angle_t pitchnroll = P_MobjPitchAndRoll(mo);

	angle_t rollsum = mo->rollangle + pitchnroll;

	if (mo->player)
	{
		rollsum += R_PlayerSliptideAngle(mo->player);
	}

	lua_pushangle(L, rollsum);

	return 1;
}

static int mobj_get(lua_State *L)
{
	mobj_t *mo = *((mobj_t **)luaL_checkudata(L, 1, META_MOBJ));

    // We still gonna have 2 strcmp for "valid" field
    const char *field = luaL_checkstring(L, 2);

	lua_settop(L, 2);

	if (!mo) {
		if (fastcmp(field, "valid")) {
			lua_pushboolean(L, 0);
			return 1;
		}
		return LUA_ErrInvalid(L, "mobj_t");
    } else if (fastcmp(field, "valid")) {
        lua_pushboolean(L, 1);
        return 1;
    }

    lua_getmetatable(L, 1);

    lua_pushvalue(L, 2); // Push field name
    lua_rawget(L, -2); // Get getter/setter table from metatable

    // If field exists, run getter for it
    if (!lua_isnil(L, -1)) {
        //CONS_Printf("Running getter for field %s\n", field);
        lua_rawgeti(L, -1, UDATALIB_GETTER);
        lua_pushlightuserdata(L, mo);
        lua_call(L, 1, 1);
        //CONS_Printf("Getter returned %s\n", lua_typename(L, lua_type(L, -1)));
        return 1;
    }

    lua_pop(L, 1);

    //CONS_Printf("Getting custom field %s\n", field);

    // Othervise, return extra value
    // extra custom variables in Lua memory
    lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
    I_Assert(lua_istable(L, -1));
    lua_pushlightuserdata(L, mo);
    lua_rawget(L, -2);
    if (!lua_istable(L, -1)) { // no extra values table
        CONS_Debug(DBG_LUA, M_GetText("'%s' has no extvars table or field named '%s'; returning nil.\n"), "mobj_t", lua_tostring(L, 2));
        return 0;
    }
    lua_pushvalue(L, 2); // field name
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) // no value for this field
        CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; returning nil.\n"), "mobj_t", lua_tostring(L, 2));

    return 1;
}

#define NOSET luaL_error(L, LUA_QL("mobj_t") " field " LUA_QS " should not be set directly.", mobj_opt[field])
#define NOSETPOS luaL_error(L, LUA_QL("mobj_t") " field " LUA_QS " should not be set directly. Use " LUA_QL("P_Move") ", " LUA_QL("P_TryMove") ", or " LUA_QL("P_SetOrigin") ", or " LUA_QL("P_MoveOrigin") " instead.", mobj_opt[field])
static int mobj_set(lua_State *L)
{
	mobj_t *mo = *((mobj_t **)luaL_checkudata(L, 1, META_MOBJ));

    lua_settop(L, 3);

	if (!mo)
		return LUA_ErrInvalid(L, "mobj_t");

    if (hud_running)
        return luaL_error(L, "Do not alter mobj_t in HUD rendering code!");

    if (hook_cmd_running)
        return luaL_error(L, "Do not alter mobj_t in BuildCMD code!");

    lua_getmetatable(L, 1); // Push metatable

    lua_pushvalue(L, 2); // Push field name
    lua_rawget(L, -2); // Get getter/setter table from metatable

    // If field exists, run setter for it
    if (!lua_isnil(L, -1)) {
        //CONS_Printf("Running setter for field %s\n", luaL_checkstring(L, 2));
        lua_rawgeti(L, -1, UDATALIB_SETTER);
        lua_pushlightuserdata(L, mo);
        lua_pushvalue(L, 3);
        lua_call(L, 2, 0);
        return 0;
    }

    lua_pop(L, 1);

    //CONS_Printf("Adding custom field %s\n", luaL_checkstring(L, 2));

    // Otherwise, set custom field

    lua_getfield(L, LUA_REGISTRYINDEX, LREG_EXTVARS);
    I_Assert(lua_istable(L, -1));
    lua_pushlightuserdata(L, mo);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) {
        // This index doesn't have a table for extra values yet, let's make one.
        lua_pop(L, 1);
        CONS_Debug(DBG_LUA, M_GetText("'%s' has no field named '%s'; adding it as Lua data.\n"), "mobj_t", lua_tostring(L, 2));
        lua_newtable(L);
        lua_pushlightuserdata(L, mo);
        lua_pushvalue(L, -2); // ext value table
        lua_rawset(L, -4); // LREG_EXTVARS table
    }
    lua_pushvalue(L, 2); // key
    lua_pushvalue(L, 3); // value to store
    lua_settable(L, -3);
    lua_pop(L, 2);

    return 0;
}

#undef UNIMPLEMENTED
#undef NOSET
#undef NOSETPOS
#undef NOFIELD

enum mapthing_e {
	mapthing_valid = 0,
	mapthing_x,
	mapthing_y,
	mapthing_angle,
	mapthing_type,
	mapthing_options,
	mapthing_z,
	mapthing_extrainfo,
	mapthing_mobj,
};

const char *const mapthing_opt[] = {
	"valid",
	"x",
	"y",
	"angle",
	"type",
	"options",
	"z",
	"extrainfo",
	"mobj",
	NULL,
};

static int mapthing_fields_ref = LUA_NOREF;

static int mapthing_get(lua_State *L)
{
	mapthing_t *mt = *((mapthing_t **)luaL_checkudata(L, 1, META_MAPTHING));
	enum mapthing_e field = Lua_optoption(L, 2, -1, mapthing_fields_ref);
	lua_settop(L, 2);

	if (!mt) {
		if (field == mapthing_valid) {
			lua_pushboolean(L, false);
			return 1;
		}
		if (devparm)
			return luaL_error(L, "accessed mapthing_t doesn't exist anymore.");
		return 0;
	}

	switch (field)
	{
		case mapthing_valid:
			lua_pushboolean(L, true);
			break;
		case mapthing_x:
			lua_pushinteger(L, mt->x);
			break;
		case mapthing_y:
			lua_pushinteger(L, mt->y);
			break;
		case mapthing_angle:
			lua_pushinteger(L, mt->angle);
			break;
		case mapthing_type:
			lua_pushinteger(L, mt->type);
			break;
		case mapthing_options:
			lua_pushinteger(L, mt->options);
			break;
		case mapthing_z:
			lua_pushinteger(L, mt->z);
			break;
		case mapthing_extrainfo:
			lua_pushinteger(L, mt->extrainfo);
			break;
		case mapthing_mobj:
			LUA_PushUserdata(L, mt->mobj, META_MOBJ);
			break;
		default:
			if (devparm)
				return luaL_error(L, LUA_QL("mapthing_t") " has no field named " LUA_QS, field);
			else
				return 0;
	}

	return 1;
}

static int mapthing_set(lua_State *L)
{
	mapthing_t *mt = *((mapthing_t **)luaL_checkudata(L, 1, META_MAPTHING));
	enum mapthing_e field = Lua_optoption(L, 2, -1, mapthing_fields_ref);
	lua_settop(L, 3);

	if (!mt)
		return luaL_error(L, "accessed mapthing_t doesn't exist anymore.");

	if (hud_running)
		return luaL_error(L, "Do not alter mapthing_t in HUD rendering code!");
	if (hook_cmd_running)
		return luaL_error(L, "Do not alter mapthing_t in BuildCMD code!");

	switch (field)
	{
		case mapthing_x:
			mt->x = (INT16)luaL_checkinteger(L, 3);
			break;
		case mapthing_y:
			mt->y = (INT16)luaL_checkinteger(L, 3);
			break;
		case mapthing_angle:
			mt->angle = (INT16)luaL_checkinteger(L, 3);
			break;
		case mapthing_type:
			mt->type = (UINT16)luaL_checkinteger(L, 3);
			break;
		case mapthing_options:
			mt->options = (UINT16)luaL_checkinteger(L, 3);
			break;
		case mapthing_z:
			mt->z = (INT16)luaL_checkinteger(L, 3);
			break;
		case mapthing_extrainfo:
		{
			INT32 extrainfo = luaL_checkinteger(L, 3);
			if (extrainfo & ~15)
				return luaL_error(L, "mapthing_t extrainfo set %d out of range (%d - %d)", extrainfo, 0, 15);
			mt->extrainfo = (UINT8)extrainfo;
			break;
		}
		case mapthing_mobj:
			mt->mobj = *((mobj_t **)luaL_checkudata(L, 3, META_MOBJ));
			break;
		default:
			return luaL_error(L, LUA_QL("mapthing_t") " has no field named " LUA_QS, field);
	}

	return 0;
}

static int lib_iterateMapthings(lua_State *L)
{
	size_t i = 0;
	if (lua_gettop(L) < 2)
		return luaL_error(L, "Don't call mapthings.iterate() directly, use it as 'for mapthing in mapthings.iterate do <block> end'.");
	lua_settop(L, 2);
	lua_remove(L, 1); // state is unused.
	if (!lua_isnil(L, 1))
		i = (size_t)(*((mapthing_t **)luaL_checkudata(L, 1, META_MAPTHING)) - mapthings) + 1;
	if (i < nummapthings)
	{
		LUA_PushUserdata(L, &mapthings[i], META_MAPTHING);
		return 1;
	}
	return 0;
}

static int lib_getMapthing(lua_State *L)
{
	int field;
	lua_settop(L, 2);
	lua_remove(L, 1); // dummy userdata table is unused.
	if (lua_isnumber(L, 1))
	{
		size_t i = lua_tointeger(L, 1);
		if (i >= nummapthings)
			return 0;
		LUA_PushUserdata(L, &mapthings[i], META_MAPTHING);
		return 1;
	}
	field = luaL_checkoption(L, 1, NULL, array_opt);
	switch(field)
	{
	case 0: // iterate
		lua_pushcfunction(L, lib_iterateMapthings);
		return 1;
	}
	return 0;
}

static int lib_nummapthings(lua_State *L)
{
	lua_pushinteger(L, nummapthings);
	return 1;
}

int LUA_MobjLib(lua_State *L)
{
	luaL_newmetatable(L, META_MOBJ);
		lua_pushcfunction(L, mobj_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, mobj_set);
		lua_setfield(L, -2, "__newindex");

        udatalib_addfields(L, -1, mobj_fields);
	lua_pop(L,1);

	luaL_newmetatable(L, META_MAPTHING);
		lua_pushcfunction(L, mapthing_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, mapthing_set);
		lua_setfield(L, -2, "__newindex");
	lua_pop(L,1);

	mapthing_fields_ref = Lua_CreateFieldTable(L, mapthing_opt);

	lua_newuserdata(L, 0);
		lua_createtable(L, 0, 2);
			lua_pushcfunction(L, lib_getMapthing);
			lua_setfield(L, -2, "__index");

			lua_pushcfunction(L, lib_nummapthings);
			lua_setfield(L, -2, "__len");
		lua_setmetatable(L, -2);
	lua_setglobal(L, "mapthings");
	return 0;
}
