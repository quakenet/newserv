/*
 * chanservdb_updates.c:
 *  Handle all the update requests for the database.
 */

#include "chanserv.h"
#include "../dbapi/dbapi.h"
#include "../core/config.h"
#include "../lib/sstring.h"
#include "../parser/parser.h"
#include "../core/events.h"

#include <string.h>
#include <stdio.h>
#include <sys/poll.h>
#include <stdarg.h>

void csdb_updateauthinfo(reguser *rup) {
  char eschost[2*HOSTLEN+1];

  dbescapestring(eschost,rup->lastuserhost->content,rup->lastuserhost->length);
  dbquery("UPDATE chanserv.users SET lastauth=%lu,lastuserhost='%s' WHERE ID=%u",
		  rup->lastauth,eschost,rup->ID);
}

void csdb_updatelastjoin(regchanuser *rcup) {
  dbquery("UPDATE chanserv.chanusers SET usetime=%lu WHERE userID=%u and channelID=%u",
	  rcup->usetime, rcup->user->ID, rcup->chan->ID);
}

void csdb_updatetopic(regchan *rcp) {
  char esctopic[TOPICLEN*2+5];

  if (rcp->topic) {
    dbescapestring(esctopic,rcp->topic->content,rcp->topic->length);
  } else {
    esctopic[0]='\0';
  }
  dbquery("UPDATE chanserv.channels SET topic='%s' WHERE ID=%u",esctopic,rcp->ID);
}

void csdb_updatechannel(regchan *rcp) {
  char escwelcome[WELCOMELEN*2+1];
  char esctopic[TOPICLEN*2+1];
  char esckey[70];
  char escreason[510];
  char esccomment[510];
  char escname[1000];

  dbescapestring(escname, rcp->index->name->content, rcp->index->name->length);

  if (rcp->welcome) 
    dbescapestring(escwelcome, rcp->welcome->content, 
			rcp->welcome->length);
  else
    escwelcome[0]='\0';

  if (rcp->topic)
    dbescapestring(esctopic, rcp->topic->content, rcp->topic->length);
  else
    esctopic[0]='\0';

  if (rcp->key)
    dbescapestring(esckey, rcp->key->content, rcp->key->length);
  else
    esckey[0]='\0';

  if (rcp->suspendreason) 
    dbescapestring(escreason, rcp->suspendreason->content, 
			rcp->suspendreason->length);
  else
    escreason[0]='\0';

  if (rcp->comment)
    dbescapestring(esccomment, rcp->comment->content, 
			rcp->comment->length);
  else
    esccomment[0]='\0';

  dbquery("UPDATE chanserv.channels SET name='%s', flags=%d, forcemodes=%d,"
		  "denymodes=%d, chanlimit=%d, autolimit=%d, banstyle=%d,"
		  "lastactive=%lu,statsreset=%lu, banduration=%lu, founder=%u,"
		  "addedby=%u, suspendby=%u, suspendtime=%lu, chantype=%d, totaljoins=%u,"
		  "tripjoins=%u, maxusers=%u, tripusers=%u,"
		  "welcome='%s', topic='%s', chankey='%s', suspendreason='%s',"
		  "comment='%s', lasttimestamp=%jd WHERE ID=%u",escname,rcp->flags,rcp->forcemodes,
		  rcp->denymodes,rcp->limit,rcp->autolimit, rcp->banstyle,
		  rcp->lastactive,rcp->statsreset,rcp->banduration,
		  rcp->founder, rcp->addedby, rcp->suspendby, rcp->suspendtime,
		  rcp->chantype,rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  escwelcome,esctopic,esckey,escreason,esccomment,(intmax_t)rcp->ltimestamp,rcp->ID);
}

void csdb_updatechannelcounters(regchan *rcp) {
  dbquery("UPDATE chanserv.channels SET "
		  "lastactive=%lu, totaljoins=%u,"
		  "tripjoins=%u, maxusers=%u, tripusers=%u "
		  "WHERE ID=%u",
		  rcp->lastactive,
		  rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  rcp->ID);
}

void csdb_updatechanneltimestamp(regchan *rcp) {
  dbquery("UPDATE chanserv.channels SET "
		  "lasttimestamp=%jd WHERE ID=%u",
		  (intmax_t)rcp->ltimestamp, rcp->ID);
}

