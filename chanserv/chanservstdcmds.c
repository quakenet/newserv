/*
 * This contains Q9's "built in" commands and CTCP handlers
 */

#include "chanserv.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../pqsql/pqsql.h"

#include <string.h>

int cs_dorehash(void *source, int cargc, char **cargv) {
  nick *sender=source;
  Command *cmdlist[200];
  int i,n;
  
  /* Reload the response text first */
  loadmessages();

  /* Now the commands */
  n=getcommandlist(cscommands, cmdlist, 200);
  
  for(i=0;i<n;i++)
    loadcommandsummary(cmdlist[i]);

  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int cs_doquit(void *source, int cargc, char **cargv) {
  char *reason="Leaving";
  nick *sender=(nick *)source;

  if (cargc>0) {
    reason=cargv[0];
  }

  chanservstdmessage(sender, QM_DONE);

  deregisterlocaluser(chanservnick, reason);  
  scheduleoneshot(time(NULL)+1,&chanservreguser,NULL);

  return CMD_OK;
}

int cs_dorename(void *source, int cargc, char **cargv) {
  char newnick[NICKLEN+1];
  nick *sender=source;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "rename");
    return CMD_ERROR;
  }
  
  strncpy(newnick,cargv[0],NICKLEN);
  newnick[NICKLEN]='\0';
  
  renamelocaluser(chanservnick, newnick);
  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int cs_doshowcommands(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup;
  Command *cmdlist[200];
  int i,n;
  int lang;
  char *message;
  cmdsummary *summary;
  
  n=getcommandlist(cscommands, cmdlist, 200);
  rup=getreguserfromnick(sender);

  if (!rup)
    lang=0;
  else
    lang=rup->languageid;

  chanservstdmessage(sender, QM_COMMANDLIST);

  for (i=0;i<n;i++) {
    if (cargc>0 && !match2strings(cargv[0],cmdlist[i]->command->content))
      continue;

    /* Don't list aliases */
    if (cmdlist[i]->level & QCMD_ALIAS)
      continue;
    
    /* Check that this user can use this command.. */
    if ((cmdlist[i]->level & QCMD_AUTHED) && !rup)
      continue;

    if ((cmdlist[i]->level & QCMD_NOTAUTHED) && rup)
      continue;

    if ((cmdlist[i]->level & QCMD_HELPER) && 
	(!rup || !UHasHelperPriv(rup)))
      continue;

    if ((cmdlist[i]->level & QCMD_OPER) &&
	(!rup || !UHasOperPriv(rup) || !IsOper(sender)))
      continue;

    if ((cmdlist[i]->level & QCMD_ADMIN) &&
	(!rup || !UHasAdminPriv(rup) || !IsOper(sender)))
      continue;
    
    if ((cmdlist[i]->level & QCMD_DEV) &&
	(!rup || !UIsDev(rup) || !IsOper(sender)))
      continue;
    
    summary=(cmdsummary *)cmdlist[i]->ext;
    
    if (summary->bylang[lang]) {
      message=summary->bylang[lang]->content;
    } else if (summary->bylang[0]) {
      message=summary->bylang[0]->content;
    } else {
      message=summary->def->content;
    }
    
    chanservsendmessage(sender, "%-20s %s",cmdlist[i]->command->content, message);
  }

  chanservstdmessage(sender, QM_ENDOFLIST);

  return CMD_OK;
}

int cs_dohelp(void *source, int cargc, char **cargv) {
  nick *sender=source;
  Command *cmd;

  if (cargc==0)
    return cs_doshowcommands(source,cargc,cargv);
  
  if (!(cmd=findcommandintree(cscommands, cargv[0], 1))) {
    chanservstdmessage(sender, QM_UNKNOWNCMD, cargv[0]);
    return CMD_ERROR;
  }

  csdb_dohelp(sender, cmd);

  return CMD_OK;
}


int cs_doctcpping(void *source, int cargc, char **cargv) {
  char *nullbuf="\001";
  
  sendnoticetouser(chanservnick, source, "%cPING %s",
		   1, cargc?cargv[0]:nullbuf);

  return CMD_OK;
}
  
