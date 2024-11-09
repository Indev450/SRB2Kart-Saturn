// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2014-2016 by John "JTE" Muniz.
// Copyright (C) 2014-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_hudlib.c
/// \brief custom HUD rendering library for Lua scripting

#include "doomdef.h"
#include "fastcmp.h"
#include "r_defs.h"
#include "r_local.h"
#include "st_stuff.h" // hudinfo[]
#include "y_inter.h"
#include "g_game.h"
#include "i_video.h" // rendermode
#include "p_local.h" // camera_t
#include "screen.h" // screen width/height
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "k_kart.h"

#include "lua_script.h"
#include "lua_libs.h"
#include "lua_hud.h"

#define HUDONLY if (!hud_running) return luaL_error(L, "HUD rendering code should not be called outside of rendering hooks!");

boolean hud_running = false;

boolean hud_interpolate = false;
UINT8 hud_interptag = 0;
UINT32 hud_interpcounter = 0;
boolean hud_interpstring = false;
boolean hud_interplatch = false;
static UINT8 hud_enabled[(hud_MAX/8)+1];

static UINT8 hudAvailable; // hud hooks field

static UINT8 camnum = 1;

// must match enum hud in lua_hud.h
static const char *const hud_disable_options[] = {
	"stagetitle",
	"textspectator",

	"time",
	"gametypeinfo",	// Bumpers / Karma / Laps depending on gametype
	"minimap",
	"item",
	"position",
	"check",		// "CHECK" f-zero indicator
	"minirankings",	// Gametype rankings to the left
	"battlerankingsbumpers",	// bumper drawer for battle. Useful if you want to make a custom battle gamemode without bumpers being involved.
	"battlefullscreen",			// battlefullscreen func (WAIT, ATTACK OR PROTECT ...)
	"battlecomebacktimer",		// come back timer in battlefullscreen
	"wanted",
	"speedometer",
	"statdisplay",
	"nametags",
	"driftgauge",
	"freeplay",
	"rankings",
	NULL};

enum hudinfo {
	hudinfo_x = 0,
	hudinfo_y
};

static const char *const hudinfo_opt[] = {
	"x",
	"y",
	NULL};

enum patch {
	patch_valid = 0,
	patch_width,
	patch_height,
	patch_leftoffset,
	patch_topoffset
};
static const char *const patch_opt[] = {
	"valid",
	"width",
	"height",
	"leftoffset",
	"topoffset",
	NULL};

enum hudhook {
	hudhook_game = 0,
	hudhook_scores = 1,
	hudhook_intermission = 2,
	hudhook_vote = 3,
};
static const char *const hudhook_opt[] = {
	"game",
	"scores",
	"intermission",
	"vote",
	NULL};

static int patch_fields_ref = LUA_NOREF;

// alignment types for v.drawString
enum align {
	align_left = 0,
	align_center,
	align_right,
	align_fixed,
	align_small,
	align_smallright,
	align_thin,
	align_thinright
};
static const char *const align_opt[] = {
	"left",
	"center",
	"right",
	"fixed",
	"small",
	"small-right",
	"thin",
	"thin-right",
	NULL};

// width types for v.stringWidth
enum widtht {
	widtht_normal = 0,
	widtht_small,
	widtht_thin
};
static const char *const widtht_opt[] = {
	"normal",
	"small",
	"thin",
	NULL};

enum cameraf {
	camera_chase = 0,
	camera_aiming,
	camera_x,
	camera_y,
	camera_z,
	camera_angle,
	camera_subsector,
	camera_floorz,
	camera_ceilingz,
	camera_radius,
	camera_height,
	camera_momx,
	camera_momy,
	camera_momz,
	camera_pnum
};


static const char *const camera_opt[] = {
	"chase",
	"aiming",
	"x",
	"y",
	"z",
	"angle",
	"subsector",
	"floorz",
	"ceilingz",
	"radius",
	"height",
	"momx",
	"momy",
	"momz",
	"pnum",
	NULL};

enum hudpatch {
	hudpatch_item = 0,
	hudpatch_itemmul
};

static const char *const hud_patch_options[] = {
	"item",
	"itemmul",
	NULL};

enum hudoffsets {
	hudoffsets_item = 0,
	hudoffsets_time,
	hudoffsets_gametypeinfo,
	hudoffsets_countdown,
	hudoffsets_speedometer,
	hudoffsets_position,
	hudoffsets_minirankings,
	hudoffsets_startcountdown,
	hudoffsets_check,
	hudoffsets_minimap,
	hudoffsets_wanted,
	hudoffsets_statdisplay
};

static const char *const hud_offsets_options[] = {
	"item",
	"time",
	"gametypeinfo",
	"countdown",
	"speedometer",
	"position",
	"minirankings",
	"startcountdown",
	"check",
	"minimap",
	"wanted",
	"statdisplay",
	NULL};

enum huddrawinfo {
	huddrawinfo_item = 0,
	huddrawinfo_gametypeinfo,
	huddrawinfo_minimap
};

static const char *const hud_drawinfo_options[] = {
	"item",
	"gametypeinfo",
	"minimap",
	NULL};

static int camera_fields_ref = LUA_NOREF;

static int lib_getHudInfo(lua_State *L)
{
	UINT32 i;
	lua_remove(L, 1);

	i = luaL_checkinteger(L, 1);
	if (i >= NUMHUDITEMS)
		return luaL_error(L, "hudinfo[] index %d out of range (0 - %d)", i, NUMHUDITEMS-1);
	LUA_PushUserdata(L, &hudinfo[i], META_HUDINFO);
	return 1;
}

static int lib_hudinfolen(lua_State *L)
{
	lua_pushinteger(L, NUMHUDITEMS);
	return 1;
}

