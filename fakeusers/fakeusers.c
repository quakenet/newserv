/*
 *  NOperserv Fakeuser module
 *
 *  Allows fakeusers to be added so as to block nicks, for example.
 *
 *  Copyright (c) Tim Gordon 2006.
 */

#include "../core/schedule.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../control/control.h"
#include "../channel/channel.h"
#include "../dbapi2/dbapi2.h"
#include "../lib/strlfunc.h"
#include "../lib/version.h"
#include <string.h>
#include <stdlib.h>

MODULE_VERSION("");

/* #define SIT_CHANNEL "#qnet.fakeusers" */
#define KILL_WAIT 10
#define KILL_TIME 60

typedef struct fakeuser {
  nick *user;
  time_t lastkill;
  struct fakeuser *next;
} fakeuser;

typedef struct user_details {
  char nick[NICKLEN + 1];
  char ident[USERLEN + 1];
  char host[HOSTLEN + 1];
  char realname[REALLEN + 1];
  time_t lastkill;
  struct user_details *next;
  void *schedule;
} user_details;

fakeuser *fakeuserlist = NULL;
user_details *killeduserlist = NULL;
int fakeusercount = 0;
int killedusercount = 0;

void fakeuser_cleanup();
int fakeuser_loaddb();
void fakeusers_load(const DBAPIResult *res, void *arg);
void fakeuser_handler(nick *user, int command, void **params);
int fakeadd(void *sender, int cargc, char **cargv);
int fakelist(void *sender, int cargc, char **cargv);
int fakekill(void *sender, int cargc, char **cargv);
void reconnectfake(void *details);

static DBAPIConn *nofudb;

void _init() {
  if (!fakeuser_loaddb())
  {
    Error("fakeuser", ERR_FATAL, "Cannot load database");
    return;
  }
  registercontrolhelpcmd("fakeuser", NO_OPER, 4, &fakeadd, "Usage: FAKEUSER nick <ident> <host> <realname>\nCreates a fake user.");
  registercontrolhelpcmd("fakelist", NO_OPER, 0, &fakelist, "Usage: FAKELIST\nLists all fake users.");
  registercontrolhelpcmd("fakekill", NO_OPER, 2, &fakekill, "Usage: FAKEKILL nick <reason>\nRemoves a fake user");
}

void _fini() {
  fakeuser_cleanup();
  deregistercontrolcmd("fakeuser", &fakeadd);
  deregistercontrolcmd("fakelist", &fakelist);
  deregistercontrolcmd("fakekill", &fakekill);
}

void fakeuser_cleanup()
{
  fakeuser *fake;
  user_details *killed;
  void *next;
  for (fake = fakeuserlist; fake; fake = next)
  {
    deregisterlocaluser(fake->user, "Signing off");
    next = fake->next;
    free(fake);
  }
  for (killed = killeduserlist; killed; killed = next)
  {
    deleteschedule(killed->schedule, &reconnectfake, killed);
    next = killed->next;
    free(killed);
  }
  fakeusercount = 0;
  killedusercount = 0;
  fakeuserlist = NULL;
  killeduserlist = NULL;
}

int fakeuser_loaddb()
{
  if (!nofudb) {
    nofudb = dbapi2open(DBAPI2_DEFAULT, "fakeusers");
    if(!nofudb) {
      Error("fakeuser", ERR_STOP, "Could not connect to database.");
      return 0;
    }
  }

  nofudb->createtable(nofudb, NULL, NULL, 
    "CREATE TABLE ? ("
    "nick      VARCHAR(?)  NOT NULL,"
    "ident     VARCHAR(?)  NOT NULL,"
    "host      VARCHAR(?)  NOT NULL,"
    "realname  VARCHAR(?)  NOT NULL,"
    "PRIMARY KEY (nick))", "Tdddd", "fakeusers", NICKLEN, USERLEN, HOSTLEN, REALLEN);

  nofudb->query(nofudb, fakeusers_load, NULL,
    "SELECT nick, ident, host, realname FROM ?", "T", "fakeusers");

  return 1;
}

void fakeusers_load(const DBAPIResult *res, void *arg) {
  user_details details;

  if(!res)
    return;

  if(!res->success) {
    Error("fakeuser", ERR_ERROR, "Error loading fakeuser list.");
    res->clear(res);
    return;
  }

  details.lastkill = 0;
  details.schedule = NULL;
  while(res->next(res)) {
    strlcpy(details.nick, res->get(res, 0), NICKLEN + 1);
    strlcpy(details.ident, res->get(res, 1), USERLEN + 1);
    strlcpy(details.host, res->get(res, 2), HOSTLEN + 1);
    strlcpy(details.realname, res->get(res, 3), REALLEN + 1);
    reconnectfake(&details);
  }

  res->clear(res);
}

