/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#ifndef _LUA_H
#define _LUA_H

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>

#include "../lib/sstring.h"

#define LUA_BOTVERSION "1.17"
#define LUA_CHANFIXBOT "Z"
#define LUA_OPERCHAN "#twilightzone"
#define LUA_PUKECHAN "#qnet.trj"

typedef struct lua_list {
  lua_State *l;
  sstring *name;

  struct lua_list *next;
  struct lua_list *prev;
} lua_list;

#define LUA_STARTLOOP(l) { lua_list *ll; for(ll=lua_head;ll;ll=ll->next) {  l = ll->l

#define LUA_ENDLOOP() } }

#define LUA_PATHLEN 1024

extern lua_list *lua_head;

lua_State *lua_loadscript(char *file);
void lua_unloadscript(lua_list *l);
lua_list *lua_scriptloaded(char *name);

#define lua_toint(l, n) (int)lua_tonumber(l, n)
#define lua_isint(l, n) lua_isnumber(l, n)
#define lua_pushint(l, n) lua_pushnumber(l, n)

#define lua_tolong(l, n) (long)lua_tonumber(l, n)
#define lua_islong(l, n) lua_isnumber(l, n)
#define lua_pushlong(l, n) lua_pushnumber(l, n)

#endif

