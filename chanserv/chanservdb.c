/*
 * chanservdb.c:
 *  Handles the SQL stuff for the channel service.
 */

#include "chanserv.h"
#include <libpq-fe.h>
#include <string.h>
#include "../core/config.h"
#include "../lib/sstring.h"
#include "../parser/parser.h"
#include "../core/events.h"
#include <stdio.h>
#include <sys/poll.h>
#include <stdarg.h>

typedef void (*QueryHandler)(void *);

typedef struct qquery {
  char          *query;    /* Text of query */
  QueryHandler   handler;  /* Handler function */
  void          *udata;    /* Other data associated with query */
  struct qquery *next;     /* Next (linked list) */  
} qquery;

struct helpinfo {
  unsigned int numeric;
  sstring *commandname;
};

typedef struct tabledesc {
  sstring *tablename; /* Name of table */
  QueryHandler init; /* Function to be called when transaction opens */
  QueryHandler data; /* Function to be called to load data */
  QueryHandler fini; /* Function to be called to clean up */
} tabledesc;

qquery *nextquery, *lastquery;

PGconn *dbconn;

regchan **allchans;
reguser **allusers;

int cs_sqlconnected;
unsigned int lastchannelID;
unsigned int lastuserID;
unsigned int lastbanID;

/* Local prototypes */
void loadallchanbans(void *arg);
void csdb_handler(int fd, short revents);
void csdb_queuequery(QueryHandler handler, void *udata, char *format, ...);
void csdb_handlestats(int hooknum, void *arg);
void loadmessages_part1(void *arg);
void loadmessages_part2(void *arg);
void loadcommandsummary_real(void *arg);
void csdb_dohelp_real(void *arg);

/* Generic loading functions */
void loadall(char *, QueryHandler, QueryHandler, QueryHandler);
void doloadall(void *arg);  

/* User loading functions */
void loadsomeusers(void *arg);
void loadusersdone(void *arg);

/* Channel loading functions */
void loadsomechannels(void *arg);
void loadchannelsdone(void *arg);

/* Chanuser loading functions */
void loadchanusersinit(void *arg);
void loadsomechanusers(void *arg);
void loadchanusersdone(void *arg);

/* Chanban loading functions */
void loadsomechanbans(void *arg);
void loadchanbansdone(void *arg);

