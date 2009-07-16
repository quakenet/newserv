/*
 * CMDNAME: cleartopic
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 2
 * CMDDESC: Clears the topic on a channel.
 * CMDFUNC: csc_docleartopic
 * CMDPROTO: int csc_docleartopic(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: CLEARTOPIC <channel>
 * CMDHELP: Clears the topic on a channel, where:
 * CMDHELP: channel - channel to use
 * CMDHELP: CLEARTOPIC requires topic (+t) or master (+m) access on the named channel.
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

int csc_docleartopic(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "cleartopic");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_TOPICPRIV, 
			   NULL, "cleartopic", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (rcp->topic)
    freesstring(rcp->topic);

  rcp->topic=NULL;
  
  if (cip->channel) {
    localsettopic(chanservnick, cip->channel, "");
  }

  chanservstdmessage(sender, QM_DONE);
  csdb_updatechannel(rcp);
  return CMD_OK;
}
