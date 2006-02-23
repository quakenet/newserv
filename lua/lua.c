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

#include <string.h>
#include <stdlib.h>

void lua_startup(void *arg);
void lua_loadscripts(void);
void lua_rehash(int hooknum, void *arg);
void lua_registercommands(lua_State *l);

void lua_startbot(void *arg);
void lua_destroybot(void);
void lua_startcontrol(void);
void lua_destroycontrol(void);

void lua_onunload(lua_State *l);
void lua_onload(lua_State *l);

int lua_setpath(void);

lua_list *lua_head = NULL, *lua_tail = NULL;

sstring *enviro = NULL, *cpath = NULL, *suffix = NULL;

void *startsched = NULL;

void _init() {
  startsched = scheduleoneshot(time(NULL) + 1, &lua_startup, NULL);
}

void lua_startup(void *arg) {
  char *env;

  startsched = NULL;

  env = getenv("LUA_PATH");
  if(env)
    enviro = getsstring(env, LUA_PATHLEN);

  registerhook(HOOK_CORE_REHASH, &lua_rehash);

  lua_startcontrol();
  lua_startbot(NULL);

  if(lua_setpath())
    lua_loadscripts();
}

#ifdef BROKEN_DLCLOSE
void __fini() {
#else
void _fini() {
#endif
  if(startsched)
    deleteschedule(startsched, &lua_startup, NULL);

  while(lua_head)
    lua_unloadscript(lua_head);

  lua_destroybot();
  lua_destroycontrol();

  deregisterhook(HOOK_CORE_REHASH, &lua_rehash);

  freesstring(cpath);

  if(enviro) {
    setenv("LUA_PATH", enviro->content);
  } else {
    unsetenv("LUA_PATH");
  }

  freesstring(enviro);
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

  lua_baselibopen(l);
  lua_strlibopen(l);
  lua_tablibopen(l);
  lua_mathlibopen(l);
  lua_iolibopen(l);

  lua_registercommands(l);

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

int lua_setpath(void) {
  char fullpath[LUA_PATHLEN];

  freesstring(cpath);
  freesstring(suffix);

  cpath = getcopyconfigitem("lua", "scriptdir", ".", 100);

  if(!cpath) {
    Error("lua", ERR_ERROR, "Error loading path.");
    return 0;
  }

  suffix = getcopyconfigitem("lua", "scriptsuffix", ".lua", 10);
  if(!suffix) {
    Error("lua", ERR_ERROR, "Error loading suffix.");
    return 0;
  }

  if(enviro) {
    snprintf(fullpath, sizeof(fullpath), "%s;%s/?", enviro->content, cpath->content);
  } else {
    snprintf(fullpath, sizeof(fullpath), "%s/?", cpath->content);
  }
  setenv("LUA_PATH", fullpath);

  return 1;
}

void lua_rehash(int hooknum, void *arg) {
  lua_setpath();
}

lua_list *lua_scriptloaded(char *name) {
  lua_list *l;

  for(l=lua_head;l;l=l->next)
    if(!strcmp(l->name->content, name))
      return l;

  return NULL;
}