user_details *getdetails(nick *user)
{
  user_details *details;
  details = malloc(sizeof(user_details));
  if (!details)
    return NULL;
  strlcpy(details->nick, user->nick, NICKLEN + 1);
  strlcpy(details->ident, user->ident, USERLEN + 1);
  strlcpy(details->host, user->host->name->content, HOSTLEN + 1);
  strlcpy(details->realname, user->realname->name->content, REALLEN + 1);
  details->schedule = NULL;
  details->lastkill = 0;
  return details;
}

#define ERR_EXISTS 1
#define ERR_MEM 2
#define ERR_WONTKILL 3
int err_code;

nick *register_fakeuser(user_details *details)
{
  nick *user;
  if ((user = getnickbynick(details->nick)) && (IsOper(user) || IsService(user) || IsXOper(user))) {
    err_code = ERR_WONTKILL;
    return NULL;
  }

  err_code = ERR_MEM;
  return registerlocaluser(details->nick, details->ident, details->host, details->realname,
    NULL, UMODE_INV | UMODE_DEAF, &fakeuser_handler);
}

fakeuser *ll_add(user_details *details) 
// Adds to the (sorted) linked list
{
  fakeuser *fake, *newfake;
  int cmp;

  if (fakeuserlist)
  {
    cmp = ircd_strcmp(details->nick, fakeuserlist->user->nick);
    if (!cmp)
    {
      err_code = ERR_EXISTS;
      return NULL;
    }
  }
  else
    cmp = -1;
  if (cmp < 0)
  {
    newfake = malloc(sizeof(fakeuser));
    if (!newfake)
    {
      err_code = ERR_MEM;
      return NULL;
    }
    newfake->user = register_fakeuser(details);
    if (!newfake->user)
    {
      free(newfake);
      return NULL;   //errcode already set by register_fakeuser
    }
    newfake->lastkill = details->lastkill;
    newfake->next = fakeuserlist;
    fakeuserlist = newfake;
    fakeusercount++;
    return newfake;
  }
  for (fake = fakeuserlist; fake->next; fake = fake->next)
  {
    cmp = ircd_strcmp(details->nick, fake->next->user->nick);
    if (!cmp)
    {
      err_code = ERR_EXISTS;
      return NULL;
    }
    if (cmp < 0)
    {
      newfake = malloc(sizeof(fakeuser));
      if (!newfake)
      {
        err_code = ERR_MEM;
        return NULL;
      }
      newfake->user = register_fakeuser(details);
      if (!newfake->user)
      {
        free(newfake);
        return NULL;   //errcode already set by register_fakeuser
      }
      newfake->lastkill = details->lastkill;
      newfake->next = fake->next;
      fake->next = newfake;
      fakeusercount++;
      return newfake;
    }
  }
  newfake = malloc(sizeof(fakeuser));
  if (!newfake)
  {
    err_code = ERR_MEM;
    return NULL;
  }
  newfake->user = register_fakeuser(details);
  if (!newfake->user)
  {
    free(newfake);
    return NULL;   //errcode already set by register_fakeuser
  }
  newfake->lastkill = details->lastkill;
  newfake->next = NULL;
  fake->next = newfake;
  fakeusercount++;
  return newfake;
}

void kll_add(user_details *details)
//Adds to the (sorted) linked list of killed users
{
  int cmp;
  user_details *killed;

  if (killeduserlist)
    cmp = ircd_strcmp(details->nick, killeduserlist->nick);
  else
    cmp = -1;
  if (cmp < 0)  //Add to the start of the list
  {
    details->next = killeduserlist;
    killeduserlist = details;
    killedusercount++;
    return;
  }
  for (killed = killeduserlist; killed->next; killed = killed->next)
  {
    cmp = ircd_strcmp(details->nick, killed->next->nick);
    if (cmp < 0)
    {
      details->next = killed->next;
      killed->next = details;
      killedusercount++;
      return;
    }
  }
  details->next = NULL;
  killed->next = details;
  killedusercount++;
  return;
}

