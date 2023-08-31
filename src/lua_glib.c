#include "lua_glib.h"

#define GLIB_DATABASE_GUID "{cf75e032-00c8-4887-b15f-87b755784ad3}"
    #define GLIB_DATABASE_GETTERS "getters"
    #define GLIB_DATABASE_SETTERS "setters"
    #define GLIB_DATABASE_ENUMS   "enums"
    #define GLIB_DATABASE_CACHE   "cache"
    #define GLIB_DATABASE_MTABLE  "metatable"

static inline void lua_glib_push_db(lua_State *L)
{
    lua_pushliteral(L, GLIB_DATABASE_GUID);
    lua_gettable(L, LUA_REGISTRYINDEX);
}

/**
 * Copy table contents to another.
 * @param L[1] Destination table.
 * @param L[2] Source table.
 */
static int lua_glib_copy_table(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    lua_pushnil(L);
    while (lua_next(L, 2))
    {
        lua_pushvalue(L, -2);   /* Push key. */
        lua_pushvalue(L, -2);   /* Push value. */
        lua_settable(L, 1);     /* Store at dst. */

        lua_pop(L, 1);  /* Pop the value. */
    }

    return 0;
}

static int lua_glib___index(lua_State *L)
{
    /* function mt:__index(key) */

    const int CACHE   = lua_upvalueindex(1); /* Enum cache. */
    const int GETTERS = lua_upvalueindex(2); /* Storage for getters. */
    const int ENUMS   = lua_upvalueindex(3); /* Enum fallback cache. */

    if (!lua_isstring(L, 1))
    {
        /* A non string key should not happen. But somebody might really want it. */
        return 0;
    }

    /* Attempt cache lookup. */

    lua_pushvalue(L, 1);
    lua_gettable(L, CACHE);
    if (!lua_isnil(L, -1))
    {
        return 1;
    }

    lua_pop(L, 1);

    /* Attempt a get property. */

    lua_pushvalue(L, 1);
    lua_gettable(L, GETTERS);
    if (lua_isfunction(L, -1))
    {
        lua_call(L, 0, 1);
        return 1;
    }

    /* Try finding a the relevant enum fallback function. */
    const char *str = lua_tostring(L, 1);
    int sz = lua_strlen(L, 1);
    for (int i = 1; i < sz; i++)
    {
        lua_pushlstring(L, str, i);
        lua_gettable(L, ENUMS);

        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, 1);
            lua_call(L, 1, 1);

            /* Store value in cache. */
            lua_pushvalue(L, 1);
            lua_pushvalue(L, -2);
            lua_settable(L, CACHE);

            /* Return it. */
            return 1;
        }
        else
        {
            lua_pop(L, 1);
        }
    }

    /* This really does not exist. Returns nil. */
    return 0;
}

static int lua_glib___newindex(lua_State *L)
{
    /* function mt:__newindex(key, value) */

    const int SETTERS = lua_upvalueindex(1);

    lua_pushvalue(L, 1);
    lua_gettable(L, SETTERS);
    if (!lua_isfunction(L, -1))
    {
        return luaL_error(L, "You cannot set the global table except known writeable properties. If this isn't a mistake use `rawset(_G, k, v)`.");
    }

    /* Set the property. */
    lua_pushvalue(L, 2);
    lua_call(L, 1, 0);
    return 0;
}

int lua_glib_require(lua_State *L)
{
    lua_pushliteral(L, GLIB_DATABASE_GUID);
    lua_newtable(L);
        /* Standard functions. */
        lua_pushliteral(L, GLIB_DATABASE_GETTERS);
        lua_newtable(L);
        lua_settable(L, -3);

        lua_pushliteral(L, GLIB_DATABASE_SETTERS);
        lua_newtable(L);
        lua_settable(L, -3);

        lua_pushliteral(L, GLIB_DATABASE_ENUMS);
        lua_newtable(L);
        lua_settable(L, -3);

        lua_pushliteral(L, GLIB_DATABASE_CACHE);
        lua_newtable(L);
        lua_settable(L, -3);

        lua_pushliteral(L, GLIB_DATABASE_MTABLE);
        lua_newtable(L);
            lua_pushliteral(L, "__index");
                lua_pushliteral(L, GLIB_DATABASE_CACHE);
                lua_gettable(L, -5);

                lua_pushliteral(L, GLIB_DATABASE_GETTERS);
                lua_gettable(L, -6);

                lua_pushliteral(L, GLIB_DATABASE_ENUMS);
                lua_gettable(L, -7);

                lua_pushcclosure(L, lua_glib___index, 3);
            lua_settable(L, -3); /* __index */

            lua_pushliteral(L, "__newindex");
                lua_pushliteral(L, GLIB_DATABASE_SETTERS);
                lua_gettable(L, -5);

                lua_pushcclosure(L, lua_glib___newindex, 1);
            lua_settable(L, -3);    /* __newindex */

        lua_settable(L, -3);    /* GLIB_DATABASE_MTABLE. */

    lua_settable(L, LUA_REGISTRYINDEX); /* GLIB_DATABASE_GUID. */

    /* Set global metatable. */
    lua_glib_push_db(L);
    lua_pushliteral(L, GLIB_DATABASE_MTABLE);
    lua_gettable(L, -2);
    lua_setmetatable(L, LUA_GLOBALSINDEX);

    return 0;
}

