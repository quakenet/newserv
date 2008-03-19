/* channelbans.c */

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "channel.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../irc/irc_config.h"
#include "../irc/irc.h"

/*
 * nickmatchban_visible:
 *  Returns true iff the supplied nick* matches the supplied ban* 
 *  Doesn't check "invisible" things like true hosts and IPs for 
 *  +x/+h users.
 *
 * copy & pasted this, touch and go whether this was a good idea.
 */

int nickmatchban_visible(nick *np, chanban *bp) {
  const char *ipstring;
  char fakehost[HOSTLEN+1];
  char *visibleident;

  /* Don't waste time on invalid bans */
  if (bp->flags & CHANBAN_INVALID)
    return 0;

  /* nick/ident section: return 0 (no match) if they don't match */

  /* Determine the visible ident for sethost users.  Don't test the real one. */  
  if (IsSetHost(np) && np->shident && *np->shident->content)
    visibleident=np->shident->content;
  else
    visibleident=np->ident;
  
  if (bp->flags & CHANBAN_USEREXACT && ircd_strcmp(visibleident,bp->user->content))
    return 0;
  
  if (bp->flags & CHANBAN_NICKEXACT && ircd_strcmp(np->nick,bp->nick->content))
    return 0;
  
  if (bp->flags & CHANBAN_USERMASK && !match2strings(bp->user->content,visibleident))
    return 0;
  
  if (bp->flags & CHANBAN_NICKMASK && !match2strings(bp->nick->content,np->nick))
    return 0;
  
  /* host section.  Return 1 (match) if they do match
   * Note that if user or ident was specified, they've already been checked
   */

  if (bp->flags & CHANBAN_HOSTANY)
    return 1;

  if ((bp->flags & CHANBAN_CIDR) && (bp->flags & CHANBAN_HOSTEXACT)) {
    unsigned int cip;
    unsigned char *ch;

    /* CIDR bans don't visibly match sethosted users */
    if (IsSetHost(np) || (IsAccount(np) && IsHideHost(np)))
      return 0;

    /* CIDR bans don't match IPv6 hosts */
    if (!irc_in_addr_is_ipv4(&(np->p_ipaddr)))
      return 0;

    /* Extract the client's IP address into a usable format */
    ch=(unsigned char *)&(np->p_ipaddr.in6_16[6]);
    cip=(ch[0]<<24) | (ch[1]<<16) | (ch[2]<<8) | (ch[3]);
    
    if ((cip & bp->mask) == bp->ipaddr)
      return 1;
    
    return 0; /* A CIDR ban won't match any other way */
  }
  
  if (bp->flags & CHANBAN_IP) {
    /* IP bans don't match sethosted users */
    if (IsSetHost(np) || (IsAccount(np) && IsHideHost(np)))
      return 0;
      
    if (bp->flags & CHANBAN_HOSTEXACT) {
      /* If it's an exact IP ban we can compare it numerically */
      unsigned int cip;
      unsigned char *ch;

      /* Well, it won't match if it's not an IPv4 host */
      if (!irc_in_addr_is_ipv4(&(np->p_ipaddr)))
        return 0;

      /* Extract the client's IP address into a usable format */
      ch=(unsigned char *)&(np->p_ipaddr.in6_16[6]);
      cip=(ch[0]<<24) | (ch[1]<<16) | (ch[2]<<8) | (ch[3]);

      if (cip==bp->ipaddr) 
        return 1;
    } else {
      /* It's not an exact IP ban so let's generate the string */
      ipstring=IPtostr(np->p_ipaddr);
 
      if (bp->flags & CHANBAN_HOSTMASK && match2strings(bp->host->content,ipstring))
        return 1;
    }
  } else {
    /* Hostname bans need to be checked against +x host, +h host (if set) 
     * and actual host.  Note that the +x host is only generated (and checked) if it's
     * possible for the ban to match a hidden host.. */

    if ((bp->flags & CHANBAN_HIDDENHOST) && IsAccount(np)) {
      sprintf(fakehost,"%s.%s",np->authname, HIS_HIDDENHOST);
      
      if ((bp->flags & CHANBAN_HOSTEXACT) && 
	  !ircd_strcmp(fakehost, bp->host->content))
	return 1;
      
      if ((bp->flags & CHANBAN_HOSTMASK) &&
	  match2strings(bp->host->content, fakehost))
	return 1;
    }
    
    if (IsSetHost(np)) {
      if ((bp->flags & CHANBAN_HOSTEXACT) &&
	  !ircd_strcmp(np->sethost->content, bp->host->content))
	return 1;
      
      if ((bp->flags & CHANBAN_HOSTMASK) &&
	  match2strings(bp->host->content, np->sethost->content))
	return 1;
    }

    /* If the user is +h or +rx don't check their real host */
    if (IsSetHost(np) || (IsHideHost(np) && IsAccount(np)))
      return 0;
      
    if (bp->flags & CHANBAN_HOSTEXACT && !ircd_strcmp(np->host->name->content,bp->host->content))
      return 1;
    
    if (bp->flags & CHANBAN_HOSTMASK && match2strings(bp->host->content,np->host->name->content))
      return 1;
  }
  
  return 0;
}