fakeuser *ll_remove(char *nickname)
//Removes from the linked list
{
  fakeuser *fake, *rmfake;
  int cmp;

  if (!fakeuserlist)
    return NULL;
  cmp = ircd_strcmp(nickname, fakeuserlist->user->nick);
  if (cmp < 0)
    return NULL;
  if (!cmp)
  {
    rmfake = fakeuserlist;
    fakeuserlist = fakeuserlist->next;
    fakeusercount--;
    rmfake->next = NULL;
    return rmfake;
  }
  for (fake = fakeuserlist; fake->next; fake = fake->next)
  {
    rmfake = fake->next;
    cmp = ircd_strcmp(nickname, rmfake->user->nick);
    if (cmp < 0)
      return NULL;
    if (!cmp)
    {
      fake->next = rmfake->next;
      fakeusercount--;
      rmfake->next = NULL;
      return rmfake;
    }
  }
  return NULL;
}

user_details *kll_remove(char *nickname) //Removes from the killed user linked list
{
  user_details *details, *rmdetails;
  int cmp;

  if (!killeduserlist)
    return NULL;
  cmp = ircd_strcmp(nickname, killeduserlist->nick);
  if (cmp < 0)
    return NULL;
  if (!cmp)
  {
    rmdetails = killeduserlist;
    killeduserlist = killeduserlist->next;
    rmdetails->next = NULL;
    killedusercount--;
    return rmdetails;
  }
  for (details = killeduserlist; details->next; details = details->next)
  {
    rmdetails = details->next;
    cmp = ircd_strcmp(nickname, rmdetails->nick);
    if (cmp < 0)
      return NULL;
    if (!cmp)
    {
      details->next = rmdetails->next;
      rmdetails->next = NULL;
      killedusercount--;
      return rmdetails;
    }
  }
  return NULL;
}

void fakeuser_handler(nick *user, int command, void **params)
{
  if (command == LU_KILLED)
  {
    user_details *details;
    fakeuser *item;
    time_t timenow = time(NULL);

    details = getdetails(user);
    item = ll_remove(details->nick);
    if (!item)
    {
      controlwall(NO_OPER, NL_FAKEUSERS, "Error: A fakeuser was killed, but wasn't found in the list");
      Error("fakeuser", ERR_ERROR, "A fakeuser was killed, but wasn't found in the list");
      return;
    }

    details->lastkill = item->lastkill;
    free(item);

    if (timenow - details->lastkill < KILL_TIME) {
      controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) KILL'ed twice under in %d seconds. Removing.", details->nick, details->ident, details->host, details->realname, KILL_TIME);
      nofudb->squery(nofudb, "DELETE FROM ? WHERE nick = ?", "Ts", "fakeusers", details->nick);
      return;
    }
    details->lastkill = timenow;

    details->schedule = scheduleoneshot(time(NULL) + KILL_WAIT, &reconnectfake, details);
    if (!details->schedule)
    {
      Error("fakeuser", ERR_ERROR, "Couldn't reschedule fakeuser for reconnect");
      free(details);
      return;
    }
    kll_add(details);
  }
}

//Usage: FAKEUSER nick <ident> <host> <realname>
int fakeadd(void *sender, int cargc, char **cargv)
{
  user_details details, *killed;
  nick *newnick;
  fakeuser *newfake;

  if (cargc == 0)
    return CMD_USAGE;
  strlcpy(details.nick, cargv[0], NICKLEN + 1);
  if (cargc < 4)
    strlcpy(details.realname, cargv[0], REALLEN + 1);
  else
    strlcpy(details.realname, cargv[3], REALLEN + 1);
  if (cargc < 3)
  {
    strlcpy(details.host, cargv[0], NICKLEN + 1);
    strlcat(details.host, ".fakeusers.quakenet.org", HOSTLEN + 1);
  }
  else
    strlcpy(details.host, cargv[2], HOSTLEN + 1);
  if (cargc < 2)
    strlcpy(details.ident, cargv[0], USERLEN + 1);
  else
    strlcpy(details.ident, cargv[1], USERLEN + 1);

  details.lastkill = 0;

  newfake = ll_add(&details);
  if (!newfake)
  {
    if (err_code == ERR_EXISTS)
      controlreply(sender, "Error: fakeuser already exists");
    else if (err_code == ERR_MEM)
    {
      controlreply(sender, "Error: memory error!!");
      Error("fakeuser", ERR_STOP, "malloc error");
    }
    else if (err_code == ERR_WONTKILL)
      controlreply(sender, "Nick already exists and I won't kill it");
    else
    {
      controlreply(sender, "Unknown error when adding fakeuser");
      Error("fakeuser", ERR_ERROR, "Unknown error when adding fakeuser");
    }
    return CMD_ERROR;
  }
  newnick = newfake->user;
  if ((killed = kll_remove(newnick->nick)))  //Is this nickname scheduled to reconnect?
  {
    deleteschedule(killed->schedule, &reconnectfake, killed);
    free(killed);
    nofudb->squery(nofudb, "UPDATE ? SET ident=?, host=?, realname=? WHERE nick = ?", "Tssss", "fakeusers", newnick->ident, newnick->host->name->content, newnick->realname->name->content, newnick->nick);
  }
  else
    nofudb->squery(nofudb, "INSERT INTO ? (nick, ident, host, realname) VALUES (?,?,?,?)", "Tssss", "fakeusers", newnick->nick, newnick->ident, newnick->host->name->content, newnick->realname->name->content);
  controlreply(sender, "Added fake user %s", newnick->nick);
  controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) added by %s/%s", newnick->nick, newnick->ident,
              newnick->host->name->content, newnick->realname->name->content, ((nick*)sender)->nick, ((nick*)sender)->authname);
