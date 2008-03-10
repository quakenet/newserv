/*
 * chanservdb_updates.c:
 *  Handle all the update requests for the database.
 */

#include "chanserv.h"
#include "../pqsql/pqsql.h"
#include "../core/config.h"
#include "../lib/sstring.h"
#include "../parser/parser.h"
#include "../core/events.h"

#include <string.h>
#include <libpq-fe.h>
#include <stdio.h>
#include <sys/poll.h>
#include <stdarg.h>

void csdb_updateauthinfo(reguser *rup) {
  char eschost[2*HOSTLEN+1];

  PQescapeString(eschost,rup->lastuserhost->content,rup->lastuserhost->length);
  pqquery("UPDATE users SET lastauth=%lu,lastuserhost='%s' WHERE ID=%u",
		  rup->lastauth,eschost,rup->ID);
}

void csdb_updatelastjoin(regchanuser *rcup) {
  pqquery("UPDATE chanusers SET usetime=%lu WHERE userID=%u and channelID=%u",
	  rcup->usetime, rcup->user->ID, rcup->chan->ID);
}

void csdb_updatetopic(regchan *rcp) {
  char esctopic[TOPICLEN*2+5];

  if (rcp->topic) {
    PQescapeString(esctopic,rcp->topic->content,rcp->topic->length);
  } else {
    esctopic[0]='\0';
  }
  pqquery("UPDATE channels SET topic='%s' WHERE ID=%u",esctopic,rcp->ID);
}

void csdb_updatechannel(regchan *rcp) {
  char escwelcome[WELCOMELEN*2+1];
  char esctopic[TOPICLEN*2+1];
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

  pqquery("UPDATE channels SET name='%s', flags=%d, forcemodes=%d,"
		  "denymodes=%d, chanlimit=%d, autolimit=%d, banstyle=%d,"
		  "lastactive=%lu,statsreset=%lu, banduration=%lu, founder=%u,"
		  "addedby=%u, suspendby=%u, suspendtime=%lu, chantype=%d, totaljoins=%u,"
		  "tripjoins=%u, maxusers=%u, tripusers=%u,"
		  "welcome='%s', topic='%s', chankey='%s', suspendreason='%s',"
		  "comment='%s', lasttimestamp=%d WHERE ID=%u",escname,rcp->flags,rcp->forcemodes,
		  rcp->denymodes,rcp->limit,rcp->autolimit, rcp->banstyle,
		  rcp->lastactive,rcp->statsreset,rcp->banduration,
		  rcp->founder, rcp->addedby, rcp->suspendby, rcp->suspendtime,
		  rcp->chantype,rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  escwelcome,esctopic,esckey,escreason,esccomment,rcp->ltimestamp,rcp->ID);
}

void csdb_updatechannelcounters(regchan *rcp) {
  pqquery("UPDATE channels SET "
		  "lastactive=%lu, totaljoins=%u,"
		  "tripjoins=%u, maxusers=%u, tripusers=%u "
		  "WHERE ID=%u",
		  rcp->lastactive,
		  rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  rcp->ID);
}

void csdb_updatechanneltimestamp(regchan *rcp) {
  pqquery("UPDATE channels SET "
		  "lasttimestamp=%u WHERE ID=%u",
		  rcp->ltimestamp, rcp->ID);
}

void csdb_createchannel(regchan *rcp) {
  char escwelcome[WELCOMELEN*2+1];
  char esctopic[TOPICLEN*2+1];
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

  pqquery("INSERT INTO channels (ID, name, flags, forcemodes, denymodes,"
		  "chanlimit, autolimit, banstyle, created, lastactive, statsreset, "
		  "banduration, founder, addedby, suspendby, suspendtime, chantype, totaljoins, tripjoins,"
		  "maxusers, tripusers, welcome, topic, chankey, suspendreason, "
		  "comment, lasttimestamp) VALUES (%u,'%s',%d,%d,%d,%d,%d,%d,%lu,%lu,%lu,%lu,%u,"
		  "%u,%u,%lu,%d,%u,%u,%u,%u,'%s','%s','%s','%s','%s',%d)",
		  rcp->ID, escname, rcp->flags,rcp->forcemodes,
		  rcp->denymodes,rcp->limit,rcp->autolimit, rcp->banstyle, rcp->created, 
		  rcp->lastactive,rcp->statsreset,rcp->banduration,
		  rcp->founder, rcp->addedby, rcp->suspendby, rcp->suspendtime,
		  rcp->chantype,rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  escwelcome,esctopic,esckey,escreason,esccomment,rcp->ltimestamp);
}

