/* Copyright (C) Chris Porter 2007 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef __USE_MISC
#define __USE_MISC /* inet_aton */
#endif

#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/poll.h>
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

static int setnonblock(int fd) {
  int ret;

  /* WTB exceptions */
  ret = fcntl(fd, F_GETFL, 0);
  if(ret < 0) {
    close(fd);
    return -1;
  }

  ret = fcntl(fd, F_SETFL, ret | O_NONBLOCK);
  if(ret < 0) {
    close(fd);
    return -1;
  }

  return fd;
}
static int getunixsocket(char *path, struct sockaddr_un *r) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);

  if(fd <= -1)
    return -1;

  memset(r, 0, sizeof(struct sockaddr_un));
  r->sun_family = PF_UNIX;
  strlcpy(r->sun_path, path, sizeof(r->sun_path));

  return setnonblock(fd);
}

/* note ADDRESS not HOSTNAME */
static int getipsocket(char *address, int port, int protocol, struct sockaddr_in *r) {
  int fd;
  int val = 1;

  memset(r, 0, sizeof(struct sockaddr_in));
  r->sin_family = PF_INET;

  if(address) {
    if(!inet_aton(address, &r->sin_addr)) {
      Error("lua", ERR_ERROR, "Could not parse address: %s", address);
      return -1;
    }
  } else {
    address = INADDR_ANY;
  }

  fd = socket(AF_INET, protocol, 0);

  if(fd <= -1)
    return -1;

  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  r->sin_port = htons(port);
  return setnonblock(fd);
}

static void registerluasocket(lua_list *ll, lua_socket *ls, int mask, int settag) {
  /* this whole identifier thing should probably use userdata stuff */
  ls->identifier = nextidentifier++;

  if(settag) {
    ls->tag = luaL_ref(ll->l, LUA_REGISTRYINDEX);
    ls->handler = luaL_ref(ll->l, LUA_REGISTRYINDEX);
  }
  ls->l = ll;

  ls->next = ll->sockets;
  ll->sockets = ls;

  registerhandler(ls->fd, mask, lua_socket_poll_event);
}

static int handleconnect(int ret, lua_State *l, lua_list *ll, lua_socket *ls) {
  if(ret == 0) {
    ls->state = SOCKET_CONNECTED;
  } else if(ret == -1 && (errno == EINPROGRESS)) {
    ls->state = SOCKET_CONNECTING;
  } else {
    luafree(ls);
    close(ls->fd);
    return 0;
  }

  ls->parent = NULL;
  registerluasocket(ll, ls, (ls->state==SOCKET_CONNECTED?POLLIN:POLLOUT) | POLLERR | POLLHUP, 1);

  lua_pushboolean(l, ls->state==SOCKET_CONNECTED?1:0);
  lua_pushlong(l, ls->identifier);
  return 2;
}

static int lua_socket_unix_connect(lua_State *l) {
  int ret;
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

  ls = (lua_socket *)luamalloc(sizeof(lua_socket));
  if(!ls)
    return 0;

  ls->sockettype = PF_UNIX;
  ls->fd = getunixsocket(path, &r);
  if(ls->fd < 0) {
    luafree(ls);
    return 0;
  }

  ret = connect(ls->fd, (struct sockaddr *)&r, sizeof(r.sun_family) + strlen(r.sun_path));
  return handleconnect(ret, l, ll, ls);
}

static int lua_socket_unix_bind(lua_State *l) {
  lua_list *ll;
  char *path;
  lua_socket *ls;
  struct sockaddr_un r;

  ll = lua_listfromstate(l);
  if(!ll)
    return 0;

  if(!lua_isfunction(l, 2))
    return 0;

  path = (char *)lua_tostring(l, 1);
  if(!path)
    return 0;

  ls = (lua_socket *)luamalloc(sizeof(lua_socket));
  if(!ls)
    return 0;

  ls->sockettype = PF_UNIX;
  ls->fd = getunixsocket(path, &r);
  if(ls->fd <= -1) {
    luafree(ls);
    return 0;
  }

  unlink(path);

  if(
    (bind(ls->fd, (struct sockaddr *)&r, strlen(r.sun_path) + sizeof(r.sun_family)) == -1) ||
    (listen(ls->fd, 5) == -1)
  ) {
    close(ls->fd);
    luafree(ls);
    return 0;
  }

  ls->state = SOCKET_LISTENING;
  ls->parent = NULL;

  registerluasocket(ll, ls, POLLIN, 1);

  lua_pushlong(l, ls->identifier);
  return 1;
}

