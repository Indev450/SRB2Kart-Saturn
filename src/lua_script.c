// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2012-2016 by John "JTE" Muniz.
// Copyright (C) 2012-2018 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_script.c
/// \brief Lua scripting basics

#include "doomdef.h"
#include "fastcmp.h"
#include "dehacked.h"
#include "z_zone.h"
#include "w_wad.h"
#include "p_setup.h"
#include "r_state.h"
#include "g_game.h"
#include "byteptr.h"
#include "p_saveg.h"
#include "p_local.h"
#include "p_slopes.h" // for P_SlopeById
#include "s_sound.h"
#include "m_menu.h"
#ifdef LUA_ALLOW_BYTECODE
#include "d_netfil.h" // for LUA_DumpFile
#endif

#include "lua_script.h"
#include "lua_libs.h"
#include "lua_glib.h"
#include "lua_hook.h"

#include "doomstat.h"

lua_State *gL = NULL;

// Mathlib global state
static lua_State *mL = NULL;

int hook_defrosting;

// List of internal libraries to load from SRB2
static lua_CFunction liblist[] = {
	LUA_EnumLib, // global metatable for enums
	LUA_SOCLib, // A_Action functions, freeslot
	LUA_BaseLib, // string concatination by +, CONS_Printf, p_local.h stuff (P_InstaThrust, P_Move), etc.
	LUA_MathLib, // fixed_t and angle_t math functions
	LUA_HookLib, // hookAdd and hook-calling functions
	LUA_ConsoleLib, // console command/variable functions and structs
	LUA_InfoLib, // info.h stuff: mobjinfo_t, mobjinfo[], state_t, states[]
	LUA_MobjLib, // mobj_t, mapthing_t
	LUA_PlayerLib, // player_t
	LUA_SkinLib, // skin_t, skins[]
	LUA_ThinkerLib, // thinker_t
	LUA_MapLib, // line_t, side_t, sector_t, subsector_t
	LUA_BlockmapLib, // blockmap stuff
	LUA_HudLib, // HUD stuff
	NULL
};

// Lua asks for memory using this.
static void *LUA_Alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	(void)ud;
	if (nsize == 0) {
		if (osize != 0)
			Z_Free(ptr);
		return NULL;
	} else
		return Z_Realloc(ptr, nsize, PU_LUA, NULL);
}

// Panic function Lua calls when there's an unprotected error.
// This function cannot return. Lua would kill the application anyway if it did.
FUNCNORETURN static int LUA_Panic(lua_State *L)
{
	CONS_Alert(CONS_ERROR,"LUA PANIC! %s\n",lua_tostring(L,-1));
	I_Error("An unfortunate Lua processing error occurred in the exe itself. This is not a scripting error on your part.");
#ifndef __GNUC__
	return -1;
#endif
}

#define LEVELS1 12 // size of the first part of the stack
#define LEVELS2 10 // size of the second part of the stack

// Error handler used with pcall() when loading scripts or calling hooks
// Takes a string with the original error message,
// appends the traceback to it, and return the result
int LUA_GetErrorMessage(lua_State *L)
{
	int level = 1;
	int firstpart = 1; // still before eventual `...'
	lua_Debug ar;

	lua_pushliteral(L, "\nstack traceback:");
	while (lua_getstack(L, level++, &ar))
	{
		if (level > LEVELS1 && firstpart)
		{
			// no more than `LEVELS2' more levels?
			if (!lua_getstack(L, level + LEVELS2, &ar))
				level--; // keep going
			else
			{
				lua_pushliteral(L, "\n    ..."); // too many levels
				while (lua_getstack(L, level + LEVELS2, &ar)) // find last levels
					level++;
			}
			firstpart = 0;
			continue;
		}
		lua_pushliteral(L, "\n    ");
		lua_getinfo(L, "Snl", &ar);
		lua_pushfstring(L, "%s:", ar.short_src);
		if (ar.currentline > 0)
			lua_pushfstring(L, "%d:", ar.currentline);
		if (*ar.namewhat != '\0') // is there a name?
			lua_pushfstring(L, " in function " LUA_QS, ar.name);
		else
		{
			if (*ar.what == 'm') // main?
				lua_pushfstring(L, " in main chunk");
			else if (*ar.what == 'C' || *ar.what == 't')
				lua_pushliteral(L, " ?"); // C function or tail call
			else
				lua_pushfstring(L, " in function <%s:%d>",
					ar.short_src, ar.linedefined);
		}
		lua_concat(L, lua_gettop(L));
	}
	lua_concat(L, lua_gettop(L));
	return 1;
}

