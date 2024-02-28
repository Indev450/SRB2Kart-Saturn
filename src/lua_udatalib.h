#ifndef __LUA_UDATALIB_H__
#define __LUA_UDATALIB_H__

#include "doomdef.h"
#include "lua_script.h"
#include "lua_libs.h"
#include "fastcmp.h"

// Indices in getter/setter tables for fields
#define UDATALIB_GETTER 1
#define UDATALIB_SETTER 2

typedef struct udata_field_s {
    const char *name;
    size_t offset;
    lua_CFunction getter;
    lua_CFunction setter;
} udata_field_t;

// Simple setters and getters, for situations where getter is simply
// "return obj.field" and setter is "obj.field = value"
int udatalib_getter_fixed(lua_State *L);
int udatalib_getter_angle(lua_State *L);
int udatalib_getter_uint8(lua_State *L);
int udatalib_getter_sint8(lua_State *L);
int udatalib_getter_uint16(lua_State *L);
int udatalib_getter_int16(lua_State *L);
int udatalib_getter_uint32(lua_State *L);
int udatalib_getter_int32(lua_State *L);
int udatalib_getter_uint64(lua_State *L);
int udatalib_getter_int64(lua_State *L);
int udatalib_getter_string(lua_State *L);
int udatalib_getter_boolean(lua_State *L);
int udatalib_getter_spritenum(lua_State *L);
int udatalib_getter_mobj(lua_State *L);
int udatalib_getter_subsector(lua_State *L);
int udatalib_getter_mobjtype(lua_State *L);
int udatalib_getter_player(lua_State *L);
int udatalib_getter_mapthing(lua_State *L);
int udatalib_getter_slope(lua_State *L);
int udatalib_getter_cmd(lua_State *L);
int udatalib_getter_playerstate(lua_State *L);
int udatalib_getter_powers(lua_State *L);
int udatalib_getter_kartstuff(lua_State *L);
int udatalib_getter_pflags(lua_State *L);
int udatalib_getter_panim(lua_State *L);
int udatalib_getter_tic(lua_State *L);

int udatalib_setter_fixed(lua_State *L);
int udatalib_setter_angle(lua_State *L);
int udatalib_setter_uint8(lua_State *L);
int udatalib_setter_sint8(lua_State *L);
int udatalib_setter_uint16(lua_State *L);
int udatalib_setter_int16(lua_State *L);
int udatalib_setter_uint32(lua_State *L);
int udatalib_setter_int32(lua_State *L);
int udatalib_setter_uint64(lua_State *L);
int udatalib_setter_int64(lua_State *L);
int udatalib_setter_string(lua_State *L);
int udatalib_setter_boolean(lua_State *L);
int udatalib_setter_boolean_nocheck(lua_State *L);
int udatalib_setter_spritenum(lua_State *L);
int udatalib_setter_mobj(lua_State *L);
int udatalib_setter_playerstate(lua_State *L);
int udatalib_setter_pflags(lua_State *L);
int udatalib_setter_panim(lua_State *L);
int udatalib_setter_tic(lua_State *L);

// Adds setter and getter for field and stores it as field.name..".get" and
// field.name..".set" in table at index mt. (metatable, usually)
void udatalib_addfield(lua_State *L, int mt, udata_field_t field);

// Convinience function to add array of fields at once.
void udatalib_addfields(lua_State *L, int mt, const udata_field_t fields[]);

// I miss C++ template functons :(

// Get pointer to field value within getter or setter
#define UDATALIB_GETFIELD(type, field) \
{ \
    size_t offset = lua_tointeger(L, lua_upvalueindex(1)); \
    field = (type*)((uint8_t*)lua_touserdata(L, 1) + offset); \
}

// Creates implementation for simple getter and setter. Super lazy, yes.
// Assumes userdata is a pointer to actual data. Maybe not really safe.
#define UDATALIB_SIMPLE_GETTER(type, pushf) \
{ \
    type *field; \
    UDATALIB_GETFIELD(type, field) \
    pushf(L, *field); \
    return 1; \
}

#define UDATALIB_SIMPLE_SETTER(type, checkf) \
{ \
    type *field; \
    UDATALIB_GETFIELD(type, field) \
    *field = checkf(L, 2); \
    return 0; \
}

#endif