void csdb_deletechannel(regchan *rcp) {
  pqquery("DELETE FROM channels WHERE ID=%u",rcp->ID);
  pqquery("DELETE FROM chanusers WHERE channelID=%u",rcp->ID);
  pqquery("DELETE FROM bans WHERE channelID=%u",rcp->ID);
}

void csdb_deleteuser(reguser *rup) {
  pqquery("DELETE FROM users WHERE ID=%u",rup->ID);
  pqquery("DELETE FROM chanusers WHERE userID=%u",rup->ID);
}

void csdb_updateuser(reguser *rup) {
  char escpassword[25];
  char escemail[210];
  char esclastuserhost[160];
  char escreason[510];
  char esccomment[510];
  char escinfo[210];
  char esclastemail[210];

  PQescapeString(escpassword, rup->password, strlen(rup->password));
   
  if (rup->email)
    PQescapeString(escemail, rup->email->content, rup->email->length);
  else
    escemail[0]='\0';

  if (rup->lastemail)
    PQescapeString(esclastemail, rup->lastemail->content, rup->lastemail->length);
  else
    esclastemail[0]='\0';

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

  pqquery("UPDATE users SET lastauth=%lu, lastemailchng=%lu, flags=%u,"
		  "language=%u, suspendby=%u, suspendexp=%lu, suspendtime=%lu, lockuntil=%lu, password='%s', email='%s',"
		  "lastuserhost='%s', suspendreason='%s', comment='%s', info='%s', lastemail='%s' WHERE ID=%u",
		  rup->lastauth, rup->lastemailchange, rup->flags, rup->languageid, rup->suspendby, rup->suspendexp,
		  rup->suspendtime, rup->lockuntil, escpassword, escemail, esclastuserhost, escreason, esccomment, escinfo, esclastemail,
		  rup->ID);
}  

void csdb_createuser(reguser *rup) {
  char escpassword[25];
  char escemail[210];
  char esclastuserhost[160];
  char escreason[510];
  char esccomment[510];
  char escusername[35];
  char escinfo[210];
  char esclastemail[210];

  PQescapeString(escusername, rup->username, strlen(rup->username));
  PQescapeString(escpassword, rup->password, strlen(rup->password));
  
  if (rup->email)
    PQescapeString(escemail, rup->email->content, rup->email->length);
  else
    escemail[0]='\0';

  if (rup->lastemail)
    PQescapeString(esclastemail, rup->lastemail->content, rup->lastemail->length);
  else
    esclastemail[0]='\0';

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

  pqquery("INSERT INTO users (ID, username, created, lastauth, lastemailchng, "
		  "flags, language, suspendby, suspendexp, suspendtime, lockuntil, password, email, lastuserhost, "
		  "suspendreason, comment, info, lastemail) VALUES (%u,'%s',%lu,%lu,%lu,%u,%u,%u,%lu,%lu,%lu,'%s','%s',"
		  "'%s','%s','%s','%s','%s')",
		  rup->ID, escusername, rup->created, rup->lastauth, rup->lastemailchange, rup->flags, 
		  rup->languageid, rup->suspendby, rup->suspendexp, rup->suspendtime, rup->lockuntil,
		  escpassword, escemail, esclastuserhost, escreason, esccomment, escinfo, esclastemail);
}  


void csdb_updatechanuser(regchanuser *rcup) {
  char escinfo[210];

  if (rcup->info) 
    PQescapeString(escinfo, rcup->info->content, rcup->info->length);
  else
    escinfo[0]='\0';

  pqquery("UPDATE chanusers SET flags=%u, changetime=%lu, "
		  "usetime=%lu, info='%s' WHERE channelID=%u and userID=%u",
		  rcup->flags, rcup->changetime, rcup->usetime, escinfo, rcup->chan->ID,rcup->user->ID);
}

void csdb_createchanuser(regchanuser *rcup) {
  char escinfo[210];

  if (rcup->info) 
    PQescapeString(escinfo, rcup->info->content, rcup->info->length);
  else
    escinfo[0]='\0';
  
  pqquery("INSERT INTO chanusers VALUES(%u, %u, %u, %lu, %lu, '%s')",
		  rcup->user->ID, rcup->chan->ID, rcup->flags, rcup->changetime, 
		  rcup->usetime, escinfo);
}