int LUA_Call(lua_State *L, int nargs, int nresults, int errorhandlerindex)
{
	int err = lua_pcall(L, nargs, nresults, errorhandlerindex);

	if (err)
	{
		CONS_Alert(CONS_WARNING, "%s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	return err;
}

// This function decides which global variables you are allowed to set.
static int noglobals(lua_State *L)
{
	const char *csname;
	char *name;
	enum actionnum actionnum;

	lua_remove(L, 1); // we're not gonna be using _G
	csname = lua_tostring(L, 1);

	// make an uppercase copy of the name
	name = Z_StrDup(csname);
	strupr(name);

	if (fastncmp(name, "A_", 2) && lua_isfunction(L, 2))
	{
		// Accept new A_Action functions
		// Add the action to Lua actions refrence table
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_ACTIONS);
		lua_pushstring(L, name); // "A_ACTION"
		lua_pushvalue(L, 2); // function
		lua_rawset(L, -3); // rawset doesn't trigger this metatable again.
		// otherwise we would've used setfield, obviously.

		actionnum = LUA_GetActionNumByName(name);
		if (actionnum < NUMACTIONS)
			actionsoverridden[actionnum] = true;

		Z_Free(name);
		return 0;
	}

	Z_Free(name);
	return luaL_error(L, "Implicit global " LUA_QS " prevented. Create a local variable instead.", csname);
}

// Clear and create a new Lua state, laddo!
// There's SCRIPTIN to be had!
void LUA_ClearState(void)
{
	lua_State *L;
	int i;

	// close previous state
	if (gL)
		lua_close(gL);
	gL = NULL;

	CONS_Printf(M_GetText("Pardon me while I initialize the Lua scripting interface...\n"));

	// allocate state
	L = lua_newstate(LUA_Alloc, NULL);
	lua_atpanic(L, LUA_Panic);

	// open base libraries
	luaL_openlibs(L);
	lua_settop(L, 0);

	// make LREG_VALID table for all pushed userdata cache.
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LREG_VALID);

	// open srb2 libraries
	for(i = 0; liblist[i]; i++) {
		lua_pushcfunction(L, liblist[i]);
		lua_call(L, 0, 0);
	}

	// lock the global namespace
	lua_getmetatable(L, LUA_GLOBALSINDEX);
		lua_pushcfunction(L, noglobals);
		lua_setfield(L, -2, "__newindex");
		lua_newtable(L);
		lua_setfield(L, -2, "__metatable");
	lua_pop(L, 1);

	// lua state is ready!
	gL = L;
}

#ifdef _DEBUG
void LUA_ClearExtVars(void)
{
	if (!gL)
		return;
	lua_newtable(gL);
	lua_setfield(gL, LUA_REGISTRYINDEX, LREG_EXTVARS);
}
#endif

// Load a script from a MYFILE
static inline void LUA_LoadFile(MYFILE *f, char *name)
{
	if (!name)
		name = wadfiles[f->wad]->filename;
	CONS_Printf("Loading Lua script from %s\n", name);
	if (!gL) // Lua needs to be initialized
		LUA_ClearState();
	lua_pushinteger(gL, f->wad);
	lua_setfield(gL, LUA_REGISTRYINDEX, "WAD");

	lua_pushcfunction(gL, LUA_GetErrorMessage);
	if (luaL_loadbuffer(gL, f->data, f->size, va("@%s",name)) || lua_pcall(gL, 0, 0, lua_gettop(gL) - 1)) {
		CONS_Alert(CONS_WARNING,"%s\n",lua_tostring(gL,-1));
		lua_pop(gL,1);
	}
	lua_gc(gL, LUA_GCCOLLECT, 0);
	lua_pop(gL, 1); // Pop error handler
}

// Load a script from a lump
void LUA_LoadLump(UINT16 wad, UINT16 lump)
{
	MYFILE f;
	char *name;
	size_t len;
	f.wad = wad;
	f.size = W_LumpLengthPwad(wad, lump);
	f.data = Z_Malloc(f.size, PU_LUA, NULL);
	W_ReadLumpPwad(wad, lump, f.data);
	f.curpos = f.data;

	len = strlen(wadfiles[wad]->filename); // length of file name

	if (wadfiles[wad]->type == RET_LUA)
	{
		name = malloc(len+1);
		strcpy(name, wadfiles[wad]->filename);
	}
	else // If it's not a .lua file, copy the lump name in too.
	{
		lumpinfo_t *lump_p = &wadfiles[wad]->lumpinfo[lump];
		len += 1 + strlen(lump_p->fullname); // length of file name, '|', and lump name
		name = malloc(len+1);
		sprintf(name, "%s|%s", wadfiles[wad]->filename, lump_p->fullname);
		name[len] = '\0';
	}

	LUA_LoadFile(&f, name); // actually load file!

	// Okay, we've modified the game beyond the point of no return.
	G_SetWadModified(multiplayer, true, wad);

	free(name);
	Z_Free(f.data);
}

#ifdef LUA_ALLOW_BYTECODE
// must match lua_Writer
static int dumpWriter(lua_State *L, const void *p, size_t sz, void *ud)
{
	FILE *handle = (FILE*)ud;
	I_Assert(handle != NULL);
	(void)L;
	if (!sz) return 0; // nothing to write? can't fail that! :D
	return (fwrite(p, 1, sz, handle) != sz); // if fwrite != sz, we've failed.
}

// Compile a script by name and dump it back to disk.
void LUA_DumpFile(const char *filename)
{
	FILE *handle;
	char filenamebuf[MAX_WADPATH];

	if (!gL) // Lua needs to be initialized
		LUA_ClearState(false);

	// find the file the SRB2 way
	strncpy(filenamebuf, filename, MAX_WADPATH);
	filenamebuf[MAX_WADPATH - 1] = '\0';
	filename = filenamebuf;
	if ((handle = fopen(filename, "rb")) == NULL)
	{
		// If we failed to load the file with the path as specified by
		// the user, strip the directories and search for the file.
		nameonly(filenamebuf);

		// If findfile finds the file, the full path will be returned
		// in filenamebuf == filename.
		if (findfile(filenamebuf, NULL, true))
		{
			if ((handle = fopen(filename, "rb")) == NULL)
			{
				CONS_Alert(CONS_ERROR, M_GetText("Can't open %s\n"), filename);
				return;
			}
		}
		else
		{
			CONS_Alert(CONS_ERROR, M_GetText("File %s not found.\n"), filename);
			return;
		}
	}
	fclose(handle);

	// pass the path we found to Lua
	// luaL_loadfile will open and read the file in as a Lua function
	if (luaL_loadfile(gL, filename)) {
		CONS_Alert(CONS_ERROR,"%s\n",lua_tostring(gL,-1));
		lua_pop(gL, 1);
		return;
	}

	// dump it back to disk
	if ((handle = fopen(filename, "wb")) == NULL)
		CONS_Alert(CONS_ERROR, M_GetText("Can't write to %s\n"), filename);
	if (lua_dump(gL, dumpWriter, handle))
		CONS_Printf("Failed while writing %s to disk... Sorry!\n", filename);
	else
		CONS_Printf("Successfully compiled %s into bytecode.\n", filename);
	fclose(handle);
	lua_pop(gL, 1); // function is still on stack after lua_dump
	lua_gc(gL, LUA_GCCOLLECT, 0);
	return;
}
#endif