static int hudinfo_get(lua_State *L)
{
	hudinfo_t *info = *((hudinfo_t **)luaL_checkudata(L, 1, META_HUDINFO));
	enum hudinfo field = luaL_checkoption(L, 2, hudinfo_opt[0], hudinfo_opt);
	I_Assert(info != NULL); // huditems are always valid

	switch(field)
	{
	case hudinfo_x:
		lua_pushinteger(L, info->x);
		break;
	case hudinfo_y:
		lua_pushinteger(L, info->y);
		break;
	}
	return 1;
}

static int hudinfo_set(lua_State *L)
{
	hudinfo_t *info = *((hudinfo_t **)luaL_checkudata(L, 1, META_HUDINFO));
	enum hudinfo field = luaL_checkoption(L, 2, hudinfo_opt[0], hudinfo_opt);
	I_Assert(info != NULL);

	switch(field)
	{
	case hudinfo_x:
		info->x = (INT32)luaL_checkinteger(L, 3);
		break;
	case hudinfo_y:
		info->y = (INT32)luaL_checkinteger(L, 3);
		break;
	}
	return 0;
}

static int hudinfo_num(lua_State *L)
{
	hudinfo_t *info = *((hudinfo_t **)luaL_checkudata(L, 1, META_HUDINFO));
	lua_pushinteger(L, info-hudinfo);
	return 1;
}

static int colormap_get(lua_State *L)
{
	return luaL_error(L, "colormap is not a struct.");
}

static int patch_get(lua_State *L)
{
	patch_t *patch = *((patch_t **)luaL_checkudata(L, 1, META_PATCH));
	enum patch field = Lua_optoption(L, 2, -1, patch_fields_ref);

	// patches are CURRENTLY always valid, expected to be cached with PU_STATIC
	// this may change in the future, so patch.valid still exists
	I_Assert(patch != NULL);

	switch (field)
	{
	case patch_valid:
		lua_pushboolean(L, patch != NULL);
		break;
	case patch_width:
		lua_pushinteger(L, SHORT(patch->width));
		break;
	case patch_height:
		lua_pushinteger(L, SHORT(patch->height));
		break;
	case patch_leftoffset:
		lua_pushinteger(L, SHORT(patch->leftoffset));
		break;
	case patch_topoffset:
		lua_pushinteger(L, SHORT(patch->topoffset));
		break;
	}
	return 1;
}

static int patch_set(lua_State *L)
{
	return luaL_error(L, LUA_QL("patch_t") " struct cannot be edited by Lua.");
}

static int camera_get(lua_State *L)
{
	camera_t *cam = *((camera_t **)luaL_checkudata(L, 1, META_CAMERA));
	enum cameraf field = Lua_optoption(L, 2, -1, camera_fields_ref);

	// cameras should always be valid unless I'm a nutter
	I_Assert(cam != NULL);

	switch (field)
	{
	case camera_chase:
		lua_pushboolean(L, cam->chase);
		break;
	case camera_aiming:
		lua_pushinteger(L, cam->aiming);
		break;
	case camera_x:
		lua_pushinteger(L, cam->x);
		break;
	case camera_y:
		lua_pushinteger(L, cam->y);
		break;
	case camera_z:
		lua_pushinteger(L, cam->z);
		break;
	case camera_angle:
		lua_pushinteger(L, cam->angle);
		break;
	case camera_subsector:
		LUA_PushUserdata(L, cam->subsector, META_SUBSECTOR);
		break;
	case camera_floorz:
		lua_pushinteger(L, cam->floorz);
		break;
	case camera_ceilingz:
		lua_pushinteger(L, cam->ceilingz);
		break;
	case camera_radius:
		lua_pushinteger(L, cam->radius);
		break;
	case camera_height:
		lua_pushinteger(L, cam->height);
		break;
	case camera_momx:
		lua_pushinteger(L, cam->momx);
		break;
	case camera_momy:
		lua_pushinteger(L, cam->momy);
		break;
	case camera_momz:
		lua_pushinteger(L, cam->momz);
		break;
	case camera_pnum:
		lua_pushinteger(L, camnum);
		break;
	}
	return 1;
}

//
// lib_draw
//

static int libd_patchExists(lua_State *L)
{
	HUDONLY
	lua_pushboolean(L, W_LumpExists(luaL_checkstring(L, 1)));
	return 1;
}

static int libd_cachePatch(lua_State *L)
{
	HUDONLY
	LUA_PushUserdata(L, W_CachePatchName(luaL_checkstring(L, 1), PU_STATIC), META_PATCH);
	return 1;
}

