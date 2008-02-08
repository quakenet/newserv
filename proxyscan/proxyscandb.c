
/*
 * proxyscandb:
 *  Handles the database interface routines for proxyscan.
 */

#include "proxyscan.h"

#include "../pqsql/pqsql.h"
#include "../core/config.h"
#include "../lib/sstring.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "../localuser/localuser.h"
#include <string.h>
#include <stdio.h>
#include <libpq-fe.h>

unsigned int lastid;
int sqlconnected = 0;
extern nick *proxyscannick;

void proxyscan_get_last_id(PGconn *dbconn, void *arg);

/*
 * proxyscandbinit():
 *  Connects to the database and recovers the last gline ID
 */

int proxyscandbinit() {
  if(!pqconnected())
    return 1;

  sqlconnected=1;

  /* Set up the table */
  pqcreatequery("CREATE TABLE openproxies ("
                "ID int8 not null,"
		"IP inet not null,"
		"PM int4 not null,"
		"TS int4 not null," 
		"RH varchar not null,"
		"PRIMARY KEY (ID))");

  pqcreatequery("CREATE INDEX openproxies_id_index ON openproxies (ID)");

  pqasyncquery(proxyscan_get_last_id, NULL,
      "SELECT ID FROM openproxies ORDER BY id DESC LIMIT 1");

  return 0;
}

void proxyscan_get_last_id(PGconn *dbconn, void *arg) {
  PGresult *pgres = PQgetResult(dbconn);
  unsigned int numrows;

  if(PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("proxyscan", ERR_ERROR, "Error loading last id.");
  }

  numrows = PQntuples(pgres);
  if ( numrows )
    lastid = atoi(PQgetvalue(pgres, 0, 0));
  else 
    lastid = 0;

  PQclear(pgres);
   Error("proxyscan",ERR_INFO,"Retrieved lastid %d from database.",lastid);
}
/*
 * scantostr:
 *  Given a scan number, returns the string associated.
 */

const char *scantostr(int type) {
  char *reason="UNKNOWN";

  switch (type) {
    case STYPE_SOCKS4:
      reason="socks4";
      break;
      
    case STYPE_SOCKS5:
      reason="socks5";
      break;
      
    case STYPE_HTTP:
      reason="http";
      break;
      
    case STYPE_WINGATE:
      reason="wingate";
      break;
      
    case STYPE_CISCO:
      reason="router";
      break;
      
    case STYPE_DIRECT:
      reason="forward";
      break;
  }
   
  return reason;
}

/*
 * scantobm:
 *  Given a scan number, returns the bitmask associated (as used in the
 *  previous version of P)
 */

int scantodm(int scannum) {
  return (1<<scannum);
}

/*
 * loggline:
 *  This function takes the given scanhost (which is assumed to have
 *  at least one "OPEN" scan) and logs it to the database.  It returns
 *  the unique ID assigned to this gline (for the gline message itself).
 */

void loggline(cachehost *chp) {
  char reasonlist[100];
  char reasonesc[200 + 1]; /* reasonlist*2+1 */
  int reasonmask=0;
  int reasonpos=0;
  foundproxy *fpp;

  if (brokendb) {
    chp->glineid=1;
    return;
  }

  reasonlist[0]='\0';
  reasonmask=0;
  for (fpp=chp->proxies;fpp;fpp=fpp->next) {
    reasonpos += sprintf(reasonlist+reasonpos, "%s:%d ",scantostr(fpp->type), fpp->port);
  }

  if (chp->glineid==0) {
    chp->glineid=++lastid;

    PQescapeString(reasonesc,reasonlist,strlen(reasonlist));
    pqquery("INSERT INTO openproxies VALUES(%u,'%s',%d,%ld,'%s')",chp->glineid,
	    IPlongtostr(chp->IP),reasonmask,getnettime(),reasonesc);
  } else {
    PQescapeString(reasonesc,reasonlist,strlen(reasonlist));
    pqquery("UPDATE openproxies SET PM=%d,RH='%s' where ID=%u",
	    reasonmask,reasonesc,chp->glineid);
  }
}