/*
 * nickmatchban:
 *  Returns true iff the supplied nick* matches the supplied ban* 
 */

int nickmatchban(nick *np, chanban *bp) {
  const char *ipstring;
  char fakehost[HOSTLEN+1];

  /* nick/ident section: return 0 (no match) if they don't match */

  if (bp->flags & CHANBAN_INVALID)
    return 0;
  
  if (bp->flags & CHANBAN_USEREXACT && 
      ircd_strcmp(np->ident,bp->user->content) && 
      (!IsSetHost(np) || !np->shident || 
       ircd_strcmp(np->shident->content,bp->user->content)))
    return 0;
  
  if (bp->flags & CHANBAN_NICKEXACT && ircd_strcmp(np->nick,bp->nick->content))
    return 0;
  
  if (bp->flags & CHANBAN_USERMASK && 
      !match2strings(bp->user->content,np->ident) && 
      (!IsSetHost(np) || !np->shident || 
       !match2strings(bp->user->content, np->shident->content)))
    return 0;
  
  if (bp->flags & CHANBAN_NICKMASK && !match2strings(bp->nick->content,np->nick))
     return 0;
  
  /* host section.  Return 1 (match) if they do match
   * Note that if user or ident was specified, they've already been checked
   */

  if (bp->flags & CHANBAN_HOSTANY)
    return 1;

  if ((bp->flags & CHANBAN_CIDR) && (bp->flags & CHANBAN_HOSTEXACT)) {
    unsigned int cip;
    unsigned char *ch;

    /* CIDR bans don't match IPv6 hosts */
    if (!irc_in_addr_is_ipv4(&(np->p_ipaddr)))
      return 0;

    /* Extract the client's IP address into a usable format */
    ch=(unsigned char *)&(np->p_ipaddr.in6_16[6]);
    cip=(ch[0]<<24) | (ch[1]<<16) | (ch[2]<<8) | (ch[3]);

    if ((cip & bp->mask) == bp->ipaddr)
      return 1;
    
    return 0; /* A CIDR ban won't match any other way */
  }
  
  if (bp->flags & CHANBAN_IP) {
    if (bp->flags & CHANBAN_HOSTEXACT) {
      /* If it's an exact IP ban we can compare it numerically */
      unsigned int cip;
      unsigned char *ch;

      /* Well, it won't match if it's not an IPv4 host */
      if (!irc_in_addr_is_ipv4(&(np->p_ipaddr)))
        return 0;

      /* Extract the client's IP address into a usable format */
      ch=(unsigned char *)&(np->p_ipaddr.in6_16[6]);
      cip=(ch[0]<<24) | (ch[1]<<16) | (ch[2]<<8) | (ch[3]);

      if (cip==bp->ipaddr) 
        return 1;
    } else {
      /* It's not an exact IP ban so let's generate the string */
      ipstring=IPtostr(np->p_ipaddr);
 
      if (bp->flags & CHANBAN_HOSTMASK && match2strings(bp->host->content,ipstring))
        return 1;
    }
  } else {
    /* Hostname bans need to be checked against +x host, +h host (if set) 
     * and actual host.  Note that the +x host is only generated (and checked) if it's
     * possible for the ban to match a hidden host.. */

    if ((bp->flags & CHANBAN_HIDDENHOST) && IsAccount(np)) {
      sprintf(fakehost,"%s.%s",np->authname, HIS_HIDDENHOST);
      
      if ((bp->flags & CHANBAN_HOSTEXACT) && 
	  !ircd_strcmp(fakehost, bp->host->content))
	return 1;
      
      if ((bp->flags & CHANBAN_HOSTMASK) &&
	  match2strings(bp->host->content, fakehost))
	return 1;
    }
    
    if (IsSetHost(np)) {
      if ((bp->flags & CHANBAN_HOSTEXACT) &&
	  !ircd_strcmp(np->sethost->content, bp->host->content))
	return 1;
      
      if ((bp->flags & CHANBAN_HOSTMASK) &&
	  match2strings(bp->host->content, np->sethost->content))
	return 1;
    }
    
    if (bp->flags & CHANBAN_HOSTEXACT && !ircd_strcmp(np->host->name->content,bp->host->content))
      return 1;
    
    if (bp->flags & CHANBAN_HOSTMASK && match2strings(bp->host->content,np->host->name->content))
      return 1;
  }
  
  return 0;
}