// this is structured like getSprite2Patch in vanilla 2.2
// v.getSpritePatch(skin, sprite, [frame, [angle, [rollangle]]])
static int libd_getSpritePatch(lua_State *L)
{
	UINT32 i; // sprite prefix
	INT32 skn = -1; // skin number
	UINT32 frame = 0; // 'A'
	UINT8 angle = 0;
	spritedef_t *sprdef;
	spriteframe_t *sprframe;
#ifdef ROTSPRITE
	spriteinfo_t *sprinfo;
#endif
	HUDONLY

	// mashing together the stuff from getSprite2Patch:
	// get skin first!
	if (!lua_isnoneornil(L, 1)) // ONLY do this if we have a skin
	{
		if (lua_isnumber(L, 1)) // find skin by number
		{
			skn = lua_tonumber(L, 1);
			if (skn < 0 || skn >= MAXSKINS)
				return luaL_error(L, "skin number %d out of range (0 - %d)", skn, MAXSKINS-1);
			if (skn >= numskins)
				return 0;
		}
		else // find skin by name
		{
			const char *name = luaL_checkstring(L, 1);
			for (skn = 0; skn < numskins; skn++)
				if (fastcmp(skins[skn].name, name))
					break;
			if (skn >= numskins)
				return 0;
		}
	}

	lua_remove(L, 1); // remove skin now

	if (lua_isnumber(L, 1)) // sprite number given, e.g. SPR_THOK
	{
		i = lua_tonumber(L, 1);
		if (i >= NUMSPRITES)
			return 0;
	}
	else if (lua_isstring(L, 1)) // sprite prefix name given, e.g. "THOK"
	{
		const char *name = lua_tostring(L, 1);
		for (i = 0; i < NUMSPRITES; i++)
			if (fastcmp(name, sprnames[i]))
				break;
		if (i >= NUMSPRITES)
			return 0;
	}
	else
		return 0;

	// get outta dodge if we're a playersprite and have no skin
	if ((i == SPR_PLAY) && (skn < 0))
		return luaL_error(L, "You must provide a skin for player sprites!");

	if (skn < 0) // standard sprite
	{
		sprdef = &sprites[i];
		sprinfo = &spriteinfo[i];
	}
	else // player skin
	{
		sprdef = &skins[skn].spritedef;
		sprinfo = &skins[skn].sprinfo;
	}

	// set frame number
	frame = luaL_optinteger(L, 2, 0);
	frame &= FF_FRAMEMASK; // ignore any bits that are not the actual frame, just in case
	if (frame >= sprdef->numframes)
		return 0;
	// set angle number
	sprframe = &sprdef->spriteframes[frame];
	angle = luaL_optinteger(L, 3, 1);

	// convert WAD editor angle numbers (1-8) to internal angle numbers (0-7)
	// keep 0 the same since we'll make it default to angle 1 (which is internally 0)
	// in case somebody didn't know that angle 0 really just maps all 8 angles to the same patch
	if (angle != 0)
		angle--;

	if (angle >= 8) // out of range?
		angle = (angle & 7); // modulus angle by 8

	// rotsprite?????
	if (lua_isnumber(L, 4) && (cv_spriteroll.value))
	{
		angle_t rollangle = luaL_checkangle(L, 4);
		INT32 rot = R_GetRollAngle(rollangle);

		if (rot) {
			patch_t *rotsprite = Patch_GetRotatedSprite(sprframe, frame, angle, sprframe->flip & (1<<angle), false, sprinfo, rot);
			LUA_PushUserdata(L, rotsprite, META_PATCH);
			lua_pushboolean(L, false);
			lua_pushboolean(L, true);
			return 3;
		}
	}

	// push both the patch and its "flip" value
	LUA_PushUserdata(L, W_CachePatchNum(sprframe->lumppat[angle], PU_STATIC), META_PATCH);
	lua_pushboolean(L, (sprframe->flip & (1<<angle)) != 0);
	return 2;
}

static int libd_draw(lua_State *L)
{
	INT32 x, y, flags;
	patch_t *patch;
	UINT8 *colormap = NULL;
	huddrawlist_h list;

	HUDONLY
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	patch = *((patch_t **)luaL_checkudata(L, 3, META_PATCH));
	flags = luaL_optinteger(L, 4, 0);
	if (!lua_isnoneornil(L, 5))
		colormap = *((UINT8 **)luaL_checkudata(L, 5, META_COLORMAP));

	flags &= ~V_PARAMMASK; // Don't let crashes happen.

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDraw(list, x, y, patch, flags, colormap);
	else
		V_DrawFixedPatch(x<<FRACBITS, y<<FRACBITS, FRACUNIT, flags, patch, colormap);
	return 0;
}

static int libd_drawScaled(lua_State *L)
{
	fixed_t x, y, scale;
	INT32 flags;
	patch_t *patch;
	UINT8 *colormap = NULL;
	huddrawlist_h list;

	HUDONLY
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	scale = luaL_checkinteger(L, 3);
	if (scale < 0)
		return luaL_error(L, "negative scale");
	patch = *((patch_t **)luaL_checkudata(L, 4, META_PATCH));
	flags = luaL_optinteger(L, 5, 0);
	if (!lua_isnoneornil(L, 6))
		colormap = *((UINT8 **)luaL_checkudata(L, 6, META_COLORMAP));

	flags &= ~V_PARAMMASK; // Don't let crashes happen.

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawScaled(list, x, y, scale, patch, flags, colormap);
	else
		V_DrawFixedPatch(x, y, scale, flags, patch, colormap);
	return 0;
}

