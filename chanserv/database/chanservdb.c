/*
 * chanservdb.c:
 *  Handles the SQL stuff for the channel service.
 */

#include "../chanserv.h"
#include "../../core/config.h"
#include "../../lib/sstring.h"
#include "../../parser/parser.h"
#include "../../core/events.h"
#include "../../core/nsmalloc.h"
#include "../../lib/strlfunc.h"
#include "../../dbapi/dbapi.h"
#include "../../lib/version.h"

#include <string.h>
#include <stdio.h>
#include <sys/poll.h>
#include <stdarg.h>

MODULE_VERSION(QVERSION);

int chanservdb_ready;

regchan **allchans;
maillock *maillocks;

int chanservext;
int chanservaext;

unsigned int lastchannelID;
unsigned int lastuserID;
unsigned int lastbanID;
unsigned int lastdomainID;
unsigned int lastmaillockID;

/* Local prototypes */
void csdb_handlestats(int hooknum, void *arg);
void loadmessages_part1(DBConn *, void *);
void loadmessages_part2(DBConn *, void *);
void loadcommandsummary_real(DBConn *, void *);

/* User loading functions */
void loadsomeusers(DBConn *, void *);
void loadusersdone(DBConn *, void *);

/* Channel loading functions */
void loadsomechannels(DBConn *, void *);
void loadchannelsdone(DBConn *, void *);

/* Chanuser loading functions */
void loadchanusersinit(DBConn *, void *);
void loadsomechanusers(DBConn *, void *);
void loadchanusersdone(DBConn *, void *);

/* Chanban loading functions */
void loadsomechanbans(DBConn *, void *);
void loadchanbansdone(DBConn *, void *);

/* Mail Domain loading functions */
void loadsomemaildomains(DBConn *, void *);
void loadmaildomainsdone(DBConn *, void *);

/* Mail lock loading functions */
void loadsomemaillocks(DBConn *, void *);
void loadmaillocksdone(DBConn *, void *);

/* Free sstrings in the structures */
void csdb_freestuff();

