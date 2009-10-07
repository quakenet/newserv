/* channelbans.c */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "bans.h"

#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../irc/irc_config.h"
#include "../core/nsmalloc.h"
#include "../lib/flags.h"
#include "../lib/version.h"

MODULE_VERSION("")

#define ALLOCUNIT 100

chanban *freebans;

void _init() {
  freebans=NULL;  
}

void _fini() {
  nsfreeall(POOL_BANS);
}

chanban *getchanban() {
  int i;
  chanban *cbp;

  if (freebans==NULL) {
    freebans=(chanban *)nsmalloc(POOL_BANS, ALLOCUNIT*sizeof(chanban));
    for (i=0;i<ALLOCUNIT-1;i++) {
      freebans[i].next=(struct chanban *)&(freebans[i+1]);
    }
    freebans[ALLOCUNIT-1].next=NULL;
  }

  cbp=freebans;
  freebans=cbp->next;

  cbp->nick=NULL;
  cbp->user=NULL;
  cbp->host=NULL;

  return cbp;
}

void freechanban(chanban *cbp) {
  cbp->next=(struct chanban *)freebans;

  if (cbp->nick)
    freesstring(cbp->nick);
  if (cbp->user)
    freesstring(cbp->user);
  if (cbp->host)
    freesstring(cbp->host);

  freebans=cbp;
}

/*
 * makeban:
 *  Converts the specified ban into a ban structure
 */
 
