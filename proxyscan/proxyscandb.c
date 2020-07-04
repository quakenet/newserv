
/*
 * proxyscandb:
 *  Handles the database interface routines for proxyscan.
 */

#include "proxyscan.h"

#include "../dbapi/dbapi.h"
#include "../core/config.h"
#include "../lib/sstring.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "../localuser/localuser.h"
#include <string.h>
#include <stdio.h>

unsigned int lastid;
int sqlconnected = 0;
extern nick *proxyscannick;

void proxyscan_get_last_id(DBConn *dbconn, void *arg);

/*
 * proxyscandbinit():
 *  Connects to the database and recovers the last gline ID
 */

int proxyscandbinit() {
  if(!dbconnected())
    return 1;

  sqlconnected=1;

  /* Set up the table */
  dbcreatequery("CREATE TABLE openproxies ("
                "ID int8 not null,"
		"IP inet not null,"
		"PM int4 not null,"
		"TS int4 not null," 
		"RH varchar not null,"
		"PRIMARY KEY (ID))");

  dbcreatequery("CREATE INDEX openproxies_id_index ON openproxies (ID)");

  dbasyncquery(proxyscan_get_last_id, NULL,
      "SELECT ID FROM openproxies ORDER BY id DESC LIMIT 1");

  return 0;
}

void proxyscan_get_last_id(DBConn *dbconn, void *arg) {
  DBResult *pgres = dbgetresult(dbconn);

  if(!dbquerysuccessful(pgres)) {
    Error("proxyscan", ERR_STOP, "Error loading last id.");
    return;
  }

  if (dbfetchrow(pgres)) 
    lastid = atoi(dbgetvalue(pgres, 0));
  else 
    lastid = 0;

  dbclear(pgres);
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

    case STYPE_DIRECT_IRC:
      reason="fwdirc";
      break;

    case STYPE_ROUTER:
      reason="router";
      break;

    case STYPE_EXT:
      reason="ext";
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

void loggline(cachehost *chp, patricia_node_t *node) {
  char reasonlist[200];
  char reasonesc[400 + 1]; /* reasonlist*2+1 */
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
    if ((reasonpos + 20) > sizeof(reasonlist))
      break;

    reasonpos += sprintf(reasonlist+reasonpos, "%s:%d ",scantostr(fpp->type), fpp->port);
  }

  if (chp->glineid==0) {
    chp->glineid=++lastid;

    dbescapestring(reasonesc,reasonlist,strlen(reasonlist));
    dbquery("INSERT INTO openproxies VALUES(%u,'%s',%d,%ld,'%s')",chp->glineid,
	    IPtostr(((patricia_node_t *)node)->prefix->sin),reasonmask,getnettime(),reasonesc);
  } else {
    dbescapestring(reasonesc,reasonlist,strlen(reasonlist));
    dbquery("UPDATE openproxies SET PM=%d,RH='%s' where ID=%u",
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

void proxyscandolistopen_real(DBConn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  DBResult *pgres;

  pgres=dbgetresult(dbconn);
  if (!dbquerysuccessful(pgres)) {
    Error("proxyscan", ERR_ERROR, "Error loading data.");
    return;
  }
  
  if (dbnumfields(pgres) != 3) {
    Error("proxyscan", ERR_ERROR, "data format error.");
    dbclear(pgres);
    return;
  }

  if (!np) {
    dbclear(pgres);
    return;
  }

  sendnoticetouser(proxyscannick,np,"%-20s %-22s %s","IP","Found at","What was open");
  while(dbfetchrow(pgres)) {
    sendnoticetouser(proxyscannick,np, "%-20s %-22s %s",dbgetvalue(pgres, 0),
                                                        dbgetvalue(pgres, 1),
							dbgetvalue(pgres, 2));
  }
  dbclear(pgres);
  sendnoticetouser(proxyscannick,np,"--- End of list ---");
}

int proxyscandolistopen(void *sender, int cargc, char **cargv) {
  nick *usernick = (nick *)sender;
   
  dbasyncquery(proxyscandolistopen_real,(void *)usernick->numeric, 
               "SELECT IP,TS,RH FROM openproxies WHERE TS>'%lu' ORDER BY TS",time(NULL)-rescaninterval);
  return CMD_OK;
}

/*
 * proxyscanspewip
 *  Check db for open proxies matching the given IP, send to user usernick.
 */

void proxyscanspewip_real(DBConn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  DBResult *pgres;
  char timebuf[30];

  pgres=dbgetresult(dbconn);
  if (!dbquerysuccessful(pgres)) {
    Error("proxyscan", ERR_ERROR, "Error loading data.");
    return;
  }

  if (dbnumfields(pgres) != 4) {
    Error("proxyscan", ERR_ERROR, "data format error.");
    dbclear(pgres);
    return;
  }

  if (!np) {
    dbclear(pgres);
    return;
  }

  sendnoticetouser(proxyscannick,np,"%-5s %-20s %-22s %s","ID","IP","Found at","What was open");
  while(dbfetchrow(pgres)) {
    time_t t = strtoul(dbgetvalue(pgres, 2), NULL, 10);
    strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M GMT", gmtime(&t));

    sendnoticetouser(proxyscannick,np, "%-5s %-20s %-22s %s",dbgetvalue(pgres, 0),
                                                             dbgetvalue(pgres, 1),
                                                             timebuf,
							     dbgetvalue(pgres, 3));
  }
  dbclear(pgres);
  sendnoticetouser(proxyscannick,np,"--- End of list ---");
}

void proxyscanspewip(nick *mynick, nick *usernick, unsigned long a, unsigned long b, unsigned long c, unsigned long d) {
  dbasyncquery(proxyscanspewip_real,(void *)usernick->numeric,
               "SELECT ID,IP,TS,RH FROM openproxies WHERE IP='%lu.%lu.%lu.%lu' ORDER BY TS DESC LIMIT 10",a,b,c,d);

}

/*
 * proxyscanshowkill
 *  Check db for open proxies matching the given kill/gline ID, send to user usernick.
 */

void proxyscanshowkill_real(DBConn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned long)arg);
  DBResult *pgres;

  pgres=dbgetresult(dbconn);
  if (!dbquerysuccessful(pgres)) {
    Error("proxyscan", ERR_ERROR, "Error loading data.");
    return;
  }

  if (dbnumfields(pgres) != 4) {
    Error("proxyscan", ERR_ERROR, "data format error.");
    dbclear(pgres);
    return;
  }

  if (!np) {
    dbclear(pgres);
    return;
  }

  sendnoticetouser(proxyscannick,np,"%-5s %-20s %-22s %s","ID","IP","Found at","What was open");
  while(dbfetchrow(pgres)) {
    sendnoticetouser(proxyscannick,np, "%-5s %-20s %-22s %s",dbgetvalue(pgres, 0),
                                                             dbgetvalue(pgres, 1),
                                                             dbgetvalue(pgres, 2),
							     dbgetvalue(pgres, 3));
  }
  dbclear(pgres);
  sendnoticetouser(proxyscannick,np,"--- End of list ---");
}

void proxyscanshowkill(nick *mynick, nick *usernick, unsigned long a) {
  dbasyncquery(proxyscanspewip_real,(void *)usernick->numeric,
               "SELECT ID,IP,TS,RH FROM openproxies WHERE ID='%lu'",a);
}
