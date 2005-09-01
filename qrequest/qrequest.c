/*
 * Q request system!
 *
 * Depends on "chanstats" 
 */
 
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../chanstats/chanstats.h"
#include "../localuser/localuser.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define QR_nick "R"
#define QR_user "qrequest"
#define QR_host "qrequest.quakenet.org"
#define QR_acct "R"

#define Q_nick "Q"
#define L_nick "L"

#define QRLstate_IDLE         0x0 /* No request active */
#define QRLstate_AWAITINGCHAN 0x1 /* Awaiting "Users for channel.." */
#define QRLstate_AWAITINGUSER 0x2 /* Looking for our user in the list */
#define QRLstate_AWAITINGEND  0x3 /* Waiting for "End of chanlev" */

#define QR_FAILED             0x0
#define QR_OK                 0x1

#define QREQUIREDSIZE         50

typedef struct requestrec {
  unsigned int       reqnumeric;  /* Who made the request */
  unsigned int       tellnumeric; /* Who we are talking to about the request */
  chanindex         *cip;         /* Which channel the request is for */
  struct requestrec *next;
} requestrec;

requestrec *nextreq, *lastreq;
requestrec *nextqreq, *lastqreq;

nick *qr_np;
int qrlstate;

void qr_handler(nick *me, int type, void **args);

void qr_registeruser(void *arg) {
  qr_np=registerlocaluser(QR_nick, QR_user, QR_host, "Q Request 0.01",
                          QR_acct, UMODE_ACCOUNT|UMODE_OPER, qr_handler);
}

/*
 * Deal with outcome of a queued request.  The request should be freed
 * as part of the process.
 */

void qr_result(requestrec *req, int outcome, char *message, ...) {
  requestrec **rh;
  char msgbuf[512];
  va_list va;
  nick *lnp, *qnp, *np, *tnp;

  /* Delete the request from the list first.. */
  for (rh=&nextreq;*rh;rh=&((*rh)->next)) {
    if (*rh==req) {
      *rh=req->next;
      break;
    }
  }

  /* If this was the last request (unlikely), 
   * we need to fix the last pointer */
  if (lastreq==req) {
    if (nextreq)
      for (lastreq=nextreq;lastreq->next;lastreq=lastreq->next)
	; /* empty loop */
    else
      lastreq=NULL;
  }

  /* Check that the nick is still here.  If not, drop the request. */
  if (!(np=getnickbynumeric(req->reqnumeric)) || 
      !(tnp=getnickbynumeric(req->tellnumeric))) {
    free(req);
    return;
  }

  if (outcome==QR_OK) {
    /* Delete L, add Q.  Check that they both exist first, though. */

    if (!(lnp=getnickbynick(L_nick)) || !(qnp=getnickbynick(Q_nick))) {
      sendnoticetouser(qr_np, tnp, 
		       "Sorry, cannot find Q and L on the network. "
		       "Please request again later.");
      free(req);
      return;
    }

    /* /msg Q ADDCHAN <channel> <flags> <owners nick> <channeltype> */
    sendmessagetouser(qr_np, qnp, "ADDCHAN %s +ap #%s upgrade",
		      req->cip->name->content,
		      np->authname);

    sendnoticetouser(qr_np, tnp, "Adding Q to channel, please wait...");
    
    if (lastqreq) 
      lastqreq->next=req;
    else
      lastqreq=nextqreq=req;
    
    req->next=NULL;
    
    /* Don't free, it's in new queue now */
  } else {
    /* Sort out the message.. */
    va_start(va, message);
    vsnprintf(msgbuf,511,message,va);
    va_end(va);

    sendnoticetouser(qr_np, tnp, "%s", msgbuf);
    /* This is a failure message.  Add disclaimer. */
    sendnoticetouser(qr_np, tnp, "Please do not complain about this result.");
    free(req);
  }
}

/*
 * qr_checksize: 
 *  Checks that a channel is beeeeg enough for teh Q
 */

