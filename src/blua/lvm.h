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
** fast track for 'gettable': 1 means 'aux' points to resulted value;
** 0 means 'aux' is metamethod (if 't' is a table) or NULL. 'f' is
** the raw get function to use.
*/
#define luaV_fastget(L,t,k,aux,f) \
  (!ttistable(t)  \
   ? (aux = NULL, 0)  /* not a table; 'aux' is NULL and result is 0 */  \
   : (aux = f(hvalue(t), k),  /* else, do raw access */  \
      !ttisnil(aux) ? 1  /* result not nil? 'aux' has it */  \
      : (aux = fasttm(L, hvalue(t)->metatable, TM_INDEX),  /* get metamethod */\
         aux != NULL  ? 0  /* has metamethod? must call it */  \
         : (aux = luaO_nilobject, 1))))  /* else, final result is nil */

/*
** standard implementation for 'gettable'
*/
#define luaV_gettable(L,t,k,v) { TValue *aux; \
  if (luaV_fastget(L,t,k,aux,luaH_get)) { setobj2s(L, v, aux); } \
  else luaV_finishget(L,t,k,v,aux); }


/*
** Fast track for set table. If 't' is a table and 't[k]' is not nil,
** call GC barrier, do a raw 't[k]=v', and return true; otherwise,
** return false with 'slot' equal to NULL (if 't' is not a table) or
** 'nil'. (This is needed by 'luaV_finishget'.) Note that, if the macro
** returns true, there is no need to 'invalidateTMcache', because the
** call is not creating a new entry.
*/
#define luaV_fastset(L,t,k,slot,f,v) \
  (!ttistable(t) \
   ? (slot = NULL, 0) \
   : (slot = f(L, hvalue(t), k), \
     ttisnil(slot) ? 0 \
     : (__extension__ ({ \
          if (valiswhite(v) && isblack(obj2gco(hvalue(t)))) \
            luaC_barrierback(L, hvalue(t)); \
          setobj2t(L, cast(TValue *,slot), v); \
          1; \
        }))))


#define luaV_settable(L,t,k,v) { TValue *slot; \
  if (!luaV_fastset(L,t,k,slot,luaH_set,v)) \
    luaV_finishset(L,t,k,v,slot); }



LUAI_FUNC int luaV_lessthan (lua_State *L, TValue *l, TValue *r);
LUAI_FUNC int luaV_equalval (lua_State *L, const TValue *t1, const TValue *t2);
LUAI_FUNC const TValue *luaV_tonumber (const TValue *obj, TValue *n);
LUAI_FUNC int luaV_tostring (lua_State *L, StkId obj);
LUAI_FUNC void luaV_finishget (lua_State *L, TValue *t, TValue *key,
                               StkId val, TValue *tm);
LUAI_FUNC void luaV_finishset (lua_State *L, TValue *t, TValue *key,
                               StkId val, TValue *oldval);
LUAI_FUNC void luaV_execute (lua_State *L, int nexeccalls);
LUAI_FUNC void luaV_concat (lua_State *L, int total, int last);

#endif
