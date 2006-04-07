/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#ifndef _LUA_BOT_H
#define _LUA_BOT_H

#include "../nick/nick.h"
#include "../channel/channel.h"

int lua_channelmessage(channel *cp, char *message, ...);
int lua_message(nick *np, char *message, ...);
int lua_notice(nick *np, char *message, ...);

nick *lua_nick;

#define LUA_OK 0
#define LUA_FAIL 1

#define LUA_TPUSHSTRING(l, param, value) lua_pushstring(l, param); lua_pushstring(l, value); lua_rawset(l, -3)
#define LUA_TPUSHNUMBER(l, param, value) lua_pushstring(l, param); lua_pushnumber(l, value); lua_rawset(l, -3)

#define LUA_PUSHNICK(l, np) lua_newtable(l); LUA_TPUSHSTRING(l, "nick", np->nick); LUA_TPUSHSTRING(l, "ident", np->ident); LUA_TPUSHSTRING(l, "hostname", np->host->name->content); LUA_TPUSHSTRING(l, "realname", np->realname->name->content); LUA_TPUSHSTRING(l, "account", np->authname); LUA_TPUSHNUMBER(l, "numeric", np->numeric) LUA_TPUSHSTRING(l, "ip", IPtostr(np->ipaddress));

#define LUA_PUSHCHAN(l, cp) lua_newtable(l); LUA_TPUSHNUMBER(l, "timestamp", cp->timestamp); LUA_TPUSHNUMBER(l, "totalusers", cp->users->totalusers)

#define LUA_RETURN(l, n) { lua_pushnumber(l, n); return 1; }

#endif