fixed_t LUA_EvalMath(const char *word)
{
	char buf[1024], *b;
	const char *p;
	fixed_t res = 0;

	// make a new state so SOC can't interefere with scripts
	// allocate state
	if (mL == NULL)
	{
		mL = lua_newstate(LUA_Alloc, NULL);
		lua_atpanic(mL, LUA_Panic);

		// open only enum lib
		lua_pushcfunction(mL, LUA_EnumLib);
		lua_pushboolean(mL, true);
		lua_call(mL, 1, 0);
	}

	// change ^ into ^^ for Lua.
	strcpy(buf, "return ");
	b = buf+strlen(buf);
	for (p = word; *p && b < &buf[1022]; p++)
	{
		*b++ = *p;
		if (*p == '^')
			*b++ = '^';
	}
	*b = '\0';

	// eval string.
	lua_settop(mL, 0);
	if (luaL_dostring(mL, buf))
	{
		p = lua_tostring(mL, -1);

		// If there is [string "..."]:1: text, skip it
		if (strstr(p, ":") != NULL)
		{
			while (*p++ != ':' && *p);

			p += 3; // "1: "
		}
		CONS_Alert(CONS_WARNING, "%s\n", p);
	}
	else
		res = lua_tointeger(mL, -1);

	return res;
}

void LUA_InvalidateMathlibCache(const char *name)
{
	// No state => no cache => nothing to invalidate :3
	if (mL == NULL) return;

	lua_pushcfunction(mL, lua_glib_invalidate_cache);
	lua_pushstring(mL, name);
	lua_call(mL, 1, 0);
}

// Takes a pointer, any pointer, and a metatable name
// Creates a userdata for that pointer with the given metatable
// Pushes it to the stack and stores it in the registry.
void LUA_PushUserdata(lua_State *L, void *data, const char *meta)
{
	void **userdata;

	if (!data) { // push a NULL
		lua_pushnil(L);
		return;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, LREG_VALID);
	I_Assert(lua_istable(L, -1));
	lua_pushlightuserdata(L, data);
	lua_rawget(L, -2);
	if (lua_isnil(L, -1)) { // no userdata? deary me, we'll have to make one.
		lua_pop(L, 1); // pop the nil

		// create the userdata
		userdata = lua_newuserdata(L, sizeof(void *));
		*userdata = data;
		luaL_getmetatable(L, meta);
		lua_setmetatable(L, -2);

		// Set it in the registry so we can find it again
		lua_pushlightuserdata(L, data); // k (store the userdata via the data's pointer)
		lua_pushvalue(L, -2); // v (copy of the userdata)
		lua_rawset(L, -4);

		// stack is left with the userdata on top, as if getting it had originally succeeded.
	}
	lua_remove(L, -2); // remove LREG_VALID
}

// When userdata is freed, use this function to remove it from Lua.
void LUA_InvalidateUserdata(void *data)
{
	void **userdata;
	if (!gL)
		return;

	// fetch the userdata
	lua_getfield(gL, LUA_REGISTRYINDEX, LREG_VALID);
	I_Assert(lua_istable(gL, -1));
		lua_pushlightuserdata(gL, data);
		lua_rawget(gL, -2);
			if (lua_isnil(gL, -1)) { // not found, not in lua
				lua_pop(gL, 2); // pop nil and LREG_VALID
				return;
			}

			// nullify any additional data
			lua_getfield(gL, LUA_REGISTRYINDEX, LREG_EXTVARS);
			I_Assert(lua_istable(gL, -1));
				lua_pushlightuserdata(gL, data);
				lua_pushnil(gL);
				lua_rawset(gL, -3);
			lua_pop(gL, 1);

			// invalidate the userdata
			userdata = lua_touserdata(gL, -1);
			*userdata = NULL;
		lua_pop(gL, 1);

		// remove it from the registry
		lua_pushlightuserdata(gL, data);
		lua_pushnil(gL);
		lua_rawset(gL, -3);
	lua_pop(gL, 1); // pop LREG_VALID
}

// Invalidate level data arrays
void LUA_InvalidateLevel(void)
{
	thinker_t *th;
	size_t i;
	if (!gL)
		return;

	for (th = thinkercap.next; th && th != &thinkercap; th = th->next)
		LUA_InvalidateUserdata(th);

	LUA_InvalidateMapthings();

	for (i = 0; i < numsubsectors; i++)
		LUA_InvalidateUserdata(&subsectors[i]);
	for (i = 0; i < numsectors; i++)
		LUA_InvalidateUserdata(&sectors[i]);
	for (i = 0; i < numlines; i++)
	{
		LUA_InvalidateUserdata(&lines[i]);
		LUA_InvalidateUserdata(lines[i].sidenum);
	}
	for (i = 0; i < numsides; i++)
		LUA_InvalidateUserdata(&sides[i]);
	for (i = 0; i < numvertexes; i++)
		LUA_InvalidateUserdata(&vertexes[i]);
}