int chanservdbinit() {
  sstring *dbhost,*dbusername,*dbpassword,*dbdatabase,*dbport;
  char connectstr[1000];
 
  cs_sqlconnected=0;
  
  registerhook(HOOK_CORE_STATSREQUEST, csdb_handlestats);

  dbhost=getcopyconfigitem("chanserv","dbhost","localhost",HOSTLEN);
  dbusername=getcopyconfigitem("chanserv","dbusername","chanserv",20);
  dbpassword=getcopyconfigitem("chanserv","dbpassword","moo",20);
  dbdatabase=getcopyconfigitem("chanserv","dbdatabase","chanserv",20);
  dbport=getcopyconfigitem("chanserv","dbport","3306",8);
  sprintf(connectstr,"dbname=%s user=%s password=%s",
/* 	  dbhost->content, dbport->content, */
	  dbdatabase->content, dbusername->content, dbpassword->content);
  
  Error("chanserv",ERR_INFO,"Attempting database connection: %s",connectstr);

  /* Blocking connect for now.. */
  dbconn=PQconnectdb(connectstr);
  
  if (!dbconn || (PQstatus(dbconn)!=CONNECTION_OK)) {
    Error("chanserv",ERR_ERROR,"Unable to connect to database!");
    return 1;
  }    

  freesstring(dbhost);
  freesstring(dbusername);
  freesstring(dbpassword);
  freesstring(dbdatabase);
  freesstring(dbport);
 
  cs_sqlconnected=1;

  /* Set up the tables */
  /* User table */
  PQclear(PQexec(dbconn,"CREATE TABLE users ("
		 "ID            INT               NOT NULL,"
		 "username      VARCHAR(16)          NOT NULL,"
		 "created       INT               NOT NULL,"
		 "lastauth      INT               NOT NULL,"
		 "lastemailchng INT               NOT NULL,"
		 "flags         INT               NOT NULL,"
		 "language      INT               NOT NULL,"
		 "suspendby     INT               NOT NULL,"
		 "suspendexp    INT               NOT NULL,"
		 "password      VARCHAR(11)          NOT NULL,"
		 "masterpass    VARCHAR(11),"
		 "email         VARCHAR(100),"
		 "lastuserhost  VARCHAR(75),"
		 "suspendreason VARCHAR(250),"
		 "comment       VARCHAR(250),"
		 "info          VARCHAR(100),"
		 "PRIMARY KEY (ID))"));

  PQclear(PQexec(dbconn,"CREATE INDEX user_username_index ON users (username)"));
  
  /* Channel table */
  PQclear(PQexec(dbconn,"CREATE TABLE channels ("
		 "ID            INT               NOT NULL,"
		 "name          VARCHAR(250)         NOT NULL,"
		 "flags         INT               NOT NULL,"
		 "forcemodes    INT               NOT NULL,"
		 "denymodes     INT               NOT NULL,"
		 "chanlimit     INT               NOT NULL,"
		 "autolimit     INT               NOT NULL,"
		 "banstyle      INT               NOT NULL,"
		 "created       INT               NOT NULL,"
		 "lastactive    INT               NOT NULL,"
		 "statsreset    INT               NOT NULL,"
		 "banduration   INT               NOT NULL,"
		 "founder       INT               NOT NULL,"
		 "addedby       INT               NOT NULL,"
		 "suspendby     INT               NOT NULL,"
		 "chantype      SMALLINT          NOT NULL,"
		 "totaljoins    INT               NOT NULL,"
		 "tripjoins     INT               NOT NULL,"
		 "maxusers      INT               NOT NULL,"
		 "tripusers     INT               NOT NULL,"
		 "welcome       VARCHAR(500),"
		 "topic         VARCHAR(250),"
		 "chankey       VARCHAR(23),"
		 "suspendreason VARCHAR(250),"
		 "comment       VARCHAR(250),"
		 "lasttimestamp   INT,"
		 "PRIMARY KEY (ID))"));

  /* Chanuser table */
  PQclear(PQexec(dbconn,"CREATE TABLE chanusers ("
		 "userID        INT               NOT NULL,"
		 "channelID     INT               NOT NULL,"
		 "flags         INT               NOT NULL,"
		 "changetime    INT               NOT NULL,"
		 "usetime       INT               NOT NULL,"
		 "info          VARCHAR(100)         NOT NULL,"
		 "PRIMARY KEY (userID, channelID))"));

  PQclear(PQexec(dbconn,"CREATE INDEX chanusers_userID_index on chanusers (userID)"));
  PQclear(PQexec(dbconn,"CREATE INDEX chanusers_channelID_index on chanusers (channelID)"));
  
  PQclear(PQexec(dbconn,"CREATE TABLE bans ("
		 "banID         INT               NOT NULL," /* Unique number for the ban to make 
                                                             DELETEs process in finite time.. */
		 "channelID     INT               NOT NULL,"
		 "userID        INT               NOT NULL," /* Who set the ban.. */
		 "hostmask      VARCHAR(100)         NOT NULL," /* needs to be at least USERLEN+NICKLEN+HOSTLEN+2 */
		 "expiry        INT               NOT NULL,"
		 "reason        VARCHAR(200),"
		 "PRIMARY KEY(banID))"));

  PQclear(PQexec(dbconn,"CREATE INDEX bans_channelID_index on bans (channelID)"));
  
  PQclear(PQexec(dbconn,"CREATE TABLE languages ("
		 "languageID    INT               NOT NULL,"
		 "code          VARCHAR(2)           NOT NULL,"
		 "name          VARCHAR(30)          NOT NULL)"));
  
  PQclear(PQexec(dbconn,"CREATE TABLE messages ("
		 "languageID    INT               NOT NULL,"
		 "messageID     INT               NOT NULL,"
		 "message       VARCHAR(250)      NOT NULL,"
		 "PRIMARY KEY (languageID, messageID))"));

  PQclear(PQexec(dbconn,"CREATE TABLE help ("
		 "commandID     INT               NOT NULL,"
		 "command       VARCHAR(30)          NOT NULL,"
		 "languageID    INT               NOT NULL,"
		 "summary       VARCHAR(200)      NOT NULL,"
		 "fullinfo      TEXT              NOT NULL,"
		 "PRIMARY KEY (commandID, languageID))"));

  PQclear(PQexec(dbconn,"CREATE TABLE email ("
		 "userID       INT               NOT NULL,"
		 "emailtype    INT               NOT NULL,"
		 "prevEmail    VARCHAR(100),"
		 "mailID       VARCHAR(128))"));

  PQsetnonblocking(dbconn, 1);

  registerhandler(PQsocket(dbconn), POLLIN, csdb_handler);

  lastuserID=lastchannelID=0;

  loadall("users",NULL,loadsomeusers,loadusersdone);
  loadall("channels",NULL,loadsomechannels,loadchannelsdone);
  loadall("chanusers",loadchanusersinit,loadsomechanusers,loadchanusersdone);
  loadall("bans",NULL,loadsomechanbans,loadchanbansdone);

  loadmessages(); 

  return 0;
}

void csdb_handler(int fd, short revents) {
  PGresult *res;
  qquery *qqp;

  if (revents & POLLIN) {
    PQconsumeInput(dbconn);
    
    if (!PQisBusy(dbconn)) {
      /* Query is complete */
      if (nextquery->handler)
	(nextquery->handler)(nextquery->udata);
      
      while ((res=PQgetResult(dbconn))) {
	switch (PQresultStatus(res)) {
	case PGRES_TUPLES_OK:
	  Error("chanserv",ERR_WARNING,"Unhandled tuples output (query=%s)",nextquery->query);
	  break;
	  
	case PGRES_NONFATAL_ERROR:
	case PGRES_FATAL_ERROR:
	  Error("chanserv",ERR_WARNING,"Unhandled error response (query=%s)",nextquery->query);
	  break;
	  
	default:
	  break;
	}
	
	PQclear(res);
      }

      /* Free the query and advance */
      qqp=nextquery;
      if (nextquery==lastquery) {
	lastquery=NULL;
      }
      nextquery=nextquery->next;
      free(qqp->query);
      free(qqp);

      if (nextquery) {
	/* Submit the next query */
	PQsendQuery(dbconn, nextquery->query);
	PQflush(dbconn);
      }
    }
  }
}

