/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#ifndef _LUA_BOT_H
#define _LUA_BOT_H

#include "../nick/nick.h"
#include "../channel/channel.h"

int lua_channelmessage(channel *cp, char *message, ...) __attribute__ ((format (printf, 2, 3)));
int lua_message(nick *np, char *message, ...) __attribute__ ((format (printf, 2, 3)));
int lua_notice(nick *np, char *message, ...) __attribute__ ((format (printf, 2, 3)));
int _lua_vpcall(lua_State *l, void *function, int mode, const char *sig, ...);
char *printallmodes(channel *cp);

nick *lua_nick;
extern sstring *luabotnick;

#define LUA_OK 0
#define LUA_FAIL 1

#define LUA_CHARMODE 0
#define LUA_POINTERMODE 1

#define LUA_TPUSHSTRING(l, param, value) { lua_pushstring(l, param); lua_pushstring(l, value); lua_rawset(l, -3); }
#define LUA_TPUSHNUMBER(l, param, value) { lua_pushstring(l, param); lua_pushnumber(l, value); lua_rawset(l, -3); }
#define LUA_TPUSHBOOLEAN(l, param, value) { lua_pushstring(l, param); lua_pushboolean(l, value); lua_rawset(l, -3); }

#define LUA_PUSHNICK(l, np) { lua_newtable(l); LUA_TPUSHSTRING(l, "nick", np->nick); LUA_TPUSHSTRING(l, "ident", np->ident); LUA_TPUSHSTRING(l, "hostname", np->host->name->content); LUA_TPUSHSTRING(l, "realname", np->realname->name->content); LUA_TPUSHSTRING(l, "account", np->authname); LUA_TPUSHNUMBER(l, "numeric", np->numeric); LUA_TPUSHSTRING(l, "ip", IPtostr(np->p_ipaddr)); }

#define LUA_PUSHCHAN(l, cp) { lua_newtable(l); LUA_TPUSHNUMBER(l, "timestamp", cp->timestamp); LUA_TPUSHNUMBER(l, "totalusers", cp->users->totalusers); if(cp->topic) { LUA_TPUSHSTRING(l, "topic", cp->topic->content); }; LUA_TPUSHSTRING(l, "modes", printallmodes(cp)); }

#define LUA_PUSHNICKCHANMODES(l, lp) { lua_newtable(l); LUA_TPUSHBOOLEAN(l, "opped", (*lp & CUMODE_OP) != 0); LUA_TPUSHBOOLEAN(l, "voiced", (*lp & CUMODE_VOICE) != 0); }

#define LUA_RETURN(l, n) { lua_pushnumber(l, n); return 1; }

#define lua_vpcall(L2, F2, S2, ...) _lua_vpcall(L2, F2, LUA_CHARMODE, S2 , ##__VA_ARGS__)
#define lua_vlpcall(L2, F2, N2, S2, ...) _lua_vpcall(L2->l, (void *)F2->handler, LUA_POINTERMODE, "Ns" S2 , F2->nick, N2, ##__VA_ARGS__)

#define lua_avpcall(F2, S2, ...) { lua_State *l; LUA_STARTLOOP(l); lua_vpcall(l, F2, S2 , ##__VA_ARGS__); LUA_ENDLOOP(); }

#endif