void csdb_createchannel(regchan *rcp) {
  char escwelcome[WELCOMELEN*2+1];
  char esctopic[TOPICLEN*2+1];
  char esckey[70];
  char escreason[510];
  char esccomment[510];
  char escname[510];

  dbescapestring(escname, rcp->index->name->content, rcp->index->name->length);

  if (rcp->welcome) 
    dbescapestring(escwelcome, rcp->welcome->content, 
			rcp->welcome->length);
  else
    escwelcome[0]='\0';

  if (rcp->topic)
    dbescapestring(esctopic, rcp->topic->content, rcp->topic->length);
  else
    esctopic[0]='\0';

  if (rcp->key)
    dbescapestring(esckey, rcp->key->content, rcp->key->length);
  else
    esckey[0]='\0';

  if (rcp->suspendreason) 
    dbescapestring(escreason, rcp->suspendreason->content, 
			rcp->suspendreason->length);
  else
    escreason[0]='\0';

  if (rcp->comment)
    dbescapestring(esccomment, rcp->comment->content, 
			rcp->comment->length);
  else
    esccomment[0]='\0';

  dbquery("INSERT INTO chanserv.channels (ID, name, flags, forcemodes, denymodes,"
		  "chanlimit, autolimit, banstyle, created, lastactive, statsreset, "
		  "banduration, founder, addedby, suspendby, suspendtime, chantype, totaljoins, tripjoins,"
		  "maxusers, tripusers, welcome, topic, chankey, suspendreason, "
		  "comment, lasttimestamp) VALUES (%u,'%s',%d,%d,%d,%d,%d,%d,%lu,%lu,%lu,%lu,%u,"
		  "%u,%u,%lu,%d,%u,%u,%u,%u,'%s','%s','%s','%s','%s',%jd)",
		  rcp->ID, escname, rcp->flags,rcp->forcemodes,
		  rcp->denymodes,rcp->limit,rcp->autolimit, rcp->banstyle, rcp->created, 
		  rcp->lastactive,rcp->statsreset,rcp->banduration,
		  rcp->founder, rcp->addedby, rcp->suspendby, rcp->suspendtime,
		  rcp->chantype,rcp->totaljoins,rcp->tripjoins,
		  rcp->maxusers,rcp->tripusers,
		  escwelcome,esctopic,esckey,escreason,esccomment,(intmax_t)rcp->ltimestamp);
}

void csdb_deletechannel(regchan *rcp) {
  dbquery("DELETE FROM chanserv.channels WHERE ID=%u",rcp->ID);
  dbquery("DELETE FROM chanserv.chanusers WHERE channelID=%u",rcp->ID);
  dbquery("DELETE FROM chanserv.bans WHERE channelID=%u",rcp->ID);
}

void csdb_deleteuser(reguser *rup) {
  dbquery("DELETE FROM chanserv.users WHERE ID=%u",rup->ID);
  dbquery("DELETE FROM chanserv.chanusers WHERE userID=%u",rup->ID);
}