// KART: draw patch on minimap from x, y coordinates on the map
static int libd_drawOnMinimap(lua_State *L)
{
	fixed_t x, y, scale;	// coordinates of the object
	patch_t *patch;	// patch we want to draw
	UINT8 *colormap = NULL;	// do we want to colormap this patch?
	boolean centered;	// the patch is centered and doesn't need readjusting on x/y coordinates.
	huddrawlist_h list;
	patch_t *AutomapPic = NULL;

	// base position of the minimap which also takes splits into account:
	INT32 MM_X, MM_Y;

	// variables used for actually drawing the icon:
	INT32 splitflags, minimaptrans;
	fixed_t amnumxpos, amnumypos;
	fixed_t amxpos, amypos;
	INT32 mm_x, mm_y;
	fixed_t patchw, patchh;

	HUDONLY	// only run this function in hud hooks
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	scale = luaL_checkinteger(L, 3);
	patch = *((patch_t **)luaL_checkudata(L, 4, META_PATCH));
	if (!lua_isnoneornil(L, 5))
		colormap = *((UINT8 **)luaL_checkudata(L, 5, META_COLORMAP));
	centered = lua_optboolean(L, 6);

	// first, check what position the mmap is supposed to be in (pasted from k_kart.c):
	MM_X = BASEVIDWIDTH - 50 + cv_mini_xoffset.value;		// 270
	MM_Y = (BASEVIDHEIGHT/2)-16 + cv_mini_yoffset.value; //  84
	if (splitscreen)
	{
		MM_Y = (BASEVIDHEIGHT/2) + cv_mini_yoffset.value;
		if (splitscreen > 1)	// 3P : bottom right
		{
			MM_X = (3*BASEVIDWIDTH/4) + cv_mini_xoffset.value;
			MM_Y = (3*BASEVIDHEIGHT/4) + cv_mini_yoffset.value;

			if (splitscreen > 2) // 4P: centered
			{
				MM_X = (BASEVIDWIDTH/2) + cv_mini_xoffset.value;
				MM_Y = (BASEVIDHEIGHT/2) + cv_mini_yoffset.value;
			}
		}
	}

	// splitscreen flags
	splitflags = (splitscreen == 3 ? 0 : V_SNAPTORIGHT);	// flags should only be 0 when it's centered (4p split)

	// translucency:
	if (timeinmap > 105)
	{
		minimaptrans = cv_kartminimap.value;
		if (timeinmap <= 113)
			minimaptrans = ((((INT32)timeinmap) - 105)*minimaptrans)/(113-105);
		if (!minimaptrans)
			return 0;
	}
	else
		return 0;

	minimaptrans = ((10-minimaptrans)<<FF_TRANSSHIFT);
	splitflags |= minimaptrans;

	if (!(splitscreen == 2))
	{
		splitflags &= ~minimaptrans;
		splitflags |= V_HUDTRANSHALF;
	}

	splitflags &= ~V_HUDTRANSHALF;
	splitflags |= V_HUDTRANS;

	// Draw the HUD only when playing in a level.
	// hu_stuff needs this, unlike st_stuff.
	if (gamestate != GS_LEVEL)
		return 0;

	if (stplyr != &players[displayplayers[0]])
		return 0;

	AutomapPic = minimapinfo.minimap_pic;

	if (!AutomapPic)
	{
		return 0; // no pic, just get outta here
	}

	// Handle offsets and stuff.
	mm_x = MM_X - (SHORT(AutomapPic->width)/2);
	mm_y = MM_Y - (SHORT(AutomapPic->height)/2);

	// let offsets transfer to the heads, too!
	if (encoremode)
	{
		mm_x += SHORT(AutomapPic->leftoffset);
	}
	else
	{
		mm_x -= SHORT(AutomapPic->leftoffset);
	}

	mm_y -= SHORT(AutomapPic->topoffset);

	// scale patch coords
	patchw = (SHORT(patch->width) * scale / 2);
	patchh = (SHORT(patch->height) * scale / 2);

	if (centered)
	{
		patchw = patchh = 0;	// patch is supposedly already centered, don't butt in.
	}

	amnumxpos = (FixedMul(x, minimapinfo.zoom) - minimapinfo.offs_x);
	amnumypos = -(FixedMul(y, minimapinfo.zoom) - minimapinfo.offs_y);

	if (encoremode)
	{
		amnumxpos = -amnumxpos;
	}

	amxpos = amnumxpos + ((mm_x + SHORT(AutomapPic->width) / 2)<<FRACBITS) - patchw;
	amypos = amnumypos + ((mm_y + SHORT(AutomapPic->height) / 2)<<FRACBITS) - patchh;

	// and NOW we can FINALLY DRAW OUR GOD DAMN PATCH :V
	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (cv_minihead.value)
	{
		amxpos += patchw / 2;
		amypos += patchh / 2;
		scale /= 2;
	}

	if (LUA_HUD_IsDrawListValid(list))
	{
		LUA_HUD_AddDrawScaled(list, amxpos, amypos, scale, patch, splitflags, colormap);
	}
	else
	{
		V_DrawFixedPatch(amxpos, amypos, scale, splitflags, patch, colormap);
	}

	return 0;
}

static int libd_drawStretched(lua_State *L)
{
	fixed_t x, y, hscale, vscale;
	INT32 flags;
	patch_t *patch;
	UINT8 *colormap = NULL;
	huddrawlist_h list;

	HUDONLY
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	hscale = luaL_checkinteger(L, 3);
	if (hscale < 0)
		return luaL_error(L, "negative horizontal scale");
	vscale = luaL_checkinteger(L, 4);
	if (vscale < 0)
		return luaL_error(L, "negative vertical scale");
	patch = *((patch_t **)luaL_checkudata(L, 5, META_PATCH));
	flags = luaL_optinteger(L, 6, 0);
	if (!lua_isnoneornil(L, 7))
		colormap = *((UINT8 **)luaL_checkudata(L, 7, META_COLORMAP));

	flags &= ~V_PARAMMASK; // Don't let crashes happen.

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawStretched(list, x, y, hscale, vscale, patch, flags, colormap);
	else
		V_DrawStretchyFixedPatch(x, y, hscale, vscale, flags, patch, colormap);
	return 0;
}

static int libd_drawNum(lua_State *L)
{
	INT32 x, y, flags, num;
	huddrawlist_h list;

	HUDONLY
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	num = luaL_checkinteger(L, 3);
	flags = luaL_optinteger(L, 4, 0);
	flags &= ~V_PARAMMASK; // Don't let crashes happen.

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawNum(list, x, y, num, flags);
	else
		V_DrawTallNum(x, y, flags, num);
	return 0;
}

static int libd_drawPaddedNum(lua_State *L)
{
	INT32 x, y, flags, num, digits;
	huddrawlist_h list;

	HUDONLY
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	num = labs(luaL_checkinteger(L, 3));
	digits = luaL_optinteger(L, 4, 2);
	flags = luaL_optinteger(L, 5, 0);
	flags &= ~V_PARAMMASK; // Don't let crashes happen.

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawPaddedNum(list, x, y, num, digits, flags);
	else
		V_DrawPaddedTallNum(x, y, flags, num, digits);
	return 0;
}


static int libd_drawPingNum(lua_State *L)
{
	INT32 x, y, flags, num;
	const UINT8 *colormap = NULL;
	huddrawlist_h list;

	HUDONLY
	x = luaL_checkinteger(L, 1);
	y = luaL_checkinteger(L, 2);
	num = luaL_checkinteger(L, 3);
	flags = luaL_optinteger(L, 4, 0);
	flags &= ~V_PARAMMASK; // Don't let crashes happen.
	if (!lua_isnoneornil(L, 5))
		colormap = *((UINT8 **)luaL_checkudata(L, 5, META_COLORMAP));

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawPingNum(list, x, y, num, flags, colormap);
	else
		V_DrawPingNum(x, y, flags, num, colormap);

	return 0;
}