int cs_doctcpversion(void *source, int cargc, char **cargv) {
  sendnoticetouser(chanservnick, source, "\01VERSION Q9 version %s (Compiled on " __DATE__ ")  (C) 2002-3 David Mansell (splidge)\01", QVERSION);
  sendnoticetouser(chanservnick, source, "\01VERSION Built on newserv version 1.00.  (C) 2002-3 David Mansell (splidge)\01");

  return CMD_OK;
}

int cs_doversion(void *source, int cargc, char **cargv) {
  chanservsendmessage((nick *)source, "Q9 version %s (Compiled on " __DATE__ ") (C) 2002-3 David Mansell (splidge)", QVERSION);
  chanservsendmessage((nick *)source, "Built on newserv version 1.00.  (C) 2002-3 David Mansell (splidge)");
  return CMD_OK;
}

int cs_doctcpgender(void *source, int cargc, char **cargv) {
  sendnoticetouser(chanservnick, source, "\1GENDER Anyone who has a bitch mode has to be female ;)\1");

  return CMD_OK;
}

void csdb_dohelp_real(PGconn *, void *);

struct helpinfo {
  unsigned int numeric;
  sstring *commandname;
};

/* Help stuff */
void csdb_dohelp(nick *np, Command *cmd) {
  struct helpinfo *hip;

  hip=(struct helpinfo *)malloc(sizeof(struct helpinfo));

  hip->numeric=np->numeric;
  hip->commandname=getsstring(cmd->command->content, cmd->command->length);

  pqasyncquery(csdb_dohelp_real, (void *)hip, 
		  "SELECT languageID, fullinfo from help where lower(command)=lower('%s')",cmd->command->content);
}

void csdb_dohelp_real(PGconn *dbconn, void *arg) {
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

void csdb_doauthhistory_real(PGconn *dbconn,void *arg);
void csdb_dochanlevhistory_real(PGconn *dbconn,void *arg);
void csdb_doaccounthistory_real(PGconn *dbconn,void *arg);
void csc_dorollbackchan_real(PGconn *dbconn,void *arg);
void csdb_dorollbackaccount_real(PGconn *dbconn,void *arg);

struct authhistoryinfo {
  unsigned int numeric;
  unsigned int userID;
};

void csdb_retreiveauthhistory(nick *np, reguser *rup, int limit) {
  struct authhistoryinfo *ahi;

  ahi=(struct authhistoryinfo *)malloc(sizeof(struct authhistoryinfo));
  ahi->numeric=np->numeric;
  ahi->userID=rup->ID;
  pqasyncquery(csdb_doauthhistory_real, (void *)ahi,
    "SELECT userID, nick, username, host, authtime, disconnecttime from authhistory where "
    "userID=%u order by authtime desc limit %d", rup->ID, limit);
}

void csdb_doauthhistory_real(PGconn *dbconn, void *arg) {
  struct authhistoryinfo *ahi=(struct authhistoryinfo*)arg;
  nick *np=getnickbynumeric(ahi->numeric);
  reguser *rup;
  char *ahnick, *ahuser, *ahhost;
  time_t ahauthtime, ahdisconnecttime;
  PGresult *pgres;
  int i, num, count=0;
  struct tm *tmp;
  char tbuf1[15], tbuf2[15];

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv", ERR_ERROR, "Error loading auth history data.");
    return;
  }

  if (PQnfields(pgres) != 6) {
    Error("chanserv", ERR_ERROR, "Auth history data format error.");
    return;
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    free(ahi);
    return;
  }

  if (!(rup=getreguserfromnick(np))) {
    PQclear(pgres);
    free(ahi);
    return;
  }
  chanservstdmessage(np, QM_AUTHHISTORYHEADER);
  for (i=0; i<num; i++) {
    if (!UHasHelperPriv(rup) && (strtoul(PQgetvalue(pgres, i, 0), NULL, 10) != rup->ID)) {
      PQclear(pgres);
      free(ahi);
      return;
    }
    ahnick=PQgetvalue(pgres, i, 1);
    ahuser=PQgetvalue(pgres, i, 2);
    ahhost=PQgetvalue(pgres, i, 3);
    ahauthtime=strtoul(PQgetvalue(pgres, i, 4), NULL, 10);
    ahdisconnecttime=strtoul(PQgetvalue(pgres, i, 5), NULL, 10);
    tmp=localtime(&ahauthtime);
    strftime(tbuf1, 15, "%d/%m/%y %H:%M", tmp);
    if (ahdisconnecttime) {
      tmp=localtime(&ahdisconnecttime);
      strftime(tbuf2, 15, "%d/%m/%y %H:%M", tmp);
    }
    chanservsendmessage(np, "#%-6d %-15s %-15s %-15s %s@%s", ++count, tbuf1, ahdisconnecttime?tbuf2:"never", ahnick, ahuser, ahhost);
  }
  chanservstdmessage(np, QM_ENDOFLIST);

  PQclear(pgres);
  free(ahi);
}