void LUA_InvalidateMapthings(void)
{
	size_t i;
	if (!gL)
		return;

	for (i = 0; i < nummapthings; i++)
		LUA_InvalidateUserdata(&mapthings[i]);
}

void LUA_InvalidatePlayer(player_t *player)
{
	if (!gL)
		return;
	LUA_InvalidateUserdata(player);
	LUA_InvalidateUserdata(player->powers);
	LUA_InvalidateUserdata(player->kartstuff);
	LUA_InvalidateUserdata(&player->cmd);
}

enum
{
	ARCH_NULL=0,
	ARCH_BOOLEAN,
	ARCH_SIGNED,
	ARCH_STRING,
	ARCH_TABLE,

	ARCH_MOBJINFO,
	ARCH_STATE,
	ARCH_MOBJ,
	ARCH_PLAYER,
	ARCH_MAPTHING,
	ARCH_VERTEX,
	ARCH_LINE,
	ARCH_SIDE,
	ARCH_SUBSECTOR,
	ARCH_SECTOR,
	ARCH_SLOPE,
	ARCH_MAPHEADER,

	ARCH_TEND=0xFF,
};

static const struct {
	const char *meta;
	UINT8 arch;
} meta2arch[] = {
	{META_MOBJINFO, ARCH_MOBJINFO},
	{META_STATE,    ARCH_STATE},
	{META_MOBJ,     ARCH_MOBJ},
	{META_PLAYER,   ARCH_PLAYER},
	{META_MAPTHING, ARCH_MAPTHING},
	{META_VERTEX,   ARCH_VERTEX},
	{META_LINE,     ARCH_LINE},
	{META_SIDE,     ARCH_SIDE},
	{META_SUBSECTOR,ARCH_SUBSECTOR},
	{META_SECTOR,   ARCH_SECTOR},
	{META_SLOPE,    ARCH_SLOPE},
	{META_MAPHEADER,   ARCH_MAPHEADER},
	{NULL,          ARCH_NULL}
};

static UINT8 GetUserdataArchType(int index)
{
	UINT8 i;
	lua_getmetatable(gL, index);

	for (i = 0; meta2arch[i].meta; i++)
	{
		luaL_getmetatable(gL, meta2arch[i].meta);
		if (lua_rawequal(gL, -1, -2))
		{
			lua_pop(gL, 2);
			return meta2arch[i].arch;
		}
		lua_pop(gL, 1);
	}

	lua_pop(gL, 1);
	return ARCH_NULL;
}