static int libd_drawFill(lua_State *L)
{
	huddrawlist_h list;
	INT32 x = luaL_optinteger(L, 1, 0);
	INT32 y = luaL_optinteger(L, 2, 0);
	INT32 w = luaL_optinteger(L, 3, BASEVIDWIDTH);
	INT32 h = luaL_optinteger(L, 4, BASEVIDHEIGHT);
	INT32 c = luaL_optinteger(L, 5, 31);

	HUDONLY

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawFill(list, x, y, w, h, c);
	else
		V_DrawFill(x, y, w, h, c);
	return 0;
}

static int libd_fadeScreen(lua_State *L)
{
	UINT16 color = luaL_checkinteger(L, 1);
	UINT8 strength = luaL_checkinteger(L, 2);
	const UINT8 maxstrength = ((color & 0xFF00) ? 32 : 10);
	huddrawlist_h list;

	HUDONLY

	if (!strength)
		return 0;

	if (strength > maxstrength)
		return luaL_error(L, "%s fade strength %d out of range (0 - %d)", ((color & 0xFF00) ? "COLORMAP" : "TRANSMAP"), strength, maxstrength);

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (strength == maxstrength) // Allow as a shortcut for drawfill...
	{
		if (LUA_HUD_IsDrawListValid(list))
			LUA_HUD_AddDrawFill(list, 0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, ((color & 0xFF00) ? 31 : color));
		else
			V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, ((color & 0xFF00) ? 31 : color));

		return 0;
	}

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddFadeScreen(list, color, strength);
	else
		V_DrawFadeScreen(color, strength);

	return 0;
}

static int libd_drawString(lua_State *L)
{
	huddrawlist_h list;
	fixed_t x = luaL_checkinteger(L, 1);
	fixed_t y = luaL_checkinteger(L, 2);
	const char *str = luaL_checkstring(L, 3);
	INT32 flags = luaL_optinteger(L, 4, V_ALLOWLOWERCASE);
	enum align align = luaL_checkoption(L, 5, "left", align_opt);

	flags &= ~V_PARAMMASK; // Don't let crashes happen.

	HUDONLY

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	// okay, sorry, this is kind of ugly
	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawString(list, x, y, str, flags, align);
	else
	switch(align)
	{
	// hu_font
	case align_left:
		V_DrawString(x, y, flags, str);
		break;
	case align_center:
		V_DrawCenteredString(x, y, flags, str);
		break;
	case align_right:
		V_DrawRightAlignedString(x, y, flags, str);
		break;
	case align_fixed:
		V_DrawStringAtFixed(x, y, flags, str);
		break;
	// hu_font, 0.5x scale
	case align_small:
		V_DrawSmallString(x, y, flags, str);
		break;
	case align_smallright:
		V_DrawRightAlignedSmallString(x, y, flags, str);
		break;
	// tny_font
	case align_thin:
		V_DrawThinString(x, y, flags, str);
		break;
	case align_thinright:
		V_DrawRightAlignedThinString(x, y, flags, str);
		break;
	}
	return 0;
}

static int libd_drawKartString(lua_State *L)
{
	fixed_t x = luaL_checkinteger(L, 1);
	fixed_t y = luaL_checkinteger(L, 2);
	const char *str = luaL_checkstring(L, 3);
	INT32 flags = luaL_optinteger(L, 4, V_ALLOWLOWERCASE);
	huddrawlist_h list;

	flags &= ~V_PARAMMASK; // Don't let crashes happen.

	HUDONLY

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
	list = (huddrawlist_h) lua_touserdata(L, -1);
	lua_pop(L, 1);

	if (LUA_HUD_IsDrawListValid(list))
		LUA_HUD_AddDrawKartString(list, x, y, str, flags);
	else
		V_DrawKartString(x, y, flags, str);
	return 0;
}

static int libd_stringWidth(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	INT32 flags = luaL_optinteger(L, 2, V_ALLOWLOWERCASE);
	enum widtht widtht = luaL_checkoption(L, 3, "normal", widtht_opt);

	HUDONLY
	switch(widtht)
	{
	case widtht_normal: // hu_font
		lua_pushinteger(L, V_StringWidth(str, flags));
		break;
	case widtht_small: // hu_font, 0.5x scale
		lua_pushinteger(L, V_SmallStringWidth(str, flags));
		break;
	case widtht_thin: // tny_font
		lua_pushinteger(L, V_ThinStringWidth(str, flags));
		break;
	}
	return 1;
}

static int libd_getColormap(lua_State *L)
{
	INT32 skinnum = TC_DEFAULT;
	skincolors_t color = luaL_optinteger(L, 2, 0);
	UINT8* colormap = NULL;
	HUDONLY
	if (lua_isnoneornil(L, 1))
		; // defaults to TC_DEFAULT
	else if (lua_type(L, 1) == LUA_TNUMBER) // skin number
	{
		skinnum = (INT32)luaL_checkinteger(L, 1);
		if (skinnum < TC_BLINK || skinnum >= MAXSKINS)
			return luaL_error(L, "skin number %d is out of range (%d - %d)", skinnum, TC_BLINK, MAXSKINS-1);
	}
	else // skin name
	{
		const char *skinname = luaL_checkstring(L, 1);
		INT32 i = R_SkinAvailable(skinname);
		if (i != -1) // if -1, just default to TC_DEFAULT as above
			skinnum = i;
	}

	// all was successful above, now we generate the colormap at last!

	colormap = R_GetTranslationColormap(skinnum, color, GTC_CACHE);
	LUA_PushUserdata(L, colormap, META_COLORMAP); // push as META_COLORMAP userdata, specifically for patches to use!
	return 1;
}

