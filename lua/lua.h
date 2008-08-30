/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#ifndef _LUA_H
#define _LUA_H

#define __USE_BSD

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include "../lib/sstring.h"
#include "../core/nsmalloc.h"

#include "lualocal.h"
#include "luasocket.h"

#define luamalloc(x) nsmalloc(POOL_LUA, x)
#define luarealloc(x, y) nsrealloc(POOL_LUA, x, y)
#define luafree(x) nsfree(POOL_LUA, x)

/*** defines ************************************/

#define LUA_BOTVERSION "1.89"
#define LUA_CHANFIXBOT "D"
#define LUA_OPERCHAN "#twilightzone"

#ifndef LUA_PUKECHAN
#define LUA_PUKECHAN "#qnet.keepout"
#endif

#define LUA_PROFILE

#define LUA_DEBUGSOCKET_ADDRESS "127.0.0.1"
#define LUA_DEBUGSOCKET_PORT 7733

#ifdef LUA_USEJIT
#include <luajit.h>
#define LUA_AUXVERSION " + " LUAJIT_VERSION
#else
#define LUA_AUXVERSION ""
#endif

#define LUA_SMALLVERSION "v" LUA_BOTVERSION " (" LUA_VERSION LUA_AUXVERSION ")"
#define LUA_FULLVERSION "Lua engine " LUA_SMALLVERSION

/*** end defines ************************************/

typedef struct lua_list {
  lua_State *l;
  sstring *name;
  unsigned long calls;
  struct timeval ru_utime, ru_stime;
  struct lua_list *next;
  struct lua_list *prev;
  lua_localnick *nicks;
  lua_socket *sockets;
} lua_list;

#define LUA_STARTLOOP(l) { lua_list *ll; for(ll=lua_head;ll;ll=ll->next) {  l = ll->l

#define LUA_ENDLOOP() } }

#define LUA_PATHLEN 150

extern lua_list *lua_head;
extern sstring *cpath;

lua_State *lua_loadscript(char *file);
void lua_unloadscript(lua_list *l);
lua_list *lua_scriptloaded(char *name);
lua_list *lua_listfromstate(lua_State *l);
int lua_listexists(lua_list *l);
int lua_lineok(const char *data);

#define lua_toint(l, n) (int)lua_tonumber(l, n)
#define lua_isint(l, n) lua_isnumber(l, n)
#define lua_pushint(l, n) lua_pushnumber(l, n)

#define lua_tolong(l, n) (long)lua_tonumber(l, n)
#define lua_islong(l, n) lua_isnumber(l, n)
#define lua_pushlong(l, n) lua_pushnumber(l, n)
#define lua_pushnumeric(l, n) lua_pushlong(l, n)
#define lua_tonumeric(l, n) lua_tolong(l, n)
#define lua_isnumeric(l, n) lua_islong(l, n)

extern struct rusage r_usages;
extern struct rusage r_usagee;

#define USEC_DIFFERENTIAL 1000000
 
#ifdef LUA_PROFILE

#define ACCOUNTING_START(l) { l->calls++; getrusage(RUSAGE_SELF, &r_usages);

#define twrap(A2, B2, C2, D2) A2(&(B2), &(C2), &(D2))

#define _SET_2TIMEDIFF(L4, S2, E2, I2) twrap(timersub, E2.I2, S2.I2, E2.I2); twrap(timeradd, L4->I2, E2.I2, L4->I2);
#define SET_TIMEDIFF(L3, S2, E2) _SET_2TIMEDIFF(L3, S2, E2, ru_utime); _SET_2TIMEDIFF(L3, S2, E2, ru_stime);

#define ACCOUNTING_STOP(l) getrusage(RUSAGE_SELF, &r_usagee); SET_TIMEDIFF(l, r_usages, r_usagee); }

#endif

#ifdef LUA_DEBUGSOCKET

void lua_debugoutput(char *p, ...) __attribute__ ((format (printf, 1, 2)));
#define DEBUGOUT(p, ...) lua_debugoutput(p , ##__VA_ARGS__)

#endif

#ifndef INLINE

#ifdef __GNUC__
#define INLINE __attribute((always_inline)) inline
#endif

#ifdef _MSC_VER
#define INLINE __forceinline
#endif

#ifndef INLINE
#define INLINE inline
#endif

#endif /* INLINE */

INLINE int lua_debugpcall(lua_State *l, char *message, int a, int b, int c);

#endif
