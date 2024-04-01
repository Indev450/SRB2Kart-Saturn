/**
 * @file lua_glib.h Lua global table field management library.
 */
#ifndef _LUA_GLIB_H_
#define _LUA_GLIB_H_

#include "doomdef.h"
#include "lua_script.h"

/**
 * Feed a list of items to append to the enum cache.
 * @param L[1] The table of tiems to add to the enum cache.
 * @returns Nothing.
 */
int lua_glib_append_cache(lua_State *L);

/**
 * Create a new enum.
 * @param L[1] The cache table for the enum.
 * @param L[2] The prefix for the enum.
 * @param L[3] The fallback function for the enum. Can be nil.
 * @returns Nothing.
 */
int lua_glib_new_enum(lua_State *L);

/* Invalidate cache entry.
 * @param L[1] Name of entry. ("MT_MYOBJECT" for example)
 */
int lua_glib_invalidate_cache(lua_State *L);

/**
 *  Create a new getter.
 * @param L[1] The name of the getter.
 * @param L[2] The getter() function.
 * @returns Nothing.
 */
int lua_glib_new_getter(lua_State *L);

/**
 * Create a new setter.
 * @param L[1] The name of the setter.
 * @param L[2] The setter(value) function.
 */
int lua_glib_new_setter(lua_State *L);

/**
 * Require the globals library.
 * @param L[1] (optional) False to inhibit setting the global metatable.
 * @remarks This is a standard Lua require function.
 */
int lua_glib_require(lua_State *L);

/**
 * Get the proxy table that can be used to get globals.
 */
void lua_glib_get_proxy(lua_State *L);

/**
 * Helper function to push a linear enum cache.
 * @param L The Lua state.
 * @param prefix enum prefix.
 * @param list Enum list.
 * @param count Number of elements in list. INT_MAX for NULL termination.
 */
inline static void lua_glib_push_linear_cache(lua_State *L, const char *prefix, const char * const *list, lua_Integer count)
{
    lua_newtable(L);
    for (lua_Integer i = 0; i < count && list[i]; i++)
    {
        lua_pushfstring(L, "%s%s", prefix, list[i]);
        lua_pushinteger(L, i);
        lua_settable(L, -3);
    }
}

/**
 * Helper function to push a bitfield enum cache.
 * @param L The Lua state.
 * @param prefix enum prefix.
 * @param list Enum list.
 * @param count Number of elements in list. INT_MAX for NULL termination.
 */
inline static void lua_glib_push_bitfield_cache(lua_State *L, const char *prefix, const char * const *list, lua_Integer count)
{
    lua_newtable(L);
    for (lua_Integer i = 0; i < count && list[i]; i++)
    {
        lua_pushfstring(L, "%s%s", prefix, list[i]);
        lua_pushinteger(L, (lua_Integer)1 << i);
        lua_settable(L, -3);
    }
}

/* Base getters. */

int lua_glib_getter_i8(lua_State *L);
int lua_glib_getter_i16(lua_State *L);
int lua_glib_getter_i32(lua_State *L);
int lua_glib_getter_i64(lua_State *L);
int lua_glib_getter_u8(lua_State *L);
int lua_glib_getter_u16(lua_State *L);
int lua_glib_getter_u32(lua_State *L);
int lua_glib_getter_u64(lua_State *L);
int lua_glib_getter_f32(lua_State *L);
int lua_glib_getter_f64(lua_State *L);
int lua_glib_getter_b8(lua_State *L);
int lua_glib_getter_b16(lua_State *L);
int lua_glib_getter_b32(lua_State *L);
int lua_glib_getter_b64(lua_State *L);
int lua_glib_getter_fxp(lua_State *L);
int lua_glib_getter_bool(lua_State *L);
int lua_glib_getter_str(lua_State *L);
int lua_glib_getter_ptr(lua_State *L);

/* Base setters. */