chanban *makeban(const char *mask) {
  int len;
  int foundat=-1,foundbang=-1;
  int foundwild=0;
  int i;
  int checklen;
  chanban *cbp;
  char tmpbuf[512];
  
  cbp=getchanban();  
  len=strlen(mask);
  
  cbp->flags=0;
  
  /* Let's catch a silly case first */
  if (!strcmp(mask,"*")) {
    cbp->flags=CHANBAN_HOSTANY | CHANBAN_USERANY | CHANBAN_NICKANY;
    cbp->nick=NULL;
    cbp->user=NULL;
    cbp->host=NULL;
    cbp->timeset=time(NULL);
    return cbp;
  }
  
  for (i=(len-1);i>=0;i--) {
    if (mask[i]=='@') {
      /* Got @ sign: everything after here is host */
      if ((len-i)-1 > HOSTLEN) {
        /* This is too long, we need to truncate it */
        strncpy(tmpbuf,&mask[len-HOSTLEN],HOSTLEN);
        tmpbuf[HOSTLEN]='\0';
        tmpbuf[0]='*';
        cbp->host=getsstring(tmpbuf,HOSTLEN);
        cbp->flags |= CHANBAN_HOSTMASK;
      } else if (i==(len-1)) {
        /* Ban ending with @, just mark it invalid */
        /* Note that "moo@*" overrides "moo@", so mark it as having a host too */ 
        cbp->flags |= (CHANBAN_INVALID | CHANBAN_HOSTNULL);
        cbp->host=NULL;
      } else if (i==(len-2) && mask[i+1]=='*') {
        /* Special case: "@*" */
        cbp->flags |= CHANBAN_HOSTANY;
        cbp->host=NULL;      
      } else {
        /* We have some string with between 1 and HOSTLEN characters.. */
        cbp->host=getsstring(&mask[i+1],HOSTLEN);

        /* If it matches an IP address, flag it as such */
        if (ipmask_parse(cbp->host->content, &(cbp->ipaddr), &(cbp->prefixlen))) {
          cbp->flags |= CHANBAN_IP;
        }

        if (foundwild) {
          /* We check all characters after the last wildcard (if any).. if they match
           * the corresponding bits of the hidden host string we mark it accordingly */
          if (!(checklen=len-foundwild-1)) { /* Ban ends in *, mark it anyway.. */
            cbp->flags |= CHANBAN_HIDDENHOST;
          } else {
            if (checklen>=strlen(HIS_HIDDENHOST)) {
              if (!ircd_strcmp(cbp->host->content+(cbp->host->length-strlen(HIS_HIDDENHOST)), HIS_HIDDENHOST))
                cbp->flags |= CHANBAN_HIDDENHOST;
            } else {
              if (!ircd_strcmp(cbp->host->content+(cbp->host->length-checklen), 
                               (HIS_HIDDENHOST)+strlen(HIS_HIDDENHOST)-checklen))
                cbp->flags |= CHANBAN_HIDDENHOST;
            }
          }
          cbp->flags |= CHANBAN_HOSTMASK;
        } else {
          /* Exact host: see if it ends with the "hidden host" string */
          cbp->flags |= CHANBAN_HOSTEXACT;
          if ((cbp->host->length > (strlen(HIS_HIDDENHOST)+1)) && 
              !ircd_strcmp(cbp->host->content+(cbp->host->length-strlen(HIS_HIDDENHOST)), HIS_HIDDENHOST)) {
            cbp->flags |= CHANBAN_HIDDENHOST;
          }
        }
      }
      foundat=i;
      break;
    } else if (mask[i]=='?' || mask[i]=='*') {
      if (!foundwild)  /* Mark last wildcard in string */
        foundwild=i;
    }
  }

  if (foundat<0) {
    /* If there wasn't an @, this ban matches any host */
    cbp->host=NULL;
    cbp->flags |= CHANBAN_HOSTANY;
  }
  
  foundwild=0;
  
  for (i=0;i<foundat;i++) {
    if (mask[i]=='!') {
      if (i==0) {
        /* Invalid mask: nick is empty */
        cbp->flags |= CHANBAN_NICKNULL;
        cbp->nick=NULL;
      } else if (i==1 && mask[0]=='*') {
        /* matches any nick */
        cbp->flags |= CHANBAN_NICKANY;
        cbp->nick=NULL;
      } else {
        if (i>NICKLEN) {
          /* too long: just take the first NICKLEN chars */
          cbp->nick=getsstring(mask,NICKLEN);
        } else {
          cbp->nick=getsstring(mask,i);
        }
        if (foundwild)
          cbp->flags |= CHANBAN_NICKMASK;
        else
          cbp->flags |= CHANBAN_NICKEXACT;
      }
      foundbang=i;
      break;
    } else if (mask[i]=='?' || mask[i]=='*') {
      if (i<NICKLEN) {
        foundwild=1;
      }
    }
  }
  
  if (foundbang<0) {
    /* We didn't find a !, what we do now depends on what happened
     * with the @ */
    if (foundat<0) {
      /* A ban with no ! or @ is treated as a nick ban only */
      /* Note that we've special-cased "*" at the top, so we can only 
       * hit the MASK or EXACT case here. */
      if (len>NICKLEN) 
        cbp->nick=getsstring(mask,NICKLEN);
      else
        cbp->nick=getsstring(mask,len);
        
      if (foundwild)
        cbp->flags |= CHANBAN_NICKMASK;
      else
        cbp->flags |= CHANBAN_NICKEXACT;
        
      cbp->flags |= (CHANBAN_USERANY | CHANBAN_HOSTANY);
      cbp->host=NULL;
      cbp->user=NULL;
    } else {
      /* A ban with @ only is treated as user@host */
      cbp->nick=NULL;
      cbp->flags |= CHANBAN_NICKANY;
    }
  }
  
  if (foundat>=0) {
    /* We found an @, so everything between foundbang+1 and foundat-1 is supposed to be ident */
    /* This is true even if there was no !.. */
    if (foundat==(foundbang+1)) {
      /* empty ident matches nothing */
      cbp->flags |= (CHANBAN_INVALID | CHANBAN_USERNULL);
      cbp->user=NULL;
    } else if (foundat - foundbang - 1 > USERLEN) {
      /* It's too long.. */
      strncpy(tmpbuf,&mask[foundat-USERLEN],USERLEN);
      tmpbuf[USERLEN]='\0';
      tmpbuf[0]='*';
      cbp->user=getsstring(tmpbuf,USERLEN);
      cbp->flags |= CHANBAN_USERMASK;
    } else if ((foundat - foundbang - 1 == 1) && mask[foundbang+1]=='*') {
      cbp->user=NULL;
      cbp->flags |= CHANBAN_USERANY;
    } else {
      cbp->user=getsstring(&mask[foundbang+1],(foundat-foundbang-1));
      if (strchr(cbp->user->content,'*') || strchr(cbp->user->content,'?'))
        cbp->flags |= CHANBAN_USERMASK;
      else
        cbp->flags |= CHANBAN_USEREXACT;
    }
    /* Username part can't contain an @ */
    if (cbp->user && strchr(cbp->user->content,'@')) {
      cbp->flags |= CHANBAN_INVALID;
    }
  }

  assert(cbp->flags & (CHANBAN_USEREXACT | CHANBAN_USERMASK | CHANBAN_USERANY | CHANBAN_USERNULL));
  assert(cbp->flags & (CHANBAN_NICKEXACT | CHANBAN_NICKMASK | CHANBAN_NICKANY | CHANBAN_NICKNULL));
  assert(cbp->flags & (CHANBAN_HOSTEXACT | CHANBAN_HOSTMASK | CHANBAN_HOSTANY | CHANBAN_HOSTNULL));

  cbp->timeset=time(NULL);

  cbp->next=NULL;

  return cbp;
}      