static UINT8 ArchiveValue(UINT8 **p, int TABLESINDEX, int myindex)
{
	if (myindex < 0)
		myindex = lua_gettop(gL)+1+myindex;
	switch (lua_type(gL, myindex))
	{
	case LUA_TNONE:
	case LUA_TNIL:
		WRITEUINT8(*p, ARCH_NULL);
		break;
	// This might be a problem. D:
	case LUA_TLIGHTUSERDATA:
	case LUA_TTHREAD:
	case LUA_TFUNCTION:
		WRITEUINT8(*p, ARCH_NULL);
		return 2;
	case LUA_TBOOLEAN:
		WRITEUINT8(*p, ARCH_BOOLEAN);
		WRITEUINT8(*p, lua_toboolean(gL, myindex));
		break;
	case LUA_TNUMBER:
	{
		lua_Integer number = lua_tointeger(gL, myindex);
        WRITEUINT8(*p, ARCH_SIGNED);
        WRITEFIXED(*p, number);
		break;
	}
	case LUA_TSTRING:
	{
		UINT16 len = (UINT16)lua_objlen(gL, myindex); // get length of string, including embedded zeros
		const char *s = lua_tostring(gL, myindex);
		UINT16 i = 0;
		WRITEUINT8(*p, ARCH_STRING);
		// if you're wondering why we're writing a string to save_p this way,
		// it turns out that Lua can have embedded zeros ('\0') in the strings,
		// so we can't use WRITESTRING as that cuts off when it finds a '\0'.
		// Saving the size of the string also allows us to get the size of the string on the other end,
		// fixing the awful crashes previously encountered for reading strings longer than 1024
		// (yes I know that's kind of a stupid thing to care about, but it'd be evil to trim or ignore them?)
		// -- Monster Iestyn 05/08/18
		WRITEUINT16(*p, len); // save size of string
		while (i < len)
			WRITECHAR(*p, s[i++]); // write chars individually, including the embedded zeros
		break;
	}
	case LUA_TTABLE:
	{
		boolean found = false;
		INT32 i;
		UINT16 t = (UINT16)lua_objlen(gL, TABLESINDEX);

		for (i = 1; i <= t && !found; i++)
		{
			lua_rawgeti(gL, TABLESINDEX, i);
			if (lua_rawequal(gL, myindex, -1))
			{
				t = i;
				found = true;
			}
			lua_pop(gL, 1);
		}
		if (!found)
		{
			t++;

			if (t == 0)
			{
				CONS_Alert(CONS_ERROR, "Too many tables to archive!\n");
				WRITEUINT8(*p, ARCH_NULL);
				return 0;
			}
		}

		WRITEUINT8(*p, ARCH_TABLE);
		WRITEUINT16(*p, t);

		if (!found)
		{
			lua_pushvalue(gL, myindex);
			lua_rawseti(gL, TABLESINDEX, t);
			return 1;
		}
		break;
	}
	case LUA_TUSERDATA:
		switch (GetUserdataArchType(myindex))
		{
		case ARCH_MOBJINFO:
		{
			mobjinfo_t *info = *((mobjinfo_t **)lua_touserdata(gL, myindex));
			WRITEUINT8(*p, ARCH_MOBJINFO);
			WRITEUINT16(*p, info - mobjinfo);
			break;
		}
		case ARCH_STATE:
		{
			state_t *state = *((state_t **)lua_touserdata(gL, myindex));
			WRITEUINT8(*p, ARCH_STATE);
			WRITEUINT16(*p, state - states);
			break;
		}
		case ARCH_MOBJ:
		{
			mobj_t *mobj = *((mobj_t **)lua_touserdata(gL, myindex));
			if (!mobj)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_MOBJ);
				WRITEUINT32(*p, mobj->mobjnum);
			}
			break;
		}
		case ARCH_PLAYER:
		{
			player_t *player = *((player_t **)lua_touserdata(gL, myindex));
			if (!player)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_PLAYER);
				WRITEUINT8(*p, player - players);
			}
			break;
		}
		case ARCH_MAPTHING:
		{
			mapthing_t *mapthing = *((mapthing_t **)lua_touserdata(gL, myindex));
			if (!mapthing)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_MAPTHING);
				WRITEUINT16(*p, mapthing - mapthings);
			}
			break;
		}
		case ARCH_VERTEX:
		{
			vertex_t *vertex = *((vertex_t **)lua_touserdata(gL, myindex));
			if (!vertex)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_VERTEX);
				WRITEUINT16(*p, vertex - vertexes);
			}
			break;
		}
		case ARCH_LINE:
		{
			line_t *line = *((line_t **)lua_touserdata(gL, myindex));
			if (!line)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_LINE);
				WRITEUINT16(*p, line - lines);
			}
			break;
		}
		case ARCH_SIDE:
		{
			side_t *side = *((side_t **)lua_touserdata(gL, myindex));
			if (!side)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_SIDE);
				WRITEUINT16(*p, side - sides);
			}
			break;
		}
		case ARCH_SUBSECTOR:
		{
			subsector_t *subsector = *((subsector_t **)lua_touserdata(gL, myindex));
			if (!subsector)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_SUBSECTOR);
				WRITEUINT16(*p, subsector - subsectors);
			}
			break;
		}
		case ARCH_SECTOR:
		{
			sector_t *sector = *((sector_t **)lua_touserdata(gL, myindex));
			if (!sector)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_SECTOR);
				WRITEUINT16(*p, sector - sectors);
			}
			break;
		}
		case ARCH_SLOPE:
		{
			pslope_t *slope = *((pslope_t **)lua_touserdata(gL, myindex));
			if (!slope)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_SLOPE);
				WRITEUINT16(*p, slope->id);
			}
			break;
		}
		case ARCH_MAPHEADER:
		{
			mapheader_t *header = *((mapheader_t **)lua_touserdata(gL, myindex));
			if (!header)
				WRITEUINT8(*p, ARCH_NULL);
			else {
				WRITEUINT8(*p, ARCH_MAPHEADER);
				WRITEUINT16(*p, header - *mapheaderinfo);
			}
			break;
		}
		default:
			WRITEUINT8(*p, ARCH_NULL);
			return 2;
		}
		break;
	}
	return 0;
}

static void ArchiveExtVars(UINT8 **p, void *pointer, const char *ptype)
{
	int TABLESINDEX;
	UINT16 i;

	if (!gL) {
		if (fastcmp(ptype,"player")) // players must always be included, even if no vars
			WRITEUINT16(*p, 0);
		return;
	}

	TABLESINDEX = lua_gettop(gL);

	lua_getfield(gL, LUA_REGISTRYINDEX, LREG_EXTVARS);
	I_Assert(lua_istable(gL, -1));
	lua_pushlightuserdata(gL, pointer);
	lua_rawget(gL, -2);
	lua_remove(gL, -2); // pop LREG_EXTVARS

	if (!lua_istable(gL, -1))
	{ // no extra values table
		lua_pop(gL, 1);
		if (fastcmp(ptype,"player")) // players must always be included, even if no vars
			WRITEUINT16(*p, 0);
		return;
	}

	lua_pushnil(gL);
	for (i = 0; lua_next(gL, -2); i++)
		lua_pop(gL, 1);

	// skip anything that has an empty table and isn't a player.
	if (i == 0)
	{
		if (fastcmp(ptype,"player")) // always include players even if they have no extra variables
			WRITEUINT16(*p, 0);
		lua_pop(gL, 1);
		return;
	}

	if (fastcmp(ptype,"mobj")) // mobjs must write their mobjnum as a header
		WRITEUINT32(*p, ((mobj_t *)pointer)->mobjnum);
	WRITEUINT16(*p, i);
	lua_pushnil(gL);
	while (lua_next(gL, -2))
	{
		I_Assert(lua_type(gL, -2) == LUA_TSTRING);
		WRITESTRING(*p, lua_tostring(gL, -2));
		if (ArchiveValue(p, TABLESINDEX, -1) == 2)
			CONS_Alert(CONS_ERROR, "Type of value for %s entry '%s' (%s) could not be archived!\n", ptype, lua_tostring(gL, -2), luaL_typename(gL, -1));
		lua_pop(gL, 1);
	}

	lua_pop(gL, 1);
}

static int NetArchive(lua_State *L)
{
	int TABLESINDEX = lua_upvalueindex(1);
	savebuffer_t *save = lua_touserdata(L, lua_upvalueindex(2));
	int i, n = lua_gettop(L);
	for (i = 1; i <= n; i++)
		ArchiveValue(&save->p, TABLESINDEX, i);
	return n;
}

