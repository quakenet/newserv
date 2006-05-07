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

#define LUA_BOTVERSION "1.36"
#define LUA_CHANFIXBOT "Z"
#define LUA_OPERCHAN "#twilightzone"
#define LUA_PUKECHAN "#qnet.keepout"

#define LUA_DEBUGSOCKET

#ifdef LUA_JITLIBNAME

#define LUA_USEJIT

#endif

#define LUA_DEBUGSOCKET_ADDRESS "127.0.0.1"
#define LUA_DEBUGSOCKET_PORT 7733

typedef struct lua_list {
  lua_State *l;
  sstring *name;
  unsigned long calls;
  struct lua_list *next;
  struct lua_list *prev;
} lua_list;

#define LUA_STARTLOOP(l) { lua_list *ll; for(ll=lua_head;ll;ll=ll->next) {  l = ll->l

#define LUA_ENDLOOP() } }

#define LUA_PATHLEN 1024

extern lua_list *lua_head;
extern sstring *cpath;

lua_State *lua_loadscript(char *file);
void lua_unloadscript(lua_list *l);
lua_list *lua_scriptloaded(char *name);
lua_list *lua_listfromstate(lua_State *l);

#define lua_toint(l, n) (int)lua_tonumber(l, n)
#define lua_isint(l, n) lua_isnumber(l, n)
#define lua_pushint(l, n) lua_pushnumber(l, n)

#define lua_tolong(l, n) (long)lua_tonumber(l, n)
#define lua_islong(l, n) lua_isnumber(l, n)
#define lua_pushlong(l, n) lua_pushnumber(l, n)

#ifdef LUA_DEBUGSOCKET

void lua_debugoutput(char *p, ...);
#define DEBUGOUT(p, ...) lua_debugoutput(p , ##__VA_ARGS__)
#define lua_debugpcall(l, message, ...) { lua_list *l2 = lua_listfromstate(l); DEBUGOUT("%s: %s\n", l2->name->content, message); l2->calls++; lua_pcall(l , ##__VA_ARGS__); }

#else

#define DEBUGOUT(p, ...)
#define lua_debugpcall(l, message, ...) { lua_listfromstate(l)->calls++; lua_pcall(l , ##__VA_ARGS__); }

#endif

#endif