/* banoverlap:
 *  Returns nonzero iff bana is a SUPERSET of banb
 */

int banoverlap(const chanban *bana, const chanban *banb) {
  /* This function works by looking for cases where bana DOESN'T overlap banb */

  /* NICK section */
  /* If bana has CHANBAN_NICKANY then it clearly overlaps 
   * in the nick section so there are no checks */

  if ((bana->flags & CHANBAN_NICKNULL) && !(banb->flags & CHANBAN_NICKNULL)) {
    return 0;
  }

  if (bana->flags & CHANBAN_NICKMASK) {
    if (banb->flags & (CHANBAN_NICKANY | CHANBAN_NICKNULL)) {
      return 0;
    }
    if ((banb->flags & CHANBAN_NICKMASK) && !match2patterns(bana->nick->content, banb->nick->content)) {
      return 0;
    }
    if ((banb->flags & CHANBAN_NICKEXACT) && !match2strings(bana->nick->content, banb->nick->content)) {
      return 0;
    }
  }
  
  if (bana->flags & CHANBAN_NICKEXACT) {
    if (banb->flags & (CHANBAN_NICKANY | CHANBAN_NICKMASK | CHANBAN_NICKNULL)) {
      return 0;
    }
    if ((!bana->nick && banb->nick) || (bana->nick && !banb->nick)) {
      return 0;
    }
    if (bana->nick && ircd_strcmp(bana->nick->content,banb->nick->content)) {
      return 0;
    }
  }

  /* USER section */
  /* If bana has CHANBAN_USERANY then it clearly overlaps 
   * in the user section so there are no checks */

  if ((bana->flags & CHANBAN_USERNULL) && !(banb->flags & CHANBAN_USERNULL)) {
    return 0;
  }

  if (bana->flags & CHANBAN_USERMASK) {
    if (banb->flags & (CHANBAN_USERANY | CHANBAN_USERNULL)) {
      return 0;
    }
    if ((banb->flags & CHANBAN_USERMASK) && !match2patterns(bana->user->content, banb->user->content)) {
      return 0;
    }
    if ((banb->flags & CHANBAN_USEREXACT) && !match2strings(bana->user->content, banb->user->content)) {
      return 0;
    }
  }
  
  if (bana->flags & CHANBAN_USEREXACT) {
    if (banb->flags & (CHANBAN_USERANY | CHANBAN_USERMASK | CHANBAN_USERNULL)) {
      return 0;
    }
    if ((!bana->user && banb->user) || (bana->user && !banb->user)) {
      return 0;
    }
    if (bana->user && ircd_strcmp(bana->user->content,banb->user->content)) {
      return 0;
    }
  }

  /* HOST section */
  /* If bana has CHANBAN_HOSTANY then it clearly overlaps 
   * in the host section so there are no checks */

  if ((bana->flags & CHANBAN_HOSTNULL) && !(banb->flags & CHANBAN_HOSTNULL)) {
    return 0;
  }

  if (bana->flags & CHANBAN_HOSTMASK) {
    if (banb->flags & (CHANBAN_HOSTANY | CHANBAN_HOSTNULL)) {
      return 0;
    }
    if ((banb->flags & CHANBAN_HOSTMASK) && !match2patterns(bana->host->content, banb->host->content)) {
      return 0;
    }
    if ((banb->flags & CHANBAN_HOSTEXACT) && !match2strings(bana->host->content, banb->host->content)) {
      return 0;
    }
  }
  
  if (bana->flags & CHANBAN_HOSTEXACT) {
    if (banb->flags & (CHANBAN_HOSTANY | CHANBAN_HOSTMASK | CHANBAN_HOSTNULL)) {
      return 0;
    }
    if ((!bana->host && banb->host) || (bana->host && !banb->host)) {
      return 0;
    }
    if (bana->host && ircd_strcmp(bana->host->content,banb->host->content)) {
      return 0;
    }
  }

  return 1;
}

