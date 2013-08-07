/* 
 * NOperserv v0.01
 *
 * A replacement for Germania's ageing Operservice2
 * DB functions
 *
 * Copyright (C) 2005 Chris Porter.
 */

#include "../nick/nick.h"
#include "../core/error.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"
#include "../dbapi2/dbapi2.h"

#include "control.h"

#include <stdlib.h>

int db_loaded = 0;
unsigned long loadedusers = 0;

void noperserv_create_tables(void);

void noperserv_free_user(no_autheduser *au);
void noperserv_load_users(const DBAPIResult *res, void *arg);

static DBAPIConn *nodb;

int noperserv_load_db(void) {
  if(!nodb) {
    nodb = dbapi2open(DBAPI2_DEFAULT, "noperserv");
    if(!nodb) {
      Error("noperserv", ERR_STOP, "Could not connect to database.");
      return 0;
    }
  }

  db_loaded = 1;

  noperserv_create_tables();

  nodb->query(nodb, noperserv_load_users, NULL,
    "SELECT userid, authname, flags, noticelevel FROM ?", "T", "users");

  return 1;
}

void noperserv_load_users(const DBAPIResult *res, void *arg) {
  no_autheduser *nu;

  if(!res)
    Error("control", ERR_STOP, "Failed to load noperserv database. Your database might be corrupted or the schema is incompatible.");

  if(!res->success) {
    Error("noperserv", ERR_ERROR, "Error loading user list.");
    res->clear(res);
    return;
  }

  while(res->next(res)) {
    nu = noperserv_new_autheduser(strtoul(res->get(res, 0), NULL, 10), res->get(res, 1));
    if(!nu)
      continue;

    nu->authlevel = strtoul(res->get(res, 2), NULL, 10);
    nu->noticelevel = strtoul(res->get(res, 3), NULL, 10);
    nu->newuser = 0;
  }

  Error("noperserv", ERR_INFO, "Loaded %lu users", loadedusers);
  
  res->clear(res);
}

void noperserv_create_tables(void) {
  nodb->createtable(nodb, NULL, NULL,
    "CREATE TABLE ? ("
      "userid        INT               NOT NULL,"
      "authname      VARCHAR(?)        NOT NULL,"
      "flags         INT               NOT NULL,"
      "noticelevel   INT               NOT NULL,"
      "PRIMARY KEY (userid))", "Td", "users", ACCOUNTLEN);
}

void noperserv_cleanup_db(void) {
  int i;
  authname *anp, *next;
  no_autheduser *au;

  for (i=0;i<AUTHNAMEHASHSIZE;i++) {
    for (anp=authnametable[i];anp;) {
      next = anp->next;

      au = anp->exts[noperserv_ext];
      if(au)
        noperserv_free_user(au);

      anp = next;
    }
  }

  nodb->close(nodb);
  nodb = NULL;
}

no_autheduser *noperserv_new_autheduser(unsigned long userid, char *name) {
  authname *anp;
  no_autheduser *au;

  anp = findorcreateauthname(userid, name);
  if(!anp)
    return NULL;

  au = malloc(sizeof(no_autheduser));
  if(!au)
    return NULL;

  au->authname = anp;

  loadedusers++;
  au->newuser = 1;

  anp->exts[noperserv_ext] = au;

  return au;
}

void noperserv_delete_autheduser(no_autheduser *au) {
  if(!au->newuser)
    nodb->squery(nodb, "DELETE FROM ? WHERE userid = ?", "Tu", "users", au->authname->userid);

  noperserv_free_user(au);
}

void noperserv_update_autheduser(no_autheduser *au) {
  if(au->newuser) {
    nodb->squery(nodb, "INSERT INTO ? (userid, authname, flags, noticelevel) VALUES (?,?,?,?)", "Tusuu", "users", au->authname->userid, au->authname->name, NOGetAuthLevel(au), NOGetNoticeLevel(au));
    au->newuser = 0;
  } else {
    nodb->squery(nodb, "UPDATE ? SET flags = ?, noticelevel = ? WHERE userid = ?", "Tuuu", "users", NOGetAuthLevel(au), NOGetNoticeLevel(au), au->authname->userid);
  }
}

void noperserv_free_user(no_autheduser *au) {
  authname *anp = au->authname;
  anp->exts[noperserv_ext] = NULL;
  releaseauthname(anp);
  free(au);

  loadedusers--;
}

no_autheduser *noperserv_get_autheduser(authname *anp) {
  if (!anp)
    return NULL;

  return anp->exts[noperserv_ext];
}

unsigned long noperserv_get_autheduser_count(void) {
  return loadedusers;
}

