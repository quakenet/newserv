/*
 * CMDNAME: giveowner
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 3
 * CMDDESC: Gives total control over a channel to another user.
 * CMDFUNC: csc_dogiveowner
 * CMDPROTO: int csc_dogiveowner(void *source, int cargc, char **cargv);
 */

#include "../chanserv.h"
#include "../../nick/nick.h"
#include "../../lib/flags.h"
#include "../../lib/irc_string.h"
#include "../../channel/channel.h"
#include "../../parser/parser.h"
#include "../../irc/irc.h"
#include "../../localuser/localuserchannel.h"
#include <string.h>
#include <stdio.h>

int csc_dogiveowner(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  regchanuser *rcup;
  reguser *rup=getreguserfromnick(sender), *target;
  char flagbuf[30];
  flag_t oldflags;
  unsigned int thehash;
  char hashstr[100];
  
  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "giveowner");
    return CMD_ERROR;
  }

  /* You need to either be +n or have the relevant override... */
  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OWNERPRIV,
			   NULL, "chanlev", QPRIV_CHANGECHANLEV, 0)))
    return CMD_ERROR;
  
  rcp=cip->exts[chanservext];

  if (!(target=findreguser(sender, cargv[1])))
    return CMD_ERROR; /* If there was an error, findreguser will have sent a message saying why.. */

  rcup=findreguseronchannel(rcp, target);

  /* You can only promote a master */
  if (!rcup || !(rcup->flags & QCUFLAG_MASTER)) {
    chanservstdmessage(sender,QM_GIVEOWNERNOTMASTER,target->username,cip->name->content);
    return CMD_ERROR;
  }
  
  /* Can't promote if already owner */
  if (rcup->flags & QCUFLAG_OWNER) {
    chanservstdmessage(sender,QM_GIVEOWNERALREADYOWNER,target->username,cip->name->content);
    return CMD_ERROR;
  }
   
  /* Compute ze hash */
  sprintf(hashstr,"%u.%u.%u",rcp->ID,target->ID,rup->ID);
  thehash=crc32(hashstr);
  
  sprintf(hashstr,"%x",thehash);

  if (cargc<3) {
    chanservstdmessage(sender,QM_GIVEOWNERNEEDHASH,cip->name->content,target->username,
                            cip->name->content,target->username,hashstr);
    return CMD_OK;
  }
  
  if (ircd_strcmp(cargv[2], hashstr)) {
    chanservstdmessage(sender,QM_GIVEOWNERWRONGHASH,target->username,
                            cip->name->content);
    return CMD_ERROR;
  }
  
  /* OK, hash matches, do it. */
  oldflags = rcup->flags;
  rcup->flags |= QCUFLAG_OWNER;
  
  chanservstdmessage(sender,QM_DONE);

  strcpy(flagbuf,printflags(oldflags,rcuflags));
  cs_log(sender,"GIVEOWNER %s #%s (%s -> %s)",cip->name->content,rcup->user->username,
	     flagbuf,printflags(rcup->flags,rcuflags));

  csdb_chanlevhistory_insert(rcp, sender, rcup->user, oldflags, rcup->flags);
  csdb_updatechanuser(rcup);
  
  return CMD_OK;
}