static void ArchiveTables(UINT8 **p)
{
	int TABLESINDEX;
	UINT16 i, n;
	UINT8 e;

	if (!gL)
		return;

	TABLESINDEX = lua_gettop(gL);

	n = (UINT16)lua_objlen(gL, TABLESINDEX);
	for (i = 1; i <= n; i++)
	{
		lua_rawgeti(gL, TABLESINDEX, i);
		lua_pushnil(gL);
		while (lua_next(gL, -2))
		{
			// Write key
			e = ArchiveValue(p, TABLESINDEX, -2); // key should be either a number or a string, ArchiveValue can handle this.
			if (e == 2) // invalid key type (function, thread, lightuserdata, or anything we don't recognise)
			{
				lua_pushvalue(gL, -2);
				CONS_Alert(CONS_ERROR, "Index '%s' (%s) of table %d could not be archived!\n", lua_tostring(gL, -1), luaL_typename(gL, -1), i);
				lua_pop(gL, 1);
			}
			// Write value
			e = ArchiveValue(p, TABLESINDEX, -1);
			if (e == 1)
				n++; // the table contained a new table we'll have to archive. :(
			else if (e == 2) // invalid value type
			{
				lua_pushvalue(gL, -2);
				CONS_Alert(CONS_ERROR, "Type of value for table %d entry '%s' (%s) could not be archived!\n", i, lua_tostring(gL, -1), luaL_typename(gL, -1));
				lua_pop(gL, 1);
			}

			lua_pop(gL, 1);
		}
		lua_pop(gL, 1);
		WRITEUINT8(*p, ARCH_TEND);
	}
}

static UINT8 UnArchiveValue(UINT8 **p, int TABLESINDEX)
{
	UINT8 type = READUINT8(*p);
	switch (type)
	{
	case ARCH_NULL:
		lua_pushnil(gL);
		break;
	case ARCH_BOOLEAN:
		lua_pushboolean(gL, READUINT8(*p));
		break;
	case ARCH_SIGNED:
		lua_pushinteger(gL, READFIXED(*p));
		break;
	case ARCH_STRING:
	{
		UINT16 len = READUINT16(*p); // length of string, including embedded zeros
		char *value;
		UINT16 i = 0;
		// See my comments in the ArchiveValue function;
		// it's much the same for reading strings as writing them!
		// (i.e. we can't use READSTRING either)
		// -- Monster Iestyn 05/08/18
		value = malloc(len); // make temp buffer of size len
		// now read the actual string
		while (i < len)
			value[i++] = READCHAR(*p); // read chars individually, including the embedded zeros
		lua_pushlstring(gL, value, len); // push the string (note: this function supports embedded zeros)
		free(value); // free the buffer
		break;
	}
	case ARCH_TABLE:
	{
		UINT16 tid = READUINT16(*p);
		lua_rawgeti(gL, TABLESINDEX, tid);
		if (lua_isnil(gL, -1))
		{
			lua_pop(gL, 1);
			lua_newtable(gL);
			lua_pushvalue(gL, -1);
			lua_rawseti(gL, TABLESINDEX, tid);
			return 2;
		}
		break;
	}
	case ARCH_MOBJINFO:
		LUA_PushUserdata(gL, &mobjinfo[READUINT16(*p)], META_MOBJINFO);
		break;
	case ARCH_STATE:
		LUA_PushUserdata(gL, &states[READUINT16(*p)], META_STATE);
		break;
	case ARCH_MOBJ:
		LUA_PushUserdata(gL, P_FindNewPosition(READUINT32(*p)), META_MOBJ);
		break;
	case ARCH_PLAYER:
		LUA_PushUserdata(gL, &players[READUINT8(*p)], META_PLAYER);
		break;
	case ARCH_MAPTHING:
		LUA_PushUserdata(gL, &mapthings[READUINT16(*p)], META_MAPTHING);
		break;
	case ARCH_VERTEX:
		LUA_PushUserdata(gL, &vertexes[READUINT16(*p)], META_VERTEX);
		break;
	case ARCH_LINE:
		LUA_PushUserdata(gL, &lines[READUINT16(*p)], META_LINE);
		break;
	case ARCH_SIDE:
		LUA_PushUserdata(gL, &sides[READUINT16(*p)], META_SIDE);
		break;
	case ARCH_SUBSECTOR:
		LUA_PushUserdata(gL, &subsectors[READUINT16(*p)], META_SUBSECTOR);
		break;
	case ARCH_SECTOR:
		LUA_PushUserdata(gL, &sectors[READUINT16(*p)], META_SECTOR);
		break;
	case ARCH_SLOPE:
		LUA_PushUserdata(gL, P_SlopeById(READUINT16(*p)), META_SLOPE);
		break;
	case ARCH_MAPHEADER:
		LUA_PushUserdata(gL, mapheaderinfo[READUINT16(*p)], META_MAPHEADER);
		break;
	case ARCH_TEND:
		return 1;
	default:
		CONS_Alert(CONS_ERROR, "Unknown value type unarchived, save is corrupted!\n");
		G_SetExitGameFlag();
		S_StartSound(NULL, sfx_syfail); // he he he
		M_StartMessage(M_GetText("Corrupted save received\nPress ESC\n"), NULL, MM_NOTHING);
		return 1;
	}
	return 0;
}


// Unarchives from demo_p:
// Return values:
// 0: Normal
// 1: Read table key
// 2: Read table value
// 3: Don't use setfield

