/* Copyright (C) Chris Porter 2006 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#ifndef _LUA_LOCALNICKS_H
#define _LUA_LOCALNICKS_H

#include "../nick/nick.h"
#include "../lib/flags.h"

typedef struct lua_localnick {
  nick *nick;
  struct lua_localnick *next;
  int handler;
  void *reconnect;

  char nickname[NICKLEN + 1];
  char ident[USERLEN + 1];
  char hostname[HOSTLEN + 1];
  char realname[REALLEN + 1];
  char account[ACCOUNTLEN + 1];
  flag_t umodes;
} lua_localnick;

#endif