/*
 * proxyscandbclose:
 *  Closes the db socket when proxyscan is unloaded 
 */

void proxyscandbclose() {
}

/*
 * proxyscandolistopen:
 *  Lists all the open proxies found since <since> to user usernick.
 */

void proxyscandolistopen_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  PGresult *pgres;
  int i, num;

  pgres=PQgetResult(dbconn);
  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("proxyscan", ERR_ERROR, "Error loading data.");
    return;
  }
  
  if (PQnfields(pgres) != 3) {
    Error("proxyscan", ERR_ERROR, "data format error.");
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    return;
  }

  sendnoticetouser(proxyscannick,np,"%-20s %-22s %s","IP","Found at","What was open");
  for (i=0; i<num; i++) {
    sendnoticetouser(proxyscannick,np, "%-20s %-22s %s",PQgetvalue(pgres, i, 0),
                                                        PQgetvalue(pgres, i, 1),
							PQgetvalue(pgres, i, 2));
  }
  sendnoticetouser(proxyscannick,np,"--- End of list ---");
}

void proxyscandolistopen(nick *mynick, nick *usernick, time_t snce) {

  pqasyncquery(proxyscandolistopen_real,(void *)usernick->numeric, 
               "SELECT IP,TS,RH FROM openproxies WHERE TS>'%lu' ORDER BY TS",snce);
}

/*
 * proxyscanspewip
 *  Check db for open proxies matching the given IP, send to user usernick.
 */

void proxyscanspewip_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  PGresult *pgres;
  int i, num;

  pgres=PQgetResult(dbconn);
  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("proxyscan", ERR_ERROR, "Error loading data.");
    return;
  }

  if (PQnfields(pgres) != 4) {
    Error("proxyscan", ERR_ERROR, "data format error.");
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    return;
  }

  sendnoticetouser(proxyscannick,np,"%-5s %-20s %-22s %s","ID","IP","Found at","What was open");
  for (i=0; i<num; i++) {
    sendnoticetouser(proxyscannick,np, "%-5s %-20s %-22s %s",PQgetvalue(pgres, i, 0),
                                                             PQgetvalue(pgres, i, 1),
                                                             PQgetvalue(pgres, i, 2),
							     PQgetvalue(pgres, i, 3));
  }
  sendnoticetouser(proxyscannick,np,"--- End of list ---");
}

void proxyscanspewip(nick *mynick, nick *usernick, unsigned long a, unsigned long b, unsigned long c, unsigned long d) {
  pqasyncquery(proxyscanspewip_real,(void *)usernick->numeric,
               "SELECT ID,IP,TS,RH FROM openproxies WHERE IP='%lu.%lu.%lu.%lu' ORDER BY TS DESC LIMIT 10",a,b,c,d);

}

/*
 * proxyscanshowkill
 *  Check db for open proxies matching the given kill/gline ID, send to user usernick.
 */

void proxyscanshowkill_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  PGresult *pgres;
  int i, num;

  pgres=PQgetResult(dbconn);
  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("proxyscan", ERR_ERROR, "Error loading data.");
    return;
  }

  if (PQnfields(pgres) != 4) {
    Error("proxyscan", ERR_ERROR, "data format error.");
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    return;
  }

  sendnoticetouser(proxyscannick,np,"%-5s %-20s %-22s %s","ID","IP","Found at","What was open");
  for (i=0; i<num; i++) {
    sendnoticetouser(proxyscannick,np, "%-5s %-20s %-22s %s",PQgetvalue(pgres, i, 0),
                                                             PQgetvalue(pgres, i, 1),
                                                             PQgetvalue(pgres, i, 2),
							     PQgetvalue(pgres, i, 3));
  }
  sendnoticetouser(proxyscannick,np,"--- End of list ---");
}


void proxyscanshowkill(nick *mynick, nick *usernick, unsigned long a) {
  pqasyncquery(proxyscanspewip_real,(void *)usernick->numeric,
               "SELECT ID,IP,TS,RH FROM openproxies WHERE ID='%lu'",a);
}