void csdb_queuequery(QueryHandler handler, void *udata, char *format, ...) {
  char querybuf[8192];
  va_list va;
  int len;
  qquery *qp;

  va_start(va, format);
  len=vsnprintf(querybuf, 8191, format, va);
  va_end(va);

  qp=(qquery *)malloc(sizeof(qquery));
  qp->query=malloc(len+1);

  strcpy(qp->query, querybuf);
  qp->udata=udata;
  qp->handler=handler;
  qp->next=NULL;

  if (lastquery) {
    lastquery->next=qp;
    lastquery=qp;
  } else {
    lastquery=nextquery=qp;
    PQsendQuery(dbconn, nextquery->query);
    PQflush(dbconn);
  }
}

void csdb_handlestats(int hooknum, void *arg) {
  int level=(int)arg;
  int i=0;
  qquery *qqp;
  char message[100];
  
  if (level>10) {
    if (nextquery)
      for (qqp=nextquery;qqp;qqp=qqp->next)
        i++;
     
    sprintf(message,"ChanServ: %6d database queries queued.",i);
    
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }
}

void chanservdbclose() {
  qquery *qqp, *nqqp;

  if (cs_sqlconnected) {
    deregisterhandler(PQsocket(dbconn), 0);
  }

  /* Throw all the queued queries away.. */
  for (qqp=nextquery;qqp;qqp=nqqp) {
    nqqp=qqp->next;
    if (qqp->handler == csdb_dohelp_real) {
      free(qqp->udata);
    }
    free(qqp->query);
    free(qqp);
  }

  deregisterhook(HOOK_CORE_STATSREQUEST, csdb_handlestats);
  PQfinish(dbconn);
}

/*
 * loadall():
 *  Generic function to handle load of an entire table..
 */

void loadall(char *table, QueryHandler init, QueryHandler data, QueryHandler fini) {
  tabledesc *thedesc;
  
  thedesc=malloc(sizeof(tabledesc));

  thedesc->tablename=getsstring(table,100);
  thedesc->init=init;
  thedesc->data=data;
  thedesc->fini=fini;

  csdb_queuequery(doloadall, thedesc, "SELECT count(*) FROM %s",thedesc->tablename->content);
}

void doloadall(void *arg) {
  PGresult *pgres;
  int i,count;
  tabledesc *thedesc=arg;

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error getting row count for %s.",thedesc->tablename->content);
    return;
  }

  if (PQnfields(pgres)!=1) {
    Error("chanserv",ERR_ERROR,"Count query format error for %s.",thedesc->tablename->content);
    return;
  }

  count=strtoul(PQgetvalue(pgres,0,0),NULL,10);

  PQclear(pgres);

  Error("chanserv",ERR_INFO,"Found %d entries in table %s, scheduling load.",count,
	thedesc->tablename->content);
  
  csdb_queuequery(thedesc->init, NULL, "BEGIN");
  csdb_queuequery(NULL, NULL, "DECLARE mycurs CURSOR FOR SELECT * from %s",
		  thedesc->tablename->content);

  for (i=0;(count-i)>1000;i+=1000) {
    csdb_queuequery(thedesc->data, NULL, "FETCH 1000 FROM mycurs");
  }

  csdb_queuequery(thedesc->data, NULL, "FETCH ALL FROM mycurs");
  
  csdb_queuequery(NULL, NULL, "CLOSE mycurs");
  csdb_queuequery(thedesc->fini, NULL, "COMMIT");

  /* Free structures.. */
  freesstring(thedesc->tablename);
  free(thedesc);
}

/*
 * loadsomeusers():
 *  Loads some users in from the SQL DB
 */

void loadsomeusers(void *arg) {
  PGresult *pgres;
  reguser *rup;
  unsigned int i,num;

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading user DB");
    return;
  }

  if (PQnfields(pgres)!=16) {
    Error("chanserv",ERR_ERROR,"User DB format error");
    return;
  }

  num=PQntuples(pgres);

  for(i=0;i<num;i++) {
    rup=getreguser();
    rup->status=0;
    rup->ID=strtoul(PQgetvalue(pgres,i,0),NULL,10);
    strncpy(rup->username,PQgetvalue(pgres,i,1),NICKLEN); rup->username[NICKLEN]='\0';
    rup->created=strtoul(PQgetvalue(pgres,i,2),NULL,10);
    rup->lastauth=strtoul(PQgetvalue(pgres,i,3),NULL,10);
    rup->lastemailchange=strtoul(PQgetvalue(pgres,i,4),NULL,10);
    rup->flags=strtoul(PQgetvalue(pgres,i,5),NULL,10);
    rup->languageid=strtoul(PQgetvalue(pgres,i,6),NULL,10);
    rup->suspendby=strtoul(PQgetvalue(pgres,i,7),NULL,10);
    rup->suspendexp=strtoul(PQgetvalue(pgres,i,8),NULL,10);
    strncpy(rup->password,PQgetvalue(pgres,i,9),PASSLEN); rup->password[PASSLEN]='\0';
    strncpy(rup->masterpass,PQgetvalue(pgres,i,10),PASSLEN); rup->masterpass[PASSLEN]='\0';
    rup->email=getsstring(PQgetvalue(pgres,i,11),100);
    rup->lastuserhost=getsstring(PQgetvalue(pgres,i,12),75);
    rup->suspendreason=getsstring(PQgetvalue(pgres,i,13),250);
    rup->comment=getsstring(PQgetvalue(pgres,i,14),250);
    rup->info=getsstring(PQgetvalue(pgres,i,15),100);
    rup->knownon=NULL;
    rup->nicks=NULL;
    rup->checkshd=NULL;
    rup->stealcount=0;
    rup->fakeuser=NULL;
    addregusertohash(rup);

    if (rup->ID > lastuserID) {
      lastuserID=rup->ID;
    }
  }

  PQclear(pgres);
}

