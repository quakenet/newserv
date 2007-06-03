/* chanservprivs.c */

#include "chanserv.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include <string.h>
#include <ctype.h>

int cs_privcheck(int privnum, nick *np) {
  reguser *rup=NULL;
  
  if (np)
    rup=getreguserfromnick(np);
    
  switch(privnum) {
  case QPRIV_SUSPENDBYPASS:
  case QPRIV_VIEWCHANFLAGS:
  case QPRIV_VIEWFULLCHANLEV:
  case QPRIV_VIEWFULLWHOIS:
  case QPRIV_VIEWCHANMODES:
  case QPRIV_VIEWAUTOLIMIT:
  case QPRIV_VIEWBANTIMER:
  case QPRIV_VIEWUSERFLAGS:
    return (rup && UHasHelperPriv(rup));
    
  case QPRIV_VIEWCOMMENTS:
  case QPRIV_VIEWEMAIL:
  case QPRIV_CHANGECHANLEV:
  case QPRIV_CHANGECHANFLAGS:
  case QPRIV_CHANGECHANMODES:
  case QPRIV_CHANGEAUTOLIMIT:
  case QPRIV_CHANGEBANTIMER:
  case QPRIV_CHANGEUSERFLAGS:
    return (np && rup && IsOper(np) && UHasOperPriv(rup));
    
  default: /* By default opers can override anything */
    return (np && rup && IsOper(np) && UHasOperPriv(rup));
  }
}

/* Generic access check function.
 * Note that the chanindex is returned for success, this
 * can be used to avoid duplicate hash lookups */

chanindex *cs_checkaccess(nick *np, const char *chan, unsigned int flags, 
			  chanindex *cip, const char *cmdname, int priv, int quiet) {
  reguser *rup=getreguserfromnick(np);
  regchan *rcp;
  regchanuser *rcup=NULL;
  unsigned long *lp;
  
  if ((flags & CA_AUTHED) && !rup)
    return NULL;
  
  if (!cip && !(cip=findchanindex(chan))) {
    if (!quiet) chanservstdmessage(np, QM_UNKNOWNCHAN, chan);
    return NULL;
  }
  
  if (!(rcp=cip->exts[chanservext]) || 
      (CIsSuspended(rcp) && !cs_privcheck(QPRIV_SUSPENDBYPASS, np))) {
    if (!quiet) chanservstdmessage(np, QM_UNKNOWNCHAN, cip->name->content);
    return NULL;
  }

  if (rcp && rup)
    rcup=findreguseronchannel(rcp, rup);
  
  if (!cs_privcheck(priv,np)) {
    if ((flags & CA_VOICEPRIV) &&
	!(rcp && (CIsVoiceAll(rcp)) && 
	  !(cip->channel && (nickbanned(np, cip->channel) || IsInviteOnly(cip->channel)))) &&
	!(rcup && (CUHasVoicePriv(rcup)))) {
      if (!quiet) chanservstdmessage(np, QM_NOACCESSONCHAN, cip->name->content, cmdname);
      return NULL;
    }

    if ((flags & CA_NEEDKNOWN) && !rup) {
      if (!quiet) chanservstdmessage(np, QM_NOACCESSONCHAN, cip->name->content, cmdname);
      return NULL;
    }
  
    if ((flags & CA_NEEDKNOWN) && (!rcup || !CUKnown(rcup))) {
      if (!quiet) chanservstdmessage(np, QM_NOACCESSONCHAN, cip->name->content, cmdname);
      return NULL;
    }

    if (((flags & CA_OPPRIV    ) && !CUHasOpPriv(rcup))     ||
        ((flags & CA_MASTERPRIV) && !CUHasMasterPriv(rcup)) ||
	((flags & CA_OWNERPRIV)  && !CUIsOwner(rcup))       ||
        ((flags & CA_TOPICPRIV ) && !CUHasTopicPriv(rcup))) {
      if (!quiet) chanservstdmessage(np, QM_NOACCESSONCHAN, cip->name->content, cmdname);
      return NULL;
    }
  }
	
  if ((flags & CA_ONCHANREQ) && !(cip->channel)) {
    if (!quiet) chanservstdmessage(np, QM_NOTONCHAN, cip->name->content);
    return NULL;
  }

  if (cip->channel) {
    lp=getnumerichandlefromchanhash(cip->channel->users, np->numeric);
    
    if ((flags & CA_ONCHANREQ) && !lp) {
      if (!quiet) chanservstdmessage(np, QM_NOTONCHAN, cip->name->content);
      return NULL;
    }
    
    if ((flags & CA_OPPED) && !(*lp & CUMODE_OP)) {
      if (!quiet) chanservstdmessage(np, QM_NOTOPPED, cip->name->content);
      return NULL;
    }
    
    if ((flags & CA_DEOPPED) && (*lp & CUMODE_OP)) {
      if (!quiet) chanservstdmessage(np, QM_ALREADYOPPED, cip->name->content);
      return NULL;
    }
    
    if ((flags & CA_VOICED) && !(*lp & CUMODE_VOICE)) {
      if (!quiet) chanservstdmessage(np, QM_NOTVOICED, cip->name->content);
      return NULL;
    }
    
    if ((flags & CA_DEVOICED) && (*lp & CUMODE_VOICE)) {
      if (!quiet) chanservstdmessage(np, QM_ALREADYVOICED, cip->name->content);
      return NULL;
    }
    
    if ((flags & CA_OFFCHAN) && lp) {
      if (!quiet) chanservstdmessage(np, QM_ALREADYONCHAN, cip->name->content);
      return NULL;
    }
  }
  
  return cip;
}