static UINT8 UnArchiveValueDemo(UINT8 **p, int TABLESINDEX, char field[1024])
{
	UINT8 type = READUINT8(*p);
	switch (type)
	{
	case ARCH_NULL:
		lua_pushnil(gL);
		break;
	case ARCH_BOOLEAN:
		lua_pushboolean(gL, READUINT8(*p));
		break;
	case ARCH_SIGNED:
		lua_pushinteger(gL, READFIXED(*p));
		break;
	case ARCH_STRING:
	{
		UINT16 len = READUINT16(*p); // length of string, including embedded zeros
		char *value;
		UINT16 i = 0;
		// See my comments in the ArchiveValue function;
		// it's much the same for reading strings as writing them!
		// (i.e. we can't use READSTRING either)
		// -- Monster Iestyn 05/08/18
		value = malloc(len); // make temp buffer of size len
		// now read the actual string
		while (i < len)
			value[i++] = READCHAR(*p); // read chars individually, including the embedded zeros
		lua_pushlstring(gL, value, len); // push the string (note: this function supports embedded zeros)
		free(value); // free the buffer
		break;
	}
	case ARCH_TABLE:
	{
		UINT16 tid = READUINT16(*p);
		lua_rawgeti(gL, TABLESINDEX, tid);
		if (lua_isnil(gL, -1))
		{
			lua_pop(gL, 1);
			lua_newtable(gL);
			lua_pushvalue(gL, -1);
			lua_rawseti(gL, TABLESINDEX, tid);
			return 2;
		}
		break;
	}
	case ARCH_MOBJINFO:
		LUA_PushUserdata(gL, &mobjinfo[READUINT16(*p)], META_MOBJINFO);
		break;
	case ARCH_STATE:
		LUA_PushUserdata(gL, &states[READUINT16(*p)], META_STATE);
		break;
	case ARCH_MOBJ:
		*p += sizeof(UINT32);	// Skip this data, we can't read a mobj here, it'd point to garbage and crash the game.
		if (field)
			CONS_Alert(CONS_WARNING,"Cannot read mobj_t stored in player variable \'%s\'. Desyncs may occur.\n", field);
		else
			CONS_Alert(CONS_WARNING,"Couldn't read mobj_t\n");
		return 3;	// Don't set the field

	case ARCH_PLAYER:
		LUA_PushUserdata(gL, &players[READUINT8(*p)], META_PLAYER);
		break;
	case ARCH_MAPTHING:
		LUA_PushUserdata(gL, &mapthings[READUINT16(*p)], META_MAPTHING);
		break;
	case ARCH_VERTEX:
		LUA_PushUserdata(gL, &vertexes[READUINT16(*p)], META_VERTEX);
		break;
	case ARCH_LINE:
		LUA_PushUserdata(gL, &lines[READUINT16(*p)], META_LINE);
		break;
	case ARCH_SIDE:
		LUA_PushUserdata(gL, &sides[READUINT16(*p)], META_SIDE);
		break;
	case ARCH_SUBSECTOR:
		LUA_PushUserdata(gL, &subsectors[READUINT16(*p)], META_SUBSECTOR);
		break;
	case ARCH_SECTOR:
		LUA_PushUserdata(gL, &sectors[READUINT16(*p)], META_SECTOR);
		break;
	case ARCH_SLOPE:
		LUA_PushUserdata(gL, P_SlopeById(READUINT16(*p)), META_SLOPE);
		break;
	case ARCH_MAPHEADER:
		LUA_PushUserdata(gL, mapheaderinfo[READUINT16(*p)], META_MAPHEADER);
		break;
	case ARCH_TEND:
		return 1;
	}
	return 0;
}

static void UnArchiveExtVars(UINT8 **p, void *pointer, boolean network)
{
	int TABLESINDEX;
	UINT16 field_count = READUINT16(*p);
	UINT16 i;
	char field[1024];

	if (field_count == 0)
		return;

	// Technically possible new, since server may have local lua scripts but no "public" ones, so
	// field_count would be non zero (there is no way to tell local field from non-local, so
	// everything gets archived)
	if (!gL)
		return;

	TABLESINDEX = lua_gettop(gL);
	lua_createtable(gL, 0, field_count); // pointer's ext vars subtable

	if (network)
	{
		for (i = 0; i < field_count; i++)
		{
			READSTRING(*p, field);

			if (UnArchiveValue(p, TABLESINDEX) == 1)
			{
				CONS_Alert(CONS_ERROR, "Unexpected end marker when reading ExtVars (field '%s')\n", field);
				break;
			}

			lua_setfield(gL, -2, field);
		}
	}
	else
	{
		for (i = 0; i < field_count; i++)
		{
			READSTRING(*p, field);
			if (UnArchiveValueDemo(p, TABLESINDEX, field) != 3)	// This will return 3 if we shouldn't set this field.
				lua_setfield(gL, -2, field);
		}
	}

	lua_getfield(gL, LUA_REGISTRYINDEX, LREG_EXTVARS);
	I_Assert(lua_istable(gL, -1));
	lua_pushlightuserdata(gL, pointer);
	lua_pushvalue(gL, -3); // pointer's ext vars subtable
	lua_rawset(gL, -3);
	lua_pop(gL, 2); // pop LREG_EXTVARS and pointer's subtable
}

static int NetUnArchive(lua_State *L)
{
	int TABLESINDEX = lua_upvalueindex(1);
	savebuffer_t *save = lua_touserdata(L, lua_upvalueindex(2));
	int i, n = lua_gettop(L);
	for (i = 1; i <= n; i++)
		UnArchiveValue(&save->p, TABLESINDEX);
	return n;
}