void loadusersdone(void *arg) {
  Error("chanserv",ERR_INFO,"Load users done (highest ID was %d)",lastuserID);
}

/*
 * Channel loading functions
 */

void loadsomechannels(void *arg) {
  PGresult *pgres;
  regchan *rcp;
  int i,num;
  chanindex *cip;
  time_t now=time(NULL);

  pgres=PQgetResult(dbconn);
   
  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading channel DB");
    return;
  }

  if (PQnfields(pgres)!=26) {
    Error("chanserv",ERR_ERROR,"Channel DB format error");
    return;
  }

  num=PQntuples(pgres);
  
  for(i=0;i<num;i++) {
    cip=findorcreatechanindex(PQgetvalue(pgres,i,1));
    if (cip->exts[chanservext]) {
      Error("chanserv",ERR_WARNING,"%s in database twice - this WILL cause problems later.",cip->name->content);
      continue;
    }
    rcp=getregchan();
    cip->exts[chanservext]=rcp;
    
    rcp->ID=strtoul(PQgetvalue(pgres,i,0),NULL,10);
    rcp->index=cip;
    rcp->flags=strtoul(PQgetvalue(pgres,i,2),NULL,10);
    rcp->status=0; /* Non-DB field */
    rcp->lastbancheck=0;
    rcp->lastcountersync=now;
    rcp->lastpart=0;
    rcp->bans=NULL;
    rcp->forcemodes=strtoul(PQgetvalue(pgres,i,3),NULL,10);
    rcp->denymodes=strtoul(PQgetvalue(pgres,i,4),NULL,10);
    rcp->limit=strtoul(PQgetvalue(pgres,i,5),NULL,10);
    rcp->autolimit=strtoul(PQgetvalue(pgres,i,6),NULL,10);
    rcp->banstyle=strtoul(PQgetvalue(pgres,i,7),NULL,10);
    rcp->created=strtoul(PQgetvalue(pgres,i,8),NULL,10);
    rcp->lastactive=strtoul(PQgetvalue(pgres,i,9),NULL,10);
    rcp->statsreset=strtoul(PQgetvalue(pgres,i,10),NULL,10);
    rcp->banduration=strtoul(PQgetvalue(pgres,i,11),NULL,10);
    rcp->founder=strtol(PQgetvalue(pgres,i,12),NULL,10);
    rcp->addedby=strtol(PQgetvalue(pgres,i,13),NULL,10);
    rcp->suspendby=strtol(PQgetvalue(pgres,i,14),NULL,10);
    rcp->chantype=strtoul(PQgetvalue(pgres,i,15),NULL,10);
    rcp->totaljoins=strtoul(PQgetvalue(pgres,i,16),NULL,10);
    rcp->tripjoins=strtoul(PQgetvalue(pgres,i,17),NULL,10);
    rcp->maxusers=strtoul(PQgetvalue(pgres,i,18),NULL,10);
    rcp->tripusers=strtoul(PQgetvalue(pgres,i,19),NULL,10);
    rcp->welcome=getsstring(PQgetvalue(pgres,i,20),500);
    rcp->topic=getsstring(PQgetvalue(pgres,i,21),TOPICLEN);
    rcp->key=getsstring(PQgetvalue(pgres,i,22),KEYLEN);
    rcp->suspendreason=getsstring(PQgetvalue(pgres,i,23),250);
    rcp->comment=getsstring(PQgetvalue(pgres,i,24),250);
    rcp->checksched=NULL;
    rcp->ltimestamp=strtoul(PQgetvalue(pgres,i,25),NULL,10);
    memset(rcp->regusers,0,REGCHANUSERHASHSIZE*sizeof(reguser *));

    if (rcp->ID > lastchannelID)
      lastchannelID=rcp->ID;
    
    if (CIsAutoLimit(rcp))
      rcp->limit=0;
  }

  PQclear(pgres);
}

void loadchannelsdone(void *arg) {
  Error("chanserv",ERR_INFO,"Channel load done (highest ID was %d)",lastchannelID);
}

void loadchanusersinit(void *arg) {
  int i;
  chanindex *cip;
  reguser *rup;
  regchan *rcp;

  allchans=(regchan **)malloc((lastchannelID+1)*sizeof(regchan *));
  memset(allchans,0,(lastchannelID+1)*sizeof(regchan *));
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if (cip->exts[chanservext]) {
        rcp=(regchan *)cip->exts[chanservext];
        allchans[rcp->ID]=rcp;
      }
    }
  }
  
  allusers=(reguser **)malloc((lastuserID+1)*sizeof(reguser *));
  memset(allusers,0,(lastuserID+1)*sizeof(reguser *));
  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) {
      allusers[rup->ID]=rup;
    }
  }
}

