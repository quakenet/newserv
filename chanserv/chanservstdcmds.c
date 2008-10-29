/*
 * This contains Q9's "built in" commands and CTCP handlers
 */

#include "chanserv.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../dbapi/dbapi.h"

#include <string.h>
#include <stdio.h>

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

int cs_dosetquitreason(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;

  if (cargc<0) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "setquitreason");
    return CMD_ERROR;
  }
  
  if (cs_quitreason)
    freesstring(cs_quitreason);
 
  cs_quitreason=getsstring(cargv[0], 250);

  chanservstdmessage(sender, QM_DONE);

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
  char cmdbuf[50];
  char *ct;
  
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
    
    if (rup && UHasHelperPriv(rup)) {
      if (cmdlist[i]->level & QCMD_DEV) {
        sprintf(cmdbuf,"+d %s",cmdlist[i]->command->content);
      } else if(cmdlist[i]->level & QCMD_ADMIN) {
        sprintf(cmdbuf,"+a %s",cmdlist[i]->command->content);
      } else if(cmdlist[i]->level & QCMD_OPER) {
        sprintf(cmdbuf,"+o %s",cmdlist[i]->command->content);
      } else if(cmdlist[i]->level & QCMD_HELPER) {
        sprintf(cmdbuf,"+h %s",cmdlist[i]->command->content);
      } else {
        sprintf(cmdbuf,"   %s",cmdlist[i]->command->content);
      }
      ct=cmdbuf;
    } else {
      ct=cmdlist[i]->command->content;
    }
    
    if (summary->bylang[lang]) {
      message=summary->bylang[lang]->content;
    } else if (summary->bylang[0]) {
      message=summary->bylang[0]->content;
    } else {
      message=summary->def->content;
    }
    
    chanservsendmessage(sender, "%-20s %s",ct, message);
  }

  chanservstdmessage(sender, QM_ENDOFLIST);

  return CMD_OK;
}

int cs_sendhelp(nick *sender, char *thecmd, int oneline) {
  Command *cmd;
  cmdsummary *sum;
  reguser *rup;
  
  if (!(cmd=findcommandintree(cscommands, thecmd, 1))) {
    chanservstdmessage(sender, QM_UNKNOWNCMD, thecmd);
    return CMD_ERROR;
  }
  
/*  Disable database help for now - splidge
  csdb_dohelp(sender, cmd); */
  
  rup=getreguserfromnick(sender);
  
  /* Don't showhelp for privileged users to others.. */
  if (((cmd->level & QCMD_HELPER) && (!rup || !UHasHelperPriv(rup))) ||
      ((cmd->level & QCMD_OPER) && (!rup || !UHasOperPriv(rup))) ||
      ((cmd->level & QCMD_ADMIN) && (!rup || !UHasAdminPriv(rup))) ||
      ((cmd->level & QCMD_DEV) && (!rup || !UIsDev(rup)))) {
    chanservstdmessage(sender, QM_NOHELP, cmd->command->content);
    return CMD_OK;
  }

  sum=cmd->ext;

  if (sum->defhelp && *(sum->defhelp)) {
    if (oneline) {
      chanservsendmessageoneline(sender, "%s", sum->defhelp);
    } else {
      chanservsendmessage(sender, "%s", sum->defhelp);
    }
  } else {
    if (!oneline)
      chanservstdmessage(sender, QM_NOHELP, cmd->command->content);
  }

  return CMD_OK;
}


int cs_dohelp(void *source, int cargc, char **cargv) {
  nick *sender=source;

  if (cargc==0)
    return cs_doshowcommands(source,cargc,cargv);
  
  return cs_sendhelp(sender, cargv[0], 0);
}


int cs_doctcpping(void *source, int cargc, char **cargv) {
  char *nullbuf="\001";
  
  sendnoticetouser(chanservnick, source, "%cPING %s",
		   1, cargc?cargv[0]:nullbuf);

  return CMD_OK;
}
  
int cs_doctcpversion(void *source, int cargc, char **cargv) {
  sendnoticetouser(chanservnick, source, "\01VERSION Q9 version %s (Compiled on " __DATE__ ")  (C) 2002-8 David Mansell (splidge) and others.\01", QVERSION);
  sendnoticetouser(chanservnick, source, "\01VERSION Built on newserv.  (C) 2002-8 David Mansell (splidge) and others.\01");

  return CMD_OK;
}

int cs_doversion(void *source, int cargc, char **cargv) {
  chanservsendmessage((nick *)source, "Q9 version %s (Compiled on " __DATE__ ") (C) 2002-8 David Mansell (splidge) and others.", QVERSION);
  chanservsendmessage((nick *)source, "Built on newserv.  (C) 2002-8 David Mansell (splidge) and others.");
  return CMD_OK;
}

int cs_doctcpgender(void *source, int cargc, char **cargv) {
  sendnoticetouser(chanservnick, source, "\1GENDER Anyone who has a bitch mode has to be female ;)\1");

  return CMD_OK;
}

void csdb_dohelp_real(DBConn *, void *);

struct helpinfo {
  unsigned int numeric;
  sstring *commandname;
  Command *cmd;
};

/* Help stuff */
void csdb_dohelp(nick *np, Command *cmd) {
  struct helpinfo *hip;

  hip=(struct helpinfo *)malloc(sizeof(struct helpinfo));

  hip->numeric=np->numeric;
  hip->commandname=getsstring(cmd->command->content, cmd->command->length);
  hip->cmd=cmd;

  q9asyncquery(csdb_dohelp_real, (void *)hip, 
		  "SELECT languageID, fullinfo from chanserv.help where lower(command)=lower('%s')",cmd->command->content);
}

void csdb_dohelp_real(DBConn *dbconn, void *arg) {
  struct helpinfo *hip=arg;
  nick *np=getnickbynumeric(hip->numeric);
  reguser *rup;
  char *result;
  DBResult *pgres;
  int j,lang=0;
  
  if(!dbconn) {
    freesstring(hip->commandname);
    free(hip);
    return; 
  }

  pgres=dbgetresult(dbconn);

  if (!dbquerysuccessful(pgres)) {
    Error("chanserv",ERR_ERROR,"Error loading help text.");
    freesstring(hip->commandname);
    free(hip);
    return; 
  }

  if (dbnumfields(pgres)!=2) {
    Error("chanserv",ERR_ERROR,"Help text format error.");
    dbclear(pgres);
    freesstring(hip->commandname);
    free(hip);
    return;
  }
  
  if (!np) {
    dbclear(pgres);
    freesstring(hip->commandname);
    free(hip);
    return;
  }

  if ((rup=getreguserfromnick(np)))
    lang=rup->languageid;

  result=NULL;
  
  while(dbfetchrow(pgres)) {
    j=strtoul(dbgetvalue(pgres,0),NULL,10);
    if ((j==0 && result==NULL) || (j==lang)) {
      result=dbgetvalue(pgres,1);
      if(strlen(result)==0)
	result=NULL;
    }
  }

  if (result) {
    chanservsendmessage(np, "%s", result);
  } else {
    cmdsummary *sum=hip->cmd->ext;
    if (sum->defhelp && *(sum->defhelp)) {
      chanservsendmessage(np, "%s", sum->defhelp);
    } else {
      chanservstdmessage(np, QM_NOHELP, hip->commandname->content);
    }
  }
  
  freesstring(hip->commandname);
  free(hip);
  dbclear(pgres);
}