static int libd_getColorHudPatch(lua_State *L)
{
	HUDONLY
	enum hudpatch option = luaL_checkoption(L, 1, NULL, hud_patch_options);
	patch_t *patch;
	UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, K_GetHudColor(), GTC_CACHE);
	boolean small, dark;

	switch (option) {
		case hudpatch_item:
			small = lua_optboolean(L, 2);
			dark = lua_optboolean(L, 3);
			patch = K_getItemBoxPatch(small, dark);
			if (!cv_colorizeditembox.value)
				colormap = NULL;
			break;
		case hudpatch_itemmul:
			small = lua_optboolean(L, 2);
			patch = K_getItemMulPatch(small);
			break;
		default:
			return 0; // you shouldn't be here
	}

	LUA_PushUserdata(L, patch, META_PATCH);
	if (colormap && K_UseColorHud())
		LUA_PushUserdata(L, colormap, META_COLORMAP);
	else
		lua_pushnil(L);

	return 2;
}

static int libd_getHudColor(lua_State *L)
{
	HUDONLY
	lua_pushinteger(L, K_GetHudColor());
	return 1;
}

static int libd_useColorHud(lua_State *L)
{
	HUDONLY
	lua_pushboolean(L, K_UseColorHud());
	return 1;
}


static int libd_width(lua_State *L)
{
	HUDONLY
	lua_pushinteger(L, vid.width); // push screen width
	return 1;
}

static int libd_height(lua_State *L)
{
	HUDONLY
	lua_pushinteger(L, vid.height); // push screen height
	return 1;
}

static int libd_dupx(lua_State *L)
{
	HUDONLY
	lua_pushinteger(L, vid.dupx); // push integral scale (patch scale)
	lua_pushfixed(L, vid.fdupx); // push fixed point scale (position scale)
	return 2;
}

static int libd_dupy(lua_State *L)
{
	HUDONLY
	lua_pushinteger(L, vid.dupy); // push integral scale (patch scale)
	lua_pushfixed(L, vid.fdupy); // push fixed point scale (position scale)
	return 2;
}

static int libd_renderer(lua_State *L)
{
	HUDONLY
	switch (rendermode) {
		case render_opengl: lua_pushliteral(L, "opengl");   break; // OpenGL renderer
		case render_soft:   lua_pushliteral(L, "software"); break; // Software renderer
		default:            lua_pushliteral(L, "none");     break; // render_none (for dedicated), in case there's any reason this should be run
	}
	return 1;
}

// 30/10/18 Lat': Get cv_translucenthud's value for HUD rendering as a normal V_xxTRANS int
// Could as well be thrown in global vars for ease of access but I guess it makes sense for it to be a HUD fn
static int libd_getlocaltransflag(lua_State *L)
{
	HUDONLY
	lua_pushinteger(L, (10-cv_translucenthud.value)*V_10TRANS);	// A bit weird that it's called "translucenthud" yet 10 is fully opaque :V
	return 1;
}

static int libd_getDrawInfo(lua_State *L)
{
	HUDONLY
	enum huddrawinfo option = luaL_checkoption(L, 1, NULL, hud_drawinfo_options);
	drawinfo_t info;

	switch(option) {
		case huddrawinfo_item:          K_getItemBoxDrawinfo(&info);  break;
		case huddrawinfo_gametypeinfo:  K_getLapsDrawinfo(&info);     break;
		case huddrawinfo_minimap:       K_getMinimapDrawinfo(&info);  break;
		default:
			return 0; // unreachable
	}

	lua_pushinteger(L, info.x);
	lua_pushinteger(L, info.y);
	lua_pushinteger(L, info.flags);
	return 3;
}

static int libd_interpolate(lua_State *L)
{
	HUDONLY

	// do type checking even if interpolation is disabled
	boolean newinterpolate;
	UINT8 newtag;

	if (lua_isnumber(L, 1))
	{
		newinterpolate = true;
		newtag = luaL_checkinteger(L, 1);
	}
	else
	{
		newinterpolate = luaL_checkboolean(L, 1);
		newtag = 0;
	}

	if (!cv_uncappedhud.value)
		return 0;

	hud_interpolate = newinterpolate;
	hud_interptag = newtag;

	return 0;
}

static int libd_interpLatch(lua_State *L)
{
	HUDONLY
	hud_interpstring = hud_interplatch = luaL_checkboolean(L, 1);
	return 0;
}

static luaL_Reg lib_draw[] = {
	{"patchExists", libd_patchExists},
	{"cachePatch", libd_cachePatch},
	{"draw", libd_draw},
	{"drawScaled", libd_drawScaled},
	{"drawStretched", libd_drawStretched},
	{"drawNum", libd_drawNum},
	{"drawPaddedNum", libd_drawPaddedNum},
	{"drawPingNum", libd_drawPingNum},
	{"drawFill", libd_drawFill},
	{"getSpritePatch",libd_getSpritePatch},
	{"fadeScreen", libd_fadeScreen},
	{"drawString", libd_drawString},
	{"drawKartString", libd_drawKartString},
	{"stringWidth", libd_stringWidth},
	{"getColormap", libd_getColormap},
	{"width", libd_width},
	{"height", libd_height},
	{"dupx", libd_dupx},
	{"dupy", libd_dupy},
	{"renderer", libd_renderer},
	{"localTransFlag", libd_getlocaltransflag},
	{"drawOnMinimap", libd_drawOnMinimap},
	{"getColorHudPatch", libd_getColorHudPatch},
	{"getDrawInfo", libd_getDrawInfo},
	{"getHudColor", libd_getHudColor},
	{"useColorHud", libd_useColorHud},
	{"interpolate", libd_interpolate},
	{"interpLatch", libd_interpLatch},
	{NULL, NULL}
};

//
// lib_hud
//

// enable vanilla HUD element
static int lib_hudenable(lua_State *L)
{
	enum hud option = luaL_checkoption(L, 1, NULL, hud_disable_options);
	hud_enabled[option/8] |= 1<<(option%8);
	return 0;
}