static void UnArchiveTables(UINT8 **p, boolean network)
{
	int TABLESINDEX;
	UINT16 i, n;

	if (!gL)
		return;

	TABLESINDEX = lua_gettop(gL);

	n = (UINT16)lua_objlen(gL, TABLESINDEX);

	for (i = 1; i <= n; i++)
	{
		lua_rawgeti(gL, TABLESINDEX, i);

		if (!lua_istable(gL, -1))
		{
			CONS_Alert(CONS_ERROR, "Value in tables list #%d is not a table! (corrupted save?)\n", i);
			continue;
		}

		while (true)
		{
			UINT8 ret;

			if (network)
			{
				if (UnArchiveValue(p, TABLESINDEX) == 1) // read key
					break;

				ret = UnArchiveValue(p, TABLESINDEX);
				if (ret == 1)
				{
					CONS_Alert(CONS_ERROR, "Unexpected end of save reached (Corrupted save?)\n");
					lua_pop(gL, 1); // Pop key
					break;
				}
				else if (ret == 2) // read value
					n++;
			}
			else
			{
				ret = UnArchiveValueDemo(p, TABLESINDEX, NULL);
				if (ret == 3)
					lua_pushnil(gL);
				else if (ret == 1) // read key
					break;

				ret = UnArchiveValueDemo(p, TABLESINDEX, NULL);
				if (ret == 3)
					lua_pushnil(gL);
				else if (ret == 2) // read value
					n++;
			}

			if (lua_isnil(gL, -2)) // if key is nil (if a function etc was accidentally saved)
			{
				CONS_Alert(CONS_ERROR, "A nil key in table %d was found! (Invalid key type or corrupted save?)\n", i);
				lua_pop(gL, 2); // pop key and value instead of setting them in the table, to prevent Lua panic errors
			}
			else
				lua_rawset(gL, -3);
		}

		lua_pop(gL, 1);
	}
}

void LUA_Step(void)
{
	if (!gL)
		return;

	if (lua_gettop(gL) != 0)
	{
		CONS_Alert(CONS_WARNING, "Eek, there is garbage on lua stack!\n");
		lua_settop(gL, 0);
		lua_gc(gL, LUA_GCSTEP, 1);
	}
}

void LUA_Archive(savebuffer_t *save, boolean network)
{
	INT32 i;
	thinker_t *th;

	if (gL)
		lua_newtable(gL); // tables to be archived.

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] && i > 0)	// NEVER skip player 0, this is for dedi servs.
			continue;
		// all players in game will be archived, even if they just add a 0.
		ArchiveExtVars(&save->p, &players[i], "player");
	}

	if (network == true)
	{
		if (gamestate == GS_LEVEL)
		{
			for (th = thinkercap.next; th != &thinkercap; th = th->next)
			{
				if (th->function.acp1 != (actionf_p1)P_MobjThinker)
					continue;

				// archive function will determine when to skip mobjs,
				// and write mobjnum in otherwise.
				ArchiveExtVars(&save->p, th, "mobj");
			}
		}
		WRITEUINT32(save->p, UINT32_MAX); // end of mobjs marker, replaces mobjnum.

		LUAh_NetArchiveHook(NetArchive, save); // call the NetArchive hook in archive mode
	}

	ArchiveTables(&save->p);

	if (gL)
		lua_pop(gL, 1); // pop tables
}

void LUA_UnArchive(savebuffer_t *save, boolean network)
{
	UINT32 mobjnum;
	INT32 i;
	thinker_t *th;

	if (gL)
		lua_newtable(gL); // tables to be read

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] && i > 0)	// same here, this is to synch dediservs properly.
			continue;

		UnArchiveExtVars(&save->p, &players[i], network);
	}

	if (network == true)
	{
		do {
			mobjnum = READUINT32(save->p); // read a mobjnum
			for (th = thinkercap.next; th != &thinkercap; th = th->next)
			{
				if (th->function.acp1 != (actionf_p1)P_MobjThinker)
					continue;

				if (((mobj_t *)th)->mobjnum == mobjnum) // find matching mobj
					UnArchiveExtVars(&save->p, th, network); // apply variables
			}
		} while(mobjnum != UINT32_MAX); // repeat until end of mobjs marker.

		LUAh_NetArchiveHook(NetUnArchive, save); // call the NetArchive hook in unarchive mode
	}

	UnArchiveTables(&save->p, network);

	if (gL)
		lua_pop(gL, 1); // pop tables
}

// For mobj_t, player_t, etc. to take custom variables.
int Lua_optoption(lua_State *L, int narg, int def, int list_ref)
{
	int result = -1;

	if (lua_isnoneornil(L, narg))
		return def;

	I_Assert(lua_checkstack(L, 2));
	luaL_checkstring(L, narg);

	lua_rawgeti(L, LUA_REGISTRYINDEX, list_ref);
	I_Assert(lua_istable(L, -1));
	lua_pushvalue(L, narg);
	lua_rawget(L, -2);

	if (lua_isnumber(L, -1))
		result = lua_tointeger(L, -1);

	lua_pop(L, 2); // Pop result and fields table

	return result;
}


int Lua_CreateFieldTable(lua_State *L, const char *const lst[])
{
	int i;

	lua_newtable(L);
	for (i = 0; lst[i] != NULL; i++)
	{
		lua_pushstring(L, lst[i]);
		lua_pushinteger(L, i);
		lua_settable(L, -3);
	}

	return luaL_ref(L, LUA_REGISTRYINDEX);
}
