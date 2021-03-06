/* Automatically generated by refactor.pl.
 *
 *
 * CMDNAME: permban
 * CMDALIASES: ban
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 3
 * CMDDESC: Permanently bans a hostmask on a channel.
 * CMDFUNC: csc_dopermban
 * CMDPROTO: int csc_dopermban(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: @UCOMMAND@ <channel> <hostmask> [<reason>]
 * CMDHELP: Permanently bans the provided hostmask on the channel.  If the ban is
 * CMDHELP: removed from the channel e.g. by a channel op or the BANTIMER feature, the
 * CMDHELP: ban will be reapplied if a matching user joins the channel.  Bans
 * CMDHELP: set with the @UCOMMAND@ command can be removed with BANCLEAR or BANDEL.  Any users
 * CMDHELP: matching the hostmask will be kicked from the channel.
 * CMDHELP: Where:
 * CMDHELP: channel  - channel to set a ban on
 * CMDHELP: hostmask - hostmask (nick!user@host) to ban.
 * CMDHELP: reason   - reason for the ban.  This will be used in kick messages when kicking
 * CMDHELP:            users matching the ban.  If this is not provided the generic message
 * CMDHELP:            \"Banned.\" will be used.
 * CMDHELP: @UCOMMAND@ requires master (+m) access on the named channel.
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

int csc_dopermban(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regban *rbp, *toreplace=NULL;
  regchan *rcp;
  reguser *rup=getreguserfromnick(sender);
  struct chanban *b;
  char banbuf[1024];
  unsigned int count = 0;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "permban");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "permban",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  /* saves us having to do repeat a LOT more sanity checking *wink* *wink* */
  b=makeban(cargv[1]);
  snprintf(banbuf,sizeof(banbuf),"%s",bantostring(b));
  freechanban(b);
  b=makeban(banbuf);

  for(rbp=rcp->bans;rbp;rbp=rbp->next) {
    count++;
    if(banequal(b,rbp->cbp)) { /* if they're equal and one is temporary we just replace it */
      if(rbp->expiry) {
        if(toreplace) { /* shouldn't happen */
          chanservsendmessage(sender, "Internal error, duplicate bans found on banlist.");
        } else {
          toreplace=rbp;
          continue;
        }
      } else {
        chanservstdmessage(sender, QM_PERMBANALREADYSET);
      }
    } else if(banoverlap(rbp->cbp,b)) { /* new ban is contained in an already existing one */
      chanservstdmessage(sender, QM_NEWBANALREADYBANNED, bantostring(rbp->cbp));
    }else if(banoverlap(b,rbp->cbp)) { /* existing ban is contained in new one */
      chanservstdmessage(sender, QM_NEWBANOVERLAPS, bantostring(rbp->cbp), banbuf);
    } else {
      continue;
    }
    freechanban(b);
    return CMD_ERROR;
  }

  if(count >= MAXBANS) {
    /* HACK: oper founder channels have 20x the ban limit */
    reguser *founder=findreguserbyID(rcp->founder);
    if(!founder || !UHasOperPriv(founder) || count >= MAXBANS * 20) {
      freechanban(b);
      chanservstdmessage(sender, QM_TOOMANYBANS);
      return CMD_ERROR;
    }
  }

  if(toreplace) {
    freechanban(b);
    chanservstdmessage(sender, QM_REPLACINGTEMPBAN);

    rbp=toreplace;
    if(rbp->reason)
      freesstring(toreplace->reason);
  } else {
    rbp=getregban();
    rbp->ID=++lastbanID;
    rbp->cbp=b;

    rbp->next=rcp->bans;
    rcp->bans=rbp;
  }

  rbp->setby=rup->ID;
  rbp->expiry=0;
  if (cargc>2)
    rbp->reason=getsstring(cargv[2],200);
  else
    rbp->reason=NULL;

  cs_setregban(cip, rbp);
  if(toreplace) {
    csdb_updateban(rcp, rbp);
  } else {
    csdb_createban(rcp, rbp);
  }
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}