int qr_checksize(chanindex *cip) {
  chanstats *csp;
  int i,tot=0;

  if (!(csp=cip->exts[csext]))
    return 0;

  for (i=0;i<HISTORYDAYS;i++) {
    tot += csp->lastdays[i];
  }

  if (tot > (QREQUIREDSIZE * 140))
    return 1;

  return 0;
}

/* This function deals with notices from L: basically we track the 
 * responses to the L chanlev requests we've been making until we can
 * decide what to do with the requests.
 *
 * Here's the L chanlev format:
 * 11:12 -L(TheLBot@lightweight.quakenet.org)- Users for channel #twilightzone
 * 11:12 -L(TheLBot@lightweight.quakenet.org)- Authname         Access flags
 * 11:12 -L(TheLBot@lightweight.quakenet.org)- -----------------------------
 * 11:12 -L(TheLBot@lightweight.quakenet.org)- Bigfoot                  amno
 * 11:12 -L(TheLBot@lightweight.quakenet.org)- End of chanlev for #twilightzone.
 */

void qr_handlenotice(nick *sender, char *message) {
  char *ch;
  chanindex *cip;
  requestrec *rrp1, *rrp2;
  nick *np;

  if (!ircd_strcmp(sender->nick, Q_nick)) {
    /* Message from Q */
    if (!nextqreq) {
      /* ERROR ERROR ERROR */
      return;
    }

    if (!ircd_strcmp(message,"Done.")) {
      /* Q added the channel: delete from L and tell the user. */
      /* If L has conspired to vanish between the request and the outcome,
       * we have a chan with Q and L... too bad. */

      if ((np=getnickbynick(L_nick))) {
	sendmessagetouser(qr_np, np, "SENDCHANLEV %s %s",
			  nextqreq->cip->name->content, Q_nick);

	sendmessagetouser(qr_np, np, "DELCHAN %s",
			  nextqreq->cip->name->content);
      }

      if ((np=getnickbynumeric(nextqreq->tellnumeric))) {
	sendnoticetouser(qr_np, np, "Request completed.  Q added and L deleted.");
      }
    } else if (!ircd_strcmp(message,"That channel already exists.")) {
      if ((np=getnickbynumeric(nextqreq->tellnumeric))) {
	sendnoticetouser(qr_np, np, 
			  "Your channel appears to have Q already "
			  "(it may be suspended).");
      }
    } else {
      return;
    }
    
    /* For either of the two messages above we want to delete the request
     * at the head of the queue. */
    rrp1=nextqreq;
    
    nextqreq=nextqreq->next;
    if (!nextqreq)
      lastqreq=NULL;
    
    free(rrp1);
  } else if (!ircd_strcmp(sender->nick, L_nick)) {
    /* Message from L */
    switch (qrlstate) {
    case QRLstate_IDLE:
      /* We're idle, do nothing */
      return;
      
    case QRLstate_AWAITINGCHAN:
      /* We're waiting for conformation of the channel name */
      if (!ircd_strncmp(message,"Users for",9)) {
	/* Looks like the right message.  Let's find a channel name */
	
	for (ch=message;*ch;ch++)
	  if (*ch=='#')
	    break;
	
	if (!*ch) {
	  Error("qrequest",ERR_WARNING,
		"Unable to parse channel name from L message: %s",message);
	  return;
	}
	
	if (!(cip=findchanindex(ch))) {
	  Error("qrequest",ERR_WARNING,
		"Unable to find channel from L message: %s",ch);
	  return;
	}
	
	if (cip==nextreq->cip) {
	  /* Ok, this is the correct channel, everything is proceeding
	   * exactly as I had forseen */
	  qrlstate = QRLstate_AWAITINGUSER;
	  return;
	} else {
	  /* Uh-oh, not the channel we wanted.  Something is fucked
	   * here.  I think the only possible way out of this mess is
	   * to skip through in case we find a match for a later channel.. 
	   */
	  for (rrp1=nextreq;rrp1;rrp1=rrp1->next)
	    if (rrp1->cip==cip)
	      break;
	  
	  if (rrp1) {
	    /* OK, we found a match further down the chain.  This means
	     * that something bad has happened to every request between
	     * the head of the list and the one we just found - send 
	     * error responses.
	     *
	     * Note weird loop head: qr_result will free up requests from
	     * the list as it goes, so we can just keep picking off the first
	     * entry
	     */
	    for(rrp2=nextreq;rrp2;rrp2=nextreq) {
	      if (rrp2==rrp1)
		break;
	      
	      Error("qrequest",ERR_WARNING,
		    "Lost response for channel %s; skipping.",
		    rrp2->cip->name->content);
	      
	      qr_result(rrp2, QR_FAILED, 
			"Sorry, an error occurred while processing your request.");
	    }
	    
	    if (rrp2) {
	      /* We seem to be back in sync. */
	      qrlstate=QRLstate_AWAITINGUSER;
	      return;
	    }
	    /* Some form of hole in the space time continuum exists 
	     * if we get here.  Unclear how to proceed. */
	    return;
	  } else {
	    /* No match - let's just ignore this completely */
	    Error("qrequest",ERR_WARNING,
		  "Ignoring L response for spurious channel %s",
		  cip->name->content);
	    return;
	  }
	}
      }
      break;
      
    case QRLstate_AWAITINGUSER:
      if (!ircd_strncmp(message, "End of chanlev",14)) {
	/* Oh dear, we got to the end of the chanlev in this state.
	 * This means that we didn't find the user.
	 */
	
	qr_result(nextreq, QR_FAILED,
		  "Sorry, you are not known on %s.",
		  nextreq->cip->name->content);
	
	if (nextreq) 
	  qrlstate=QRLstate_AWAITINGCHAN;
	else
	  qrlstate=QRLstate_IDLE;
	
	return;
      } else {
	/* Brutalise the message :-) */
	
	if (!(ch=strchr(message, ' ')))
	  return;

	*ch++='\0';
	
	if (!(np=getnickbynumeric(nextreq->reqnumeric)))
	  return;
	
	if (ircd_strcmp(message, np->authname)) {
	  /* This is not the user you are looking for */
	  return;
	}
	
	/* Check for owner flag.  Both branches of this if will
	 * take the request off the list, one way or the other. */
	if (strchr(ch, 'n')) {
	  /* They iz teh +n! */
	  if (qr_checksize(nextreq->cip)) {
	    qr_result(nextreq, QR_OK, "OK");
	  } else {
	    qr_result(nextreq, QR_FAILED, 
		      "Sorry, your channel is not large enough to request Q. Please continue to use L instead.");
	  }
	} else {
	  qr_result(nextreq, QR_FAILED,
		    "Sorry, you don't hold the +n flag on %s.",
		    nextreq->cip->name->content);
	}
	
	/* OK, we found what we wanted so make sure we skip the rest */
	qrlstate=QRLstate_AWAITINGEND;
	
	return;
      }
      break;
      
    case QRLstate_AWAITINGEND:
      if (!ircd_strncmp(message, "End of chanlev",14)) {
	/* Found end of list */
	
	if (nextreq)
	  qrlstate=QRLstate_AWAITINGCHAN;
	else 
	  qrlstate=QRLstate_IDLE;
	
	return;
      }
      break;
    }
  }    
}  

