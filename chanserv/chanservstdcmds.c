/*
 * This contains Q9's "built in" commands and CTCP handlers
 */

#include "chanserv.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"

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
  sendnoticetouser(chanservnick, source, "\01VERSION Q9 version 0.75.  (C) 2002-3 David Mansell (splidge)\01");
  sendnoticetouser(chanservnick, source, "\01VERSION Built on newserv version 1.00.  (C) 2002-3 David Mansell (splidge)\01");

  return CMD_OK;
}

int cs_doctcpgender(void *source, int cargc, char **cargv) {
  sendnoticetouser(chanservnick, source, "\1GENDER Anyone who has a bitch mode has to be female ;)\1");

  return CMD_OK;
}

