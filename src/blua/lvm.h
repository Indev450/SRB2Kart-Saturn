/*
** $Id: lvm.h,v 2.5.1.1 2007/12/27 13:02:25 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#ifndef lvm_h
#define lvm_h


#include "ldo.h"
#include "lobject.h"
#include "ltm.h"


#define tostring(L,o) ((ttype(o) == LUA_TSTRING) || (luaV_tostring(L, o)))

#define tonumber(o,n)	(ttype(o) == LUA_TNUMBER || \
                         (((o) = luaV_tonumber(o,n)) != NULL))

#define equalobj(L,o1,o2) \
	(ttype(o1) == ttype(o2) && luaV_equalval(L, o1, o2))


/*
** fast track for 'gettable': if 't' is a table and 't[k]' is not nil,
** return 1 with 'slot' pointing to 't[k]' (final result).  Otherwise,
** return 0 (meaning it will have to check metamethod) with 'slot'
** pointing to a nil 't[k]' (if 't' is a table) or NULL (otherwise).
** 'f' is the raw get function to use.
*/
#define luaV_fastget(L,t,k,slot,f) \
  (!ttistable(t)  \
   ? (slot = NULL, 0)  /* not a table; 'slot' is NULL and result is 0 */  \
   : (slot = f(hvalue(t), k),  /* else, do raw access */  \
      !ttisnil(slot)))  /* result not nil? */

/*
** standard implementation for 'gettable'
*/
#define luaV_gettable(L,t,k,v) { TValue *slot; \
  if (luaV_fastget(L,t,k,slot,luaH_get)) { setobj2s(L, v, slot); } \
  else luaV_finishget(L,t,k,v,slot); }



LUAI_FUNC int luaV_lessthan (lua_State *L, TValue *l, TValue *r);
LUAI_FUNC int luaV_equalval (lua_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC const TValue *luaV_tonumber (const TValue *obj, TValue *n);
LUAI_FUNC int luaV_tostring (lua_State *L, StkId obj);
LUAI_FUNC void luaV_finishget (lua_State *L, TValue *t, TValue *key,
                               StkId val, TValue *tm);
LUAI_FUNC void luaV_settable (lua_State *L, TValue *t, TValue *key,
                                            StkId val);
LUAI_FUNC void luaV_execute (lua_State *L, int nexeccalls);
LUAI_FUNC void luaV_concat (lua_State *L, int total, int last);

#endif
