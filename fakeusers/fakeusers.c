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

#define KILL_WAIT 10
#define KILL_TIME 60

typedef struct fakeuser {
  char nick[NICKLEN + 1];
  char ident[USERLEN + 1];
  char host[HOSTLEN + 1];
  char realname[REALLEN + 1];
  nick *user;
  time_t lastkill;
  struct fakeuser *next;
} fakeuser;

fakeuser *fakeuserlist = NULL;

static void fakeuser_cleanup();
static int fakeuser_loaddb();
static void fakeusers_load(const DBAPIResult *res, void *arg);
static void fakeuser_handler(nick *user, int command, void **params);
static int fakeadd(void *sender, int cargc, char **cargv);
static int fakelist(void *sender, int cargc, char **cargv);
static int fakekill(void *sender, int cargc, char **cargv);
static void schedulefakeuser(void *arg);
static fakeuser *findfakeuserbynick(char *nick);
static void fake_remove(char *nickname);
static fakeuser *fake_add(fakeuser *details);

static DBAPIConn *nofudb;

void fakeuser_cleanup() {
  fakeuser *fake;
  void *next;

  for (fake = fakeuserlist; fake; fake = next) {
    deregisterlocaluser(fake->user, "Signing off");
    next = fake->next;
    free(fake);
  }

  fakeuserlist = NULL;
}

int fakeuser_loaddb() {
  if (!nofudb) {
    nofudb = dbapi2open(DBAPI2_DEFAULT, "fakeusers");

    if (!nofudb) {
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
  fakeuser fakeuser;

  if (!res)
    return;

  if (!res->success) {
    Error("fakeuser", ERR_ERROR, "Error loading fakeuser list.");
    res->clear(res);
    return;
  }

  while (res->next(res)) {
    strlcpy(fakeuser.nick, res->get(res, 0), NICKLEN + 1);
    strlcpy(fakeuser.ident, res->get(res, 1), USERLEN + 1);
    strlcpy(fakeuser.host, res->get(res, 2), HOSTLEN + 1);
    strlcpy(fakeuser.realname, res->get(res, 3), REALLEN + 1);
    fake_add(&fakeuser);
  }

  scheduleoneshot(time(NULL) + 1, schedulefakeuser, NULL);
  res->clear(res);
}

nick *register_fakeuseronnet(fakeuser *details) {
  nick *user;

  if ((user = getnickbynick(details->nick)) && (IsOper(user) || IsService(user) || IsXOper(user))) {
    return NULL;
  }

  return registerlocaluser(details->nick, details->ident, details->host, details->realname,
                           NULL, UMODE_INV | UMODE_DEAF, &fakeuser_handler);
}

fakeuser *fake_add(fakeuser *details) {
  fakeuser *newfake;

  newfake = malloc(sizeof(fakeuser));

  if (!newfake) {
    return NULL;
  }

  memcpy(newfake, details, sizeof(fakeuser));

  newfake->user = NULL;
  newfake->lastkill = 0;

  newfake->next = fakeuserlist;
  fakeuserlist = newfake;
  return newfake;
}

void fake_remove(char *nickname) {
  fakeuser *fake, *prev;

  for (fake = fakeuserlist; fake; fake = fake->next) {
    if (!ircd_strcmp(nickname, fake->nick)) {
      if (fake == fakeuserlist)
        fakeuserlist = fake->next;
      else
        prev->next = fake->next;

      if(fake->user)
        deregisterlocaluser(fake->user, "Signing off");

      free(fake);
      return;
    }

    prev = fake;
  }
}

void fakeuser_handler(nick *user, int command, void **params) {
  if (command == LU_KILLED) {
    fakeuser *item;
    time_t timenow = time(NULL);

    item = findfakeuserbynick(user->nick);

    if (!item) {
      controlwall(NO_OPER, NL_FAKEUSERS, "Error: A fakeuser was killed, but wasn't found in the list");
      Error("fakeuser", ERR_ERROR, "A fakeuser was killed, but wasn't found in the list");
      return;
    }

    item->user = NULL;

    if (timenow - item->lastkill < KILL_TIME) {
      controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) KILL'ed twice under in %d seconds. Removing.", item->nick, item->ident, item->host, item->realname, KILL_TIME);
      nofudb->squery(nofudb, "DELETE FROM ? WHERE nick = ?", "Ts", "fakeusers", item->nick);
      fake_remove(item->nick);
      return;
    }

    item->lastkill = timenow;

    scheduleoneshot(time(NULL) + KILL_WAIT, schedulefakeuser, item);
  }
}

int fakeadd(void *sender, int cargc, char **cargv) {
  fakeuser newfake;
  fakeuser *fake;

  if (cargc == 0)
    return CMD_USAGE;

  fake = findfakeuserbynick(cargv[0]);

  if (fake) {
    controlreply(sender, "Fake User with nick %s already found", cargv[0]);
    return CMD_ERROR;
  }

  strlcpy(newfake.nick, cargv[0], NICKLEN + 1);

  if (cargc < 4)
    strlcpy(newfake.realname, cargv[0], REALLEN + 1);
  else
    strlcpy(newfake.realname, cargv[3], REALLEN + 1);

  if (cargc < 3) {
    strlcpy(newfake.host, cargv[0], NICKLEN + 1);
    strlcat(newfake.host, ".fakeusers.quakenet.org", HOSTLEN + 1);
  } else
    strlcpy(newfake.host, cargv[2], HOSTLEN + 1);

  if (cargc < 2)
    strlcpy(newfake.ident, cargv[0], USERLEN + 1);
  else
    strlcpy(newfake.ident, cargv[1], USERLEN + 1);

  fake = fake_add(&newfake);

  if (!fake) {
    return CMD_ERROR;
  }

  nofudb->squery(nofudb, "INSERT INTO ? (nick, ident, host, realname) VALUES (?,?,?,?)", "Tssss", "fakeusers", fake->nick, fake->ident, fake->host, fake->realname);
  controlreply(sender, "Added fake user %s", fake->nick);
  controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) added by %s/%s", fake->nick, fake->ident,
              fake->host, fake->realname, ((nick *)sender)->nick, ((nick *)sender)->authname);

  scheduleoneshot(time(NULL) + 1, schedulefakeuser, fake);
  return CMD_OK;
}

