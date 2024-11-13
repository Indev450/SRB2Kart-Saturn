/*
** Library initialization.
** Copyright (C) 2005-2023 Mike Pall. See Copyright Notice in luajit.h
**
** Major parts taken verbatim from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lib_init_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_arch.h"

static const luaL_Reg lj_lib_load[] = {
  { "",			luaopen_base },
#if !LJ_SRB2LIB
  { LUA_LOADLIBNAME,	luaopen_package },
#endif
  { LUA_TABLIBNAME,	luaopen_table },
  { LUA_IOLIBNAME,	luaopen_io },
#if !LJ_SRB2LIB || (LJ_SRB2LIB && LJ_SRB2OS)
  { LUA_OSLIBNAME,	luaopen_os },
#endif
  { LUA_STRLIBNAME,	luaopen_string },
#if !LJ_SRB2LIB
  { LUA_MATHLIBNAME,	luaopen_math },
#endif
#if !LJ_SRB2LIB || (LJ_SRB2LIB && LJ_SRB2DEBUG)
  { LUA_DBLIBNAME,	luaopen_debug },
#endif
#if !LJ_SRB2LIB
  { LUA_BITLIBNAME,	luaopen_bit },
#endif
  { LUA_JITLIBNAME,	luaopen_jit },
  { NULL,		NULL }
};

static const luaL_Reg lj_lib_preload[] = {
#if LJ_HASFFI
  { LUA_FFILIBNAME,	luaopen_ffi },
#endif
  { NULL,		NULL }
};

LUALIB_API void luaL_openlibs(lua_State *L)
{
  const luaL_Reg *lib;
  for (lib = lj_lib_load; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
  }
  luaL_findtable(L, LUA_REGISTRYINDEX, "_PRELOAD",
		 sizeof(lj_lib_preload)/sizeof(lj_lib_preload[0])-1);
  for (lib = lj_lib_preload; lib->func; lib++) {
    lua_pushcfunction(L, lib->func);
    lua_setfield(L, -2, lib->name);
  }
  lua_pop(L, 1);
}