void csdb_retreivechanlevhistory(nick *np, regchan *rcp, time_t starttime) {
  pqasyncquery(csdb_dochanlevhistory_real, (void *)np->numeric,
    "SELECT userID, channelID, targetID, changetime, authtime, oldflags, newflags from chanlevhistory where "
    "channelID=%u and changetime>%lu order by changetime desc limit 1000", rcp->ID, starttime);
}
void csdb_dochanlevhistory_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned int)arg);
  reguser *rup, *crup1, *crup2;
  unsigned int userID, channelID, targetID;
  time_t changetime, authtime;
  flag_t oldflags, newflags;
  PGresult *pgres;
  int i, num, count=0;
  struct tm *tmp;
  char tbuf[15], fbuf[18];

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv", ERR_ERROR, "Error loading chanlev history data.");
    return;
  }

  if (PQnfields(pgres) != 7) {
    Error("chanserv", ERR_ERROR, "Chanlev history data format error.");
    return;
  }
  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    return;
  }

  if (!(rup=getreguserfromnick(np)) || !UHasHelperPriv(rup)) {
    PQclear(pgres);
    return;
  }

  chanservsendmessage(np, "Number: Time:           Changing user:  Changed user:   Old flags:      New flags:");
  for (i=0; i<num; i++) {
    userID=strtoul(PQgetvalue(pgres, i, 0), NULL, 10);
    channelID=strtoul(PQgetvalue(pgres, i, 1), NULL, 10);
    targetID=strtoul(PQgetvalue(pgres, i, 2), NULL, 10);
    changetime=strtoul(PQgetvalue(pgres, i, 3), NULL, 10);
    authtime=strtoul(PQgetvalue(pgres, i, 4), NULL, 10);
    oldflags=strtoul(PQgetvalue(pgres, i, 5), NULL, 10);
    newflags=strtoul(PQgetvalue(pgres, i, 6), NULL, 10);
    tmp=localtime(&changetime);
    strftime(tbuf, 15, "%d/%m/%y %H:%M", tmp);
    strncpy(fbuf, printflags(oldflags, rcuflags), 17);
    fbuf[17]='\0';
    chanservsendmessage(np, "#%-6d %-15s %-15s %-15s %-15s %s", ++count, tbuf,
      (crup1=findreguserbyID(userID))?crup1->username:"Unknown", (crup2=findreguserbyID(targetID))?crup2->username:"Unknown",
      fbuf, printflags(newflags, rcuflags));
  }
  chanservstdmessage(np, QM_ENDOFLIST);

  PQclear(pgres);
}

void csdb_rollbackchanlevhistory(nick *np, regchan *rcp, reguser* rup, time_t starttime) {
  if (rup)
    pqasyncquery(csc_dorollbackchan_real, (void *)np->numeric,
      "SELECT userID, channelID, targetID, changetime, authtime, oldflags, newflags from chanlevhistory where "
      "userID=%u and channelID=%u and changetime>%lu order by changetime desc limit 1000", rup->ID, rcp->ID, starttime);
  else
    pqasyncquery(csc_dorollbackchan_real, (void *)np->numeric,
      "SELECT userID, channelID, targetID, changetime, authtime, oldflags, newflags from chanlevhistory where "
      "channelID=%u and changetime>%lu order by changetime desc limit 1000", rcp->ID, starttime);
}

