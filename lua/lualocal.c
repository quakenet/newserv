#include <stdlib.h>

#include "../localuser/localuser.h"
#include "../lib/strlfunc.h"
#include "../core/schedule.h"
#include "../channel/channel.h"
#include "../localuser/localuserchannel.h"

#include "lua.h"
#include "luabot.h"
#include "lualocal.h"

void lua_localnickhandler(nick *target, int type, void **args);
void lua_reconnectlocal(void *arg);

static int lua_registerlocaluser(lua_State *ps) {
  lua_list *l;
  lua_localnick *ln;
  char *nickname, *ident, *hostname, *realname, *account;
  flag_t modes = 0;
  
  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2) || !lua_isstring(ps, 3) || !lua_isstring(ps, 4) || !lua_isstring(ps, 5) || !lua_isstring(ps, 6) || !lua_isfunction(ps, 7))
    return 0;

  nickname = (char *)lua_tostring(ps, 1);
  ident = (char *)lua_tostring(ps, 2);
  hostname = (char *)lua_tostring(ps, 3);
  realname = (char *)lua_tostring(ps, 4);
  account = (char *)lua_tostring(ps, 5);

  setflags(&modes, UMODE_ALL, (char *)lua_tostring(ps, 6), umodeflags, REJECT_NONE);

  if(!lua_lineok(nickname) || !lua_lineok(ident) || !lua_lineok(hostname) || !lua_lineok(realname) || !lua_lineok(account))
    return 0;

  l = lua_listfromstate(ps);
  if(!l)
    return 0;

  ln = (lua_localnick *)malloc(sizeof(lua_localnick));
  if(!ln)
    return 0;

  ln->reconnect = NULL;

  ln->nick = registerlocaluser(nickname, ident, hostname, realname, account, modes, &lua_localnickhandler);
  if(!ln->nick) {
    free(ln);
    return 0;
  }

  ln->handler = luaL_ref(ps, LUA_REGISTRYINDEX);

  ln->next = l->nicks;
  l->nicks = ln;

  lua_pushlong(ps, ln->nick->numeric);
  return 1;
}

void lua_freelocalnick(lua_State *ps, lua_localnick *l, char *quitm) {
  if(l->nick)
    deregisterlocaluser(l->nick, quitm);

  if(l->reconnect)
    deleteschedule(l->reconnect, &lua_reconnectlocal, l);

  luaL_unref(ps, LUA_REGISTRYINDEX, l->handler);

  free(l);
}

int lua_getlocalnickbynick(nick *np, lua_list **rl, lua_localnick **rln) {
  lua_list *l;
  lua_localnick *ln;

  for(l=lua_head;l;l=l->next)
    for(ln=l->nicks;ln;ln=ln->next)
      if(ln->nick == np) {
        *rl = l;
        *rln = ln;
        return 1;
      }

  return 0;
}

