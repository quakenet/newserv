/* 
 * The interface to the actual database
 */

#include "authtracker.h"
#include "../../dbapi/dbapi.h"
#include "../../nick/nick.h"
#include "../../core/error.h"

#include <string.h>
#include <stdlib.h>
 
void at_logquit(unsigned long userid, time_t accountts, time_t when, char *reason) {
  char lreason[100], escreason[205];
  strncpy(lreason,reason,99);
  lreason[99]='\0';

  dbescapestring(escreason, lreason, strlen(lreason));

  dbquery("UPDATE chanserv.authhistory SET disconnecttime=%lu, quitreason='%s' WHERE userID=%lu AND authtime=%lu",
              when, escreason, userid, accountts);
}

void at_lognewsession(unsigned int userid, nick *np) {
  char escnick[NICKLEN*2+1];
  char escuser[USERLEN*2+1];
  char eschost[HOSTLEN*2+1];

  dbescapestring(escnick, np->nick, strlen(np->nick));
  dbescapestring(escuser, np->ident, strlen(np->ident));
  dbescapestring(eschost, np->host->name->content, np->host->name->length);

  dbquery("INSERT INTO chanserv.authhistory (userID, nick, username, host, authtime, disconnecttime, numeric) "
    "VALUES (%u, '%s', '%s', '%s', %lu, %lu, %lu)",
    userid, escnick, escuser, eschost, np->accountts, 0UL, np->numeric);
}

static void real_at_finddanglingsessions(DBConn *dbconn, void *arg) {
  DBResult *pgres;

  if(!dbconn)
    return;

  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading dangling sessions.");
    return;
  }

  if (dbnumfields(pgres)!=3) {
    Error("authtracker",ERR_ERROR,"Dangling sessions format error");
    dbclear(pgres);
    return;
  }
  
  while(dbfetchrow(pgres)) {
    at_lostnick(
      strtoul(dbgetvalue(pgres,0),NULL,10),  /* numeric */
      strtoul(dbgetvalue(pgres,1),NULL,10),  /* userID */
      strtoul(dbgetvalue(pgres,2),NULL,10),  /* authtime */
      time(NULL),
      AT_RESTART
    );
  }
  
  dbclear(pgres);  

  at_dbloaded(0, NULL);  
}

void at_finddanglingsessions() {
  dbasyncqueryi(authtrackerdb, real_at_finddanglingsessions, NULL, 
	"SELECT numeric,userID,authtime FROM chanserv.authhistory WHERE disconnecttime=0");

}
