/* Copyright (C) Chris Porter 2007 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include "../lib/strlfunc.h"
#include "../core/events.h"

#include "lua.h"
#include "luabot.h"

#define lua_vnpcall(F2, N2, S2, ...) _lua_vpcall(F2->l->l, (void *)F2->handler, LUA_POINTERMODE, "lsR" S2 , F2->identifier, N2, F2->tag , ##__VA_ARGS__)

/*
 * instead of these identifiers I could just use the file descriptor...
 * BUT I can't remember the exact semantics of the fd table WRT reuse...
 */
static long nextidentifier = 0;
static void lua_socket_poll_event(int fd, short events);

static int lua_socket_unix_connect(lua_State *l) {
  int len, ret;
  struct sockaddr_un r;
  char *path;
  lua_socket *ls;
  lua_list *ll;

  ll = lua_listfromstate(l);
  if(!ll)
    return 0;

  if(!lua_isfunction(l, 2))
    return 0;

  path = (char *)lua_tostring(l, 1);
  if(!path)
    return 0;

  ls = (lua_socket *)malloc(sizeof(lua_socket));
  if(!ls)
    return 0;

  ls->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(ls->fd <= -1) {
    free(ls);
    return 0;
  }

  memset(&r, 0, sizeof(r));
  r.sun_family = AF_UNIX;
  strlcpy(r.sun_path, path, sizeof(r.sun_path));

  /* don't really like this, there's a macro in sys/un.h but it's not there under FreeBSD */
  len = sizeof(r.sun_family) + strlen(r.sun_path) + 1;

  /* WTB exceptions */
  ret = fcntl(ls->fd, F_GETFL, 0);
  if(ret < 0) {
    free(ls);
    close(ls->fd);
    return 0;
  }

  ret = fcntl(ls->fd, F_SETFL, ret | O_NONBLOCK);
  if(ret < 0) {
    free(ls);
    close(ls->fd);
    return 0;
  }

  ret = connect(ls->fd, (struct sockaddr *)&r, len);
  if(ret == 0) {
    ls->state = SOCKET_CONNECTED;
  } else if(ret == -1 && (errno == EINPROGRESS)) {
    ls->state = SOCKET_CONNECTING;
  } else {
    free(ls);
    close(ls->fd);
    return 0;
  }

  /* this whole identifier thing should probably use userdata stuff */
  ls->identifier = nextidentifier++;
  ls->tag = luaL_ref(l, LUA_REGISTRYINDEX);
  ls->handler = luaL_ref(l, LUA_REGISTRYINDEX);

  ls->l = ll;

  ls->next = ll->sockets;
  ll->sockets = ls;

  registerhandler(ls->fd, (ls->state==SOCKET_CONNECTED?POLLIN:POLLOUT) | POLLERR | POLLHUP, lua_socket_poll_event);

  lua_pushboolean(l, ls->state==SOCKET_CONNECTED?1:0);
  lua_pushlong(l, ls->identifier);
  return 2;
}

static lua_socket *socketbyfd(int fd) {
  lua_list *l;
  lua_socket *ls;

  for(l=lua_head;l;l=l->next)
    for(ls=l->sockets;ls;ls=ls->next)
      if(ls->fd == fd)
        return ls;

  return NULL;
}

static lua_socket *socketbyidentifier(long identifier) {
  lua_list *l;
  lua_socket *ls;

  for(l=lua_head;l;l=l->next)
    for(ls=l->sockets;ls;ls=ls->next)
      if(ls->identifier == identifier)
        return ls;

  return NULL;
}

static void lua_socket_call_close(lua_socket *ls) {
  lua_socket *p, *c;

  for(c=ls->l->sockets,p=NULL;c;p=c,c=c->next) {
    if(c == ls) {
      if(ls->state == SOCKET_CLOSED)
        return;

      ls->state = SOCKET_CLOSED;
      deregisterhandler(ls->fd, 1);

      lua_vnpcall(ls, "close", "");

      if(!p) {
        ls->l->sockets = ls->next;
      } else {
        p->next = ls->next;
      }

      luaL_unref(ls->l->l, LUA_REGISTRYINDEX, ls->tag);
      luaL_unref(ls->l->l, LUA_REGISTRYINDEX, ls->handler);

      free(ls);
      return;
    }
  }

}

static int lua_socket_write(lua_State *l) {
  char *buf;
  long len;
  lua_socket *ls;
  int ret;

  buf = (char *)lua_tostring(l, 2);

  if(!lua_islong(l, 1) || !buf) {
    lua_pushint(l, -1);
    return 1;
  }
  len = lua_strlen(l, 2);

  ls = socketbyidentifier(lua_tolong(l, 1));
  if(!ls || (ls->state != SOCKET_CONNECTED)) {
    lua_pushint(l, -1);
    return 1;
  }

  ret = write(ls->fd, buf, len);
  if(ret == -1 && (errno == EAGAIN)) {
    deregisterhandler(ls->fd, 0);
    registerhandler(ls->fd, POLLIN | POLLOUT | POLLERR | POLLHUP, lua_socket_poll_event);

    lua_pushint(l, 0);
    return 1;
  }

  if(ret == -1)
    lua_socket_call_close(ls);

  if(ret < len) {
    deregisterhandler(ls->fd, 0);
    registerhandler(ls->fd, POLLIN | POLLOUT | POLLERR | POLLHUP, lua_socket_poll_event);
  }

  lua_pushint(l, ret);
  return 1;
}

static int lua_socket_close(lua_State *l) {
  lua_socket *ls;

  if(!lua_islong(l, 1))
    LUA_RETURN(l, LUA_FAIL);

  ls = socketbyidentifier(lua_tolong(l, 1));
  if(!ls || (ls->state == SOCKET_CLOSED))
    LUA_RETURN(l, LUA_FAIL);

  lua_socket_call_close(ls);

  LUA_RETURN(l, LUA_OK);
}

static void lua_socket_poll_event(int fd, short events) {
  lua_socket *ls = socketbyfd(fd);
  if(!ls || (ls->state == SOCKET_CLOSED))
    return;

  if(events & (POLLERR | POLLHUP)) {
    lua_socket_call_close(ls);
    return;
  }

  switch(ls->state) {
    case SOCKET_CONNECTING:
      if(events & POLLOUT) {
        deregisterhandler(fd, 0);
        registerhandler(fd, POLLIN | POLLERR | POLLHUP, lua_socket_poll_event);
        ls->state = SOCKET_CONNECTED;

        lua_vnpcall(ls, "connect", "");
      }
      break;
    case SOCKET_CONNECTED:
      if(events & POLLOUT) {
        deregisterhandler(fd, 0);
        registerhandler(fd, POLLIN | POLLERR | POLLHUP, lua_socket_poll_event);

        lua_vnpcall(ls, "flush", "");
      }
      if(events & POLLIN) {
        char buf[8192 * 2];
        int bytesread;

        bytesread = read(fd, buf, sizeof(buf));
        if((bytesread == -1) && (errno == EAGAIN))
          return;

        if(bytesread <= 0) {
          lua_socket_call_close(ls);
         return;
        }

        lua_vnpcall(ls, "read", "L", buf, (long)bytesread);
      }
      break;
  }
}

void lua_socket_closeall(lua_list *l) {
  while(l->sockets)
    lua_socket_call_close(l->sockets);
}

void lua_registersocketcommands(lua_State *l) {
  lua_register(l, "socket_unix_connect", lua_socket_unix_connect);
  lua_register(l, "socket_close", lua_socket_close);
  lua_register(l, "socket_write", lua_socket_write);
}