int fakelist(void *sender, int cargc, char **cargv) {
  fakeuser *fake;
  int fakeusercount = 0;

  for (fake = fakeuserlist; fake; fake = fake->next) {
    if (!fake->user)
      controlreply(sender, "%s!%s@%s (%s) - RECONNECTING", fake->nick, fake->ident,
                   fake->host, fake->realname);
    else
      controlreply(sender, "%s!%s@%s (%s)", fake->nick, fake->ident,
                   fake->host, fake->realname);

    fakeusercount++;
  }

  controlreply(sender, "%d fakeusers are currently connected", fakeusercount);
  return CMD_OK;
}

int fakekill(void *sender, int cargc, char **cargv) {
  fakeuser *fake;

  if (cargc == 0)
    return CMD_USAGE;

  fake = findfakeuserbynick(cargv[0]);

  if (!fake) {
    controlreply(sender, "No Fake User with nick %s found", cargv[0]);
    return CMD_ERROR;
  }

  nofudb->squery(nofudb, "DELETE FROM ? WHERE nick = ?", "Ts", "fakeusers", fake->nick);
  controlreply(sender, "Killed fake user %s", fake->nick);
  controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) removed by %s/%s", fake->nick, fake->ident,
              fake->host, fake->realname, ((nick *)sender)->nick, ((nick *)sender)->authname);

  fake_remove(cargv[0]);
  return CMD_OK;
}

void schedulefakeuser(void *arg) {
  fakeuser *fake;

  for (fake = fakeuserlist; fake; fake = fake->next)
    if (!fake->user)
      fake->user = register_fakeuseronnet(fake);
}

fakeuser *findfakeuserbynick(char *nick) {
  fakeuser *fake;

  for (fake = fakeuserlist; fake; fake = fake->next)
    if (!ircd_strcmp(nick, fake->nick))
      return fake;

  return NULL;
}

void _init() {
  if (!fakeuser_loaddb()) {
    Error("fakeuser", ERR_FATAL, "Cannot load database");
    return;
  }

  registercontrolhelpcmd("fakeuser", NO_OPER, 4, &fakeadd, "Usage: FAKEUSER nick <ident> <host> <realname>\nCreates a fake user.");
  registercontrolhelpcmd("fakelist", NO_OPER, 0, &fakelist, "Usage: FAKELIST\nLists all fake users.");
  registercontrolhelpcmd("fakekill", NO_OPER, 2, &fakekill, "Usage: FAKEKILL nick\nRemoves a fake user");
}

void _fini() {
  fakeuser_cleanup();
  deleteallschedules(schedulefakeuser);
  deregistercontrolcmd("fakeuser", &fakeadd);
  deregistercontrolcmd("fakelist", &fakelist);
  deregistercontrolcmd("fakekill", &fakekill);
}
