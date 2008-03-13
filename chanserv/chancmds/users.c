/*
 * CMDNAME: users
 * CMDLEVEL: QCMD_AUTHED
 * CMDARGS: 1
 * CMDDESC: Displays a list of users on the channel.
 * CMDFUNC: csc_dousers
 * CMDPROTO: int csc_dousers(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: USERS <channel>
 * CMDHELP: Displays a list of users on the named channel along with their usernames and flags
 * CMDHELP: on the channel, where:
 * CMDHELP: channel - channel to use
 * CMDHELP: USERS requires operator (+o) access on the named channel.
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

#define ISQ   0x40000000
#define ISOP  0x20000000
#define ISV   0x10000000

struct chanuserrec {
  unsigned int flags; /* Something that sorts nicely */
  regchanuser *rcup;
  reguser *rup;
  nick *np;
};

static int comparetheflags(const void *a, const void *b) {
  const struct chanuserrec *ra=a, *rb=b;
  
  return rb->flags-ra->flags;
}

int csc_dousers(void *source, int cargc, char **cargv) {
  nick *sender=source, *np;
  chanindex *cip;
  regchan *rcp;
  struct chanuserrec *theusers;
  regchanuser *rcup;
  channel *cp;
  unsigned int i,j;
  char *flagbuf, *unbuf, modechar;
  char uhbuf[USERLEN+HOSTLEN+2];
  flag_t flagmask;
  unsigned int ops,voices,users,flags,qops,masters;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "users");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV,
			   NULL, "users", QPRIV_VIEWFULLCHANLEV, 0)))
    return CMD_ERROR;
  
  rcp=cip->exts[chanservext];

  if (!(cp=cip->channel)) {
    chanservstdmessage(sender,QM_EMPTYCHAN,cip->name->content);
    return CMD_ERROR;
  }
  
  if (!cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender) && !getnumerichandlefromchanhash(cip->channel->users,sender->numeric)) {
    chanservstdmessage(sender,QM_NOTONCHAN,cip->name->content);
    return CMD_ERROR;
  }
  
  theusers=malloc(cp->users->totalusers * sizeof(struct chanuserrec));
  memset(theusers,0,cp->users->totalusers * sizeof(struct chanuserrec));
  
  for (i=0,j=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i]==nouser)
      continue;
    
    if (!(np=getnickbynumeric(cp->users->content[i])))
      goto out;

    theusers[j].np=np;

    theusers[j].flags=(cp->users->content[i]>>2) & 0x30000000;

    if (np==chanservnick) {
      theusers[j].flags|=0x40000000;
    }
    
    theusers[j].rup=getreguserfromnick(np);
    if (theusers[j].rup) {
      theusers[j].rcup=findreguseronchannel(rcp, theusers[j].rup);
      if (theusers[j].rcup)
        theusers[j].flags |= theusers[j].rcup->flags;
    }
    j++;
  }
  
  qsort(theusers, j, sizeof(struct chanuserrec), comparetheflags);
  
  chanservstdmessage(sender,QM_USERSHEADER, cip->name->content);
  
  flagmask=QCUFLAGS_PUBLIC;
  ops=voices=users=flags=qops=masters=0;
  if (cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, "users", 0, 1))
    flagmask |= QCUFLAGS_PUNISH;
  
  if (cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender))
    flagmask = QCUFLAG_ALL;
  
  for (i=0;i<j;i++) {
    if (theusers[i].flags & ISOP) {
      ops++;
      modechar='@';
    } else if (theusers[i].flags & ISV) {
      voices++;
      modechar='+';
    } else {
      users++;
      modechar=' ';
    } 

    if (theusers[i].flags & ISQ) {
      modechar='@';
      unbuf="It's me!";
      flagbuf="";
    } else {
      if (theusers[i].rup) {
        unbuf=theusers[i].rup->username;
      } else {
        unbuf="";
      }
      
      rcup=theusers[i].rcup;
      
      if (rcup) {
        flags++;
        if (CUHasOpPriv(rcup))
          qops++;
        
        if (CUIsMaster(rcup) || CUIsOwner(rcup))
          masters++;
      }        
      
      if (theusers[i].rcup && (theusers[i].rcup->flags & flagmask)) {
        flagbuf=printflags((flagmask | (theusers[i].np==sender?QCUFLAGS_PERSONAL:0)) & theusers[i].rcup->flags, rcuflags);
      } else {
        flagbuf="";
      }
    }
    np=theusers[i].np;
    chanservsendmessage(sender, "%c%-15s %-15s %-12s (%s)",modechar,np->nick,unbuf,flagbuf,visibleuserhost(np, uhbuf));
  }
  free(theusers);
  
  chanservstdmessage(sender, QM_ENDOFLIST);
  chanservstdmessage(sender, QM_USERSSUMMARY, j, ops, voices, users, flags, qops, masters);
  return CMD_OK;
  
  out:
  free(theusers);
  return CMD_ERROR;
}