static void setuptables() {
  /* Set up the tables */
  /* User table */
  dbattach("chanserv");
  dbcreatequery("CREATE TABLE chanserv.users ("
               "ID            INT               NOT NULL,"
               "username      VARCHAR(16)          NOT NULL,"
               "created       INT               NOT NULL,"
               "lastauth      INT               NOT NULL,"
               "lastemailchng INT               NOT NULL,"
               "flags         INT               NOT NULL,"
               "language      INT               NOT NULL,"
               "suspendby     INT               NOT NULL,"
               "suspendexp    INT               NOT NULL,"
               "suspendtime   INT               NOT NULL,"
               "lockuntil     INT               NOT NULL,"
               "password      VARCHAR(11)          NOT NULL,"
               "email         VARCHAR(100),"
               "lastemail     VARCHAR(100),"
               "lastuserhost  VARCHAR(75),"
               "suspendreason VARCHAR(250),"
               "comment       VARCHAR(250),"
               "info          VARCHAR(100),"
               "lastpasschng  INT               NOT NULL,"
               "PRIMARY KEY (ID))");

  dbcreatequery("CREATE INDEX user_username_index ON chanserv.users (username)");
  
  /* Channel table */
  dbcreatequery("CREATE TABLE chanserv.channels ("
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
                 "suspendtime   INT               NOT NULL,"
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
  dbcreatequery("CREATE TABLE chanserv.chanusers ("
                 "userID        INT               NOT NULL,"
                 "channelID     INT               NOT NULL,"
                 "flags         INT               NOT NULL,"
                 "changetime    INT               NOT NULL,"
                 "usetime       INT               NOT NULL,"
                 "info          VARCHAR(100)         NOT NULL,"
                 "PRIMARY KEY (userID, channelID))");

  dbcreatequery("CREATE INDEX chanusers_userID_index on chanserv.chanusers (userID)");
  dbcreatequery("CREATE INDEX chanusers_channelID_index on chanserv.chanusers (channelID)");
  
  dbcreatequery("CREATE TABLE chanserv.bans ("
                 "banID         INT               NOT NULL," /* Unique number for the ban to make 
                                                             DELETEs process in finite time.. */
                 "channelID     INT               NOT NULL,"
                 "userID        INT               NOT NULL," /* Who set the ban.. */
                 "hostmask      VARCHAR(100)         NOT NULL," /* needs to be at least USERLEN+NICKLEN+HOSTLEN+2 */
                 "expiry        INT               NOT NULL,"
                 "reason        VARCHAR(200),"
                 "PRIMARY KEY(banID))");

  dbcreatequery("CREATE INDEX bans_channelID_index on chanserv.bans (channelID)");
  
  dbcreatequery("CREATE TABLE chanserv.languages ("
                 "languageID    INT               NOT NULL,"
                 "code          VARCHAR(2)           NOT NULL,"
                 "name          VARCHAR(30)          NOT NULL)");
  
  dbcreatequery("CREATE TABLE chanserv.messages ("
                 "languageID    INT               NOT NULL,"
                 "messageID     INT               NOT NULL,"
                 "message       VARCHAR(250)      NOT NULL,"
                 "PRIMARY KEY (languageID, messageID))");

  dbcreatequery("CREATE TABLE chanserv.help ("
                 "commandID     INT               NOT NULL,"
                 "command       VARCHAR(30)          NOT NULL,"
                 "languageID    INT               NOT NULL,"
                 "summary       VARCHAR(200)      NOT NULL,"
                 "fullinfo      TEXT              NOT NULL,"
                 "PRIMARY KEY (commandID, languageID))");

  dbcreatequery("CREATE TABLE chanserv.email ("
                 "userID       INT               NOT NULL,"
                 "emailtype    INT               NOT NULL,"
                 "prevEmail    VARCHAR(100),"
                 "mailID       SERIAL,"
		 "PRIMARY KEY (mailID))");

  dbcreatequery("CREATE TABLE chanserv.maildomain ("
                 "ID           INT               NOT NULL,"
                 "name         VARCHAR           NOT NULL,"
                 "domainlimit  INT               NOT NULL,"
                 "actlimit     INT               NOT NULL,"
                 "flags         INT              NOT NULL,"
                 "PRIMARY KEY (ID))");

   dbcreatequery("CREATE TABLE chanserv.authhistory ("
                  "userID         INT         NOT NULL,"
                  "nick           VARCHAR(15) NOT NULL,"
                  "username       VARCHAR(10) NOT NULL,"
                  "host           VARCHAR(63) NOT NULL,"
                  "authtime       INT         NOT NULL,"
                  "disconnecttime INT	      NOT NULL,"
                  "numeric        INT         NOT NULL,"
                  "quitreason	  VARCHAR(100) ,"
                  "PRIMARY KEY (userID, authtime))");

   dbcreatequery("CREATE INDEX authhistory_userID_index on chanserv.authhistory(userID)");
   dbcreatequery("CREATE TABLE chanserv.chanlevhistory ("
                  "userID         INT         NOT NULL,"
                  "channelID      INT         NOT NULL,"
                  "targetID       INT         NOT NULL,"
                  "changetime     INT         NOT NULL,"
                  "authtime       INT         NOT NULL,"
                  "oldflags       INT         NOT NULL,"
                  "newflags       INT         NOT NULL)");

   dbcreatequery("CREATE INDEX chanlevhistory_userID_index on chanserv.chanlevhistory(userID)");
   dbcreatequery("CREATE INDEX chanlevhistory_channelID_index on chanserv.chanlevhistory(channelID)");
   dbcreatequery("CREATE INDEX chanlevhistory_targetID_index on chanserv.chanlevhistory(targetID)");

   dbcreatequery("CREATE TABLE chanserv.accounthistory ("
                  "userID INT NOT NULL,"
                  "changetime INT NOT NULL,"
                  "authtime INT NOT NULL,"
                  "oldpassword VARCHAR(11),"
                  "newpassword VARCHAR(11),"
                  "oldemail VARCHAR(100),"
                  "newemail VARCHAR(100),"
                  "PRIMARY KEY (userID, changetime))");

   dbcreatequery("CREATE INDEX accounthistory_userID_index on chanserv.accounthistory(userID)");

   dbcreatequery("CREATE TABLE chanserv.maillocks ("
                 "ID           INT               NOT NULL,"
                 "pattern      VARCHAR           NOT NULL,"
                 "reason       VARCHAR           NOT NULL,"
                 "createdby    INT               NOT NULL,"
                 "created      INT               NOT NULL,"
                 "PRIMARY KEY (ID))");
}

void _init() {
  chanservext=registerchanext("chanserv");
  chanservaext=registerauthnameext("chanserv",1);

  /* Set up the allocators and hashes */
  chanservallocinit();
  chanservhashinit();

  /* And the messages */
  initmessages();
  
  if (dbconnected() && (chanservext!=-1) && (chanservaext!=-1)) {
    registerhook(HOOK_CORE_STATSREQUEST, csdb_handlestats);

    setuptables();

    lastuserID=lastchannelID=lastdomainID=0;

    dbloadtable("chanserv.users",NULL,loadsomeusers,loadusersdone);
    dbloadtable("chanserv.channels",NULL,loadsomechannels,loadchannelsdone);
    dbloadtable("chanserv.chanusers",loadchanusersinit,loadsomechanusers,loadchanusersdone);
    dbloadtable("chanserv.bans",NULL,loadsomechanbans,loadchanbansdone);
    dbloadtable("chanserv.maildomain",NULL, loadsomemaildomains,loadmaildomainsdone);
    dbloadtable("chanserv.maillocks",NULL, loadsomemaillocks,loadmaillocksdone);
    
    loadmessages(); 
  }
}

void _fini() {
  deregisterhook(HOOK_CORE_STATSREQUEST, csdb_handlestats);
  
  csdb_freestuff();

  if (chanservext!=-1)
    releasechanext(chanservext);
  
  if (chanservaext!=-1)
    releaseauthnameext(chanservaext);
    
  nsfreeall(POOL_CHANSERVDB);
  dbdetach("chanserv");
}

void csdb_handlestats(int hooknum, void *arg) {
/*   long level=(long)arg; */

  /* Keeping options open here */
}

void chanservdbclose() {

  deregisterhook(HOOK_CORE_STATSREQUEST, csdb_handlestats);

}

/*
 * loadsomeusers():
 *  Loads some users in from the SQL DB
 */

void loadsomeusers(DBConn *dbconn, void *arg) {
  DBResult *pgres;
  reguser *rup;
  char *local;
  char mailbuf[1024];

  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading user DB");
    return;
  }

  if (dbnumfields(pgres)!=19) {
    Error("chanserv",ERR_ERROR,"User DB format error");
    return;
  }

  while(dbfetchrow(pgres)) {
    rup=getreguser();
    rup->status=0;
    rup->ID=strtoul(dbgetvalue(pgres,0),NULL,10);
    strncpy(rup->username,dbgetvalue(pgres,1),NICKLEN); rup->username[NICKLEN]='\0';
    rup->created=strtoul(dbgetvalue(pgres,2),NULL,10);
    rup->lastauth=strtoul(dbgetvalue(pgres,3),NULL,10);
    rup->lastemailchange=strtoul(dbgetvalue(pgres,4),NULL,10);
    rup->flags=strtoul(dbgetvalue(pgres,5),NULL,10);
    rup->languageid=strtoul(dbgetvalue(pgres,6),NULL,10);
    rup->suspendby=strtoul(dbgetvalue(pgres,7),NULL,10);
    rup->suspendexp=strtoul(dbgetvalue(pgres,8),NULL,10);
    rup->suspendtime=strtoul(dbgetvalue(pgres,9),NULL,10);
    rup->lockuntil=strtoul(dbgetvalue(pgres,10),NULL,10);
    strncpy(rup->password,dbgetvalue(pgres,11),PASSLEN); rup->password[PASSLEN]='\0';
    rup->email=getsstring(dbgetvalue(pgres,12),100);
    if (rup->email) {
      rup->domain=findorcreatemaildomain(rup->email->content);
      addregusertomaildomain(rup, rup->domain);

      strlcpy(mailbuf, rup->email->content, sizeof(mailbuf));
      if((local=strchr(mailbuf, '@'))) {
        *(local++)='\0';
        rup->localpart=getsstring(mailbuf,EMAILLEN);
      } else {
        rup->localpart=NULL;
      }
    } else {
      rup->domain=NULL;
      rup->localpart=NULL;
    }
    rup->lastemail=getsstring(dbgetvalue(pgres,13),100);
    rup->lastuserhost=getsstring(dbgetvalue(pgres,14),75);
    rup->suspendreason=getsstring(dbgetvalue(pgres,15),250);
    rup->comment=getsstring(dbgetvalue(pgres,16),250);
    rup->info=getsstring(dbgetvalue(pgres,17),100);
    rup->lastpasschange=strtoul(dbgetvalue(pgres,18),NULL,10);
    rup->knownon=NULL;
    rup->checkshd=NULL;
    rup->stealcount=0;
    rup->fakeuser=NULL;
    addregusertohash(rup);
    
    if (rup->ID > lastuserID) {
      lastuserID=rup->ID;
    }
  }

  dbclear(pgres);
}

