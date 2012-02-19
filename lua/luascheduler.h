/* Copyright (C) Chris Porter 2007 */

#ifndef _LUA_SCHED_H
#define _LUA_SCHED_H

#include "lua.h"
#include "luabot.h"

struct lua_scheduler;

typedef struct lua_scheduler_entry {
  int ref;
  void *psch;
  struct lua_scheduler *sched;
  struct lua_scheduler_entry *prev, *next;
} lua_scheduler_entry;

typedef struct lua_scheduler {
  unsigned int count;
  lua_State *ps;
  lua_scheduler_entry *entries;
  struct lua_scheduler *next;
} lua_scheduler;

#endif

