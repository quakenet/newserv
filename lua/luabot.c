/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"

#include "lua.h"
#include "luabot.h"

nick *lua_nick = NULL;
void *myureconnect = NULL, *myublip = NULL, *myutick = NULL;

void lua_bothandler(nick *target, int type, void **args);

void lua_onnewnick(int hooknum, void *arg);
void lua_blip(void *arg);
void lua_tick(void *arg);
void lua_onkick(int hooknum, void *arg);
void lua_ontopic(int hooknum, void *arg);
void lua_onauth(int hooknum, void *arg);
void lua_ondisconnect(int hooknum, void *arg);
void lua_onmode(int hooknum, void *arg);
void lua_onop(int hooknum, void *arg);
void lua_onquit(int hooknum, void *arg);
void lua_onrename(int hooknum, void *arg);
void lua_onconnect(int hooknum, void *arg);
void lua_onjoin(int hooknum, void *arg);

void lua_registerevents(void) {
  registerhook(HOOK_NICK_NEWNICK, &lua_onnewnick);
  registerhook(HOOK_IRC_DISCON, &lua_ondisconnect);
  registerhook(HOOK_IRC_PRE_DISCON, &lua_ondisconnect);
  registerhook(HOOK_NICK_ACCOUNT, &lua_onauth);
  registerhook(HOOK_CHANNEL_TOPIC, &lua_ontopic);
  registerhook(HOOK_CHANNEL_KICK, &lua_onkick);
  registerhook(HOOK_CHANNEL_OPPED, &lua_onop);
  registerhook(HOOK_CHANNEL_DEOPPED, &lua_onop);
  registerhook(HOOK_NICK_LOSTNICK, &lua_onquit);
  registerhook(HOOK_NICK_RENAME, &lua_onrename);
  registerhook(HOOK_IRC_CONNECTED, &lua_onconnect);
  registerhook(HOOK_SERVER_END_OF_BURST, &lua_onconnect);
  registerhook(HOOK_CHANNEL_JOIN, &lua_onjoin);
}

void lua_deregisterevents(void) {
  deregisterhook(HOOK_CHANNEL_JOIN, &lua_onjoin);
  deregisterhook(HOOK_SERVER_END_OF_BURST, &lua_onconnect);
  deregisterhook(HOOK_IRC_CONNECTED, &lua_onconnect);
  deregisterhook(HOOK_NICK_RENAME, &lua_onrename);
  deregisterhook(HOOK_NICK_LOSTNICK, &lua_onquit);
  deregisterhook(HOOK_CHANNEL_DEOPPED, &lua_onop);
  deregisterhook(HOOK_CHANNEL_OPPED, &lua_onop);
  deregisterhook(HOOK_CHANNEL_KICK, &lua_onkick);
  deregisterhook(HOOK_CHANNEL_TOPIC, &lua_ontopic);
  deregisterhook(HOOK_NICK_ACCOUNT, &lua_onauth);
  deregisterhook(HOOK_IRC_PRE_DISCON, &lua_ondisconnect);
  deregisterhook(HOOK_IRC_DISCON, &lua_ondisconnect);
  deregisterhook(HOOK_NICK_NEWNICK, &lua_onnewnick);
}

void lua_startbot(void *arg) {
  channel *cp;

  myureconnect = NULL;

  lua_nick = registerlocaluser("U", "lua", "quakenet.department.of.corrections", "Lua engine v" LUA_BOTVERSION " (" LUA_VERSION ")", "U", UMODE_ACCOUNT | UMODE_DEAF | UMODE_OPER | UMODE_SERVICE, &lua_bothandler);
  if(!lua_nick) {
    myureconnect = scheduleoneshot(time(NULL) + 1, &lua_startbot, NULL);
    return;
  }

  cp = findchannel(LUA_OPERCHAN);
  if(cp && localjoinchannel(lua_nick, cp))
    localgetops(lua_nick, cp);

  cp = findchannel(LUA_PUKECHAN);
  if(cp && localjoinchannel(lua_nick, cp))
    localgetops(lua_nick, cp);

  myublip = schedulerecurring(time(NULL) + 1, 0, 60, &lua_blip, NULL);
  myutick = schedulerecurring(time(NULL) + 1, 0, 1, &lua_tick, NULL);

  lua_registerevents();
}