// disable vanilla HUD element
static int lib_huddisable(lua_State *L)
{
	enum hud option = luaL_checkoption(L, 1, NULL, hud_disable_options);
	hud_enabled[option/8] &= ~(1<<(option%8));
	return 0;
}

// 30/10/18: Lat': How come this wasn't here before?
static int lib_hudenabled(lua_State *L)
{
	enum hud option = luaL_checkoption(L, 1, NULL, hud_disable_options);
	if (hud_enabled[option/8] & (1<<(option%8)))
		lua_pushboolean(L, true);
	else
		lua_pushboolean(L, false);

	return 1;
}

// add a HUD element for rendering
static int lib_hudadd(lua_State *L)
{
	enum hudhook field;

	luaL_checktype(L, 1, LUA_TFUNCTION);
	field = luaL_checkoption(L, 2, "game", hudhook_opt);

	lua_getfield(L, LUA_REGISTRYINDEX, "HUD");
	I_Assert(lua_istable(L, -1));
	lua_rawgeti(L, -1, field+2); // HUD[2+]
	I_Assert(lua_istable(L, -1));
	lua_remove(L, -2);

	lua_pushvalue(L, 1);
	lua_rawseti(L, -2, (int)(lua_objlen(L, -2) + 1));

	hudAvailable |= 1<<field;
	return 0;
}

static int lib_hudsetvotebackground(lua_State *L)
{
	if (lua_isnoneornil(L, 1))
	{
		if (luaVoteScreen)
		{
			free(luaVoteScreen);
		}

		luaVoteScreen = NULL;

		return 0;
	}

	const char *prefix = luaL_checkstring(L, 1);

	if (strlen(prefix) != 4)
	{
		return luaL_argerror(L, 1, "prefix should 4 characters wide");
	}

	if (!luaVoteScreen)
	{
		luaVoteScreen = (char*)malloc(5);
		luaVoteScreen[4] = 0;
	}

	strncpy(luaVoteScreen, prefix, 4);

	strupr(luaVoteScreen);

	return 0;
}

static int lib_hudgetoffsets(lua_State *L)
{
	enum hudoffsets option = luaL_checkoption(L, 1, NULL, hud_offsets_options);
	INT32 ofx, ofy;

#define OFS(it) ofx = cv_##it##_xoffset.value; ofy = cv_##it##_yoffset.value; break;
#define OFY(it) ofx = 0; ofy = cv_##it##_yoffset.value; break;
	switch(option) {
		case hudoffsets_item:           OFS(item)
		case hudoffsets_time:           OFS(time)
		case hudoffsets_gametypeinfo:   OFS(laps)
		case hudoffsets_countdown:      OFS(dnft)
		case hudoffsets_speedometer:    OFS(speed)
		case hudoffsets_position:       OFS(posi)
		case hudoffsets_minirankings:   OFS(face)
		case hudoffsets_startcountdown: OFS(stcd)
		case hudoffsets_check:          OFY(chek)
		case hudoffsets_minimap:        OFS(mini)
		case hudoffsets_wanted:         OFS(want)
		case hudoffsets_statdisplay:    OFS(stat)
		default:
			return 0; // unreachable
	}
#undef OFS
#undef OFY

	lua_pushinteger(L, ofx);
	lua_pushinteger(L, ofy);
	return 2;
}

static luaL_Reg lib_hud[] = {
	{"enable", lib_hudenable},
	{"disable", lib_huddisable},
	{"enabled", lib_hudenabled},
	{"add", lib_hudadd},
	{"setVoteBackground", lib_hudsetvotebackground},
	{"getOffsets", lib_hudgetoffsets},
	{NULL, NULL}
};

//
//
//

int LUA_HudLib(lua_State *L)
{
	memset(hud_enabled, 0xff, (hud_MAX/8)+1);

	lua_newtable(L); // HUD registry table
		lua_newtable(L);
		luaL_register(L, NULL, lib_draw);
		lua_rawseti(L, -2, 1); // HUD[1] = lib_draw

		lua_newtable(L);
		lua_rawseti(L, -2, 2); // HUD[2] = game rendering functions array

		lua_newtable(L);
		lua_rawseti(L, -2, 3); // HUD[3] = scores rendering functions array

		lua_newtable(L);
		lua_rawseti(L, -2, 4); // HUD[4] = intermission rendering functions array

		lua_newtable(L);
		lua_rawseti(L, -2, 5); // HUD[5] = vote rendering functions array
	lua_setfield(L, LUA_REGISTRYINDEX, "HUD");

	luaL_newmetatable(L, META_HUDINFO);
		lua_pushcfunction(L, hudinfo_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, hudinfo_set);
		lua_setfield(L, -2, "__newindex");

		lua_pushcfunction(L, hudinfo_num);
		lua_setfield(L, -2, "__len");
	lua_pop(L,1);

	lua_newuserdata(L, 0);
		lua_createtable(L, 0, 2);
			lua_pushcfunction(L, lib_getHudInfo);
			lua_setfield(L, -2, "__index");

			lua_pushcfunction(L, lib_hudinfolen);
			lua_setfield(L, -2, "__len");
		lua_setmetatable(L, -2);
	lua_setglobal(L, "hudinfo");

	luaL_newmetatable(L, META_COLORMAP);
		lua_pushcfunction(L, colormap_get);
		lua_setfield(L, -2, "__index");
	lua_pop(L,1);

	luaL_newmetatable(L, META_PATCH);
		lua_pushcfunction(L, patch_get);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, patch_set);
		lua_setfield(L, -2, "__newindex");
	lua_pop(L,1);

	patch_fields_ref = Lua_CreateFieldTable(L, patch_opt);

	luaL_newmetatable(L, META_CAMERA);
		lua_pushcfunction(L, camera_get);
		lua_setfield(L, -2, "__index");
	lua_pop(L,1);

	camera_fields_ref = Lua_CreateFieldTable(L, camera_opt);

	luaL_register(L, "hud", lib_hud);
	return 0;
}

