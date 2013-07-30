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

static fakeuser *fakeuserlist;
static DBAPIConn *nofudb;

static void reconnectfakeuser(void *arg);

static fakeuser *findfakeuserbynick(char *nick) {
  fakeuser *fake;

  for (fake = fakeuserlist; fake; fake = fake->next)
    if (!ircd_strcmp(nick, fake->nick))
      return fake;

  return NULL;
}

static void fake_free(fakeuser *fake) {
  fakeuser **pnext;

  if (fake->user)
    deregisterlocaluser(fake->user, "Signing off");

  deleteschedule(NULL, &reconnectfakeuser, fake);
  
  for (pnext = &fakeuserlist; *pnext; pnext = &((*pnext)->next)) {
    if (*pnext == fake) {
      *pnext = fake->next;
      break;
    }
  }

  free(fake);
}

static void fake_remove(fakeuser *fake) {
  nofudb->squery(nofudb, "DELETE FROM ? WHERE nick = ?", "Ts", "fakeusers", fake->nick);
  
  fake_free(fake);
}

static void fakeuser_handler(nick *user, int command, void **params) {
  if (command == LU_KILLED) {
    fakeuser *fake;
    time_t timenow = time(NULL);

    fake = findfakeuserbynick(user->nick);

    if (!fake) {
      controlwall(NO_OPER, NL_FAKEUSERS, "Error: A fakeuser was killed, but wasn't found in the list");
      Error("fakeuser", ERR_ERROR, "A fakeuser was killed, but wasn't found in the list");
      return;
    }

    fake->user = NULL;

    if (timenow - fake->lastkill < KILL_TIME) {
      controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) KILL'ed twice under in %d seconds. Removing.",
                  fake->nick, fake->ident, fake->host, fake->realname, KILL_TIME);
      fake_remove(fake);
      return;
    }

    fake->lastkill = timenow;

    scheduleoneshot(time(NULL) + KILL_WAIT, &reconnectfakeuser, fake);
  }
}

static void reconnectfakeuser(void *arg) {
  fakeuser *fake = arg;
  nick *user;
  
  if (fake->user)
    return;

  if ((user = getnickbynick(fake->nick)) && (IsOper(user) || IsService(user) || IsXOper(user))) {
    fake_remove(fake);
    return;
  }

  fake->user = registerlocaluser(fake->nick, fake->ident, fake->host, fake->realname,
                                 NULL, UMODE_INV | UMODE_DEAF, &fakeuser_handler);
}

static fakeuser *fake_add(const char *nick, const char *ident, const char *host, const char *realname) {
  fakeuser *fake;

  fake = malloc(sizeof(fakeuser));

  if (!fake)
    return NULL;

  strlcpy(fake->nick, nick, NICKLEN + 1);
  strlcpy(fake->ident, ident, USERLEN + 1);
  strlcpy(fake->host, host, HOSTLEN + 1);
  strlcpy(fake->realname, realname, REALLEN + 1);

  fake->user = NULL;
  fake->lastkill = 0;

  fake->next = fakeuserlist;
  fakeuserlist = fake;

  scheduleoneshot(time(NULL) + 1, reconnectfakeuser, fake);
  
  return fake;
}

static fakeuser *fake_create(const char *nick, const char *ident, const char *host, const char *realname) {
  fakeuser *fake;
  
  fake = fake_add(nick, ident, host, realname);

  if (!fake)
    return NULL;
  
  nofudb->squery(nofudb, "INSERT INTO ? (nick, ident, host, realname) VALUES (?,?,?,?)", "Tssss", "fakeusers",
                 fake->nick, fake->ident, fake->host, fake->realname);

  return fake;
}

static void fakeusers_load(const DBAPIResult *res, void *arg) {
  if (!res)
    return;

  if (!res->success) {
    Error("fakeuser", ERR_ERROR, "Error loading fakeuser list.");
    res->clear(res);
    return;
  }

  while (res->next(res))
    fake_add(res->get(res, 0), res->get(res, 1), res->get(res, 2), res->get(res, 3));

  res->clear(res);
}

static int fakeuser_loaddb() {
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

static int fakeadd(void *source, int cargc, char **cargv) {
  nick *sender = source;
  fakeuser *fake;
  char *nick, *ident, *realname;
  char host[HOSTLEN + 1];

  if (cargc == 0)
    return CMD_USAGE;

  fake = findfakeuserbynick(cargv[0]);

  if (fake) {
    controlreply(sender, "Fake User with nick %s already found", cargv[0]);
    return CMD_ERROR;
  }

  nick = cargv[0];

  if (cargc < 4)
    realname = cargv[0];
  else
    realname = cargv[3];

  if (cargc < 3) {
    strlcpy(host, cargv[0], NICKLEN + 1);
    strlcat(host, ".fakeusers.quakenet.org", HOSTLEN + 1);
  } else
    strlcpy(host, cargv[2], HOSTLEN + 1);

  if (cargc < 2)
    ident = cargv[0];
  else
    ident = cargv[1];

  fake = fake_create(nick, ident, host, realname);

  if (!fake)
    return CMD_ERROR;

  controlreply(sender, "Added fake user %s", fake->nick);
  controlwall(NO_OPER, NL_FAKEUSERS, "%s added fake user: %s!%s@%s (%s)", controlid(sender),
              fake->nick, fake->ident, fake->host, fake->realname);

  scheduleoneshot(time(NULL) + 1, &reconnectfakeuser, fake);

  return CMD_OK;
}

static int fakelist(void *sender, int cargc, char **cargv) {
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

static int fakekill(void *sender, int cargc, char **cargv) {
  fakeuser *fake;

  if (cargc == 0)
    return CMD_USAGE;

  fake = findfakeuserbynick(cargv[0]);

  if (!fake) {
    controlreply(sender, "No Fake User with nick %s found", cargv[0]);
    return CMD_ERROR;
  }

  controlreply(sender, "Killed fake user %s", fake->nick);
  controlwall(NO_OPER, NL_FAKEUSERS, "Fake user %s!%s@%s (%s) removed by %s/%s", fake->nick, fake->ident,
              fake->host, fake->realname, ((nick *)sender)->nick, ((nick *)sender)->authname);

  fake_remove(fake);

  return CMD_OK;
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
  fakeuser *fake, *next;

  for (fake = fakeuserlist; fake; fake = next) {
    next = fake->next;
    fake_free(fake);
  }

  deregistercontrolcmd("fakeuser", &fakeadd);
  deregistercontrolcmd("fakelist", &fakelist);
  deregistercontrolcmd("fakekill", &fakekill);
}