static int lua_deregisterlocaluser(lua_State *ps) {
  lua_list *l;
  lua_localnick *l2, *lp = NULL;
  long numeric;
  char *quitm;

  if(!lua_islong(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  quitm = lua_isstring(ps, 2)?(char *)lua_tostring(ps, 2):"localuser unregistered.";

  l = lua_listfromstate(ps);

  for(l2=l->nicks;l2;lp=l2,l2=l2->next) {
    if(l2->nick->numeric == numeric) {
      lp->next = l2->next;
      lua_freelocalnick(ps, l2, quitm);
      LUA_RETURN(ps, LUA_OK);
    }
  }

  LUA_RETURN(ps, LUA_FAIL);
}

void lua_deregisternicks(lua_list *l) {
  struct lua_localnick *ln, *pn;

  for(ln=l->nicks;ln;ln=pn) {
    pn = ln->next;

    lua_freelocalnick(l->l, ln, "Script unloaded.");
  }

  l->nicks = NULL;
}

/* todo */
void lua_localnickhandler(nick *target, int type, void **args) {
  nick *np;
  char *p;
  lua_localnick *ln;
  lua_list *l;
  channel *c;

  if(!lua_getlocalnickbynick(target, &l, &ln))
    return;

  switch(type) {
    case LU_PRIVMSG:
      np = (nick *)args[0];
      p = (char *)args[1];

      if(!np || !p)
        return;

      lua_vlpcall(l, ln, "irc_onmsg", "Ns", np, p);

      break;

    case LU_CHANMSG:
      np = (nick *)args[0];
      c = (channel *)args[1];
      p = (char *)args[2];

      if(!np || !p || !c || !c->index || !c->index->name || !c->index->name->content)
        return;

      lua_vlpcall(l, ln, "irc_onchanmsg", "Nss", np, c->index->name->content, p);
      break;

    case LU_KILLED:
      lua_vlpcall(l, ln, "irc_onkilled", "");

      strlcpy(ln->nickname, target->nick, sizeof(ln->nickname));
      strlcpy(ln->ident, target->ident, sizeof(ln->ident));
      strlcpy(ln->hostname, target->host->name->content, sizeof(ln->hostname));
      strlcpy(ln->realname, target->realname->name->content, sizeof(ln->realname));
      strlcpy(ln->account, target->authname, sizeof(ln->account));

      ln->umodes = target->umodes;
      ln->nick = NULL;

      ln->reconnect = scheduleoneshot(time(NULL) + 1, &lua_reconnectlocal, ln);

      break;
  }
}

void lua_reconnectlocal(void *arg) {
  lua_list *l;
  lua_localnick *ln = (lua_localnick *)arg;

  ln->nick = registerlocaluser(ln->nickname, ln->ident, ln->hostname, ln->realname, ln->account, ln->umodes, &lua_localnickhandler);
  if(!ln->nick) {
    ln->reconnect = scheduleoneshot(time(NULL) + 1, &lua_reconnectlocal, ln);
    return;
  }

  ln->reconnect = NULL;

  if(lua_getlocalnickbynick(ln->nick, &l, &ln)) /* hacky! */
    lua_vlpcall(l, ln, "irc_onkillreconnect", "");
}

static int lua_localjoin(lua_State *ps) {
  nick *source;
  channel *target;
  char *chan;

  if(!lua_islong(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  source = getnickbynumeric(lua_tolong(ps, 1));
  if(!source)
    LUA_RETURN(ps, LUA_FAIL);

  chan = (char *)lua_tostring(ps, 2);

  if(!lua_lineok(chan)) 
    LUA_RETURN(ps, LUA_FAIL);
    
  target = findchannel(chan);
  if(target) {
    localjoinchannel(source, target);
  } else {
    localcreatechannel(source, chan);
  }

  LUA_RETURN(ps, LUA_OK);
}

static int lua_localpart(lua_State *ps) {
  nick *source;
  channel *target;
  char *chan;

  if(!lua_islong(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  source = getnickbynumeric(lua_tolong(ps, 1));
  if(!source)
    LUA_RETURN(ps, LUA_FAIL);

  chan = (char *)lua_tostring(ps, 2);

  if(!lua_lineok(chan)) 
    LUA_RETURN(ps, LUA_FAIL);
    
  target = findchannel(chan);
  if(target) {
    localpartchannel(source, target);
  } else {
    LUA_RETURN(ps, LUA_FAIL);
  }

  LUA_RETURN(ps, LUA_OK);
}

static int lua_localchanmsg(lua_State *ps) {
  char *msg;
  nick *source;
  channel *target;

  if(!lua_islong(ps, 1) || !lua_isstring(ps, 2) || !lua_isstring(ps, 3))
    LUA_RETURN(ps, LUA_FAIL);

  source = getnickbynumeric(lua_tolong(ps, 1));
  if(!source)
    LUA_RETURN(ps, LUA_FAIL);

  target = findchannel((char *)lua_tostring(ps, 2));
  if(!target)
    LUA_RETURN(ps, LUA_FAIL);

  msg = (char *)lua_tostring(ps, 3);

  if(!lua_lineok(msg))
    LUA_RETURN(ps, LUA_FAIL);

  sendmessagetochannel(source, target, "%s", msg);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_localnotice(lua_State *ps) {
  char *msg;
  nick *source;
  nick *target;

  if(!lua_islong(ps, 1) || !lua_islong(ps, 2) || !lua_isstring(ps, 3))
    LUA_RETURN(ps, LUA_FAIL);

  source = getnickbynumeric(lua_tolong(ps, 1));
  if(!source)
    LUA_RETURN(ps, LUA_FAIL);

  target = getnickbynumeric(lua_tolong(ps, 2));
  if(!target)
    LUA_RETURN(ps, LUA_FAIL);

  msg = (char *)lua_tostring(ps, 3);

  if(!lua_lineok(msg))
    LUA_RETURN(ps, LUA_FAIL);

  sendnoticetouser(source, target, "%s", msg);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_localovmode(lua_State *ps) {
  char *mode;
  nick *source, *target;
  channel *chan;
  int add, realmode;

  if(!lua_islong(ps, 1) || !lua_isstring(ps, 2) || !lua_isboolean(ps, 3) || !lua_isstring(ps, 4) || !lua_islong(ps, 5))
    LUA_RETURN(ps, LUA_FAIL);

  source = getnickbynumeric(lua_tolong(ps, 1));
  if(!source)
    LUA_RETURN(ps, LUA_FAIL);

  chan = findchannel((char *)lua_tostring(ps, 2));
  if(!chan)
    LUA_RETURN(ps, LUA_FAIL);

  add = (int)lua_toboolean(ps, 3);
  mode = (char *)lua_tostring(ps, 4);

  target = getnickbynumeric(lua_tolong(ps, 5));
  if(!target)
    LUA_RETURN(ps, LUA_FAIL);

  if((*mode == 'o') && add) {
    realmode = MC_OP;
  } else if (*mode == 'o') {
    realmode = MC_DEOP;
  } else if((*mode == 'v') && add) {
    realmode = MC_VOICE;
  } else if (*mode == 'v') {
    realmode = MC_DEVOICE;
  } else {
    LUA_RETURN(ps, LUA_FAIL);
  }

  localsetmodes(source, chan, target, realmode);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_localtopic(lua_State *ps) {
  nick *np;
  channel *cp;
  char *topic;

  if(!lua_islong(ps, 1) || !lua_isstring(ps, 2) || !lua_isstring(ps, 3))
    LUA_RETURN(ps, LUA_FAIL);

  np = getnickbynumeric(lua_tolong(ps, 1));
  if(!np)
    LUA_RETURN(ps, LUA_FAIL);

  cp = findchannel((char *)lua_tostring(ps, 2));
  if(!cp)
    LUA_RETURN(ps, LUA_FAIL);

  topic = (char *)lua_tostring(ps, 3);
  if(!topic || !lua_lineok(topic))
    LUA_RETURN(ps, LUA_FAIL);

  localsettopic(np, cp, topic);

  LUA_RETURN(ps, LUA_OK);
}

void lua_registerlocalcommands(lua_State *l) {
  lua_register(l, "irc_localregisteruser", lua_registerlocaluser);
  lua_register(l, "irc_localderegisteruser", lua_deregisterlocaluser);
  lua_register(l, "irc_localjoin", lua_localjoin);
  lua_register(l, "irc_localpart", lua_localpart);
  lua_register(l, "irc_localchanmsg", lua_localchanmsg);
  lua_register(l, "irc_localnotice", lua_localnotice);

  lua_register(l, "irc_localovmode", lua_localovmode);
  lua_register(l, "irc_localtopic", lua_localtopic);

}