void loadusersdone(DBConn *conn, void *arg) {
  Error("chanserv",ERR_INFO,"Load users done (highest ID was %d)",lastuserID);
}

/*
 * Channel loading functions
 */

void loadsomechannels(DBConn *dbconn, void *arg) {
  DBResult *pgres;
  regchan *rcp;
  int j;
  chanindex *cip;
  time_t now=time(NULL);

  pgres=dbgetresult(dbconn);
   
  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading channel DB");
    return;
  }

  if (dbnumfields(pgres)!=27) {
    Error("chanserv",ERR_ERROR,"Channel DB format error");
    return;
  }

  while(dbfetchrow(pgres)) {
    cip=findorcreatechanindex(dbgetvalue(pgres,1));
    if (cip->exts[chanservext]) {
      Error("chanserv",ERR_WARNING,"%s in database twice - this WILL cause problems later.",cip->name->content);
      continue;
    }
    rcp=getregchan();
    cip->exts[chanservext]=rcp;
    
    rcp->ID=strtoul(dbgetvalue(pgres,0),NULL,10);
    rcp->index=cip;
    rcp->flags=strtoul(dbgetvalue(pgres,2),NULL,10);
    rcp->status=0; /* Non-DB field */
    rcp->lastbancheck=0;
    rcp->lastcountersync=now;
    rcp->lastpart=0;
    rcp->bans=NULL;
    rcp->forcemodes=strtoul(dbgetvalue(pgres,3),NULL,10);
    rcp->denymodes=strtoul(dbgetvalue(pgres,4),NULL,10);
    rcp->limit=strtoul(dbgetvalue(pgres,5),NULL,10);
    rcp->autolimit=strtoul(dbgetvalue(pgres,6),NULL,10);
    rcp->banstyle=strtoul(dbgetvalue(pgres,7),NULL,10);
    rcp->created=strtoul(dbgetvalue(pgres,8),NULL,10);
    rcp->lastactive=strtoul(dbgetvalue(pgres,9),NULL,10);
    rcp->statsreset=strtoul(dbgetvalue(pgres,10),NULL,10);
    rcp->banduration=strtoul(dbgetvalue(pgres,11),NULL,10);
    rcp->founder=strtol(dbgetvalue(pgres,12),NULL,10);
    rcp->addedby=strtol(dbgetvalue(pgres,13),NULL,10);
    rcp->suspendby=strtol(dbgetvalue(pgres,14),NULL,10);
    rcp->suspendtime=strtol(dbgetvalue(pgres,15),NULL,10);
    rcp->chantype=strtoul(dbgetvalue(pgres,16),NULL,10);
    rcp->totaljoins=strtoul(dbgetvalue(pgres,17),NULL,10);
    rcp->tripjoins=strtoul(dbgetvalue(pgres,18),NULL,10);
    rcp->maxusers=strtoul(dbgetvalue(pgres,19),NULL,10);
    rcp->tripusers=strtoul(dbgetvalue(pgres,20),NULL,10);
    rcp->welcome=getsstring(dbgetvalue(pgres,21),500);
    rcp->topic=getsstring(dbgetvalue(pgres,22),TOPICLEN);
    rcp->key=getsstring(dbgetvalue(pgres,23),KEYLEN);
    rcp->suspendreason=getsstring(dbgetvalue(pgres,24),250);
    rcp->comment=getsstring(dbgetvalue(pgres,25),250);
    rcp->checksched=NULL;
    rcp->ltimestamp=strtoul(dbgetvalue(pgres,26),NULL,10);
    memset(rcp->regusers,0,REGCHANUSERHASHSIZE*sizeof(reguser *));

    if (rcp->ID > lastchannelID)
      lastchannelID=rcp->ID;
    
    if (CIsAutoLimit(rcp))
      rcp->limit=0;
    
    for (j=0;j<CHANOPHISTORY;j++) {
      rcp->chanopnicks[j][0]='\0';
      rcp->chanopaccts[j]=0;
    }
    rcp->chanoppos=0;
  }

  dbclear(pgres);
}

