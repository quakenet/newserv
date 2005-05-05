/*
 * lpart: simple newserv module to instruct L to leave a channel
 *        after it has had no other users for a certain time.
 */
 
#include "../lib/sstring.h"
#include "../channel/channel.h"
#include "../core/config.h"
#include "../core/hooks.h"
#include "../nick/nick.h"
#include "../core/schedule.h"
#include "../control/control.h"
#include "../core/error.h"
 
sstring *targetnick; /* Name of the service to talk to */
int lpartext;        /* Our chanext index */
int timeout;         /* How long to wait for */
int shorttimeout;    /* How long to wait if there are no interesting modes */

void lp_handlepart(int hooknum, void *arg);
void lp_dopart(void *arg);
void lp_schedpart(channel *cp);

void _init() {
  sstring *tss;
  int i;
  chanindex *cip;
  nick *np;
  
  lpartext=registerchanext("lpart");
  
  /* Set up the targetnick */
  targetnick=getcopyconfigitem("lpart","servicenick","L",NICKLEN);
  
  /* Set up the timeout */
  tss=getcopyconfigitem("lpart","timeout","86400",6);
  timeout=strtol(tss->content,NULL,10);
  freesstring(tss);

  tss=getcopyconfigitem("lpart","shorttimeout","300",6);
  shorttimeout=strtol(tss->content,NULL,10);
  freesstring(tss);
  
  if ((np=getnickbynick(targetnick->content))) {
    for(i=0;i<CHANNELHASHSIZE;i++) {
      for (cip=chantable[i];cip;cip=cip->next) {
        if (cip->channel && cip->channel->users->totalusers==1 &&
            getnumerichandlefromchanhash(cip->channel->users,np->numeric)) {
          lp_schedpart(cip->channel);         
        }
      }
    }
  }
  
  registerhook(HOOK_CHANNEL_LOSTNICK,&lp_handlepart);
}

void _fini() {
  /* We need to clean up any outstanding callbacks we have */
  deleteallschedules(&lp_dopart);
  releasechanext(lpartext);
  deregisterhook(HOOK_CHANNEL_LOSTNICK,&lp_handlepart);
}

void lp_handlepart(int hooknum, void *arg) {
  void **args=(void **)arg;
  channel *cp;
  nick *np;
    
  /* For a part, first arg is channel and second is nick */
  cp=(channel *)args[0];
  
  /* We're only interested if there is now one user on the channel.
     Note that the parting user is STILL ON THE CHANNEL at this point,
     so we're interested if there are two or fewer users left. */
     
  if (cp->users->totalusers!=2)
    return;
   
  /* Let's see if our user is even on the network */
  if ((np=getnickbynick(targetnick->content))==NULL)
    return;
    
  /* And if it's on the channel in question */
  if (getnumerichandlefromchanhash(cp->users,np->numeric)==NULL)
    return;
    
  /* OK, now let's see if we already had something scheduled for this channel */
  if (cp->index->exts[lpartext]!=NULL) {
    /* We delete the old schedule at this point */
    deleteschedule(cp->index->exts[lpartext],&lp_dopart, (void *)cp->index);
  }
 
/*  Error("lpart",ERR_DEBUG,"Scheduling part of channel %s",cp->index->name->content); */
  lp_schedpart(cp); 
}

void lp_schedpart(channel *cp) {
  int thetimeout;
    
  /* If the channel has anything that might be worth preserving, use the full timeout.
   * Otherwise, use the shorter one 
   */
     
  if ((cp->flags & ~(CHANMODE_NOEXTMSG | CHANMODE_TOPICLIMIT)) || cp->topic || cp->bans) {
    thetimeout=timeout;
  } else {
    thetimeout=shorttimeout;
  }
  
  cp->index->exts[lpartext]=scheduleoneshot(time(NULL)+thetimeout,&lp_dopart,(void *)cp->index);
}

void lp_dopart(void *arg) {
  chanindex *cip;
  nick *np;
  
  cip=(chanindex *)arg;
  cip->exts[lpartext]=NULL;
  
  /* Check the chan still exists */
  if (cip->channel==NULL) {
    releasechanindex(cip);
    return;
  }
  
  /* Check the usercount is 1 */
  if (cip->channel->users->totalusers>1)
    return;
    
  /* Let's see if our user is even on the network */
  if ((np=getnickbynick(targetnick->content))==NULL)
    return;
        
  /* And if it's on the channel in question */
  if (getnumerichandlefromchanhash(cip->channel->users,np->numeric)==NULL)
    return;
  
  /* OK, we've established that the channel is of size 1 and has our nick on it.  Send the part command. 
     Use controlreply() for this */
  
/*   Error("lpart",ERR_DEBUG,"Telling L to part %s",cip->name->content); */
  controlreply(np,"part %s",cip->name->content);
}