void csdb_updateuser(reguser *rup) {
  char escpassword[25];
  char escemail[210];
  char esclastuserhost[160];
  char escreason[510];
  char esccomment[510];
  char escinfo[210];
  char esclastemail[210];

  dbescapestring(escpassword, rup->password, strlen(rup->password));
   
  if (rup->email)
    dbescapestring(escemail, rup->email->content, rup->email->length);
  else
    escemail[0]='\0';

  if (rup->lastemail)
    dbescapestring(esclastemail, rup->lastemail->content, rup->lastemail->length);
  else
    esclastemail[0]='\0';

  if (rup->lastuserhost)
    dbescapestring(esclastuserhost, rup->lastuserhost->content, rup->lastuserhost->length);
  else
    esclastuserhost[0]='\0';

  if (rup->suspendreason)
    dbescapestring(escreason, rup->suspendreason->content, rup->suspendreason->length);
  else
    escreason[0]='\0';

  if (rup->comment)
    dbescapestring(esccomment, rup->comment->content, rup->comment->length);
  else
    esccomment[0]='\0';

  if (rup->info)
    dbescapestring(escinfo, rup->info->content, rup->info->length);
  else
    escinfo[0]='\0';

  dbquery("UPDATE chanserv.users SET lastauth=%lu, lastemailchng=%lu, flags=%u,"
		  "language=%u, suspendby=%u, suspendexp=%lu, suspendtime=%lu, lockuntil=%lu, password='%s', email='%s',"
		  "lastuserhost='%s', suspendreason='%s', comment='%s', info='%s', lastemail='%s', lastpasschng=%lu "
                  " WHERE ID=%u",
		  rup->lastauth, rup->lastemailchange, rup->flags, rup->languageid, rup->suspendby, rup->suspendexp,
		  rup->suspendtime, rup->lockuntil, escpassword, escemail, esclastuserhost, escreason, esccomment, escinfo, esclastemail,
                  rup->lastpasschange,
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

  dbescapestring(escusername, rup->username, strlen(rup->username));
  dbescapestring(escpassword, rup->password, strlen(rup->password));
  
  if (rup->email)
    dbescapestring(escemail, rup->email->content, rup->email->length);
  else
    escemail[0]='\0';

  if (rup->lastemail)
    dbescapestring(esclastemail, rup->lastemail->content, rup->lastemail->length);
  else
    esclastemail[0]='\0';

  if (rup->lastuserhost)
    dbescapestring(esclastuserhost, rup->lastuserhost->content, rup->lastuserhost->length);
  else
    esclastuserhost[0]='\0';

  if (rup->suspendreason)
    dbescapestring(escreason, rup->suspendreason->content, rup->suspendreason->length);
  else
    escreason[0]='\0';

  if (rup->comment)
    dbescapestring(esccomment, rup->comment->content, rup->comment->length);
  else
    esccomment[0]='\0';

  if (rup->info)
    dbescapestring(escinfo, rup->info->content, rup->info->length);
  else
    escinfo[0]='\0';

  dbquery("INSERT INTO chanserv.users (ID, username, created, lastauth, lastemailchng, "
		  "flags, language, suspendby, suspendexp, suspendtime, lockuntil, password, email, lastuserhost, "
		  "suspendreason, comment, info, lastemail, lastpasschng)"
		  "VALUES (%u,'%s',%lu,%lu,%lu,%u,%u,%u,%lu,%lu,%lu,'%s','%s','%s','%s','%s','%s','%s',%lu)",
		  rup->ID, escusername, rup->created, rup->lastauth, rup->lastemailchange, rup->flags, 
		  rup->languageid, rup->suspendby, rup->suspendexp, rup->suspendtime, rup->lockuntil,
		  escpassword, escemail, esclastuserhost, escreason, esccomment, escinfo, esclastemail,
                  rup->lastpasschange);
}  


void csdb_updatechanuser(regchanuser *rcup) {
  char escinfo[210];

  if (rcup->info) 
    dbescapestring(escinfo, rcup->info->content, rcup->info->length);
  else
    escinfo[0]='\0';

  dbquery("UPDATE chanserv.chanusers SET flags=%u, changetime=%lu, "
		  "usetime=%lu, info='%s' WHERE channelID=%u and userID=%u",
		  rcup->flags, rcup->changetime, rcup->usetime, escinfo, rcup->chan->ID,rcup->user->ID);
}

void csdb_createchanuser(regchanuser *rcup) {
  char escinfo[210];

  if (rcup->info) 
    dbescapestring(escinfo, rcup->info->content, rcup->info->length);
  else
    escinfo[0]='\0';
  
  dbquery("INSERT INTO chanserv.chanusers VALUES(%u, %u, %u, %lu, %lu, '%s')",
		  rcup->user->ID, rcup->chan->ID, rcup->flags, rcup->changetime, 
		  rcup->usetime, escinfo);
}

void csdb_deletechanuser(regchanuser *rcup) {
  dbquery("DELETE FROM chanserv.chanusers WHERE channelid=%u AND userID=%u",
		  rcup->chan->ID, rcup->user->ID);
}

void csdb_createban(regchan *rcp, regban *rbp) {
  char escreason[500];
  char banstr[100];
  char escban[200];

  strcpy(banstr,bantostring(rbp->cbp));
  dbescapestring(escban,banstr,strlen(banstr));
  
  if (rbp->reason)
    dbescapestring(escreason, rbp->reason->content, rbp->reason->length);
  else
    escreason[0]='\0';

  dbquery("INSERT INTO chanserv.bans (banID, channelID, userID, hostmask, "
		  "expiry, reason) VALUES (%u,%u,%u,'%s',%lu,'%s')", rbp->ID, rcp->ID, 
		  rbp->setby, escban, rbp->expiry, escreason);
}

void csdb_updateban(regchan *rcp, regban *rbp) {
  char escreason[500];
  char banstr[100];
  char escban[200];

  strcpy(banstr,bantostring(rbp->cbp));
  dbescapestring(escban,banstr,strlen(banstr));
  
  if (rbp->reason)
    dbescapestring(escreason, rbp->reason->content, rbp->reason->length);
  else
    escreason[0]='\0';

  dbquery("UPDATE chanserv.bans set channelID=%u, userID=%u, hostmask='%s', expiry=%lu, reason='%s' "
                  "WHERE banID=%u", rcp->ID, rbp->setby, escban, rbp->expiry, escreason, rbp->ID);
}

void csdb_deleteban(regban *rbp) {
  dbquery("DELETE FROM chanserv.bans WHERE banID=%u", rbp->ID);
}

void csdb_createmail(reguser *rup, int type) {
  char sqlquery[6000];
  char escemail[210];

  if (type == QMAIL_NEWEMAIL) {
    if (rup->email) {
      dbescapestring(escemail, rup->email->content, rup->email->length);
      sprintf(sqlquery, "INSERT INTO chanserv.email (userID, emailType, prevEmail) "
	      "VALUES (%u,%u,'%s')", rup->ID, type, escemail);
    }
  } else {
    sprintf(sqlquery, "INSERT INTO chanserv.email (userID, emailType) VALUES (%u,%u)", rup->ID, type);
  }

  dbquery("%s", sqlquery);
}

void csdb_deletemaildomain(maildomain *mdp) {
  dbquery("DELETE FROM chanserv.maildomain WHERE ID=%u", mdp->ID);
}

void csdb_createmaildomain(maildomain *mdp) {
  char escdomain[210];
  dbescapestring(escdomain, mdp->name->content, mdp->name->length);

  dbquery("INSERT INTO chanserv.maildomain (id, name, domainlimit, actlimit, flags) VALUES(%u, '%s', %u, %u, %u)", mdp->ID,escdomain,mdp->limit,mdp->actlimit,mdp->flags);
}

void csdb_updatemaildomain(maildomain *mdp) {
  char escdomain[210];
  dbescapestring(escdomain, mdp->name->content, mdp->name->length);

  dbquery("UPDATE chanserv.maildomain SET domainlimit=%u, actlimit=%u, flags=%u, name='%s' WHERE ID=%u", mdp->limit,mdp->actlimit,mdp->flags,escdomain,mdp->ID);
}

void csdb_chanlevhistory_insert(regchan *rcp, nick *np, reguser *trup, flag_t oldflags, flag_t newflags) {
  reguser *rup=getreguserfromnick(np);

  dbquery("INSERT INTO chanserv.chanlevhistory (userID, channelID, targetID, changetime, authtime, "
    "oldflags, newflags) VALUES (%u, %u, %u, %lu, %lu, %u, %u)",  rup->ID, rcp->ID, trup->ID, getnettime(), np->accountts,
    oldflags, newflags);
}

void csdb_accounthistory_insert(nick *np, char *oldpass, char *newpass, char *oldemail, char *newemail) {
  reguser *rup=getreguserfromnick(np);
  char escoldpass[PASSLEN*2+5];
  char escnewpass[PASSLEN*2+5];
  char escoldemail[EMAILLEN*2+5];
  char escnewemail[EMAILLEN*2+5];

  if (!rup || UHasOperPriv(rup))
    return;

  if (oldpass)
    dbescapestring(escoldpass, oldpass, CSMIN(strlen(oldpass), PASSLEN));
  else
    escoldpass[0]='\0';

  if (newpass)
    dbescapestring(escnewpass, newpass, CSMIN(strlen(newpass), PASSLEN));
  else
    escnewpass[0]='\0';

  if (oldemail)
    dbescapestring(escoldemail, oldemail, CSMIN(strlen(oldemail), EMAILLEN));
  else
    escoldemail[0]='\0';
  if (newemail)
    dbescapestring(escnewemail, newemail, CSMIN(strlen(newemail), EMAILLEN));
  else
    escnewemail[0]='\0';

  dbquery("INSERT INTO chanserv.accounthistory (userID, changetime, authtime, oldpassword, newpassword, oldemail, "
    "newemail) VALUES (%u, %lu, %lu, '%s', '%s', '%s', '%s')", rup->ID, getnettime(), np->accountts, escoldpass, escnewpass,
    escoldemail, escnewemail);
}

void csdb_cleanuphistories() {
  time_t expire_time=getnettime()-604800;

  Error("chanserv", ERR_INFO, "Cleaning histories.");
  dbquery("DELETE FROM chanserv.authhistory WHERE authtime < %lu", expire_time);
  dbquery("DELETE FROM chanserv.chanlevhistory WHERE changetime < %lu", expire_time);
  dbquery("DELETE FROM chanserv.accounthistory WHERE changetime < %lu", expire_time);
}

void csdb_deletemaillock(maillock *mlp) {
  dbquery("DELETE FROM chanserv.maillocks WHERE ID=%u", mlp->id);
}

void csdb_createmaillock(maillock *mlp) {
  char escpattern[1024], escreason[1024];

  dbescapestring(escpattern, mlp->pattern->content, mlp->pattern->length);

  if (mlp->reason)
    dbescapestring(escreason, mlp->reason->content, mlp->reason->length);
  else
    escreason[0]='\0';

  dbquery("INSERT INTO chanserv.maillocks (id, pattern, reason, createdby, created) VALUES(%u, '%s', '%s', %u, %jd)",
          mlp->id,escpattern,escreason,mlp->createdby,(intmax_t)mlp->created);
}

void csdb_updatemaillock(maillock *mlp) {
  char escpattern[1024], escreason[1024];

  dbescapestring(escpattern, mlp->pattern->content, mlp->pattern->length);

  if (mlp->reason)
    dbescapestring(escreason, mlp->reason->content, mlp->reason->length);
  else
    escreason[0]='\0';

  dbquery("UPDATE chanserv.maillocks SET pattern='%s', reason='%s', createdby=%u, created=%jd WHERE ID=%u", escpattern, escreason, mlp->createdby, (intmax_t)mlp->created, mlp->id);
}

