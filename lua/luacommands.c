/* Copyright (C) Chris Porter 2005 */
/* ALL RIGHTS RESERVED. */
/* Don't put this into the SVN repo. */

#include "../channel/channel.h"
#include "../control/control.h"
#include "../nick/nick.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../lib/irc_string.h"

#include "lua.h"
#include "luabot.h"

#include <stdarg.h>

static int lua_smsg(lua_State *ps);
static int lua_skill(lua_State *ps);

int lua_lineok(const char *data) {
  if(strchr(data, '\r') || strchr(data, '\n'))
    return 0;
  return 1;
}

int lua_cmsg(char *channell, char *message, ...) {
  char buf[512];
  va_list va;
  channel *cp;

  va_start(va, message);
  vsnprintf(buf, sizeof(buf), message, va);
  va_end(va);

  cp = findchannel(channell);
  if(!cp)
    return LUA_FAIL;

  if(!lua_lineok(buf))
    return LUA_FAIL;

  lua_channelmessage(cp, "%s", buf);

  return LUA_OK;
}

static int lua_chanmsg(lua_State *ps) {
  if(!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  LUA_RETURN(ps, lua_cmsg(LUA_PUKECHAN, "lua: %s", lua_tostring(ps, 1)));
}

static int lua_scripterror(lua_State *ps) {
  if(!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  LUA_RETURN(ps, lua_cmsg(LUA_PUKECHAN, "lua-error: %s", lua_tostring(ps, 1)));
}

static int lua_ctcp(lua_State *ps) {
  const char *n, *msg;
  nick *np;

  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  n = lua_tostring(ps, 1);
  msg = lua_tostring(ps, 2);

  np = getnickbynick(n);
  if(!np || !lua_lineok(msg))
    LUA_RETURN(ps, LUA_FAIL);

  lua_message(np, "\001%s\001", msg);

  LUA_RETURN(ps, lua_cmsg(LUA_PUKECHAN, "lua-ctcp: %s (%s)", np->nick, msg));
}

static int lua_noticecmd(lua_State *ps) {
  const char *n, *msg;
  nick *np;

  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  n = lua_tostring(ps, 1);
  msg = lua_tostring(ps, 2);

  np = getnickbynick(n);
  if(!np || !lua_lineok(msg))
    LUA_RETURN(ps, LUA_FAIL);

  lua_notice(np, "%s", msg);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_kill(lua_State *ps) {
  const char *n, *msg;
  nick *np;

  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  n = lua_tostring(ps, 1);
  msg = lua_tostring(ps, 2);

  np = getnickbynick(n);
  if(!np)
    LUA_RETURN(ps, LUA_FAIL);

  if(IsOper(np) || IsService(np) || IsXOper(np))
    LUA_RETURN(ps, LUA_FAIL);

  if(!lua_lineok(msg))
    LUA_RETURN(ps, LUA_FAIL);

  killuser(lua_nick, np, "%s", msg);

  LUA_RETURN(ps, lua_cmsg(LUA_PUKECHAN, "lua-KILL: %s (%s)", np->nick, msg));
}

static int lua_gline(lua_State *ps) {
  const char *reason;
  nick *target;
  char mask[512];
  int duration, usercount = 0;
  host *hp;
  
  if(!lua_isstring(ps, 1) || !lua_isint(ps, 2) || !lua_isstring(ps, 3))
    LUA_RETURN(ps, LUA_FAIL);

  duration = lua_toint(ps, 2);
  if((duration < 1) || (duration > 86400))
    LUA_RETURN(ps, LUA_FAIL);

  reason = lua_tostring(ps, 3);
  if(!lua_lineok(reason) || !reason)
    LUA_RETURN(ps, LUA_FAIL);

  target = getnickbynick(lua_tostring(ps, 1));
  if(!target || (IsOper(target) || IsXOper(target) || IsService(target)))
    LUA_RETURN(ps, LUA_FAIL);

  hp = target->host;
  if(!hp)
    LUA_RETURN(ps, LUA_FAIL);

  usercount = hp->clonecount;
  if(usercount > 10) { /* (decent) trusted host */
    int j;
    nick *np;

    usercount = 0;

    for (j=0;j<NICKHASHSIZE;j++)
      for (np=nicktable[j];np;np=np->next)
        if (np && (np->host == hp) && (!ircd_strcmp(np->ident, target->ident)))
          usercount++;

    if(usercount > 50)
      LUA_RETURN(ps, LUA_FAIL);

    snprintf(mask, sizeof(mask), "*%s@%s", target->ident, IPtostr(target->ipaddress));
  } else {
    snprintf(mask, sizeof(mask), "*@%s", IPtostr(target->ipaddress));
  }

  irc_send("%s GL * +%s %d :%s", mynumeric->content, mask, duration, reason);
  LUA_RETURN(ps, lua_cmsg(LUA_PUKECHAN, "lua-GLINE: %s (%d users, %d seconds -- %s)", mask, usercount, duration, reason));
}

static int lua_getchaninfo(lua_State *ps) {
  channel *cp;

  if(!lua_isstring(ps, 1))
    return 0;

  cp = findchannel((char *)lua_tostring(ps, 1));
  if(!cp)
    return 0;

  LUA_PUSHCHAN(ps, cp);

  return 1;
}

static int lua_opchan(lua_State *ps) {
  channel *cp;
  nick *np;

  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  cp = findchannel((char *)lua_tostring(ps, 1));
  if(!cp)
    LUA_RETURN(ps, LUA_FAIL);

  np = getnickbynick((char *)lua_tostring(ps, 2));
  if(!np)
    LUA_RETURN(ps, LUA_FAIL);

  localsetmodes(lua_nick, cp, np, MC_OP);
  LUA_RETURN(ps, LUA_OK);
}

static int lua_voicechan(lua_State *ps) {
  channel *cp;
  nick *np;

  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  cp = findchannel((char *)lua_tostring(ps, 1));
  if(!cp)
    LUA_RETURN(ps, LUA_FAIL);

  np = getnickbynick((char *)lua_tostring(ps, 2));
  if(!np)
    LUA_RETURN(ps, LUA_FAIL);

  localsetmodes(lua_nick, cp, np, MC_VOICE);
  LUA_RETURN(ps, LUA_OK);
}

static int lua_counthost(lua_State *ps) {
  long numeric;
  nick *np;

  if(!lua_islong(ps, 1))
    return 0;

  numeric = lua_tolong(ps, 1);

  np = getnickbynumeric(numeric);
  if(!np)
    return 0;

  lua_pushint(ps, np->host->clonecount);
  return 1;
}

static int lua_versioninfo(lua_State *ps) {
  lua_pushstring(ps, LUA_VERSION);
  lua_pushstring(ps, LUA_BOTVERSION);
  lua_pushstring(ps, __DATE__);
  lua_pushstring(ps, __TIME__);

  return 4;
}

/* O(n) */
static int lua_getuserbyauth(lua_State *l) {
  const char *acc;
  nick *np;
  int i, found = 0;

  if(!lua_isstring(l, 1))
    return 0;

  for(i=0;i<NICKHASHSIZE;i++) {
    for(np=nicktable[i];np;np=np->next) {
      if(np && np->authname && !ircd_strcmp(np->authname, acc)) {
        LUA_PUSHNICK(l, np);
        found++;
      }
    }
  }

  return found;
}

static int lua_getnickchans(lua_State *l) {
  nick *np;
  int i;
  channel **channels;

  if(!lua_islong(l, 1))
    return 0;

  np = getnickbynumeric(lua_tolong(l, 1));
  if(!np)
    return 0;

  channels = (channel **)np->channels->content;
  for(i=0;i<np->channels->cursi;i++)
    lua_pushstring(l, channels[i]->index->name->content);

  return np->channels->cursi;
}

static int lua_gethostusers(lua_State *l) {
  nick *np;
  int count;

  if(!lua_islong(l, 1))
    return 0;

  np = getnickbynumeric(lua_tolong(l, 1));
  if(!np || !np->host || !np->host->nicks)
    return 0;

  np = np->host->nicks;
  count = np->host->clonecount;

  do {
    LUA_PUSHNICK(l, np);
    np = np->nextbyhost;
  } while(np);

  return count;
}

/*
static int lua_iteratenickhash(lua_State *l) {
  nick *np;
  int i, top;
  void *fp;

  if(!lua_isfunction(l, 1))
    LUA_RETURN(LUA_FAIL);

  fp = lua_touserdata(l, 1);
  if(!fp)
    LUA_RETURN(LUA_FAIL);
 
  for(i=0;i<NICKHASHSIZE;i++) {
    for(np=nicktable[i];np;np=np->next) {
      if(np) {
        top = lua_gettop(l);

        lua_getglobal(l, "scripterror");
        lua_insert

        LUA_PUSHNICK(l, np);
        lua_pcall(l, 1, 0, top + 1);

        lua_settop(l, top);
      }
    }
  }

  LUA_RETURN(LUA_OK);
}
*/

static int lua_chanfix(lua_State *ps) {
  channel *cp;
  nick *np;

  if(!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  cp = findchannel((char *)lua_tostring(ps, 1));
  if(!cp)
    LUA_RETURN(ps, LUA_FAIL);

  np = getnickbynick(LUA_CHANFIXBOT);
  if(!np)
    LUA_RETURN(ps, LUA_FAIL);

  lua_message(np, "chanfix %s", cp->index->name->content);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_clearmode(lua_State *ps) {
  channel *cp;
  int i;
  nick *np;
  unsigned long *lp;
  modechanges changes;

  if(!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  cp = findchannel((char *)lua_tostring(ps, 1));
  if(!cp)
    LUA_RETURN(ps, LUA_FAIL);

  localsetmodeinit(&changes, cp, lua_nick);

  localdosetmode_key(&changes, NULL, MCB_DEL);
  localdosetmode_simple(&changes, 0, CHANMODE_INVITEONLY | CHANMODE_LIMIT);

  while(cp->bans)
    localdosetmode_ban(&changes, bantostring(cp->bans), MCB_DEL);

  for(i=0,lp=cp->users->content;i<cp->users->hashsize;i++,lp++)
    if((*lp != nouser) && (*lp & CUMODE_OP)) {
      np = getnickbynumeric(*lp);
      if(np && !IsService(np))
        localdosetmode_nick(&changes, np, MC_DEOP);
    }

  localsetmodeflush(&changes, 1);

  LUA_RETURN(ps, LUA_OK);
}

void lua_registercommands(lua_State *l) {
  lua_register(l, "irc_smsg", lua_smsg);
  lua_register(l, "irc_skill", lua_skill);

  lua_register(l, "chanmsg", lua_chanmsg);
  lua_register(l, "scripterror", lua_scripterror);
  lua_register(l, "versioninfo", lua_versioninfo);

  lua_register(l, "irc_report", lua_chanmsg);
  lua_register(l, "irc_ctcp", lua_ctcp);
  lua_register(l, "irc_kill", lua_kill);
  lua_register(l, "irc_gline", lua_gline);
  lua_register(l, "irc_getchaninfo", lua_getchaninfo);
  lua_register(l, "irc_counthost", lua_counthost);
  lua_register(l, "irc_getuserbyauth", lua_getuserbyauth);
  lua_register(l, "irc_notice", lua_noticecmd);
  lua_register(l, "irc_opchan", lua_opchan);
  lua_register(l, "irc_voicechan", lua_voicechan);
  lua_register(l, "irc_chanfix", lua_chanfix);
  lua_register(l, "irc_clearmode", lua_clearmode);

  lua_register(l, "irc_getnickchans", lua_getnickchans);
  lua_register(l, "irc_gethostusers", lua_gethostusers);

/*  lua_register(l, "irc_iteratenickhash", lua_iteratenickhash); */
}

/* --- */

static int lua_smsg(lua_State *ps) {
  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  LUA_RETURN(ps, lua_cmsg((char *)lua_tostring(ps, 2), "%s", lua_tostring(ps, 1)));
}

static int lua_skill(lua_State *ps) {
  const char *n, *msg;
  nick *np;

  if(!lua_isstring(ps, 1) || !lua_isstring(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  n = lua_tostring(ps, 1);
  msg = lua_tostring(ps, 2);

  np = getnickbynick(n);
  if(!np)
    LUA_RETURN(ps, LUA_FAIL);

  if(IsOper(np) || IsService(np) || IsXOper(np))
    LUA_RETURN(ps, LUA_FAIL);

  if(!lua_lineok(msg))
    LUA_RETURN(ps, LUA_FAIL);

  killuser(lua_nick, np, "%s", msg);

  LUA_RETURN(ps, LUA_OK);
}

