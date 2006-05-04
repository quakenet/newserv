/* Lua bindings for Newserv */

/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#include "../core/config.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../lib/array.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"

#include "lua.h"

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
void lua_loadlibs(lua_State *l);
void lua_require(lua_State *l, char *module);

void lua_startbot(void *arg);
void lua_destroybot(void);
void lua_startcontrol(void);
void lua_destroycontrol(void);

void lua_onunload(lua_State *l);
void lua_onload(lua_State *l);

void lua_setpath(lua_State *l);

void lua_setupdebugsocket(void);
void lua_freedebugsocket(void);

#ifdef LUA_DEBUGSOCKET

struct sockaddr_in debugsocketdest;
int debugsocket = -1;

#endif

lua_list *lua_head = NULL, *lua_tail = NULL;

sstring *cpath = NULL, *suffix = NULL;

void *startsched = NULL;

int loaded = 0;

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
  
void _init() {
  lua_setupdebugsocket();

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
  }

  freesstring(cpath);
  freesstring(suffix);

  lua_freedebugsocket();
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

lua_State *lua_loadscript(char *file) {
  char fullpath[LUA_PATHLEN];
  int top;
  lua_State *l;
  lua_list *n;

  if(!cpath || !suffix)
    return NULL;

  delchars(file, "./\\;");

  if(lua_scriptloaded(file))
    return NULL;

  l = lua_open();
  if(!l)
    return NULL;

  n = (lua_list *)malloc(sizeof(lua_list));;
  if(!n) {
    Error("lua", ERR_ERROR, "Error allocing list for %s.", file);
    return NULL;
  }

  n->name = getsstring(file, LUA_PATHLEN);
  if(!n->name) {
    Error("lua", ERR_ERROR, "Error allocing name item for %s.", file);
    free(n);
    return NULL;
  }

  lua_loadlibs(l);
  lua_registercommands(l);
  lua_setpath(l);

#ifdef LUA_USEJIT
  lua_require(l, "lib/jit");
#endif

  lua_require(l, "lib/bootstrap");

  snprintf(fullpath, sizeof(fullpath), "%s/%s%s", cpath->content, file, suffix->content);
  if(luaL_loadfile(l, fullpath)) {
    Error("lua", ERR_ERROR, "Error loading %s.", file);
    lua_close(l);
    freesstring(n->name);
    free(n);
    return NULL;
  }

  top = lua_gettop(l);

  if(lua_pcall(l, 0, 0, 0)) {
    Error("lua", ERR_ERROR, "Error setting pcall on %s.", file);
    lua_close(l);
    freesstring(n->name);
    free(n);
    return NULL;
  }

  lua_settop(l, top);

  Error("lua", ERR_INFO, "Loaded %s.", file);
  n->l = l;

  n->next = NULL;
  n->prev = lua_tail;

  if(!lua_head) { 
    lua_head = n;
  } else {
    lua_tail->next = n;
  }

  lua_tail = n;

  lua_onload(l);

  return l;
}

void lua_unloadscript(lua_list *l) {
  lua_onunload(l->l);
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

  free(l);
}

void lua_setpath(lua_State *l) {
  char fullpath[LUA_PATHLEN], *prev = NULL;

  int top = lua_gettop(l);

  lua_getglobal(l, "package");
  if(!lua_istable(l, 1)) {
    Error("lua", ERR_ERROR, "Unable to set package.path (package is not a table).");
    return;
  }

  lua_pushstring(l, "path");
  lua_gettable(l, -2);

  if(lua_isstring(l, -1))
    prev = (char *)lua_tostring(l, -1);

  if(prev) {
    snprintf(fullpath, sizeof(fullpath), "%s;%s/?%s", prev, cpath->content, suffix->content);
  } else {
    snprintf(fullpath, sizeof(fullpath), "%s/?%s", cpath->content, suffix->content);
  }

  /* pop broke! */

  lua_getglobal(l, "package");
  if(!lua_istable(l, 1)) {
    Error("lua", ERR_ERROR, "Unable to set package.path (package is not a table).");
    return;
  }

  lua_pushstring(l, "path");

  lua_pushstring(l, fullpath);
  lua_settable(l, -3);

  lua_settop(l, top);
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

char *lua_namefromstate(lua_State *l) {
  lua_list *i = lua_head;
  for(;i;i=i->next)
    if(i->l == l)
      return i->name->content;

  return "???";
}