int lua_glib_new_enum(lua_State *L)
{
    lua_glib_push_db(L);

    if (lua_istable(L, 1))
    {
        /* copy_table(dst, src) */

        lua_pushcfunction(L, lua_glib_copy_table);

        lua_pushliteral(L, GLIB_DATABASE_CACHE);
        lua_gettable(L, -3);    /* dst */

        lua_pushvalue(L, 1);    /* src */
        lua_call(L, 2, 0);
    }

    if (lua_isstring(L, 2) && lua_isfunction(L, 3))
    {
        lua_pushliteral(L, GLIB_DATABASE_ENUMS);
        lua_gettable(L, -2);

        lua_pushvalue(L, 1);    /* prefix */
        lua_pushvalue(L, 3);    /* fallback */
    }

    return 0;
}

int lua_glib_new_getter(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_glib_push_db(L);
    lua_pushliteral(L, GLIB_DATABASE_GETTERS);
    lua_gettable(L, -2);

    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_settable(L, -3);

    return 0;
}

int lua_glib_new_setter(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_glib_push_db(L);
    lua_pushliteral(L, GLIB_DATABASE_SETTERS);
    lua_gettable(L, -2);

    lua_pushvalue(L, 1);
    lua_pushvalue(L, 2);
    lua_settable(L, -3);

    return 0;
}

#define _GLIB_DECL_INT_GETTER(T, name) \
    int lua_glib_getter_##name(lua_State *L) { \
        const T* ptr = (const T*)lua_topointer(L, lua_upvalueindex(1)); \
        lua_pushinteger(L, (lua_Integer)*ptr); \
        return 1; \
    }

#define _GLIB_DECL_INT_SETTER(T, name) \
    int lua_glib_setter_##name(lua_State *L) { \
        luaL_checktype(L, 1, LUA_TNUMBER); \
        T* ptr = (T*)lua_topointer(L, lua_upvalueindex(1)); \
        *ptr = (T)lua_tointeger(L, 1); \
        return 1; \
    }

#define _GLIB_DECL_FLOAT_GETTER(T, name) \
    int lua_glib_getter_##name(lua_State *L) { \
        const T* ptr = (const T*)lua_topointer(L, lua_upvalueindex(1)); \
        lua_pushnumber(L, (lua_Number)*ptr); \
        return 1; \
    }

#define _GLIB_DECL_FLOAT_SETTER(T, name) \
    int lua_glib_setter_##name(lua_State *L) { \
        luaL_checktype(L, 1, LUA_TNUMBER); \
        T* ptr = (T*)lua_topointer(L, lua_upvalueindex(1)); \
        *ptr = (T)lua_tonumber(L, 1); \
        return 1; \
    }

_GLIB_DECL_INT_GETTER(int8_t,  i8);
_GLIB_DECL_INT_GETTER(int16_t, i16);
_GLIB_DECL_INT_GETTER(int32_t, i32);
_GLIB_DECL_INT_GETTER(int64_t, i64);
_GLIB_DECL_INT_GETTER(uint8_t,  u8);
_GLIB_DECL_INT_GETTER(uint16_t, u16);
_GLIB_DECL_INT_GETTER(uint32_t, u32);
_GLIB_DECL_INT_GETTER(uint64_t, u64);
_GLIB_DECL_FLOAT_GETTER(float, f32);
_GLIB_DECL_FLOAT_GETTER(double, f64);

_GLIB_DECL_INT_SETTER(int8_t,  i8);
_GLIB_DECL_INT_SETTER(int16_t, i16);
_GLIB_DECL_INT_SETTER(int32_t, i32);
_GLIB_DECL_INT_SETTER(int64_t, i64);
_GLIB_DECL_INT_SETTER(uint8_t,  u8);
_GLIB_DECL_INT_SETTER(uint16_t, u16);
_GLIB_DECL_INT_SETTER(uint32_t, u32);
_GLIB_DECL_INT_SETTER(uint64_t, u64);
_GLIB_DECL_FLOAT_SETTER(float, f32);
_GLIB_DECL_FLOAT_SETTER(double, f64);

int lua_glib_getter_str(lua_State *L)
{
    const char *str = (const char*)lua_topointer(L, lua_upvalueindex(1));
    lua_pushstring(L, str);
    return 1;
}

int lua_glib_getter_ptr(lua_State *L)
{
    lua_pushvalue(L, lua_upvalueindex(1));
    return 1;
}