void csc_dorollbackchan_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned int)arg);
  reguser *rup, *crup1, *crup2;
  chanindex *cip;
  regchan *rcp=NULL;
  regchanuser *rcup;
  unsigned int userID, channelID, targetID;
  time_t changetime, authtime;
  flag_t oldflags, newflags;
  PGresult *pgres;
  int i, j, num, count=0, newuser;
  struct tm *tmp;
  char tbuf[15], fbuf[18];

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv", ERR_ERROR, "Error loading chanlev history data.");
    return;
  }

  if (PQnfields(pgres) != 7) {
    Error("chanserv", ERR_ERROR, "Chanlev history data format error.");
    return;
  }

  num=PQntuples(pgres);

  if (!np) {
    Error("chanserv", ERR_ERROR, "No nick pointer in rollback.");
    PQclear(pgres);
    return;
  }
  if (!(rup=getreguserfromnick(np)) || !UHasOperPriv(rup)) {
    Error("chanserv", ERR_ERROR, "No reguser pointer or oper privs in rollback.");
    PQclear(pgres);
    return;
  }

  for (i=0; i<num; i++) {
    userID=strtoul(PQgetvalue(pgres, i, 0), NULL, 10);
    channelID=strtoul(PQgetvalue(pgres, i, 1), NULL, 10);

    if (!rcp) {
      for (j=0; j<CHANNELHASHSIZE && !rcp; j++) {
        for (cip=chantable[j]; cip && !rcp; cip=cip->next) {
          if (!cip->exts[chanservext])
            continue;

          if (((regchan*)cip->exts[chanservext])->ID == channelID)
            rcp=(regchan*)cip->exts[chanservext];
        }
      }

      if (!rcp) {
        Error("chanserv", ERR_ERROR, "No regchan pointer or oper privs in rollback.");
        PQclear(pgres);
        return;
      }

      cip=rcp->index;

      chanservsendmessage(np, "Attempting to roll back %s:", cip->name->content);
    }
    targetID=strtoul(PQgetvalue(pgres, i, 2), NULL, 10);
    changetime=strtoul(PQgetvalue(pgres, i, 3), NULL, 10);
    authtime=strtoul(PQgetvalue(pgres, i, 4), NULL, 10);
    oldflags=strtoul(PQgetvalue(pgres, i, 5), NULL, 10);
    newflags=strtoul(PQgetvalue(pgres, i, 6), NULL, 10);
    strncpy(fbuf, printflags(newflags, rcuflags), 17);
    fbuf[17]='\0';
    crup1=findreguserbyID(userID);
    crup2=findreguserbyID(targetID);

    if (!crup2) {
      chanservsendmessage(np, "Affected user (ID: %d) is no longer in database, continuing...", targetID);
      continue;
    }

    if (!(rcup=findreguseronchannel(rcp, crup2))) {
      rcup=getregchanuser();
      rcup->user=crup2;
      rcup->chan=rcp;
      rcup->flags=0;
      rcup->changetime=time(NULL);
      rcup->usetime=0;
      rcup->info=NULL;
      newuser=1;
    }
    else
      newuser=0;
    csdb_chanlevhistory_insert(rcp, np, rcup->user, rcup->flags, oldflags);
    rcup->flags=oldflags;
    chanservsendmessage(np, "%s user flags for %s (%s -> %s)", newflags?oldflags?"Restoring":"Deleting":"Readding",
      crup2->username, fbuf, printflags(oldflags, rcuflags));

    if (rcup->flags) {
      if (newuser) {
        addregusertochannel(rcup);
        csdb_createchanuser(rcup);
      }
      else
        csdb_updatechanuser(rcup);
    }
    else {
      if (!newuser) {
        csdb_deletechanuser(rcup);
        delreguserfromchannel(rcp, crup2);
      }

      freesstring(rcup->info);
      freeregchanuser(rcup);
      rcup=NULL;

      for (j=0; j<REGCHANUSERHASHSIZE; j++)
        if (rcp->regusers[j])
          break;

      if (i==REGCHANUSERHASHSIZE) {
        cs_log(np, "DELCHAN %s (Cleared chanlev from rollback)", cip->name->content);
        chanservsendmessage(np, "Rollback cleared chanlev list, channel deleted.");
        rcp=NULL;
      }
    }
  }
  chanservstdmessage(np, QM_DONE);

  PQclear(pgres);
}

void csdb_retreiveaccounthistory(nick *np, reguser *rup, int limit) {
  pqasyncquery(csdb_doaccounthistory_real, (void *)np->numeric,
    "SELECT userID, changetime, authtime, oldpassword, newpassword, oldemail, newemail from accounthistory where "
    "userID=%u order by changetime desc limit %d", rup->ID, limit);
}