void loadchannelsdone(DBConn *dbconn, void *arg) {
  Error("chanserv",ERR_INFO,"Channel load done (highest ID was %d)",lastchannelID);
}

void loadchanusersinit(DBConn *dbconn, void *arg) {
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

void loadsomechanusers(DBConn *dbconn, void *arg) {
  DBResult *pgres;
  regchanuser *rcup;
  regchan *rcp;
  reguser *rup;
  authname *anp;
  int uid,cid;
  int total=0;

  /* Set up the allchans array */
  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading chanusers.");
    return; 
  }

  if (dbnumfields(pgres)!=6) {
    Error("chanserv",ERR_ERROR,"Chanusers format error");
    return;
  }

  while(dbfetchrow(pgres)) {
    uid=strtol(dbgetvalue(pgres,0),NULL,10);
    cid=strtol(dbgetvalue(pgres,1),NULL,10);

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
	    dbgetvalue(pgres,0),dbgetvalue(pgres,1));
    } else {
      rcup=getregchanuser();
      rcup->user=rup;
      rcup->chan=rcp;
      rcup->flags=strtol(dbgetvalue(pgres,2),NULL,10);
      rcup->changetime=strtol(dbgetvalue(pgres,3),NULL,10);
      rcup->usetime=strtol(dbgetvalue(pgres,4),NULL,10);
      rcup->info=getsstring(dbgetvalue(pgres,5),100);
      addregusertochannel(rcup);
      total++;
    }
  }

  dbclear(pgres);
}