/*
 * banequal:
 *  Returns nonzero iff the bans are EXACTLY the same
 */
 
int banequal(chanban *bana, chanban *banb) {
  if (bana->flags != banb->flags) 
    return 0;

  if ((!bana->nick && banb->nick) || (bana->nick && !banb->nick))
    return 0;
 
  if (bana->nick && ircd_strcmp(bana->nick->content,banb->nick->content))
    return 0;

  if ((!bana->user && banb->user) || (bana->user && !banb->user))
    return 0;
 
  if (bana->user && ircd_strcmp(bana->user->content,banb->user->content))
    return 0;

  if ((!bana->host && banb->host) || (bana->host && !banb->host))
    return 0;
 
  if (bana->host && ircd_strcmp(bana->host->content,banb->host->content))
    return 0;

  return 1;
}

/*
 * bantostring:
 *  Convert the specified ban to a string
 */

char *bantostring(chanban *bp) {
  static char outstring[NICKLEN+USERLEN+HOSTLEN+5];
  int strpos=0;

  if (bp->flags & CHANBAN_NICKANY) {
    strpos += sprintf(outstring+strpos,"*");
  } else if (bp->nick) {
    strpos += sprintf(outstring+strpos,"%s",bp->nick->content);
  }

  strpos += sprintf(outstring+strpos,"!");

  if (bp->flags & CHANBAN_USERANY) {
    strpos += sprintf(outstring+strpos,"*");
  } else if (bp->user) {
    strpos += sprintf(outstring+strpos,"%s",bp->user->content);
  }

  strpos += sprintf(outstring+strpos,"@");

  if (bp->flags & CHANBAN_HOSTANY) {
    strpos += sprintf(outstring+strpos,"*");
  } else if (bp->host) {
    strpos += sprintf(outstring+strpos,"%s",bp->host->content);
  }

  return outstring;
} 

/*
 * bantostringdebug:
 *  Convert the specified ban to a string (debugging version)
 */

char *bantostringdebug(chanban *bp) {
  static char outstring[NICKLEN+USERLEN+HOSTLEN+5];
  int strpos=0;

  strpos += sprintf(outstring+strpos, "flags=%04x ",bp->flags);

  if (bp->nick) {
    strpos += sprintf(outstring+strpos, "nick=%s ",bp->nick->content);
  } else {
    strpos += sprintf(outstring+strpos, "nonick ");
  }

  if (bp->user) {
    strpos += sprintf(outstring+strpos, "user=%s ",bp->user->content);
  } else {
    strpos += sprintf(outstring+strpos, "nouser ");
  }

  if (bp->host) {
    strpos += sprintf(outstring+strpos, "host=%s ",bp->host->content);
  } else {
    strpos += sprintf(outstring+strpos, "nohost ");
  }


  return outstring;
} 

