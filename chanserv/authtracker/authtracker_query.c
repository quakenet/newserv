/* 
 * The interface to the actual database
 */

#include "authtracker.h"
#include "../../pqsql/pqsql.h"
#include "../../nick/nick.h"
#include "../../core/error.h"

#include <libpq-fe.h>
#include <string.h>
#include <stdlib.h>
 
void at_logquit(unsigned long userid, time_t accountts, time_t when, char *reason) {
  char lreason[100], escreason[205];
  strncpy(lreason,reason,99);
  lreason[99]='\0';

  PQescapeString(escreason, lreason, strlen(lreason));

  pqquery("UPDATE authhistory SET disconnecttime=%lu, quitreason='%s' WHERE userID=%lu AND authtime=%lu",
              when, escreason, userid, accountts);
}

void at_lognewsession(unsigned int userid, nick *np) {
  char escnick[NICKLEN*2+1];
  char escuser[USERLEN*2+1];
  char eschost[HOSTLEN*2+1];

  PQescapeString(escnick, np->nick, strlen(np->nick));
  PQescapeString(escuser, np->ident, strlen(np->ident));
  PQescapeString(eschost, np->host->name->content, np->host->name->length);

  pqquery("INSERT INTO authhistory (userID, nick, username, host, authtime, disconnecttime, numeric) "
    "VALUES (%lu, '%s', '%s', '%s', %lu, %lu, %u)",
    userid, escnick, escuser, eschost, np->accountts, 0, np->numeric);
}

static void real_at_finddanglingsessions(PGconn *dbconn, void *arg) {
  PGresult *pgres;
  unsigned int i,num;
  
  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading dangling sessions.");
    return;
  }

  if (PQnfields(pgres)!=3) {
    Error("authtracker",ERR_ERROR,"Dangling sessions format error");
    return;
  }
  
  num=PQntuples(pgres);
  
  for (i=0;i<num;i++) {
    at_lostnick(
      strtoul(PQgetvalue(pgres,i,0),NULL,10),  /* numeric */
      strtoul(PQgetvalue(pgres,i,1),NULL,10),  /* userID */
      strtoul(PQgetvalue(pgres,i,2),NULL,10),  /* authtime */
      time(NULL),
      AT_RESTART
    );
  }
  
  PQclear(pgres);  

  at_dbloaded(0, NULL);  
}

void at_finddanglingsessions() {
  pqasyncquery(real_at_finddanglingsessions, NULL, 
	"SELECT numeric,userID,authtime FROM authhistory WHERE disconnecttime=0");

}