/*
 * nickbanned:
 *  Returns true iff the supplied nick* is banned on the supplied chan*
 * 
 * Also nickbanned_visible - doesn't violate privacy by checking hidden
 * hosts and idents.  Factored into one function to reduce copy&paste.
 */
static int nickbanned_real(nick *np, channel *cp, int (*cmpfunc)(nick *, chanban *)) {
  chanban *cbp;

  for (cbp=cp->bans;cbp;cbp=cbp->next) {
    if (cmpfunc(np,cbp))
      return 1; 
  }

  return 0;
}

int nickbanned(nick *np, channel *cp) {
  return nickbanned_real(np,cp,nickmatchban);
}

int nickbanned_visible(nick *np, channel *cp) {
  return nickbanned_real(np,cp,nickmatchban_visible);
}
              
/*
 * setban:
 *  Set the specified ban; if it completely encloses any existing bans
 *  then remove them.
 *
 * Returns 1 if the ban was set, or 0 if the ban was not set because an
 * existing ban overlapped it.
 */  

int setban(channel *cp, const char *ban) {
  chanban **cbh,*cbp,*cbp2;

  cbp=makeban(ban);
  
  /* Don't set our ban if something encloses it */
  for (cbp2=cp->bans;cbp2;cbp2=cbp2->next) {
    if (banoverlap(cbp2, cbp)) {
      /* This ban overlaps the one we are adding.  Abort. */
      freechanban(cbp);
      return 0;
    }
  }
    
  /* Remove enclosed bans first */
  for (cbh=&(cp->bans);*cbh;) {
    if (banoverlap(cbp,*cbh)) {
      cbp2=(*cbh);
      (*cbh)=cbp2->next;
      freechanban(cbp2);
      /* Break out of the loop if we just deleted the last ban */
      if ((*cbh)==NULL) {
        break;
      }
    } else {
      cbh=(chanban **)&((*cbh)->next);
    }
  }
  
  /* Now set the new ban */
  cbp->next=(struct chanban *)cp->bans;
  cp->bans=cbp;
  
  return 1;
}

/*
 * clearban:
 *  Remove the specified ban iff an exact match is found
 *  Returns 1 if the ban was cleared, 0 if the ban didn't exist.
 *  If "optional" is 0 and the ban didn't exist, flags an error
 */

int clearban(channel *cp, const char *ban, int optional) {
  chanban **cbh,*cbp,*cbp2;  
  int found=0;
  int i=0;

  if (!cp)
    return 0;

  cbp=makeban(ban);

  for (cbh=&(cp->bans);*cbh;cbh=(chanban **)&((*cbh)->next)) {
    if (banequal(cbp,*cbh)) {
      cbp2=(*cbh);
      (*cbh)=cbp2->next;
      freechanban(cbp2);
      found=1;
      break;        
    }
  }

  if (!found && !optional) {
    Error("channel",ERR_DEBUG,"Couldn't remove ban %s from %s.  Dumping banlist:",ban,cp->index->name->content);
    for (cbp2=cp->bans;cbp2;cbp2=cbp2->next) {
      Error("channel",ERR_DEBUG,"%s %d %s",cp->index->name->content, i++, bantostringdebug(cbp2));
    }
  }

  freechanban(cbp);

  return found;
}

/*
 * clearallbans: 
 *  Just free all the bans on the channel 
 */

void clearallbans(channel *cp) {
  chanban *cbp,*ncbp;
  
  for (cbp=cp->bans;cbp;cbp=ncbp) {
    ncbp=(chanban *)cbp->next;
    freechanban(cbp);
  }
  
  cp->bans=NULL;
}
