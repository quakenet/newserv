/* Lua bindings for Newserv */

/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>

#include "../core/config.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../lib/array.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"
#include "../lib/version.h"
#include "../lib/strlfunc.h"
#include "lua.h"

MODULE_VERSION(LUA_SMALLVERSION);

#ifdef LUA_DEBUGSOCKET

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>

#endif

void lua_startup(void *arg);
void lua_loadscripts(void);
void lua_registercommands(lua_State *l);
void lua_registerdbcommands(lua_State *l);
void lua_initnickpusher(void);
void lua_initchanpusher(void);
void lua_loadlibs(lua_State *l);
void lua_require(lua_State *l, char *module);

void lua_startbot(void *arg);
void lua_destroybot(void);
void lua_startcontrol(void);
void lua_destroycontrol(void);
void lua_destroydb(void);

void lua_onunload(lua_State *l);
void lua_onload(lua_State *l);

void lua_setpath(void);

void lua_setupdebugsocket(void);
void lua_freedebugsocket(void);

void lua_deregisternicks(lua_list *l);
void lua_registerlocalcommands(lua_State *ps);
void lua_registerdebug(lua_State *ps);
void lua_socket_closeall(lua_list *l);
void lua_scheduler_freeall(lua_list *l);
void lua_registersocketcommands(lua_State *ps);
void lua_registercryptocommands(lua_State *ps);
void lua_registerschedulercommands(lua_State *ps);

#ifdef LUA_DEBUGSOCKET

struct sockaddr_in debugsocketdest;
int debugsocket = -1;

#endif

lua_list *lua_head = NULL, *lua_tail = NULL;

sstring *cpath = NULL, *suffix = NULL;

void *startsched = NULL;

int loaded = 0;

struct rusage r_usages;
struct rusage r_usagee;