void csdb_doaccounthistory_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned int)arg);
  reguser *rup;
  unsigned int userID;
  char *oldpass, *newpass, *oldemail, *newemail;
  time_t changetime, authtime;
  PGresult *pgres;
  int i, num, count=0;
  struct tm *tmp;
  char tbuf[15];

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv", ERR_ERROR, "Error loading account history data.");
    return;
  }

  if (PQnfields(pgres) != 7) {
    Error("chanserv", ERR_ERROR, "Account history data format error.");
    return;
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    return;
  }

  if (!(rup=getreguserfromnick(np)) || !UHasOperPriv(rup)) {
    Error("chanserv", ERR_ERROR, "No reguser pointer or oper privs in account history.");
    PQclear(pgres);
    return;
  }

  chanservsendmessage(np, "Number: Time:           Old password:  New password:  Old email:                     New email:");
  for (i=0; i<num; i++) {
    userID=strtoul(PQgetvalue(pgres, i, 0), NULL, 10);
    changetime=strtoul(PQgetvalue(pgres, i, 1), NULL, 10);
    authtime=strtoul(PQgetvalue(pgres, i, 2), NULL, 10);
    oldpass=PQgetvalue(pgres, i, 3);
    newpass=PQgetvalue(pgres, i, 4);
    oldemail=PQgetvalue(pgres, i, 5);
    newemail=PQgetvalue(pgres, i, 6);
    tmp=localtime(&changetime);
    strftime(tbuf, 15, "%d/%m/%y %H:%M", tmp);
    chanservsendmessage(np, "#%-6d %-15s %-14s %-14s %-30s %s", ++count, tbuf, oldpass, newpass, oldemail, newemail);
  }
  chanservstdmessage(np, QM_ENDOFLIST);

  PQclear(pgres);
}

void csdb_rollbackaccounthistory(nick *np, reguser* rup, time_t starttime) {
  pqasyncquery(csdb_dorollbackaccount_real, (void *)np->numeric,
    "SELECT userID, changetime, authtime, oldpassword, newpassword, oldemail, newemail from accounthistory where "
    "userID=%u and changetime>%lu order by changetime desc limit 10", rup->ID, starttime);
}

void csdb_dorollbackaccount_real(PGconn *dbconn, void *arg) {
  nick *np=getnickbynumeric((unsigned int)arg);
  reguser *rup;
  unsigned int userID;
  char *oldpass, *newpass, *oldemail, *newemail;
  time_t changetime, authtime;
  PGresult *pgres;
  int i, num, count=0;
  struct tm *tmp;
  char tbuf[15];

  pgres=PQgetResult(dbconn);

  if (PQresultStatus(pgres) != PGRES_TUPLES_OK) {
    Error("chanserv", ERR_ERROR, "Error loading account rollback data.");
    return;
  }

  if (PQnfields(pgres) != 7) {
    Error("chanserv", ERR_ERROR, "Account rollback data format error.");
    return;
  }

  num=PQntuples(pgres);

  if (!np) {
    PQclear(pgres);
    return;
  }

  if (!(rup=getreguserfromnick(np)) || !UHasOperPriv(rup)) {
    Error("chanserv", ERR_ERROR, "No reguser pointer or oper privs in rollback account.");
    PQclear(pgres);
    return;
  }

  chanservsendmessage(np, "Attempting to rollback account %s:", rup->username);
  for (i=0; i<num; i++) {
    userID=strtoul(PQgetvalue(pgres, i, 0), NULL, 10);
    changetime=strtoul(PQgetvalue(pgres, i, 1), NULL, 10);
    authtime=strtoul(PQgetvalue(pgres, i, 2), NULL, 10);
    oldpass=PQgetvalue(pgres, i, 3);
    newpass=PQgetvalue(pgres, i, 4);
    oldemail=PQgetvalue(pgres, i, 5);
    newemail=PQgetvalue(pgres, i, 6);
    if (strlen(newpass) > 0) {
      setpassword(rup, oldpass);
      chanservsendmessage(np, "Restoring old password (%s -> %s)", newpass, oldpass);
    }
    else if (strlen(newemail) > 0) {
      freesstring(rup->email);
      rup->email=getsstring(oldemail, EMAILLEN);
      rup->lastemailchange=changetime;
      chanservsendmessage(np, "Restoring old email (%s -> %s)", newemail, oldemail);
    }
  }
  csdb_updateuser(rup);
  chanservstdmessage(np, QM_DONE);

  PQclear(pgres);
}

