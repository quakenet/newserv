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
 * CMDHELP: channel - channel to list
 * CMDHELP: USERS requires you to be known (+k) on the named channel. You must also be on the 
 * CMDHELP: channel yourself.
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

/* Random flags and structs and sort functions which will come in handly later */

#define ISQ   0x40000000
#define ISOP  0x20000000
#define ISV   0x10000000

struct chanuserrec {
  unsigned int flags;
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
  reguser *rup=getreguserfromnick(sender);
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
  unsigned int notonchan=0;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "users");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_KNOWN,
			   NULL, "users", QPRIV_VIEWFULLCHANLEV, 0)))
    return CMD_ERROR;
  
  rcp=cip->exts[chanservext];

  if (!(cp=cip->channel)) {
    chanservstdmessage(sender,QM_EMPTYCHAN,cip->name->content);
    return CMD_ERROR;
  }
  
  /* Two part logic here.  Firstly, only staff are allowed to look at
   * channels they are not on.  But later we will prevent them looking at
   * channels with opers on them, but clearly it's ok to do this on any
   * channel you are actually on regardless of whether there is an oper or
   * not.  So extract this "notonchan" status here and use it in the later
   * checks.  We set "notonchan" if you are not an oper and not on the
   * channel.  Non-staff with "notonchan" set are then immediately rebuffed. 
   */
  if (!getnumerichandlefromchanhash(cip->channel->users,sender->numeric) && 
      !UHasOperPriv(rup))
    notonchan=1;
  
  if (!cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender) && notonchan) {
    chanservstdmessage(sender,QM_NOTONCHAN,cip->name->content);
    return CMD_ERROR;
  }

  /* malloc() time: if we abort after here we should goto out; rather than
   * return; */
  theusers=malloc(cp->users->totalusers * sizeof(struct chanuserrec));
  memset(theusers,0,cp->users->totalusers * sizeof(struct chanuserrec));
 
  /* OK, fill in our custom struct with useful info about each actual user */ 
  for (i=0,j=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i]==nouser)
      continue;
    
    /* shouldn't happen */
    if (!(np=getnickbynumeric(cp->users->content[i])))
      goto out;

    /* Don't let non-opers look at channels with opers in.  How paranoid are we?
     * Obviously if you are on the channel yourself, no such silliness is necessary. 
     * Making the arbitrary assumption that one letter nicks are OK as per helpmod2.
     */
    if (notonchan && IsOper(np) && np->nick[1] && 
        (IsSecret(cp) || IsPrivate(cp) || IsKey(cp) || IsInviteOnly(cp))) {
      chanservstdmessage(sender,QM_OPERONCHAN,"users",cip->name->content);
      goto out;
    }

    theusers[j].np=np;

    /* Copy op and voice bits from the chanuser */
    theusers[j].flags=(cp->users->content[i]>>2) & 0x30000000;

    /* Make sure we come out at the top of the list */
    if (np==chanservnick) {
      theusers[j].flags|=0x40000000;
    }
    
    theusers[j].rup=getreguserfromnick(np);
    if (theusers[j].rup) {
      theusers[j].rcup=findreguseronchannel(rcp, theusers[j].rup);
      /* OR in the chanlev flags at the bottom if they are known */
      if (theusers[j].rcup)
        theusers[j].flags |= theusers[j].rcup->flags;
    }
    j++;
  }
  
  qsort(theusers, j, sizeof(struct chanuserrec), comparetheflags);
  
  chanservstdmessage(sender, QM_USERSHEADER, cip->name->content);

  /* Determine what flags to show.  Public flags always OK, punishment flags
   * for masters only, personal flags for yourself only except staff who can
   * see all.  Obviously we have to do the bit about personal flags later on... 
   */
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
      /* OK so in reality we may actually have a username and even some
       * flags.  But those don't really matter so let's sub in a cute message. */
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
      
      /* Subtlety here: we conditionally OR in the personal flags for
       * display but not for the check.  This is because if you have
       * personal flags you must have some public flag too by definition. 
       * Compare based on reguser so you can see personal flags for all
       * clones on your account. */
      if (theusers[i].rcup && (theusers[i].rcup->flags & flagmask)) {
        flagbuf=printflags((flagmask | (theusers[i].rup==rup?QCUFLAGS_PERSONAL:0)) & theusers[i].rcup->flags, rcuflags);
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
