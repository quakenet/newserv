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
#include "../dbapi/dbapi.h"

#include "noperserv.h"
#include "noperserv_db.h"

#include <stdlib.h>

int db_loaded = 0;
unsigned long loadedusers = 0;

unsigned long lastuserid;

no_autheduser *authedusers = NULL;

void noperserv_create_tables(void);

void noperserv_free_user(no_autheduser *au);
void noperserv_load_users(DBConn *dbconn, void *arg);

void noperserv_check_nick(nick *np);
void noperserv_nick_account(int hooknum, void *arg);
void noperserv_quit_account(int hooknum, void *arg);

void nopserserv_delete_from_autheduser(nick *np, no_autheduser *au);

int noperserv_load_db(void) {
  if(!dbconnected()) {
    Error("noperserv", ERR_STOP, "Could not connect to database.");
    return 0;
  }

  if(db_loaded)
    noperserv_cleanup_db();

  db_loaded = 1;

  authedusers = NULL;

  noperserv_create_tables();

  dbasyncquery(noperserv_load_users, NULL,
    "SELECT ID, authname, flags, noticelevel FROM noperserv.users");

  return 1;
}

void noperserv_load_users(DBConn *dbconn, void *arg) {
  DBResult *pgres = dbgetresult(dbconn);
  no_autheduser *nu;
  nick *np;
  int i;

  if(!dbquerysuccessful(pgres)) {
    Error("noperserv", ERR_ERROR, "Error loading user list.");
    dbclear(pgres);
    return; 
  }

  lastuserid = 0;

  while(dbfetchrow(pgres)) {
    nu = noperserv_new_autheduser(dbgetvalue(pgres, 1));
    if(!nu)
      continue;

    nu->id = strtoul(dbgetvalue(pgres, 0), NULL, 10);
    nu->authlevel = strtoul(dbgetvalue(pgres, 2), NULL, 10);
    nu->noticelevel = strtoul(dbgetvalue(pgres, 3), NULL, 10);
    nu->newuser = 0;
    if(nu->id > lastuserid)
      lastuserid = nu->id;
  }

  Error("noperserv", ERR_INFO, "Loaded %d users", loadedusers);
  
  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      if(IsAccount(np))
        noperserv_check_nick(np);

  registerhook(HOOK_NICK_ACCOUNT, &noperserv_nick_account);
  registerhook(HOOK_NICK_NEWNICK, &noperserv_nick_account);
  registerhook(HOOK_NICK_LOSTNICK, &noperserv_quit_account);

  dbclear(pgres);
}

void noperserv_create_tables(void) {
  dbattach("noperserv");
  dbcreatequery(
    "CREATE TABLE noperserv.users ("
      "ID            INT               NOT NULL,"
      "authname      VARCHAR(%d)       NOT NULL,"
      "flags         INT               NOT NULL,"
      "noticelevel   INT               NOT NULL,"
      "PRIMARY KEY (ID))", ACCOUNTLEN);
}

void noperserv_cleanup_db(void) {
  no_autheduser *ap, *np;

  deregisterhook(HOOK_NICK_LOSTNICK, &noperserv_quit_account);
  deregisterhook(HOOK_NICK_NEWNICK, &noperserv_nick_account);
  deregisterhook(HOOK_NICK_ACCOUNT, &noperserv_nick_account);

  ap = authedusers;
  while(ap) {
    np = ap->next;
    noperserv_free_user(ap);
    ap = np;
  }

  dbdetach("noperserv");
}

no_autheduser *noperserv_new_autheduser(char *authname) {
  no_autheduser *au = (no_autheduser *)malloc(sizeof(no_autheduser));
  if(!au)
    return NULL;

  au->authname = getsstring(authname, ACCOUNTLEN);
  if(!au->authname) {
    free(au);
    return NULL;
  }

  loadedusers++;
  au->newuser = 1;
  au->nick = NULL;

  au->next = authedusers;
  authedusers = au;

  return au;
}

void noperserv_delete_autheduser(no_autheduser *au) {
  no_autheduser *ap = authedusers, *lp = NULL;

  if(!au->newuser)
    dbquery("DELETE FROM noperserv.users WHERE id = %d", au->id);

  for(;ap;lp=ap,ap=ap->next) {
    if(ap == au) {
      if(lp) {
        lp->next = ap->next;
      } else {
        authedusers = ap->next;
      }
      noperserv_free_user(ap);
      return;
    }
  }
}

void noperserv_update_autheduser(no_autheduser *au) {
  if(au->newuser) {
    char escapedauthname[ACCOUNTLEN * 2 + 1];
    dbescapestring(escapedauthname, au->authname->content, au->authname->length);
    dbquery("INSERT INTO noperserv.users (id, authname, flags, noticelevel) VALUES (%lu,'%s',%u,%u)", au->id, au->authname->content, NOGetAuthLevel(au), NOGetNoticeLevel(au));
    au->newuser = 0;
  } else {
    dbquery("UPDATE noperserv.users SET flags = %u, noticelevel = %u WHERE id = %lu", NOGetAuthLevel(au), NOGetNoticeLevel(au), au->id);
  }
}

void noperserv_free_user(no_autheduser *au) {
  no_nicklist *cp = au->nick, *np;

  while(cp) {
    cp->nick->exts[noperserv_ext] = NULL;
    np = cp->next;
    free(cp);
    cp = np;
  }

  freesstring(au->authname);
  free(au);

  loadedusers--;
}

void noperserv_check_nick(nick *np) {
  no_autheduser *au = noperserv_get_autheduser(np->authname);
  if(au)
    noperserv_add_to_autheduser(np, au);
}

void noperserv_nick_account(int hooknum, void *arg) {
  noperserv_check_nick((nick *)arg);
}

void noperserv_quit_account(int hooknum, void *arg) {
  nick *np = (void *)arg;
  no_autheduser *au = NOGetAuthedUser(np);
  no_nicklist *nl, *ln = NULL;
  if(!au)
    return;

  for(nl=au->nick;nl;ln=nl,nl=nl->next)
    if(nl->nick == np) {
      if(ln) {
        ln->next = nl->next;
      } else {
        au->nick = nl->next;
      }
      free(nl);
      break;
    }
}

no_autheduser *noperserv_get_autheduser(char *authname) {
  no_autheduser *au = authedusers;

  for(;au;au=au->next)
    if(!ircd_strcmp(authname, au->authname->content))
      return au;

  return NULL;
}

unsigned long noperserv_get_autheduser_count(void) {
  return loadedusers;
}

unsigned long noperserv_next_autheduser_id(void) {
  return ++lastuserid;
}

void noperserv_add_to_autheduser(nick *np, no_autheduser *au) {
  no_nicklist *nl = (no_nicklist *)malloc(sizeof(no_nicklist));
  if(!nl)
    return;

  np->exts[noperserv_ext] = au;

  nl->nick = np;

  nl->next = au->nick;
  au->nick = nl;
}

void nopserserv_delete_from_autheduser(nick *np, no_autheduser *au) {
  no_nicklist *cp = au->nick, *lp = NULL;

  for(;cp;lp=cp,cp=cp->next)
    if(cp->nick == np) {
      if(lp) {
        lp->next = cp->next;
      } else {
        au->nick = cp->next;
      }
      free(cp);
      break;
    }
}