int lua_glib_setter_i8(lua_State *L);
int lua_glib_setter_i16(lua_State *L);
int lua_glib_setter_i32(lua_State *L);
int lua_glib_setter_i64(lua_State *L);
int lua_glib_setter_u8(lua_State *L);
int lua_glib_setter_u16(lua_State *L);
int lua_glib_setter_u32(lua_State *L);
int lua_glib_setter_u64(lua_State *L);
int lua_glib_setter_f32(lua_State *L);
int lua_glib_setter_f64(lua_State *L);
int lua_glib_setter_b8(lua_State *L);
int lua_glib_setter_b16(lua_State *L);
int lua_glib_setter_b32(lua_State *L);
int lua_glib_setter_b64(lua_State *L);
int lua_glib_setter_fxp(lua_State *L);
int lua_glib_setter_bool(lua_State *L);

#define _LUA_GLIB_DECL_PGETTER(T, name) \
    static inline void lua_glib_push_##name##_getter(lua_State *L, T* ptr) { \
        lua_pushlightuserdata(L, (void*)ptr); \
        lua_pushcclosure(L, lua_glib_getter_##name, 1); \
    }

    _LUA_GLIB_DECL_PGETTER(int8_t,  i8);
    _LUA_GLIB_DECL_PGETTER(int16_t, i16);
    _LUA_GLIB_DECL_PGETTER(int32_t, i32);
    _LUA_GLIB_DECL_PGETTER(int64_t, i64);
    _LUA_GLIB_DECL_PGETTER(uint8_t,  u8);
    _LUA_GLIB_DECL_PGETTER(uint16_t, u16);
    _LUA_GLIB_DECL_PGETTER(uint32_t, u32);
    _LUA_GLIB_DECL_PGETTER(uint64_t, u64);
    _LUA_GLIB_DECL_PGETTER(char, str);
    _LUA_GLIB_DECL_PGETTER(void, ptr);
    _LUA_GLIB_DECL_PGETTER(uint8_t,  b8);
    _LUA_GLIB_DECL_PGETTER(uint16_t, b16);
    _LUA_GLIB_DECL_PGETTER(uint32_t, b32);
    _LUA_GLIB_DECL_PGETTER(uint64_t, b64);
    _LUA_GLIB_DECL_PGETTER(fixed_t, fxp);
    _LUA_GLIB_DECL_PGETTER(boolean, bool);
#undef _LUA_GLIB_DECL_PGETTER

#define _LUA_GLIB_DECL_PSETTER(T, name) \
    static inline void lua_glib_push_##name##_setter(lua_State *L, T* ptr) { \
        lua_pushlightuserdata(L, ptr); \
        lua_pushcclosure(L, lua_glib_setter_##name, 1); \
    }

    _LUA_GLIB_DECL_PSETTER(int8_t,  i8);
    _LUA_GLIB_DECL_PSETTER(int16_t, i16);
    _LUA_GLIB_DECL_PSETTER(int32_t, i32);
    _LUA_GLIB_DECL_PSETTER(int64_t, i64);
    _LUA_GLIB_DECL_PSETTER(uint8_t,  u8);
    _LUA_GLIB_DECL_PSETTER(uint16_t, u16);
    _LUA_GLIB_DECL_PSETTER(uint32_t, u32);
    _LUA_GLIB_DECL_PSETTER(uint64_t, u64);
    _LUA_GLIB_DECL_PSETTER(uint8_t,  b8);
    _LUA_GLIB_DECL_PSETTER(uint16_t, b16);
    _LUA_GLIB_DECL_PSETTER(uint32_t, b32);
    _LUA_GLIB_DECL_PSETTER(uint64_t, b64);
    _LUA_GLIB_DECL_PSETTER(fixed_t, fxp);
    _LUA_GLIB_DECL_PSETTER(boolean, bool);
#undef _LUA_GLIB_DECL_PSETTER

#endif