void lua_destroybot(void) {
  if(myutick)
    deleteschedule(myutick, &lua_tick, NULL);

  if(myublip)
    deleteschedule(myublip, &lua_blip, NULL);

  lua_deregisterevents();

  if (myureconnect)
    deleteschedule(myureconnect, &lua_startbot, NULL);

  if(lua_nick)
    deregisterlocaluser(lua_nick, NULL);
}

void lua_bothandler(nick *target, int type, void **args) {
  nick *np;
  char *p;
  lua_State *l;
  int le, top;

  switch(type) {
    case LU_PRIVMSG:
      np = (nick *)args[0];
      p = (char *)args[1];

      if(!np || !p)
        return;

      LUA_STARTLOOP(l);

        top = lua_gettop(l);

        lua_getglobal(l, "scripterror");
        lua_getglobal(l, "irc_onmsg");

        if(lua_isfunction(l, -1)) {
          LUA_PUSHNICK(l, np);
          lua_pushstring(l, p);

          lua_pcall(l, 2, 0, top + 1);
        }

        lua_settop(l, top);

      LUA_ENDLOOP();

      break;
    case LU_PRIVNOTICE:
      np = (nick *)args[0];
      p = (char *)args[1];

      if(!np || !p)
        return;

      le = strlen(p);
      if((le > 1) && (p[0] == '\001')) {
        if(p[le - 1] == '\001')
          p[le - 1] = '\000';

        LUA_STARTLOOP(l);

          top = lua_gettop(l);

          lua_getglobal(l, "scripterror");
          lua_getglobal(l, "irc_onctcp");

          if(lua_isfunction(l, -1)) {
            LUA_PUSHNICK(l, np);
            lua_pushstring(l, p + 1);

            lua_pcall(l, 2, 0, top + 1);
          }

          lua_settop(l, top);

        LUA_ENDLOOP();

      }

      break;

    case LU_KILLED:
      if(myublip) {
        myublip = NULL;
        deleteschedule(myublip, &lua_blip, NULL);
      }

      if(myutick) {
        myutick = NULL;
        deleteschedule(myutick, &lua_tick, NULL);
      }

      lua_nick = NULL;
      myureconnect = scheduleoneshot(time(NULL) + 1, &lua_startbot, NULL);

      break;
  }
}

