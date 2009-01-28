/* Copyright (C) Chris Porter 2007 */

#ifndef _LUA_SOCKET_H
#define _LUA_SOCKET_H

#include "lua.h"

typedef struct lua_socket {
  int fd;
  int state;
  int sockettype;
  long handler;
  long tag;
  unsigned long identifier;
  struct lua_list *l;
  struct lua_socket *parent;

  struct lua_socket *next;
} lua_socket;

#define SOCKET_CONNECTING 0x00
#define SOCKET_CONNECTED  0x01
#define SOCKET_CLOSED     0x02
#define SOCKET_LISTENING  0x03

#endif