void loadchanusersdone(DBConn *dbconn, void *arg) {
  Error("chanserv",ERR_INFO,"Channel user load done.");
}

void loadsomechanbans(DBConn *dbconn, void *arg) {
  DBResult *pgres;
  regban  *rbp;
  regchan *rcp;
  int uid,cid,bid;
  time_t expiry;
  int total=0;

  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading bans.");
    return; 
  }

  if (dbnumfields(pgres)!=6) {
    Error("chanserv",ERR_ERROR,"Ban format error");
    return;
  }

  while(dbfetchrow(pgres)) {
    bid=strtoul(dbgetvalue(pgres,0),NULL,10);
    cid=strtoul(dbgetvalue(pgres,1),NULL,10);
    uid=strtoul(dbgetvalue(pgres,2),NULL,10);
    expiry=strtoul(dbgetvalue(pgres,4),NULL,10);

    if (cid>lastchannelID || !(rcp=allchans[cid])) {
      Error("chanserv",ERR_WARNING,"Skipping ban for unknown chan %d",cid);
      continue;
    }

    rbp=getregban();
    rbp->setby=uid;
    rbp->ID=bid;
    rbp->expiry=expiry;
    rbp->reason=getsstring(dbgetvalue(pgres,5),200);
    rbp->cbp=makeban(dbgetvalue(pgres,3));
    rbp->next=rcp->bans;
    rcp->bans=rbp;
   
    total++;

    if (bid>lastbanID)
      lastbanID=bid;
  }

  dbclear(pgres);
}