/*
 * This function deals with requests from users for Q.
 * Some sanity checks are made (on channel, opped) and the
 * request is added to the queue.
 */

void qr_handlemsg(nick *sender, char *message) {
  char *ch, *ch2;
  chanindex *cip;
  nick *lnp;
  unsigned long *lp;
  nick *requester;

  if (!ircd_strncmp(message,"requestq ",9)) {
    /* Looks like a sensible request. */
    if (!(ch=strchr(message,'#'))) {
      sendnoticetouser(qr_np, sender,
		       "Usage: requestq #channel");
      return;
    }

    /* If an oper is sending the request, allow them to specify
     * a second parameter indicating which user they are making the
     * request on behalf of.  All status messages will be sent to the
     * oper issuing "requestq", but the move will be done on behalf of
     * the named user */

    if (IsOper(sender) && (ch2=strchr(ch,' '))) {
      *ch2++='\0';
      if (!(requester=getnickbynick(ch2))) {
	sendnoticetouser(qr_np,sender,"Can't find user: %s",ch2);
	return;
      }
    } else {
      requester=sender;
    }

    /* Check:
     *  - user is authed
     *  - channel exists
     *  - Q exists
     *  - L exists
     *  - L is on the channel
     *  - user is on the channel
     *  - user is opped on the channel
     *  - we have some form of channel stats for the channel
     *
     * Note that the actual channel stats will not be checked
     * until we're sure the user has +n on the channel.
     */
    
    if (!IsAccount(requester)) {
      sendnoticetouser(qr_np, sender, 
		       "Sorry, you must be authed to make a request.");
      return;
    }

    if (!(cip=findchanindex(ch)) || !cip->channel) {
      sendnoticetouser(qr_np, sender, "Unable to find channel %s.",ch);
      return;
    }

    if (!(lnp=getnickbynick(L_nick))) {
      sendnoticetouser(qr_np, sender, 
		       "Can't find L on the network, please try later.");
      return;
    }
    
    if (!getnickbynick(Q_nick)) {
      sendnoticetouser(qr_np, sender,
		       "Can't find Q on the network, please try later.");
      return;
    }

    if (!getnumerichandlefromchanhash(cip->channel->users, lnp->numeric)) {
      sendnoticetouser(qr_np, sender,
		       "L is not on %s.", cip->name->content);
      return;
    }
    
    if (!(lp=getnumerichandlefromchanhash(cip->channel->users, 
					   requester->numeric))) {
      sendnoticetouser(qr_np, sender,
		       "You are not on %s.", cip->name->content);
      return;
    }
    
    if (!(*lp & CUMODE_OP)) {
      sendnoticetouser(qr_np, sender,
		       "You are not opped on %s.", cip->name->content);
      return;
    }

    if (!cip->exts[csext]) {
      sendnoticetouser(qr_np, sender,
		       "Sorry, no historical record exists for %s.", 
		       cip->name->content);
      return;
    }

    /* Request stats from L */
    sendmessagetouser(qr_np, lnp, "CHANLEV %s", cip->name->content);
    
    /* Sort out a request record */
    if (lastreq) {
      lastreq->next = (requestrec *)malloc(sizeof(requestrec));
      lastreq=lastreq->next;
    } else {
      lastreq=nextreq=(requestrec *)malloc(sizeof(requestrec));
    }

    lastreq->next = NULL;
    lastreq->cip = cip;
    lastreq->reqnumeric = requester->numeric;
    lastreq->tellnumeric = sender->numeric;
      
    if (qrlstate == QRLstate_IDLE)
      qrlstate = QRLstate_AWAITINGCHAN;

    sendnoticetouser(qr_np, sender, 
		     "Checking your L access.  "
		     "This may take a while, please be patient...");
  }
}

void qr_handler(nick *me, int type, void **args) {
  
  switch(type) {
  case LU_KILLED:
    qr_np=NULL;
    scheduleoneshot(time(NULL)+1, qr_registeruser, NULL);
    return;

  case LU_PRIVNOTICE:
    qr_handlenotice(args[0], args[1]);
    return;

  case LU_PRIVMSG:
    qr_handlemsg(args[0], args[1]);
    return;
  }
}

void _init() {
  nextreq=lastreq=NULL;
  nextqreq=lastqreq=NULL;
  qr_np=NULL;
  qr_registeruser(NULL);
}

void _fini() {
  struct requestrec *rp;

  while (nextreq) {
    rp=nextreq;
    nextreq=nextreq->next;
    free(rp);
  }

  while (nextqreq) {
    rp=nextqreq;
    nextqreq=nextqreq->next;
    free(rp);
  }

  if (qr_np)
    deregisterlocaluser(qr_np, NULL);

  deleteallschedules(qr_registeruser);
  
}