void loadsomechanusers(void *arg) {
  PGresult *pgres;
  regchanuser *rcup;
  int i,num;
  regchan *rcp;
  reguser *rup;
  int uid,cid;
  int total=0;

  /* Set up the allchans and allusers arrays */
  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading chanusers.");
    return; 
  }

  if (PQnfields(pgres)!=6) {
    Error("chanserv",ERR_ERROR,"Chanusers format error");
    return;
  }

  num=PQntuples(pgres);
  
  for(i=0;i<num;i++) {
    uid=strtol(PQgetvalue(pgres,i,0),NULL,10);
    cid=strtol(PQgetvalue(pgres,i,1),NULL,10);

    if (uid>lastuserID || !(rup=allusers[uid])) {
      Error("chanserv",ERR_WARNING,"Skipping channeluser for unknown user %d",uid);
      continue;
    }

    if (cid>lastchannelID || !(rcp=allchans[cid])) {
      Error("chanserv",ERR_WARNING,"Skipping channeluser for unknown chan %d",cid);
      continue;
    }
    
    if (rup==NULL || rcp==NULL) {
      Error("chanserv",ERR_ERROR,"Can't add user %s on channel %s",
	    PQgetvalue(pgres,i,0),PQgetvalue(pgres,i,1));
    } else {
      rcup=getregchanuser();
      rcup->user=rup;
      rcup->chan=rcp;
      rcup->flags=strtol(PQgetvalue(pgres,i,2),NULL,10);
      rcup->changetime=strtol(PQgetvalue(pgres,i,3),NULL,10);
      rcup->usetime=strtol(PQgetvalue(pgres,i,4),NULL,10);
      rcup->info=getsstring(PQgetvalue(pgres,i,5),100);
      addregusertochannel(rcup);
      total++;
    }
  }

  PQclear(pgres);
}

void loadchanusersdone(void *arg) {
  Error("chanserv",ERR_INFO,"Channel user load done.");
}

void loadsomechanbans(void *arg) {
  PGresult *pgres;
  regban  *rbp;
  int i,num;
  regchan *rcp;
  int uid,cid,bid;
  time_t expiry,now;
  int total=0;

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading bans.");
    return; 
  }

  if (PQnfields(pgres)!=6) {
    Error("chanserv",ERR_ERROR,"Ban format error");
    return;
  }

  num=PQntuples(pgres);

  now=time(NULL);

  for(i=0;i<num;i++) {
    bid=strtoul(PQgetvalue(pgres,i,0),NULL,10);
    cid=strtoul(PQgetvalue(pgres,i,1),NULL,10);
    uid=strtoul(PQgetvalue(pgres,i,2),NULL,10);
    expiry=strtoul(PQgetvalue(pgres,i,4),NULL,10);

    if (cid>lastchannelID || !(rcp=allchans[cid])) {
      Error("chanserv",ERR_WARNING,"Skipping ban for unknown chan %d",cid);
      continue;
    }

    rbp=getregban();
    rbp->setby=uid;
    rbp->ID=bid;
    rbp->expiry=expiry;
    rbp->reason=getsstring(PQgetvalue(pgres,i,5),200);
    rbp->cbp=makeban(PQgetvalue(pgres,i,3));
    rbp->next=rcp->bans;
    rcp->bans=rbp;
   
    total++;

    if (bid>lastbanID)
      lastbanID=bid;
  }

  PQclear(pgres);
}

void loadchanbansdone(void *arg) {
  free(allusers);
  free(allchans);
  
  Error("chanserv",ERR_INFO,"Channel ban load done.");

  triggerhook(HOOK_CHANSERV_DBLOADED, NULL);
}

void loadmessages() {
  csdb_queuequery(loadmessages_part1, NULL, "SELECT * from languages");
}

void loadmessages_part1(void *arg) {
  int i,j;
  PGresult *pgres;
  int num;
      
  /* Firstly, clear up any stale messages */
  for (i=0;i<MAXLANG;i++) {
    if (cslanguages[i]) {
      freesstring(cslanguages[i]->name);
      free(cslanguages[i]);
    }
    for (j=0;j<MAXMESSAGES;j++) {
      if (csmessages[i][j]) {
        freesstring(csmessages[i][j]);
        csmessages[i][j]=NULL;
      }
    }
  }    

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading language list.");
    return; 
  }

  if (PQnfields(pgres)!=3) {
    Error("chanserv",ERR_ERROR,"Language list format error.");
    return;
  }

  num=PQntuples(pgres);

  for (i=0;i<num;i++) {
    j=strtol(PQgetvalue(pgres,i,0),NULL,10);
    if (j<MAXLANG && j>=0) {
      cslanguages[j]=(cslang *)malloc(sizeof(cslang));

      strncpy(cslanguages[j]->code,PQgetvalue(pgres,i,1),2); cslanguages[j]->code[2]='\0';
      cslanguages[j]->name=getsstring(PQgetvalue(pgres,i,2),30);
    }
  }
   
  PQclear(pgres);

  if (i>MAXLANG)
    Error("chanserv",ERR_ERROR,"Found too many languages (%d > %d)",i,MAXLANG); 

  csdb_queuequery(loadmessages_part2, NULL, "SELECT * from messages");
}

void loadmessages_part2(void *arg) {
  PGresult *pgres;
  int i,j,k,num;

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading message list.");
    return; 
  }

  if (PQnfields(pgres)!=3) {
    Error("chanserv",ERR_ERROR,"Message list format error.");
    return;
  }
  
  num=PQntuples(pgres);

  for (i=0;i<num;i++) {
    k=strtol(PQgetvalue(pgres,i,0),NULL,10);
    j=strtol(PQgetvalue(pgres,i,1),NULL,10);
    
    if (k<0 || k >= MAXLANG) {
      Error("chanserv",ERR_WARNING,"Language ID out of range on message: %d",k);
      continue;
    }
    
    if (j<0 || j >= MAXMESSAGES) {
      Error("chanserv",ERR_WARNING,"Message ID out of range on message: %d",j);
      continue;
    }
    
    csmessages[k][j]=getsstring(PQgetvalue(pgres,i,2),250);
  } 
                          
  PQclear(pgres);
}