void csdb_deletechanuser(regchanuser *rcup) {
  pqquery("DELETE FROM chanusers WHERE channelid=%u AND userID=%u",
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

  pqquery("INSERT INTO bans (banID, channelID, userID, hostmask, "
		  "expiry, reason) VALUES (%u,%u,%u,'%s',%lu,'%s')", rbp->ID, rcp->ID, 
		  rbp->setby, escban, rbp->expiry, escreason);
}

void csdb_updateban(regchan *rcp, regban *rbp) {
  char escreason[500];
  char banstr[100];
  char escban[200];

  strcpy(banstr,bantostring(rbp->cbp));
  PQescapeString(escban,banstr,strlen(banstr));
  
  if (rbp->reason)
    PQescapeString(escreason, rbp->reason->content, rbp->reason->length);
  else
    escreason[0]='\0';

  pqquery("UPDATE bans set channelID=%u, userID=%u, hostmask='%s', expiry=%lu, reason='%s' "
                  "WHERE banID=%u", rcp->ID, rbp->setby, escban, rbp->expiry, escreason, rbp->ID);
}

void csdb_deleteban(regban *rbp) {
  pqquery("DELETE FROM bans WHERE banID=%u", rbp->ID);
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

  pqquery("%s", sqlquery);
}

void csdb_deletemaildomain(maildomain *mdp) {
  pqquery("DELETE FROM maildomain WHERE ID=%u", mdp->ID);
}

void csdb_createmaildomain(maildomain *mdp) {
  char escdomain[210];
  PQescapeString(escdomain, mdp->name->content, mdp->name->length);

  pqquery("INSERT INTO maildomain (id, name, domainlimit, actlimit, flags) VALUES(%u, '%s', %u, %u, %u)", mdp->ID,escdomain,mdp->limit,mdp->actlimit,mdp->flags);
}

void csdb_updatemaildomain(maildomain *mdp) {
  char escdomain[210];
  PQescapeString(escdomain, mdp->name->content, mdp->name->length);

  pqquery("UPDATE maildomain SET domainlimit=%u, actlimit=%u, flags=%u, name='%s' WHERE ID=%u", mdp->limit,mdp->actlimit,mdp->flags,escdomain,mdp->ID);
}

void csdb_chanlevhistory_insert(regchan *rcp, nick *np, reguser *trup, flag_t oldflags, flag_t newflags) {
  reguser *rup=getreguserfromnick(np);

  pqquery("INSERT INTO chanlevhistory (userID, channelID, targetID, changetime, authtime, "
    "oldflags, newflags) VALUES (%u, %u, %u, %lu, %lu, %u, %u)",  rup->ID, rcp->ID, trup->ID, getnettime(), np->accountts,
    oldflags, newflags);
}

void csdb_accounthistory_insert(nick *np, char *oldpass, char *newpass, sstring *oldemail, sstring *newemail) {
  reguser *rup=getreguserfromnick(np);
  char escoldpass[30];
  char escnewpass[30];
  char escoldemail[130];
  char escnewemail[130];

  if (!rup || UHasOperPriv(rup))
    return;

  if (oldpass)
    PQescapeString(escoldpass, oldpass, strlen(oldpass));
  else
    escoldpass[0]='\0';

  if (newpass)
    PQescapeString(escnewpass, newpass, strlen(newpass));
  else
    escnewpass[0]='\0';

  if (oldemail)
    PQescapeString(escoldemail, oldemail->content, oldemail->length);
  else
    escoldemail[0]='\0';
  if (newemail)
    PQescapeString(escnewemail, newemail->content, newemail->length);
  else
    escnewemail[0]='\0';

  pqquery("INSERT INTO accounthistory (userID, changetime, authtime, oldpassword, newpassword, oldemail, "
    "newemail) VALUES (%u, %lu, %lu, '%s', '%s', '%s', '%s')", rup->ID, getnettime(), np->accountts, escoldpass, escnewpass,
    escoldemail, escnewemail);

  if (newemail)
    freesstring(newemail);
}

void csdb_cleanuphistories() {
  time_t expire_time=getnettime()-604800;

  Error("chanserv", ERR_INFO, "Cleaning histories.");
  pqquery("DELETE FROM authhistory WHERE authtime < %lu", expire_time);
  pqquery("DELETE FROM chanlevhistory WHERE changetime < %lu", expire_time);
  pqquery("DELETE FROM accounthistory WHERE changetime < %lu", expire_time);
}

