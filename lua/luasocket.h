/* Copyright (C) Chris Porter 2007 */

#ifndef _LUA_SOCKET_H
#define _LUA_SOCKET_H

#include "lua.h"

typedef struct lua_socket {
  int fd;
  int state;
  long handler;
  unsigned long identifier;
  struct lua_list *l;
  struct lua_socket *next;
} lua_socket;

#endif