void lua_blip(void *arg) {
  lua_State *l;
  int top;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "onblip");

    if(lua_isfunction(l, -1))
      lua_pcall(l, 0, 0, top + 1);

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_tick(void *arg) {
  lua_State *l;
  int top;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "ontick");

    if(lua_isfunction(l, -1))
      lua_pcall(l, 0, 0, top + 1);

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onnewnick(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  int top;
  lua_State *l;

  if(!np)
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "irc_onnewnick");

    if(lua_isfunction(l, -1)) {
      LUA_PUSHNICK(l, np);

      lua_pcall(l, 1, 0, top + 1);
    }

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onkick(int hooknum, void *arg) {
  void **arglist = (void **)arg;
  chanindex *ci = ((channel *)arglist[0])->index;
  nick *kicked = arglist[1];
  nick *kicker = arglist[2];
  char *message = (char *)arglist[3];
  lua_State *l;
  int top;

  if(!kicker || IsOper(kicker) || IsService(kicker) || IsXOper(kicker)) /* bloody Cruicky */
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "irc_onkick");

    if(lua_isfunction(l, -1)) {
      lua_pushstring(l, ci->name->content);
      LUA_PUSHNICK(l, kicked);
      LUA_PUSHNICK(l, kicker);
      lua_pushstring(l, message);

      lua_pcall(l, 4, 0, top + 1);
    }

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_ontopic(int hooknum, void *arg) {
  void **arglist = (void **)arg;
  channel *cp=(channel*)arglist[0];
  nick *np = (nick *)arglist[1];
  lua_State *l;
  int top;

  if(!np || IsOper(np) || IsService(np) || IsXOper(np))
    return;
  if(!cp || !cp->topic) 
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "irc_ontopic");

    if(lua_isfunction(l, -1)) {
      lua_pushstring(l, cp->index->name->content);
      LUA_PUSHNICK(l, np);
      lua_pushstring(l, cp->topic->content);

      lua_pcall(l, 3, 0, top + 1);
    }
    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onop(int hooknum, void *arg) {
  void **arglist = (void **)arg;
  chanindex *ci = ((channel *)arglist[0])->index;
  nick *np = arglist[1];
  nick *target = arglist[2];
  lua_State *l;
  int top;

  if(!target)
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    if(hooknum == HOOK_CHANNEL_OPPED) {
      lua_getglobal(l, "irc_onop");
    } else {
      lua_getglobal(l, "irc_ondeop");
    }

    if(lua_isfunction(l, -1)) {
      lua_pushstring(l, ci->name->content);

      if(np) {
        LUA_PUSHNICK(l, np);
      } else {
        lua_pushnil(l);
      }
      LUA_PUSHNICK(l, target);

      lua_pcall(l, 3, 0, top + 1);
    }

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onjoin(int hooknum, void *arg) {
  void **arglist = (void **)arg;
  chanindex *ci = ((channel *)arglist[0])->index;
  nick *np = arglist[1];
  lua_State *l;
  int top;

  if(!ci || !np)
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "irc_onjoin");

    if(lua_isfunction(l, -1)) {
      lua_pushstring(l, ci->name->content);

      LUA_PUSHNICK(l, np);

      lua_pcall(l, 2, 0, top + 1);
    }

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onrename(int hooknum, void *arg) {
  nick *np = (void *)arg;
  lua_State *l;
  int top;

  if(!np)
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "irc_onrename");

    if(lua_isfunction(l, -1)) {
      LUA_PUSHNICK(l, np);

      lua_pcall(l, 1, 0, top + 1);
    }

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onquit(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  lua_State *l;
  int top;

  if(!np)
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "irc_onquit");

    if(lua_isfunction(l, -1)) {
      LUA_PUSHNICK(l, np);

      lua_pcall(l, 1, 0, top + 1);
    }

    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onauth(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  lua_State *l;
  int top;

  if(!np)
    return;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    lua_getglobal(l, "irc_onauth");

    if(lua_isfunction(l, -1)) {
      LUA_PUSHNICK(l, np);

      lua_pcall(l, 1, 0, top + 1);
    }
    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_ondisconnect(int hooknum, void *arg) {
  lua_State *l;
  int top;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    if(hooknum == HOOK_IRC_DISCON) {
      lua_getglobal(l, "irc_ondisconnect");
    } else {
      lua_getglobal(l, "irc_onpredisconnect");
    }
    if(lua_isfunction(l, -1))
      lua_pcall(l, 0, 0, top + 1);
    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onconnect(int hooknum, void *arg) {
  lua_State *l;
  int top;

  LUA_STARTLOOP(l);
    top = lua_gettop(l);

    lua_getglobal(l, "scripterror");
    if(hooknum == HOOK_IRC_CONNECTED) {
      lua_getglobal(l, "irc_onconnect");
    } else {
      lua_getglobal(l, "irc_onendofburst");
    }
    if(lua_isfunction(l, -1))
      lua_pcall(l, 0, 0, top + 1);
    lua_settop(l, top);
  LUA_ENDLOOP();
}

void lua_onload(lua_State *l) {
  int top;

  top = lua_gettop(l);

  lua_getglobal(l, "scripterror");
  lua_getglobal(l, "onload");

  if(lua_isfunction(l, -1))
    lua_pcall(l, 0, 0, top + 1);
  lua_settop(l, top);
}

void lua_onunload(lua_State *l) {
  int top;

  top = lua_gettop(l);

  lua_getglobal(l, "scripterror");
  lua_getglobal(l, "onunload");

  if(lua_isfunction(l, -1))
    lua_pcall(l, 0, 0, top + 1);
  lua_settop(l, top);
}

int lua_channelmessage(channel *cp, char *message, ...) {
  char buf[512];
  va_list va;

  va_start(va, message);
  vsnprintf(buf, sizeof(buf), message, va);
  va_end(va);

  sendmessagetochannel(lua_nick, cp, "%s", buf);

  return 0;
}

int lua_message(nick *np, char *message, ...) {
  char buf[512];
  va_list va;

  va_start(va, message);
  vsnprintf(buf, sizeof(buf), message, va);
  va_end(va);

  sendmessagetouser(lua_nick, np, "%s", buf);

  return 0;
}

int lua_notice(nick *np, char *message, ...) {
  char buf[512];
  va_list va;

  va_start(va, message);
  vsnprintf(buf, sizeof(buf), message, va);
  va_end(va);

  sendnoticetouser(lua_nick, np, "%s", buf);

  return 0;
}


