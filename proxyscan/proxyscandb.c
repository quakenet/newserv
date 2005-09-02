
/*
 * proxyscandb:
 *  Handles the database interface routines for proxyscan.
 */

#include "proxyscan.h"

#include <mysql/mysql.h>
#include "../core/config.h"
#include "../lib/sstring.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "../localuser/localuser.h"
#include <string.h>
#include <stdio.h>

MYSQL proxyscansql;
unsigned int lastid;
int sqlconnected;

/*
 * proxyscandbinit():
 *  Connects to the database and recovers the last gline ID
 */

int proxyscandbinit() {
  sstring *dbhost,*dbusername,*dbpassword,*dbdatabase,*dbport;
  MYSQL_RES *myres;
  MYSQL_ROW myrow;
 
  sqlconnected=0;

  dbhost=getcopyconfigitem("proxyscan","dbhost","localhost",HOSTLEN);
  dbusername=getcopyconfigitem("proxyscan","dbusername","proxyscan",20);
  dbpassword=getcopyconfigitem("proxyscan","dbpassword","moo",20);
  dbdatabase=getcopyconfigitem("proxyscan","dbdatabase","proxyscan",20);
  dbport=getcopyconfigitem("proxyscan","dbport","3306",8);
  
  mysql_init(&proxyscansql);
  if (!mysql_real_connect(&proxyscansql,dbhost->content,dbusername->content,
			  dbpassword->content,dbdatabase->content,
			  strtol(dbport->content,NULL,10), NULL, 0)) {
    Error("proxyscan",ERR_ERROR,"Unable to connect to database");
    return 1;
  }

  freesstring(dbhost);
  freesstring(dbusername);
  freesstring(dbpassword);
  freesstring(dbdatabase);
  freesstring(dbport);
 
  sqlconnected=1;

  /* Set up the table */
  mysql_query(&proxyscansql,"CREATE TABLE openproxies (ID BIGINT not null, IP VARCHAR (20) not null, PM INT not null, TS DATETIME not null, RH TEXT not null, PRIMARY KEY (ID), INDEX (ID), UNIQUE (ID))");

  /* Get max ID */
  if ((mysql_query(&proxyscansql,"SELECT max(ID) FROM openproxies"))!=0) {
    Error("proxyscan",ERR_ERROR,"Unable to retrieve max ID from database");
    return 1;
  }
    
  myres=mysql_store_result(&proxyscansql);
  if (mysql_num_fields(myres)!=1) {
    Error("proxyscan",ERR_ERROR,"Weird format retrieving max ID");
    mysql_free_result(myres);
    return 1;
  }

  lastid=0;
  if ((myrow=mysql_fetch_row(myres))) {
    if (myrow[0]==NULL) {
      lastid=0; 
    } else {
      lastid=strtol(myrow[0],NULL,10);
    }
    Error("proxyscan",ERR_INFO,"Retrieved lastid %d from database.",lastid);
  }
 
  mysql_free_result(myres);
  return 0;
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
  char reasonesc[100];
  char sqlquery[4000];
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

    mysql_escape_string(reasonesc,reasonlist,strlen(reasonlist));
    sprintf(sqlquery,"INSERT INTO openproxies VALUES(%u,'%s',%d,NOW(),'%s')",chp->glineid,
	    IPtostr(chp->IP),reasonmask,reasonesc);
    mysql_query(&proxyscansql,sqlquery);
  } else {
    mysql_escape_string(reasonesc,reasonlist,strlen(reasonlist));
    sprintf(sqlquery,"UPDATE openproxies SET PM=%d,RH='%s' where ID=%u",
	    reasonmask,reasonesc,chp->glineid);
    mysql_query(&proxyscansql,sqlquery);
  }
}

/*
 * proxyscandbclose:
 *  Closes the db socket when proxyscan is unloaded 
 */

void proxyscandbclose() {
  if (sqlconnected==1) {
    mysql_close(&proxyscansql);
  }
}

/*
 * proxyscandolistopen:
 *  Lists all the open proxies found since <since> to user usernick.
 */

void proxyscandolistopen(nick *mynick, nick *usernick, time_t snce) {
  char mysqlquery[2000];
  char timestmp[30];
  MYSQL_RES *myres;
  MYSQL_ROW myrow;

  strftime(timestmp,30,"%Y-%m-%d %H:%M:%S",localtime(&snce));
  sprintf(mysqlquery,"SELECT IP,TS,RH FROM openproxies WHERE TS>'%s' ORDER BY TS",timestmp);

  if ((mysql_query(&proxyscansql,mysqlquery))!=0) {
    sendnoticetouser(mynick,usernick,"Error performing database query!");
    Error("proxyscan",ERR_ERROR,"Error performing listopen query");
    return;
  }

  myres=mysql_use_result(&proxyscansql);
  if (mysql_num_fields(myres)!=3) {
    sendnoticetouser(mynick,usernick,"Error performing database query!");
    Error("proxyscan",ERR_ERROR,"Error performing listopen query");
    return;
  }

  sendnoticetouser(mynick,usernick,"%-20s %-22s %s","IP","Found at","What was open");
  while ((myrow=mysql_fetch_row(myres))) {
    sendnoticetouser(mynick,usernick,"%-20s %-22s %s",myrow[0],myrow[1],myrow[2]);
  }
  sendnoticetouser(mynick,usernick,"--- End of list ---");
  mysql_free_result(myres);
}
