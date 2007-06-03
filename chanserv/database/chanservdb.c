/*
 * chanservdb.c:
 *  Handles the SQL stuff for the channel service.
 */

#include "../chanserv.h"
#include "../../pqsql/pqsql.h"
#include "../../core/config.h"
#include "../../lib/sstring.h"
#include "../../parser/parser.h"
#include "../../core/events.h"
#include "../../core/nsmalloc.h"

#include <string.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <sys/poll.h>
#include <stdarg.h>

int chanservdb_ready;

typedef struct tabledesc {
  sstring *tablename; /* Name of table */
  PQQueryHandler init; /* Function to be called when transaction opens */
  PQQueryHandler data; /* Function to be called to load data */
  PQQueryHandler fini; /* Function to be called to clean up */
} tabledesc;

regchan **allchans;

int chanservext;
int chanservaext;

unsigned int lastchannelID;
unsigned int lastuserID;
unsigned int lastbanID;
unsigned int lastdomainID;

/* Local prototypes */
void csdb_handlestats(int hooknum, void *arg);
void loadmessages_part1(PGconn *, void *);
void loadmessages_part2(PGconn *, void *);
void loadcommandsummary_real(PGconn *, void *);

/* Generic loading functions */
void loadall(char *, PQQueryHandler, PQQueryHandler, PQQueryHandler);
void doloadall(PGconn *, void *);  

/* User loading functions */
void loadsomeusers(PGconn *, void *);
void loadusersdone(PGconn *, void *);

/* Channel loading functions */
void loadsomechannels(PGconn *, void *);
void loadchannelsdone(PGconn *, void *);

/* Chanuser loading functions */
void loadchanusersinit(PGconn *, void *);
void loadsomechanusers(PGconn *, void *);
void loadchanusersdone(PGconn *, void *);

/* Chanban loading functions */
void loadsomechanbans(PGconn *, void *);
void loadchanbansdone(PGconn *, void *);

/* Mail Domain loading functions */
void loadsomemaildomains(PGconn *, void *);
void loadmaildomainsdone(PGconn *, void *);

/* Free sstrings in the structures */
void csdb_freestuff();

static void setuptables() {
  /* Set up the tables */
  /* User table */
  pqcreatequery("CREATE TABLE users ("
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
               "PRIMARY KEY (ID))");

  pqcreatequery("CREATE INDEX user_username_index ON users (username)");
  
  /* Channel table */
  pqcreatequery("CREATE TABLE channels ("
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
                 "PRIMARY KEY (ID))");

  /* Chanuser table */
  pqcreatequery("CREATE TABLE chanusers ("
                 "userID        INT               NOT NULL,"
                 "channelID     INT               NOT NULL,"
                 "flags         INT               NOT NULL,"
                 "changetime    INT               NOT NULL,"
                 "usetime       INT               NOT NULL,"
                 "info          VARCHAR(100)         NOT NULL,"
                 "PRIMARY KEY (userID, channelID))");

  pqcreatequery("CREATE INDEX chanusers_userID_index on chanusers (userID)");
  pqcreatequery("CREATE INDEX chanusers_channelID_index on chanusers (channelID)");
  
  pqcreatequery("CREATE TABLE bans ("
                 "banID         INT               NOT NULL," /* Unique number for the ban to make 
                                                             DELETEs process in finite time.. */
                 "channelID     INT               NOT NULL,"
                 "userID        INT               NOT NULL," /* Who set the ban.. */
                 "hostmask      VARCHAR(100)         NOT NULL," /* needs to be at least USERLEN+NICKLEN+HOSTLEN+2 */
                 "expiry        INT               NOT NULL,"
                 "reason        VARCHAR(200),"
                 "PRIMARY KEY(banID))");

  pqcreatequery("CREATE INDEX bans_channelID_index on bans (channelID)");
  
  pqcreatequery("CREATE TABLE languages ("
                 "languageID    INT               NOT NULL,"
                 "code          VARCHAR(2)           NOT NULL,"
                 "name          VARCHAR(30)          NOT NULL)");
  
  pqcreatequery("CREATE TABLE messages ("
                 "languageID    INT               NOT NULL,"
                 "messageID     INT               NOT NULL,"
                 "message       VARCHAR(250)      NOT NULL,"
                 "PRIMARY KEY (languageID, messageID))");

  pqcreatequery("CREATE TABLE help ("
                 "commandID     INT               NOT NULL,"
                 "command       VARCHAR(30)          NOT NULL,"
                 "languageID    INT               NOT NULL,"
                 "summary       VARCHAR(200)      NOT NULL,"
                 "fullinfo      TEXT              NOT NULL,"
                 "PRIMARY KEY (commandID, languageID))");

  pqcreatequery("CREATE TABLE email ("
                 "userID       INT               NOT NULL,"
                 "emailtype    INT               NOT NULL,"
                 "prevEmail    VARCHAR(100),"
                 "mailID       VARCHAR(128))");

  pqcreatequery("CREATE TABLE maildomain ("
                 "ID           INT               NOT NULL,"
                 "name         VARCHAR           NOT NULL,"
                 "domainlimit  INT               NOT NULL,"
                 "actlimit     INT               NOT NULL,"
                 "flags         INT              NOT NULL,"
                 "PRIMARY KEY (ID))");

   pqcreatequery("CREATE TABLE authhistory ("
                  "userID         INT         NOT NULL,"
                  "nick           VARCHAR(15) NOT NULL,"
                  "username       VARCHAR(10) NOT NULL,"
                  "host           VARCHAR(63) NOT NULL,"
                  "authtime       INT         NOT NULL,"
                  "disconnecttime INT	      NOT NULL,"
                  "numeric        INT         NOT NULL,"
                  "quitreason	  VARCHAR(100) ,"
                  "PRIMARY KEY (userID, authtime))");

   pqcreatequery("CREATE INDEX authhistory_userID_index on authhistory(userID)");
   pqcreatequery("CREATE TABLE chanlevhistory ("
                  "userID         INT         NOT NULL,"
                  "channelID      INT         NOT NULL,"
                  "targetID       INT         NOT NULL,"
                  "changetime     INT         NOT NULL,"
                  "authtime       INT         NOT NULL,"
                  "oldflags       INT         NOT NULL,"
                  "newflags       INT         NOT NULL)");

   pqcreatequery("CREATE INDEX chanlevhistory_userID_index on chanlevhistory(userID)");
   pqcreatequery("CREATE INDEX chanlevhistory_channelID_index on chanlevhistory(channelID)");
   pqcreatequery("CREATE INDEX chanlevhistory_targetID_index on chanlevhistory(targetID)");

   pqcreatequery("CREATE TABLE accounthistory ("
                  "userID INT NOT NULL,"
                  "changetime INT NOT NULL,"
                  "authtime INT NOT NULL,"
                  "oldpassword VARCHAR(11),"
                  "newpassword VARCHAR(11),"
                  "oldemail VARCHAR(100),"
                  "newemail VARCHAR(100),"
                  "PRIMARY KEY (userID, changetime))");

   pqcreatequery("CREATE INDEX accounthistory_userID_index on accounthistory(userID)");
}