static int lua_socket_ip_connect(lua_State *l, int protocol) {
  int ret;
  struct sockaddr_in r;
  char *address;
  int port;
  lua_socket *ls;
  lua_list *ll;

  ll = lua_listfromstate(l);
  if(!ll)
    return 0;

  if(!lua_isfunction(l, 3) || !lua_islong(l, 2))
    return 0;

  address = (char *)lua_tostring(l, 1);
  if(!address)
    return 0;

  port = lua_tolong(l, 2);

  ls = (lua_socket *)luamalloc(sizeof(lua_socket));
  if(!ls)
    return 0;

  ls->sockettype = PF_INET;
  ls->fd = getipsocket(address, port, protocol, &r);
  if(ls->fd < 0) {
    luafree(ls);
    return 0;
  }

  ret = connect(ls->fd, (struct sockaddr *)&r, sizeof(struct sockaddr_in));
  return handleconnect(ret, l, ll, ls);
}

static int lua_socket_ip_bind(lua_State *l, int protocol) {
  lua_list *ll;
  char *address;
  int port;
  lua_socket *ls;
  struct sockaddr_in r;

  ll = lua_listfromstate(l);
  if(!ll)
    return 0;

  if(!lua_isfunction(l, 3) || !lua_islong(l, 2))
    return 0;

  /* address can be nil or a string */
  if(!lua_isnil(l, 1) && !lua_isstring(l, 1))
    return 0;

  address = (char *)lua_tostring(l, 1);
  port = lua_tolong(l, 2);

  ls = (lua_socket *)luamalloc(sizeof(lua_socket));
  if(!ls)
    return 0;

  ls->sockettype = PF_INET;
  ls->fd = getipsocket(address, port, protocol, &r);
  if(ls->fd <= -1) {
    luafree(ls);
    return 0;
  }

  if(
    (bind(ls->fd, (struct sockaddr *)&r, sizeof(struct sockaddr_in)) == -1) ||
    (listen(ls->fd, 5) == -1)
  ) {
    close(ls->fd);
    luafree(ls);
    return 0;
  }

  ls->state = SOCKET_LISTENING;
  ls->parent = NULL;

  registerluasocket(ll, ls, POLLIN, 1);

  lua_pushlong(l, ls->identifier);
  return 1;
}

static int lua_socket_tcp_connect(lua_State *l) {
  return lua_socket_ip_connect(l, SOCK_STREAM);
}

static int lua_socket_tcp_bind(lua_State *l) {
  return lua_socket_ip_bind(l, SOCK_STREAM);
}

static int lua_socket_udp_connect(lua_State *l) {
  return lua_socket_ip_connect(l, SOCK_DGRAM);
}

static int lua_socket_udp_bind(lua_State *l) {
  return lua_socket_ip_bind(l, SOCK_DGRAM);
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

  for(c=ls->l->sockets;c;c=p) {
    p = c->next;
    if(c->parent == ls)
      lua_socket_call_close(c);
  }

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

      if(!ls->parent) {
        luaL_unref(ls->l->l, LUA_REGISTRYINDEX, ls->tag);
        luaL_unref(ls->l->l, LUA_REGISTRYINDEX, ls->handler);
      }

      luafree(ls);
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
    case SOCKET_LISTENING:
      if(events & POLLIN) {
        struct sockaddr_in rip;
        struct sockaddr_un run;
        lua_socket *ls2;
        unsigned int len;
        int fd2;

        if(ls->sockettype == PF_INET) {
          len = sizeof(rip);
          fd2 = accept(fd, (struct sockaddr *)&rip, &len);
        } else {
          len = sizeof(run);
          fd2 = accept(fd, (struct sockaddr *)&run, &len);
        }
        if(fd2 == -1)
          return;

        ls2 = (lua_socket *)luamalloc(sizeof(lua_socket));
        if(!ls2) {
          close(fd2);
          return;
        }

        ls2->fd = fd2;
        ls2->state = SOCKET_CONNECTED;
        ls2->tag = ls->tag;
        ls2->handler = ls->handler;
        ls2->parent = ls;
        ls2->sockettype = ls->sockettype;

        registerluasocket(ls->l, ls2, POLLIN | POLLERR | POLLHUP, 0);
        lua_vnpcall(ls, "accept", "l", ls2->identifier);
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
  lua_register(l, "socket_unix_bind", lua_socket_unix_bind);
  lua_register(l, "socket_tcp_connect", lua_socket_tcp_connect);
  lua_register(l, "socket_tcp_bind", lua_socket_tcp_bind);
  lua_register(l, "socket_udp_connect", lua_socket_udp_connect);
  lua_register(l, "socket_udp_bind", lua_socket_udp_bind);
  lua_register(l, "socket_close", lua_socket_close);
  lua_register(l, "socket_write", lua_socket_write);
}
