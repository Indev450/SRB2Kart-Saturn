#include "doomtype.h"
#include "m_fixed.h"
#include "lua_udatalib.h"

// Getters
int udatalib_getter_fixed(lua_State *L)
UDATALIB_SIMPLE_GETTER(fixed_t, lua_pushinteger)

int udatalib_getter_angle(lua_State *L)
UDATALIB_SIMPLE_GETTER(angle_t, lua_pushangle)

int udatalib_getter_uint8(lua_State *L)
UDATALIB_SIMPLE_GETTER(UINT8, lua_pushinteger)

int udatalib_getter_sint8(lua_State *L)
UDATALIB_SIMPLE_GETTER(SINT8, lua_pushinteger)

int udatalib_getter_uint16(lua_State *L)
UDATALIB_SIMPLE_GETTER(UINT16, lua_pushinteger)

int udatalib_getter_int16(lua_State *L)
UDATALIB_SIMPLE_GETTER(INT16, lua_pushinteger)

int udatalib_getter_uint32(lua_State *L)
UDATALIB_SIMPLE_GETTER(UINT32, lua_pushinteger)

int udatalib_getter_int32(lua_State *L)
UDATALIB_SIMPLE_GETTER(INT32, lua_pushinteger)

int udatalib_getter_uint64(lua_State *L)
UDATALIB_SIMPLE_GETTER(UINT64, lua_pushinteger)

int udatalib_getter_int64(lua_State *L)
UDATALIB_SIMPLE_GETTER(INT64, lua_pushinteger)

int udatalib_getter_string(lua_State *L)
UDATALIB_SIMPLE_GETTER(const char*, lua_pushstring)

int udatalib_getter_boolean(lua_State *L)
UDATALIB_SIMPLE_GETTER(boolean, lua_pushboolean)

int udatalib_getter_spritenum(lua_State *L)
UDATALIB_SIMPLE_GETTER(spritenum_t, lua_pushinteger)

int udatalib_getter_tic(lua_State *L)
UDATALIB_SIMPLE_GETTER(tic_t, lua_pushinteger)

// Setters
int udatalib_setter_fixed(lua_State *L)
UDATALIB_SIMPLE_SETTER(fixed_t, luaL_checkfixed)

int udatalib_setter_angle(lua_State *L)
UDATALIB_SIMPLE_SETTER(angle_t, luaL_checkangle)

int udatalib_setter_uint8(lua_State *L)
UDATALIB_SIMPLE_SETTER(UINT8, (UINT8)luaL_checkinteger)

int udatalib_setter_sint8(lua_State *L)
UDATALIB_SIMPLE_SETTER(SINT8, luaL_checkinteger)

int udatalib_setter_uint16(lua_State *L)
UDATALIB_SIMPLE_SETTER(UINT16, (UINT16)luaL_checkinteger)

int udatalib_setter_int16(lua_State *L)
UDATALIB_SIMPLE_SETTER(INT16, luaL_checkinteger)

int udatalib_setter_uint32(lua_State *L)
UDATALIB_SIMPLE_SETTER(UINT32, (UINT32)luaL_checkinteger)

int udatalib_setter_int32(lua_State *L)
UDATALIB_SIMPLE_SETTER(INT32, luaL_checkinteger)

int udatalib_setter_uint64(lua_State *L)
UDATALIB_SIMPLE_SETTER(UINT64, (UINT64)luaL_checkinteger)

int udatalib_setter_int64(lua_State *L)
UDATALIB_SIMPLE_SETTER(INT64, luaL_checkinteger)

int udatalib_setter_string(lua_State *L)
UDATALIB_SIMPLE_SETTER(const char*, luaL_checkstring)

int udatalib_setter_boolean(lua_State *L)
UDATALIB_SIMPLE_SETTER(boolean, luaL_checkboolean)

int udatalib_setter_spritenum(lua_State *L)
UDATALIB_SIMPLE_SETTER(spritenum_t, (spritenum_t)luaL_checkinteger)

int udatalib_setter_tic(lua_State *L)
UDATALIB_SIMPLE_SETTER(tic_t, (tic_t)luaL_checkinteger)


void udatalib_addfield(lua_State *L, int mt, udata_field_t field)
{
    int idx = abs_index(L, mt);

    lua_pushstring(L, field.name);
    lua_pushliteral(L, ".get");
    lua_concat(L, 2);

    lua_pushinteger(L, field.offset);
    lua_pushcclosure(L, field.getter, 1);

    lua_settable(L, idx);

    lua_pushstring(L, field.name);
    lua_pushliteral(L, ".set");
    lua_concat(L, 2);

    lua_pushinteger(L, field.offset);
    lua_pushcclosure(L, field.setter, 1);

    lua_settable(L, idx);
}

void udatalib_addfields(lua_State *L, int mt, const udata_field_t fields[])
{
    for (unsigned i = 0; fields[i].name != NULL; ++i)
    {
        udatalib_addfield(L, mt, fields[i]);
        CONS_Printf("Add field name=%s, offset=%d, getter=%p, setter=%p\n", fields[i].name, fields[i].offset, fields[i].getter, fields[i].setter);
    }
}
