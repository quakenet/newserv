/*
 * CMDNAME: chanophistory
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 1
 * CMDDESC: Displays a list of who has been opped on a channel recently with account names.
 * CMDFUNC: csc_dochanophistory
 * CMDPROTO: int csc_dochanophistory(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: CHANOPHISTORY <channel>
 * CMDHELP: Displays a list of users who have recently been opped on a channel by the
 * CMDHELP: service, along with the account name responsible for the opping.  Usually
 * CMDHELP: this is the account the user being opped was using, but in the case of the 
 * CMDHELP: OP command being used to op other users, the account used by the user issuing
 * CMDHELP: the OP command will be shown.  Where:
 * CMDHELP: channel  - the channel to use
 * CMDHELP: CHANOPHISTORY requires operator (+o) access on the named channel.
 */

#include "../chanserv.h"

int csc_dochanophistory(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  reguser *opact;
  unsigned int i;
  unsigned int found=0;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "chanophistory");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "chanophistory",
			   QPRIV_VIEWFULLCHANLEV, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  for (i=0;i<CHANOPHISTORY;i++) {
    unsigned int idx=(i+rcp->chanoppos)%CHANOPHISTORY;

    if (!*rcp->chanopnicks[idx])
      continue;
      
    if (!found) {    
      chanservstdmessage(sender, QM_CHANOPHISTORYHEADER, cip->name->content);
      found=1;
    }

    opact=findreguserbyID(rcp->chanopaccts[idx]);
    chanservsendmessage(sender, "%-15s %-15s", rcp->chanopnicks[idx], opact?opact->username:"(unknown)");
  }
  if (!found) {
    chanservstdmessage(sender, QM_NOCHANOPHISTORY, cip->name->content);
  } else {
    chanservstdmessage(sender, QM_ENDOFLIST);
  }

  return CMD_OK;
}
