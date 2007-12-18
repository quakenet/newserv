/* Copyright (C) Chris Porter 2007 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "../lib/strlfunc.h"
#include "../core/events.h"

#include "lua.h"
#include "luabot.h"

#define lua_vnpcall(F2, N2, S2, ...) _lua_vpcall(F2->l->l, (void *)F2->handler, LUA_POINTERMODE, "ls" S2 , F2->identifier, N2 , ##__VA_ARGS__)

/*
 * instead of these identifiers I could just use the file descriptor...
 * BUT I can't remember the exact semantics of the fd table WRT reuse...
 */
static long nextidentifier = 0;
void lua_socket_poll_event(int fd, short events);

static int lua_socket_unix_connect(lua_State *l) {
  int len, ret;
  struct sockaddr_un r;
  char *path;
  lua_socket *ls;
  lua_list *ll;

  ll = lua_listfromstate(l);
  if(!ll)
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

  /*
   * this socket is blocking.
   * we could potentially block the IRC socket...
   * BUT it's far far too much work to make it writes non-blocking...
   */
  ret = connect(ls->fd, (struct sockaddr *)&r, len);
  if(ret < 0) {
    free(ls);
    close(ls->fd);
    return 0;
  }

  /* this whole identifier thing should probably use userdata stuff */
  ls->identifier = nextidentifier++;
  ls->handler = luaL_ref(l, LUA_REGISTRYINDEX);
  ls->l = ll;

  ls->next = ll->sockets;
  ll->sockets = ls;

  registerhandler(ls->fd, POLLIN | POLLERR | POLLHUP, lua_socket_poll_event);

  lua_pushlong(l, ls->identifier);
  return 1;
}

lua_socket *socketbyfd(int fd) {
  lua_list *l;
  lua_socket *ls;

  for(l=lua_head;l;l=l->next)
    for(ls=l->sockets;ls;ls=ls->next)
      if(ls->fd == fd)
        return ls;

  return NULL;
}

lua_socket *socketbyidentifier(long identifier) {
  lua_list *l;
  lua_socket *ls;

  for(l=lua_head;l;l=l->next)
    for(ls=l->sockets;ls;ls=ls->next)
      if(ls->identifier == identifier)
        return ls;

  return NULL;
}

void lua_socket_call_close(lua_socket *ls) {
  lua_socket *p, *c;

  for(c=ls->l->sockets,p=NULL;c;p=c,c=c->next) {
    if(c == ls) {
      deregisterhandler(ls->fd, 1);

      lua_vnpcall(ls, "close", "");

      if(!p) {
        ls->l->sockets = ls->next;
      } else {
        p->next = ls->next;
      }

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

  if(!lua_islong(l, 1))
    LUA_RETURN(l, LUA_FAIL);

  buf = (char *)lua_tostring(l, 2);
  if(!buf)
    LUA_RETURN(l, LUA_FAIL);

  len = lua_strlen(l, 2);

  ls = socketbyidentifier(lua_tolong(l, 1));
  if(!ls)
    LUA_RETURN(l, LUA_FAIL);

  ret = write(ls->fd, buf, len);
  if(ret != len) {
    lua_socket_call_close(ls);
    lua_pushint(l, -1);
    return 1;
  }

  lua_pushint(l, 0);
  return 1;
}

static int lua_socket_close(lua_State *l) {
  lua_socket *ls;

  if(!lua_islong(l, 1))
    LUA_RETURN(l, LUA_FAIL);

  ls = socketbyidentifier(lua_tolong(l, 1));
  if(!ls)
    LUA_RETURN(l, LUA_FAIL);

  lua_socket_call_close(ls);

  LUA_RETURN(l, LUA_OK);
}

void lua_socket_poll_event(int fd, short events) {
  lua_socket *ls = socketbyfd(fd);
  if(!ls)
    return;

  if(events & (POLLERR | POLLHUP)) {
    lua_socket_call_close(ls);
    return;
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

    lua_vnpcall(ls, "read", "L", buf, bytesread);
  }
}

void lua_socket_closeall(lua_list *l) {
  while(l->sockets)
    lua_socket_call_close(l->sockets);
}

void lua_registersocketcomments(lua_State *l) {
  lua_register(l, "socket_unix_connect", lua_socket_unix_connect);
  lua_register(l, "socket_close", lua_socket_close);
  lua_register(l, "socket_write", lua_socket_write);
}