static const luaL_Reg ourlibs[] = {
  {"", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
#ifdef LUA_USEJIT
  {LUA_JITLIBNAME, luaopen_jit},
#endif
  {NULL, NULL}
};

lua_list dummy;
  
void _init() {
  lua_setupdebugsocket();
  lua_initnickpusher();
  lua_initchanpusher();

  dummy.name = getsstring("???", 10);
  if(!dummy.name) {
    Error("lua", ERR_ERROR, "Cannot set dummy name.");
    return;
  }

  cpath = getcopyconfigitem("lua", "scriptdir", "", 500);

  if(!cpath || !cpath->content || !cpath->content[0]) {
    Error("lua", ERR_ERROR, "Error loading path.");
    return;
  }

  suffix = getcopyconfigitem("lua", "scriptsuffix", ".lua", 10);
  if(!suffix) {
    Error("lua", ERR_ERROR, "Error loading suffix.");
    return;
  }

  lua_setpath();

  loaded = 1;

  startsched = scheduleoneshot(time(NULL) + 1, &lua_startup, NULL);
}

void lua_startup(void *arg) {
  startsched = NULL;

  lua_startcontrol();
  lua_startbot(NULL);

  lua_loadscripts();
}

#ifdef BROKEN_DLCLOSE
void __fini() {
#else
void _fini() {
#endif

  if(loaded) {
    if(startsched)
      deleteschedule(startsched, &lua_startup, NULL);

    while(lua_head)
      lua_unloadscript(lua_head);

    lua_destroybot();
    lua_destroycontrol();
    lua_destroydb();
  }

  freesstring(cpath);
  freesstring(suffix);
  freesstring(dummy.name);

  lua_freedebugsocket();
  nscheckfreeall(POOL_LUA);
}

void lua_loadscripts(void) {
  array *ls;

  ls = getconfigitems("lua", "script");
  if(!ls) {
    Error("lua", ERR_INFO, "No scripts loaded.");
  } else {
    sstring **scripts = (sstring **)(ls->content);
    int i;
    for(i=0;i<ls->cursi;i++)
      lua_loadscript(scripts[i]->content);
  }
}

/* taken from the lua manual, modified to use nsmalloc */
static void *lua_nsmalloc(void *ud, void *ptr, size_t osize, size_t nsize) {
  if(nsize == 0) {
    if(ptr != NULL)
      luafree(ptr);
    return NULL;
  }

  return luarealloc(ptr, nsize);
}

lua_State *lua_loadscript(char *file) {
  char fullpath[LUA_PATHLEN];
  int top;
  lua_State *l;
  lua_list *n;
  char buf[1024];
  void *args[2];

  if(!cpath || !suffix)
    return NULL;

  strlcpy(buf, file, sizeof(buf));

  delchars(buf, "./\\;");

  if(lua_scriptloaded(buf))
    return NULL;

  l = lua_newstate(lua_nsmalloc, NULL);
  if(!l)
    return NULL;

  n = (lua_list *)luamalloc(sizeof(lua_list));;
  if(!n) {
    Error("lua", ERR_ERROR, "Error allocing list for %s.", buf);
    return NULL;
  }

  n->name = getsstring(buf, LUA_PATHLEN);
  if(!n->name) {
    Error("lua", ERR_ERROR, "Error allocing name item for %s.", buf);
    luafree(n);
    return NULL;
  }
  n->calls = 0;

  timerclear(&n->ru_utime);
  timerclear(&n->ru_stime);

  lua_loadlibs(l);
  lua_registerdebug(l);
  lua_registercommands(l);
  lua_registerlocalcommands(l);
  lua_registerdbcommands(l);
  lua_registersocketcommands(l);
  lua_registercryptocommands(l);
  lua_registerschedulercommands(l);

  args[0] = file;
  args[1] = l;
  triggerhook(HOOK_LUA_LOADSCRIPT, args);

#ifdef LUA_USEJIT
  lua_require(l, "lib/jit");
#endif

  lua_require(l, "lib/bootstrap");

  snprintf(fullpath, sizeof(fullpath), "%s/%s%s", cpath->content, file, suffix->content);
  if(luaL_loadfile(l, fullpath)) {
    Error("lua", ERR_ERROR, "Error loading %s.", file);
    lua_close(l);
    freesstring(n->name);
    luafree(n);
    return NULL;
  }

  n->l = l;

  n->next = NULL;
  n->prev = lua_tail;
  n->nicks = NULL;
  n->sockets = NULL;
  n->schedulers = NULL;

  if(!lua_head) { 
    lua_head = n;
  } else {
    lua_tail->next = n;
  }

  lua_tail = n;

  top = lua_gettop(l);

  if(lua_pcall(l, 0, 0, 0)) {
    Error("lua", ERR_ERROR, "Error pcalling: %s.", file);
    lua_close(l);
    freesstring(n->name);

    if(lua_head == n)
      lua_head = NULL;

    lua_tail = n->prev;
    if(lua_tail)
      lua_tail->next = NULL;

    luafree(n);
    return NULL;
  }

  lua_settop(l, top);

  Error("lua", ERR_INFO, "Loaded %s.", file);
  lua_onload(l);

  return l;
}

void lua_unloadscript(lua_list *l) {
  triggerhook(HOOK_LUA_UNLOADSCRIPT, l->l);

  lua_onunload(l->l);
  lua_deregisternicks(l);
  lua_socket_closeall(l);
  lua_scheduler_freeall(l);
  lua_close(l->l);
  freesstring(l->name);

  /* well, at least it's O(1) */

  if(!l->prev) { /* head */
    lua_head = l->next;
    if(!lua_head) {
      lua_tail = NULL;
    } else {
      lua_head->prev = NULL;
    }
  } else {
    if(!l->next) { /* tail */
      lua_tail = lua_tail->prev;
      if(!lua_tail) {
        lua_head = NULL;
      } else {
        lua_tail->next = NULL;
      }
    } else {
      l->prev->next = l->next;
      l->next->prev = l->prev;
     }
  }

  luafree(l);
}

void lua_setpath(void) {
  char fullpath[LUA_PATHLEN];

  snprintf(fullpath, sizeof(fullpath), "%s/?%s", cpath->content, suffix->content);
  setenv("LUA_PATH", fullpath, 1);
}

lua_list *lua_scriptloaded(char *name) {
  lua_list *l;

  for(l=lua_head;l;l=l->next)
    if(!strcmp(l->name->content, name))
      return l;

  return NULL;
}

void lua_loadlibs(lua_State *l) {
  const luaL_Reg *lib = ourlibs;

  for (;lib->func;lib++) {
    lua_pushcfunction(l, lib->func);
    lua_pushstring(l, lib->name);
    lua_call(l, 1, 0);
  }
}

void lua_require(lua_State *l, char *module) {
  int top = lua_gettop(l);

  lua_getglobal(l, "require");
  lua_pushstring(l, module);

  if(lua_pcall(l, 1, 1, 0))
    Error("lua", ERR_ERROR, "Error requiring %s: %s", module, lua_tostring(l, -1));

  lua_settop(l, top);
}

void lua_setupdebugsocket(void) {
#ifdef LUA_DEBUGSOCKET

  debugsocket = socket(AF_INET, SOCK_DGRAM, 0);
  if(debugsocket < 0) {
    debugsocket = -1;
    Error("lua", ERR_ERROR, "Cannot create debug socket.");

    return;
  }

  memset(&debugsocketdest, 0, sizeof(debugsocketdest));

  debugsocketdest.sin_family = AF_INET;
  debugsocketdest.sin_port = htons(LUA_DEBUGSOCKET_PORT);
  debugsocketdest.sin_addr.s_addr = inet_addr(LUA_DEBUGSOCKET_ADDRESS);

#endif
}

void lua_freedebugsocket(void) {
#ifdef LUA_DEBUGSOCKET

  if(debugsocket == -1)
    return;

  close(debugsocket);


  debugsocket = -1;

#endif
}

#ifdef LUA_DEBUGSOCKET
void lua_debugoutput(char *format, ...) {
  char buf[1024];
  va_list va;
  int size;

  if(debugsocket == -1)
    return;

  va_start(va, format);
  size = vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  if(size >= sizeof(buf))
    size = sizeof(buf) - 1;

  if(size > 0)
    sendto(debugsocket, buf, size, 0, (struct sockaddr *)&debugsocketdest, sizeof(debugsocketdest));
}
#endif

lua_list *lua_listfromstate(lua_State *l) {
  lua_list *i = lua_head;
  for(;i;i=i->next)
    if(i->l == l)
      return i;

  return &dummy;
}

int lua_listexists(lua_list *l) {
  lua_list *i;

  for(i=lua_head;i;i=i->next)
    if(i == l)
      return 1;

  return 0;
}

int lua_lineok(const char *data) {
  if(strchr(data, '\r') || strchr(data, '\n'))
    return 0;
  return 1;
}

int lua_debugpcall(lua_State *l, char *message, int a, int b, int c) {
  lua_list *l2 = lua_listfromstate(l);
  int ret;

#ifdef LUA_DEBUGSOCKET
  DEBUGOUT("%s: %s\n", l2->name->content, message);
#endif

#ifdef LUA_PROFILE
  ACCOUNTING_START(l2);
#endif

  ret = lua_pcall(l, a, b, c);

#ifdef LUA_PROFILE
  ACCOUNTING_STOP(l2);
#endif

  return ret;
}