void loadcommandsummary(Command *cmd) {
  csdb_queuequery(loadcommandsummary_real, (void *)cmd, 
		  "SELECT languageID,summary from help where lower(command) = lower('%s')",cmd->command->content);
}

void loadcommandsummary_real(void *arg) {
  int i,j,num;
  PGresult *pgres;
  cmdsummary *cs;
  Command *cmd=arg;

  /* Clear up old text first */
  cs=cmd->ext;
  for (i=0;i<MAXLANG;i++) {
    if (cs->bylang[i])
      freesstring(cs->bylang[i]);
  }
  
  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading command summary.");
    return; 
  }

  if (PQnfields(pgres)!=2) {
    Error("chanserv",ERR_ERROR,"Command summary format error.");
    return;
  }

  num=PQntuples(pgres);

  for (i=0;i<num;i++) {
    j=strtol(PQgetvalue(pgres,i,0),NULL,10);
    if (j<MAXLANG && j>=0) {
      cs->bylang[j]=getsstring(PQgetvalue(pgres,i,1),200);
    }
  }

  PQclear(pgres);
}

void csdb_updateauthinfo(reguser *rup) {
  char eschost[150];

  PQescapeString(eschost,rup->lastuserhost->content,rup->lastuserhost->length);
  csdb_queuequery(NULL, NULL, "UPDATE users SET lastauth=%lu,lastuserhost='%s' WHERE ID=%u",
		  rup->lastauth,eschost,rup->ID);
}

void csdb_updatelastjoin(regchanuser *rcup) {
  csdb_queuequery(NULL, NULL, "UPDATE chanusers SET usetime=%lu WHERE userID=%u and channelID=%u",
	  rcup->usetime, rcup->user->ID, rcup->chan->ID);
}

void csdb_updatetopic(regchan *rcp) {
  char esctopic[TOPICLEN*2+5];

  if (rcp->topic) {
    PQescapeString(esctopic,rcp->topic->content,rcp->topic->length);
  } else {
    esctopic[0]='\0';
  }
  csdb_queuequery(NULL, NULL, "UPDATE channels SET topic='%s' WHERE ID=%u",esctopic,rcp->ID);
}

void csdb_updatechannel(regchan *rcp) {
  char escwelcome[510];
  char esctopic[510];
  char esckey[70];
  char escreason[510];
  char esccomment[510];
  char escname[1000];

  PQescapeString(escname, rcp->index->name->content, rcp->index->name->length);

  if (rcp->welcome) 
    PQescapeString(escwelcome, rcp->welcome->content, 
			rcp->welcome->length);
  else
    escwelcome[0]='\0';

  if (rcp->topic)
    PQescapeString(esctopic, rcp->topic->content, rcp->topic->length);
  else
    esctopic[0]='\0';

  if (rcp->key)
    PQescapeString(esckey, rcp->key->content, rcp->key->length);
  else
    esckey[0]='\0';

  if (rcp->suspendreason) 
    PQescapeString(escreason, rcp->suspendreason->content, 
			rcp->suspendreason->length);
  else
    escreason[0]='\0';

  if (rcp->comment)
    PQescapeString(esccomment, rcp->comment->content, 
			rcp->comment->length);
  else
    esccomment[0]='\0';

  csdb_queuequery(NULL, NULL, "UPDATE channels SET name='%s', flags=%d, forcemodes=%d,"
		  "denymodes=%d, chanlimit=%d, autolimit=%d, banstyle=%d,"
		  "lastactive=%lu,statsreset=%lu, banduration=%lu, founder=%u,"
		  "addedby=%u, suspendby=%u, chantype=%d, totaljoins=%u,"
		  "tripjoins=%u, maxusers=%u, tripusers=%u,"
		  "welcome='%s', topic='%s', chankey='%s', suspendreason='%s',"
		  "comment='%s', lasttimestamp=%d WHERE ID=%u",escname,rcp->flags,rcp->forcemodes,
		  rcp->denymodes,rcp->limit,rcp->autolimit, rcp->banstyle,
		  rcp->lastactive,rcp->statsreset,rcp->banduration,
		  rcp->founder, rcp->addedby, rcp->suspendby,
		  rcp->chantype,rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  escwelcome,esctopic,esckey,escreason,esccomment,rcp->ltimestamp,rcp->ID);
}

void csdb_updatechannelcounters(regchan *rcp) {
  csdb_queuequery(NULL, NULL, "UPDATE channels SET "
		  "lastactive=%lu, totaljoins=%u,"
		  "tripjoins=%u, maxusers=%u, tripusers=%u "
		  "WHERE ID=%u",
		  rcp->lastactive,
		  rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  rcp->ID);
}

void csdb_updatechanneltimestamp(regchan *rcp) {
  csdb_queuequery(NULL, NULL, "UPDATE channels SET "
		  "lasttimestamp=%u WHERE ID=%u",
		  rcp->ltimestamp, rcp->ID);
}