boolean LUA_HudEnabled(enum hud option)
{
	if (!gL || hud_enabled[option/8] & (1<<(option%8)))
		return true;
	return false;
}

// Hook for HUD rendering
void LUAh_GameHUD(huddrawlist_h list)
{
	if (!gL || !(hudAvailable & (1<<hudhook_game)))
		return;
	
	lua_pushlightuserdata(gL, list);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");

	hud_running = true;
	lua_settop(gL, 0);
	
	lua_pushcfunction(gL, LUA_GetErrorMessage);

	lua_getfield(gL, LUA_REGISTRYINDEX, "HUD");
	I_Assert(lua_istable(gL, -1));
	lua_rawgeti(gL, -1, hudhook_game+2); // HUD[2] = rendering funcs
	I_Assert(lua_istable(gL, -1));

	lua_rawgeti(gL, -2, 1); // HUD[1] = lib_draw
	I_Assert(lua_istable(gL, -1));
	lua_remove(gL, -3); // pop HUD
	LUA_PushUserdata(gL, stplyr, META_PLAYER);
	LUA_PushUserdata(gL, &camera[stplyrnum], META_CAMERA);
	camnum = stplyrnum + 1;

	hud_interpcounter = 0;
	lua_pushnil(gL);
	while (lua_next(gL, -5) != 0) {
		hud_interpolate = hud_interpstring = hud_interplatch = false;
		hud_interpcounter++;
		lua_pushvalue(gL, -5); // graphics library (HUD[1])
		lua_pushvalue(gL, -5); // stplyr
		lua_pushvalue(gL, -5); // camera
		LUA_Call(gL, 3, 0, 1);
	}
	lua_settop(gL, 0);
	hud_running = false;

	lua_pushlightuserdata(gL, NULL);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
}

void LUAh_ScoresHUD(huddrawlist_h list)
{
	if (!gL || !(hudAvailable & (1<<hudhook_scores)))
		return;
	
	lua_pushlightuserdata(gL, list);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");

	hud_running = true;
	lua_settop(gL, 0);
	
	lua_pushcfunction(gL, LUA_GetErrorMessage);

	lua_getfield(gL, LUA_REGISTRYINDEX, "HUD");
	I_Assert(lua_istable(gL, -1));
	lua_rawgeti(gL, -1, hudhook_scores+2); // HUD[3] = rendering funcs
	I_Assert(lua_istable(gL, -1));

	lua_rawgeti(gL, -2, 1); // HUD[1] = lib_draw
	I_Assert(lua_istable(gL, -1));
	lua_remove(gL, -3); // pop HUD
	lua_pushnil(gL);
	hud_interpcounter = 0;
	while (lua_next(gL, -3) != 0) {
		hud_interpolate = hud_interpstring = hud_interplatch = false;
		hud_interpcounter++;
		lua_pushvalue(gL, -3); // graphics library (HUD[1])
		LUA_Call(gL, 1, 0, 1);
	}
	lua_settop(gL, 0);
	hud_running = false;

	lua_pushlightuserdata(gL, NULL);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
}

void LUAh_IntermissionHUD(huddrawlist_h list)
{
	if (!gL || !(hudAvailable & (1<<hudhook_intermission)))
		return;
	
	lua_pushlightuserdata(gL, list);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");

	hud_running = true;
	lua_settop(gL, 0);
	
	lua_pushcfunction(gL, LUA_GetErrorMessage);

	lua_getfield(gL, LUA_REGISTRYINDEX, "HUD");
	I_Assert(lua_istable(gL, -1));
	lua_rawgeti(gL, -1, hudhook_intermission+2); // HUD[4] = rendering funcs
	I_Assert(lua_istable(gL, -1));

	lua_rawgeti(gL, -2, 1); // HUD[1] = lib_draw
	I_Assert(lua_istable(gL, -1));
	lua_remove(gL, -3); // pop HUD
	lua_pushnil(gL);
	hud_interpcounter = 0;
	while (lua_next(gL, -3) != 0) {
		hud_interpolate = hud_interpstring = hud_interplatch = false;
		hud_interpcounter++;
		lua_pushvalue(gL, -3); // graphics library (HUD[1])
		LUA_Call(gL, 1, 0, 1);
	}
	lua_settop(gL, 0);
	hud_running = false;

	lua_pushlightuserdata(gL, NULL);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
}

void LUAh_VoteHUD(huddrawlist_h list)
{
	if (!gL || !(hudAvailable & (1<<hudhook_vote)))
		return;
	
	lua_pushlightuserdata(gL, list);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");

	hud_running = true;
	lua_settop(gL, 0);
	
	lua_pushcfunction(gL, LUA_GetErrorMessage);

	lua_getfield(gL, LUA_REGISTRYINDEX, "HUD");
	I_Assert(lua_istable(gL, -1));
	lua_rawgeti(gL, -1, hudhook_vote+2); // HUD[5] = rendering funcs
	I_Assert(lua_istable(gL, -1));

	lua_rawgeti(gL, -2, 1); // HUD[1] = lib_draw
	I_Assert(lua_istable(gL, -1));
	lua_remove(gL, -3); // pop HUD
	lua_pushnil(gL);
	hud_interpcounter = 0;
	while (lua_next(gL, -3) != 0) {
		hud_interpolate = hud_interpstring = hud_interplatch = false;
		hud_interpcounter++;
		lua_pushvalue(gL, -3); // graphics library (HUD[1])
		LUA_Call(gL, 1, 0, 1);
	}
	lua_settop(gL, 0);
	hud_running = false;

	lua_pushlightuserdata(gL, NULL);
	lua_setfield(gL, LUA_REGISTRYINDEX, "HUD_DRAW_LIST");
}