void loadchanbansdone(DBConn *dbconn, void *arg) {
  free(allchans);
  
  Error("chanserv",ERR_INFO,"Channel ban load done, highest ID was %d",lastbanID);
}

void loadmessages() {
  dbasyncquery(loadmessages_part1, NULL, "SELECT * from chanserv.languages");
}

void loadmessages_part1(DBConn *dbconn, void *arg) {
  int i,j;
  DBResult *pgres;
      
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

  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading language list.");
    return; 
  }

  if (dbnumfields(pgres)!=3) {
    Error("chanserv",ERR_ERROR,"Language list format error.");
    return;
  }

  while(dbfetchrow(pgres)) {
    j=strtol(dbgetvalue(pgres,0),NULL,10);
    if (j<MAXLANG && j>=0) {
      cslanguages[j]=(cslang *)malloc(sizeof(cslang));

      strncpy(cslanguages[j]->code,dbgetvalue(pgres,1),2); cslanguages[j]->code[2]='\0';
      cslanguages[j]->name=getsstring(dbgetvalue(pgres,2),30);
    }
  }
   
  dbclear(pgres);

  if (i>MAXLANG)
    Error("chanserv",ERR_ERROR,"Found too many languages (%d > %d)",i,MAXLANG); 

  dbasyncquery(loadmessages_part2, NULL, "SELECT * from chanserv.messages");
}

void loadmessages_part2(DBConn *dbconn, void *arg) {
  DBResult *pgres;
  int j,k;

  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading message list.");
    return; 
  }

  if (dbnumfields(pgres)!=3) {
    Error("chanserv",ERR_ERROR,"Message list format error.");
    return;
  }
  
  while(dbfetchrow(pgres)) {
    k=strtol(dbgetvalue(pgres,0),NULL,10);
    j=strtol(dbgetvalue(pgres,1),NULL,10);
    
    if (k<0 || k >= MAXLANG) {
      Error("chanserv",ERR_WARNING,"Language ID out of range on message: %d",k);
      continue;
    }
    
    if (j<0 || j >= MAXMESSAGES) {
      Error("chanserv",ERR_WARNING,"Message ID out of range on message: %d",j);
      continue;
    }
    
    csmessages[k][j]=getsstring(dbgetvalue(pgres,2),250);
  } 
                          
  dbclear(pgres);
}

void loadcommandsummary(Command *cmd) {
  dbasyncquery(loadcommandsummary_real, (void *)cmd, 
		  "SELECT languageID,summary from chanserv.help where lower(command) = lower('%s')",cmd->command->content);
}