void _init() {
  chanservext=registerchanext("chanserv");
  chanservaext=registerauthnameext("chanserv");

  /* Set up the allocators and hashes */
  chanservallocinit();
  chanservhashinit();

  /* And the messages */
  initmessages();
  
  if (pqconnected() && (chanservext!=-1) && (chanservaext!=-1)) {
    registerhook(HOOK_CORE_STATSREQUEST, csdb_handlestats);

    setuptables();

    lastuserID=lastchannelID=lastdomainID=0;

    loadall("users",NULL,loadsomeusers,loadusersdone);
    loadall("channels",NULL,loadsomechannels,loadchannelsdone);
    loadall("chanusers",loadchanusersinit,loadsomechanusers,loadchanusersdone);
    loadall("bans",NULL,loadsomechanbans,loadchanbansdone);
    loadall("maildomain",NULL, loadsomemaildomains,loadmaildomainsdone);
    
    loadmessages(); 
  }
}

void _fini() {
  if (chanservext!=-1)
    releasechanext(chanservext);
  
  if (chanservaext!=-1)
    releaseauthnameext(chanservaext);
    
  csdb_freestuff();
  nsfreeall(POOL_CHANSERVDB);
}

void csdb_handlestats(int hooknum, void *arg) {
/*   long level=(long)arg; */

  /* Keeping options open here */
}

void chanservdbclose() {

  deregisterhook(HOOK_CORE_STATSREQUEST, csdb_handlestats);

}

/*
 * loadall():
 *  Generic function to handle load of an entire table..
 */

void loadall(char *table, PQQueryHandler init, PQQueryHandler data, PQQueryHandler fini) {
  tabledesc *thedesc;
  
  thedesc=malloc(sizeof(tabledesc));

  thedesc->tablename=getsstring(table,100);
  thedesc->init=init;
  thedesc->data=data;
  thedesc->fini=fini;

  pqasyncquery(doloadall, thedesc, "SELECT count(*) FROM %s",thedesc->tablename->content);
}

void doloadall(PGconn *dbconn, void *arg) {
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
  
  pqasyncquery(thedesc->init, NULL, "BEGIN");
  pqquery("DECLARE mycurs CURSOR FOR SELECT * from %s",
		  thedesc->tablename->content);

  for (i=0;(count-i)>1000;i+=1000) {
    pqasyncquery(thedesc->data, NULL, "FETCH 1000 FROM mycurs");
  }

  pqasyncquery(thedesc->data, NULL, "FETCH ALL FROM mycurs");
  
  pqquery("CLOSE mycurs");
  pqasyncquery(thedesc->fini, NULL, "COMMIT");

  /* Free structures.. */
  freesstring(thedesc->tablename);
  free(thedesc);
}

/*
 * loadsomeusers():
 *  Loads some users in from the SQL DB
 */