void csdb_createchannel(regchan *rcp) {
  char escwelcome[510];
  char esctopic[510];
  char esckey[70];
  char escreason[510];
  char esccomment[510];
  char escname[510];

  PQescapeString(escname, rcp->index->name->content, rcp->index->name->length);

  if (rcp->welcome) 
    PQescapeString(escwelcome, rcp->welcome->content, 
			rcp->welcome->length);
  else
    escwelcome[0]='\0';

  if (rcp->topic)
    PQescapeString(esctopic, rcp->topic->content, rcp->topic->length);
  else
    esctopic[0]='\0';

  if (rcp->key)
    PQescapeString(esckey, rcp->key->content, rcp->key->length);
  else
    esckey[0]='\0';

  if (rcp->suspendreason) 
    PQescapeString(escreason, rcp->suspendreason->content, 
			rcp->suspendreason->length);
  else
    escreason[0]='\0';

  if (rcp->comment)
    PQescapeString(esccomment, rcp->comment->content, 
			rcp->comment->length);
  else
    esccomment[0]='\0';

  csdb_queuequery(NULL, NULL, "INSERT INTO channels (ID, name, flags, forcemodes, denymodes,"
		  "chanlimit, autolimit, banstyle, created, lastactive, statsreset, "
		  "banduration, founder, addedby, suspendby, chantype, totaljoins, tripjoins,"
		  "maxusers, tripusers, welcome, topic, chankey, suspendreason, "
		  "comment, lasttimestamp) VALUES (%u,'%s',%d,%d,%d,%d,%d,%d,%lu,%lu,%lu,%lu,%u,"
		  "%u,%u,%d,%u,%u,%u,%u,'%s','%s','%s','%s','%s',%d)",
		  rcp->ID, escname, rcp->flags,rcp->forcemodes,
		  rcp->denymodes,rcp->limit,rcp->autolimit, rcp->banstyle, rcp->created, 
		  rcp->lastactive,rcp->statsreset,rcp->banduration,
		  rcp->founder, rcp->addedby, rcp->suspendby,
		  rcp->chantype,rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  escwelcome,esctopic,esckey,escreason,esccomment,rcp->ltimestamp);
}

void csdb_deletechannel(regchan *rcp) {
  csdb_queuequery(NULL,NULL,"DELETE FROM channels WHERE ID=%u",rcp->ID);
  csdb_queuequery(NULL,NULL,"DELETE FROM chanusers WHERE channelID=%u",rcp->ID);
  csdb_queuequery(NULL,NULL,"DELETE FROM bans WHERE channelID=%u",rcp->ID);
}

void csdb_deleteuser(reguser *rup) {
  csdb_queuequery(NULL,NULL,"DELETE FROM users WHERE ID=%u",rup->ID);
  csdb_queuequery(NULL,NULL,"DELETE FROM chanusers WHERE userID=%u",rup->ID);
}

void csdb_updateuser(reguser *rup) {
  char escpassword[25];
  char escmasterpass[25];
  char escemail[210];
  char esclastuserhost[160];
  char escreason[510];
  char esccomment[510];
  char escinfo[210];
  
  PQescapeString(escpassword, rup->password, strlen(rup->password));
  PQescapeString(escmasterpass, rup->masterpass, strlen(rup->masterpass));
   
  if (rup->email)
    PQescapeString(escemail, rup->email->content, rup->email->length);
  else
    escemail[0]='\0';

  if (rup->lastuserhost)
    PQescapeString(esclastuserhost, rup->lastuserhost->content, rup->lastuserhost->length);
  else
    esclastuserhost[0]='\0';

  if (rup->suspendreason)
    PQescapeString(escreason, rup->suspendreason->content, rup->suspendreason->length);
  else
    escreason[0]='\0';

  if (rup->comment)
    PQescapeString(esccomment, rup->comment->content, rup->comment->length);
  else
    esccomment[0]='\0';

  if (rup->info)
    PQescapeString(escinfo, rup->info->content, rup->info->length);
  else
    escinfo[0]='\0';

  csdb_queuequery(NULL, NULL, "UPDATE users SET lastauth=%lu, lastemailchng=%lu, flags=%u,"
		  "language=%u, suspendby=%u, suspendexp=%lu, password='%s', masterpass='%s', email='%s',"
		  "lastuserhost='%s', suspendreason='%s', comment='%s', info='%s' WHERE ID=%u",
		  rup->lastauth, rup->lastemailchange, rup->flags, rup->languageid, rup->suspendby, rup->suspendexp,
		  escpassword, escmasterpass, escemail, esclastuserhost, escreason, esccomment, escinfo,
		  rup->ID);
}  

void csdb_createuser(reguser *rup) {
  char escpassword[25];
  char escmasterpass[25];
  char escemail[210];
  char esclastuserhost[160];
  char escreason[510];
  char esccomment[510];
  char escusername[35];
  char escinfo[210];

  PQescapeString(escusername, rup->username, strlen(rup->username));
  PQescapeString(escpassword, rup->password, strlen(rup->password));
  PQescapeString(escmasterpass, rup->masterpass, strlen(rup->masterpass));
  
  if (rup->email)
    PQescapeString(escemail, rup->email->content, rup->email->length);
  else
    escemail[0]='\0';

  if (rup->lastuserhost)
    PQescapeString(esclastuserhost, rup->lastuserhost->content, rup->lastuserhost->length);
  else
    esclastuserhost[0]='\0';

  if (rup->suspendreason)
    PQescapeString(escreason, rup->suspendreason->content, rup->suspendreason->length);
  else
    escreason[0]='\0';

  if (rup->comment)
    PQescapeString(esccomment, rup->comment->content, rup->comment->length);
  else
    esccomment[0]='\0';

  if (rup->info)
    PQescapeString(escinfo, rup->info->content, rup->info->length);
  else
    escinfo[0]='\0';

  csdb_queuequery(NULL, NULL, "INSERT INTO users (ID, username, created, lastauth, lastemailchng, "
		  "flags, language, suspendby, suspendexp, password, masterpass, email, lastuserhost, "
		  "suspendreason, comment, info) VALUES (%u,'%s',%lu,%lu,%lu,%u,%u,%u,%lu,'%s','%s',"
		  "'%s','%s','%s','%s','%s')",
		  rup->ID, escusername, rup->created, rup->lastauth, rup->lastemailchange, rup->flags, 
		  rup->languageid, rup->suspendby, rup->suspendexp,
		  escpassword, escmasterpass, escemail, esclastuserhost, escreason, esccomment, escinfo);
}  