void loadcommandsummary_real(DBConn *dbconn, void *arg) {
  int i,j;
  DBResult *pgres;
  cmdsummary *cs;
  Command *cmd=arg;

  /* Clear up old text first */
  cs=cmd->ext;
  for (i=0;i<MAXLANG;i++) {
    if (cs->bylang[i])
      freesstring(cs->bylang[i]);
  }
  
  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading command summary.");
    return; 
  }

  if (dbnumfields(pgres)!=2) {
    Error("chanserv",ERR_ERROR,"Command summary format error.");
    dbclear(pgres);
    return;
  }

  while(dbfetchrow(pgres)) {
    j=strtol(dbgetvalue(pgres,0),NULL,10);
    if (j<MAXLANG && j>=0) {
      cs->bylang[j]=getsstring(dbgetvalue(pgres,1),200);
    }
  }

  dbclear(pgres);
}

void csdb_freestuff() {
  int i;
  chanindex *cip, *ncip;
  regchan *rcp;
  reguser *rup;
  regchanuser *rcup;
  regban *rbp;
  maildomain *mdp;
  maillock *mlp, *nmlp;

  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) {
      freesstring(rup->email);
      freesstring(rup->localpart);
      freesstring(rup->lastemail);
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

  for(mlp=maillocks;mlp;mlp=nmlp) {
    nmlp=mlp->next;
    freemaillock(mlp);
  }
  maillocks=NULL;
}

void loadsomemaildomains(DBConn *dbconn,void *arg) {
  DBResult *pgres;
  maildomain *mdp;
  char *domain;
  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading maildomain DB");
    return;
  }

  if (dbnumfields(pgres)!=5) {
    Error("chanserv",ERR_ERROR,"Mail Domain DB format error");
    dbclear(pgres);
    return;
  }
  lastdomainID=0;

  while(dbfetchrow(pgres)) {
    domain=dbgetvalue(pgres,1);
    mdp=findorcreatemaildomain(domain); //@@@ LEN

    mdp->ID=strtoul(dbgetvalue(pgres,0),NULL,10);
    mdp->limit=strtoul(dbgetvalue(pgres,2),NULL,10);
    mdp->actlimit=strtoul(dbgetvalue(pgres,3),NULL,10);
    mdp->flags=strtoul(dbgetvalue(pgres,4),NULL,10);

    if (mdp->ID > lastdomainID) {
      lastdomainID=mdp->ID;
    }
  }

  dbclear(pgres);
}

void loadmaildomainsdone(DBConn *dbconn, void *arg) {
  Error("chanserv",ERR_INFO,"Load Mail Domains done (highest ID was %d)",lastdomainID);
}

void loadsomemaillocks(DBConn *dbconn,void *arg) {
  DBResult *pgres;
  maillock *mlp;
  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading maillock DB");
    return;
  }

  if (dbnumfields(pgres)!=5) {
    Error("chanserv",ERR_ERROR,"Maillock DB format error");
    dbclear(pgres);
    return;
  }
  lastmaillockID=0;

  while(dbfetchrow(pgres)) {
    mlp=getmaillock();
    mlp->id=strtoul(dbgetvalue(pgres,0),NULL,10);
    mlp->pattern=getsstring(dbgetvalue(pgres,1), 300);
    mlp->reason=getsstring(dbgetvalue(pgres,2), 300);
    mlp->createdby=strtoul(dbgetvalue(pgres,3),NULL,10);
    mlp->created=strtoul(dbgetvalue(pgres,4),NULL,10);
    mlp->next=maillocks;
    maillocks=mlp;

    if (mlp->id > lastmaillockID)
      lastmaillockID=mlp->id;
  }

  dbclear(pgres);
}

void loadmaillocksdone(DBConn *dbconn, void *arg) {
  Error("chanserv",ERR_INFO,"Load Mail Locks done (highest ID was %d)",lastmaillockID);

  chanservdb_ready=1;
  triggerhook(HOOK_CHANSERV_DBLOADED, NULL);
}