#ifdef SIT_CHANNEL
  {
    channel *sitchannel;
    if (!(sitchannel = findchannel(SIT_CHANNEL)))
      sitchannel = createchannel(SIT_CHANNEL);
    localjoinchannel(newnick, sitchannel);
  }
#endif
  return CMD_OK;
}

//Usage: FAKELIST
int fakelist(void *sender, int cargc, char **cargv)
{
  fakeuser *fake;
  if (fakeusercount == 1)
    controlreply(sender, "1 fakeuser is currently connected");
  else
    controlreply(sender, "%d fakeusers are currently connected", fakeusercount);
  for(fake = fakeuserlist; fake; fake = fake->next)
    controlreply(sender, "%s!%s@%s (%s)", fake->user->nick, fake->user->ident,
                 fake->user->host->name->content, fake->user->realname->name->content);
  if (killeduserlist)
  {
    user_details *k_fake;
    if (fakeusercount == 1)
      controlreply(sender, "1 fakeuser is currently waiting to reconnect");
    else
      controlreply(sender, "%d fakeusers are currently waiting to reconnect", killedusercount);
      for(k_fake = killeduserlist; k_fake; k_fake = k_fake->next)
        controlreply(sender, "%s!%s@%s (%s)", k_fake->nick, k_fake->ident, k_fake->host, k_fake->realname);
  }
  return CMD_OK;
}

//Usage: FAKEKILL nick <reason>
int fakekill(void *sender, int cargc, char **cargv)
{
  fakeuser *fake;
  user_details *k_fake;
  if (cargc == 0)
    return CMD_USAGE;
  if ((fake = ll_remove(cargv[0])))
  {
    nofudb->squery(nofudb, "DELETE FROM ? WHERE nick = ?", "Ts", "fakeusers", fake->user->nick);
    controlreply(sender, "Killed fake user %s", fake->user->nick);
    controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) removed by %s/%s", fake->user->nick, fake->user->ident,
                fake->user->host->name->content, fake->user->realname->name->content, ((nick*)sender)->nick, ((nick*)sender)->authname);
    if (cargc > 1)
      deregisterlocaluser(fake->user, cargv[1]);
    else
      deregisterlocaluser(fake->user, "Signing off");
    free(fake);
    return CMD_OK;
  }
  if ((k_fake = kll_remove(cargv[0])))  //Fakeuser might be waiting to rejoin
  {
    nofudb->squery(nofudb, "DELETE FROM ? WHERE nick = ?", "Ts", "fakeusers", k_fake->nick);
    controlreply(sender, "Killed fake user %s", k_fake->nick);
    controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) removed by %s/%s", k_fake->nick,
                k_fake->ident, k_fake->host, k_fake->realname, ((nick*)sender)->nick, ((nick*)sender)->authname);
    free(k_fake);
    return CMD_OK;
  }
  controlreply(sender, "No fakeuser with that nick exists");
  return CMD_ERROR;
}

void reconnectfake(void *details)
//Called after the timeout period has expired since a fakeuser was killed, or on load
{
  fakeuser *fake;
  user_details *u_details = details;
  if (u_details->schedule)
    kll_remove(u_details->nick); //Fakeuser was /kill'd
  fake = ll_add(u_details);
  if (u_details->schedule)
    free(u_details);
  if (!fake)
    return;
#ifdef SIT_CHANNEL
  {
    channel *sitchannel;
    if (!(sitchannel = findchannel(SIT_CHANNEL)))
      sitchannel = createchannel(SIT_CHANNEL);
    localjoinchannel(fake->user, sitchannel);
  }
#endif
}