void csdb_updatechanuser(regchanuser *rcup) {
  char escinfo[210];

  if (rcup->info) 
    PQescapeString(escinfo, rcup->info->content, rcup->info->length);
  else
    escinfo[0]='\0';

  csdb_queuequery(NULL, NULL, "UPDATE chanusers SET flags=%u, changetime=%lu, "
		  "usetime=%lu, info='%s' WHERE channelID=%u and userID=%u",
		  rcup->flags, rcup->changetime, rcup->usetime, escinfo, rcup->chan->ID,rcup->user->ID);
}

void csdb_createchanuser(regchanuser *rcup) {
  char escinfo[210];

  if (rcup->info) 
    PQescapeString(escinfo, rcup->info->content, rcup->info->length);
  else
    escinfo[0]='\0';
  
  csdb_queuequery(NULL, NULL, "INSERT INTO chanusers VALUES(%u, %u, %u, %lu, %lu, '%s')",
		  rcup->user->ID, rcup->chan->ID, rcup->flags, rcup->changetime, 
		  rcup->usetime, escinfo);
}

void csdb_deletechanuser(regchanuser *rcup) {
  csdb_queuequery(NULL, NULL, "DELETE FROM chanusers WHERE channelid=%u AND userID=%u",
		  rcup->chan->ID, rcup->user->ID);
}

void csdb_createban(regchan *rcp, regban *rbp) {
  char escreason[500];
  char banstr[100];
  char escban[200];

  strcpy(banstr,bantostring(rbp->cbp));
  PQescapeString(escban,banstr,strlen(banstr));
  
  if (rbp->reason)
    PQescapeString(escreason, rbp->reason->content, rbp->reason->length);
  else
    escreason[0]='\0';

  csdb_queuequery(NULL, NULL, "INSERT INTO bans (banID, channelID, userID, hostmask, "
		  "expiry, reason) VALUES (%u,%u,%u,'%s',%lu,'%s')", rbp->ID, rcp->ID, 
		  rbp->setby, escban, rbp->expiry, escreason);
}

void csdb_deleteban(regban *rbp) {
  csdb_queuequery(NULL, NULL, "DELETE FROM bans WHERE banID=%u", rbp->ID);
}

/* Help stuff */

void csdb_dohelp(nick *np, Command *cmd) {
  struct helpinfo *hip;

  hip=(struct helpinfo *)malloc(sizeof(struct helpinfo));

  hip->numeric=np->numeric;
  hip->commandname=getsstring(cmd->command->content, cmd->command->length);

  csdb_queuequery(csdb_dohelp_real, (void *)hip, 
		  "SELECT languageID, fullinfo from help where lower(command)=lower('%s')",cmd->command->content);
}

void csdb_dohelp_real(void *arg) {
  struct helpinfo *hip=arg;
  nick *np=getnickbynumeric(hip->numeric);
  reguser *rup;
  char *result;
  PGresult *pgres;
  int i,j,num,lang=0;
  
  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading help text.");
    return; 
  }

  if (PQnfields(pgres)!=2) {
    Error("chanserv",ERR_ERROR,"Help text format error.");
    return;
  }
  
  num=PQntuples(pgres);
  
  if (!np) {
    PQclear(pgres);
    freesstring(hip->commandname);
    free(hip);
    return;
  }

  if ((rup=getreguserfromnick(np)))
    lang=rup->languageid;

  result=NULL;
  
  for (i=0;i<num;i++) {
    j=strtoul(PQgetvalue(pgres,i,0),NULL,10);
    if ((j==0 && result==NULL) || (j==lang)) {
      result=PQgetvalue(pgres,i,1);
      if(strlen(result)==0)
	result=NULL;
    }
  }

  if (!result)
    chanservstdmessage(np, QM_NOHELP, hip->commandname->content);
  else
    chanservsendmessage(np, result);
  
  freesstring(hip->commandname);
  free(hip);
}

void csdb_createmail(reguser *rup, int type) {
  char sqlquery[6000];
  char escemail[210];

  if (type == QMAIL_NEWEMAIL) {
    if (rup->email) {
      PQescapeString(escemail, rup->email->content, rup->email->length);
      sprintf(sqlquery, "INSERT INTO email (userID, emailType, prevEmail) "
	      "VALUES (%u,%u,'%s')", rup->ID, type, escemail);
    }
  } else {
    sprintf(sqlquery, "INSERT INTO email (userID, emailType) VALUES (%u,%u)", rup->ID, type);
  }

  csdb_queuequery(NULL, NULL, "%s", sqlquery);
}  