void loadsomeusers(PGconn *dbconn, void *arg) {
  PGresult *pgres;
  reguser *rup;
  unsigned int i,num;
  char *local;

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
    if (rup->email) {
      rup->domain=findorcreatemaildomain(rup->email->content);
      addregusertomaildomain(rup, rup->domain);
     
      char *dupemail = strdup(rup->email->content);
      if(local=strchr(dupemail, '@')) {
        *(local++)='\0';
        rup->localpart=getsstring(local,EMAILLEN);
      } else {
        rup->localpart=NULL;
      }
      free(dupemail);
    } else {
      rup->domain=NULL;
      rup->localpart=NULL;
    }
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

void loadusersdone(PGconn *conn, void *arg) {
  Error("chanserv",ERR_INFO,"Load users done (highest ID was %d)",lastuserID);
}

/*
 * Channel loading functions
 */

void loadsomechannels(PGconn *dbconn, void *arg) {
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

void loadchannelsdone(PGconn *dbconn, void *arg) {
  Error("chanserv",ERR_INFO,"Channel load done (highest ID was %d)",lastchannelID);
}

void loadchanusersinit(PGconn *dbconn, void *arg) {
  int i;
  chanindex *cip;
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
}

void loadsomechanusers(PGconn *dbconn, void *arg) {
  PGresult *pgres;
  regchanuser *rcup;
  int i,num;
  regchan *rcp;
  reguser *rup;
  authname *anp;
  int uid,cid;
  int total=0;

  /* Set up the allchans array */
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

    if (!(anp=findauthname(uid)) || !(rup=anp->exts[chanservaext])) {
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

void loadchanusersdone(PGconn *dbconn, void *arg) {
  Error("chanserv",ERR_INFO,"Channel user load done.");
}

void loadsomechanbans(PGconn *dbconn, void *arg) {
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

void loadchanbansdone(PGconn *dbconn, void *arg) {
  free(allchans);
  
  Error("chanserv",ERR_INFO,"Channel ban load done, highest ID was %d",lastbanID);

  chanservdb_ready=1;
  triggerhook(HOOK_CHANSERV_DBLOADED, NULL);
}

void loadmessages() {
  pqasyncquery(loadmessages_part1, NULL, "SELECT * from languages");
}

void loadmessages_part1(PGconn *dbconn, void *arg) {
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

  pqasyncquery(loadmessages_part2, NULL, "SELECT * from messages");
}

void loadmessages_part2(PGconn *dbconn, void *arg) {
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
  pqasyncquery(loadcommandsummary_real, (void *)cmd, 
		  "SELECT languageID,summary from help where lower(command) = lower('%s')",cmd->command->content);
}

void loadcommandsummary_real(PGconn *dbconn, void *arg) {
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

void csdb_freestuff() {
  int i;
  chanindex *cip, *ncip;
  regchan *rcp;
  reguser *rup;
  regchanuser *rcup;
  regban *rbp;
  maildomain *mdp;

  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) {
      freesstring(rup->email);
      freesstring(rup->lastuserhost);
      freesstring(rup->suspendreason);
      freesstring(rup->comment);
      freesstring(rup->info);

      for (rcup=rup->knownon;rcup;rcup=rcup->nextbyuser)
        freesstring(rcup->info);
    }
  }

  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=ncip) {
      ncip=cip->next;
      if ((rcp=cip->exts[chanservext])) {
        freesstring(rcp->welcome);
        freesstring(rcp->topic);
        freesstring(rcp->key);
        freesstring(rcp->suspendreason);
        freesstring(rcp->comment);
        for (rbp=rcp->bans;rbp;rbp=rbp->next) {
          freesstring(rbp->reason);
          freechanban(rbp->cbp);
        }
        cip->exts[chanservext]=NULL;
        releasechanindex(cip);
      }
    }
  }

  for (i=0; i<MAILDOMAINHASHSIZE; i++) {
    for (mdp=maildomainnametable[i]; mdp; mdp=mdp->nextbyname)
      freesstring(mdp->name);
  }
}

void loadsomemaildomains(PGconn *dbconn,void *arg) {
  PGresult *pgres;
  maildomain *mdp;
  unsigned int i,num;
  char *domain;
  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv",ERR_ERROR,"Error loading maildomain DB");
    return;
 }

  if (PQnfields(pgres)!=4) {
    Error("chanserv",ERR_ERROR,"Mail Domain DB format error");
    return;
  }
  num=PQntuples(pgres);
  lastdomainID=0;

  for(i=0;i<num;i++) {
    domain=strdup(PQgetvalue(pgres,i,1));
    mdp=findorcreatemaildomain(domain); //@@@ LEN

    mdp->ID=strtoul(PQgetvalue(pgres,i,0),NULL,10);
    mdp->limit=strtoul(PQgetvalue(pgres,i,2),NULL,10);
    mdp->actlimit=strtoul(PQgetvalue(pgres,i,3),NULL,10);
    mdp->flags=strtoul(PQgetvalue(pgres,i,4),NULL,10);

    if (mdp->ID > lastdomainID) {
      lastdomainID=mdp->ID;
    }
  }

  PQclear(pgres);
}

void loadmaildomainsdone(PGconn *dbconn, void *arg) {
  Error("chanserv",ERR_INFO,"Load Mail Domains done (highest ID was %d)",lastdomainID);
}

