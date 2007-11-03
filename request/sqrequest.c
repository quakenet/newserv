/*
 * S and Q request system!
 *
 * Depends on "chanstats" and "chanfix"
 */

#include <stdio.h>
#include "request.h"
#include "sqrequest.h"
#include "request_block.h"
#include "../chanfix/chanfix.h"
#include "../chanstats/chanstats.h"
#include "../localuser/localuser.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"
#include "../core/config.h"
#include "../spamscan2/spamscan2.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define QRLstate_IDLE         0x0 /* No request active */
#define QRLstate_AWAITINGCHAN 0x1 /* Awaiting "Users for channel.." */
#define QRLstate_AWAITINGUSER 0x2 /* Looking for our user in the list */
#define QRLstate_AWAITINGEND  0x3 /* Waiting for "End of chanlev" */

#define QR_FAILED             0x0
#define QR_OK                 0x1

#define QR_CSERVE             0x0
#define QR_SPAMSCAN           0x1

#define QR_L                  0x0
#define QR_Q                  0x1

#define min(a,b) ((a > b) ? b : a)

typedef struct requestrec {
  unsigned int       reqnumeric;  /* Who made the request */
  chanindex         *cip;         /* Which channel the request is for */
  int                what;        /* Which service does the user want? */
  int                who;         /* Who are we talking to about CHANLEV? */
  struct requestrec *next;
} requestrec;

requestrec *nextreql, *lastreql;
requestrec *nextreqq, *lastreqq;


static requestrec *nextqreq, *lastqreq;

extern nick *rqnick;
int rlstate;
int rqstate;

/* stats counters */
int qr_suspended = 0;
int qr_nohist = 0;
int qr_toosmall = 0;
int qr_nochanlev = 0;
int qr_notowner = 0;

int qr_a = 0;
int qr_b = 0;
int qr_c = 0;
int qr_d = 0;
int qr_e = 0;

/* Check whether the user is blocked */
int qr_blockcheck(requestrec *req) {
  nick *np;
  rq_block *block;
  
  np = getnickbynumeric(req->reqnumeric);

  /* user is not online anymore */
  if (np == NULL)
    return 0;
    
  block = rq_findblock(np->authname);
  
  if (block != NULL)
    return 1; /* user is blocked */
  else
    return 0;
}

unsigned int rq_countchanusers(channel *cp) {
  int i, count=0;

  for (i=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i]==nouser)
      continue;

    count++;
  }

  return count;
}

/*
 * Deal with outcome of a queued request.  The request should be freed
 * as part of the process.
 */

static void qr_result(requestrec *req, int outcome, char failcode, char *message, ...) {
  sstring *user, *password;
  requestrec **rh;
  char msgbuf[512];
  va_list va;
  nick *lnp, *qnp, *np, *tnp, *snp;
  char now[50];
  time_t now_ts;
  unsigned int unique, total;

  /* Delete the request from the list first.. */
  for (rh=&nextreql;*rh;rh=&((*rh)->next)) {
    if (*rh==req) {
      *rh=req->next;
      break;
    }
  }

  for (rh=&nextreqq;*rh;rh=&((*rh)->next)) {
    if (*rh==req) {
      *rh=req->next;
      break;
    }
  }

  /* If this was the last request (unlikely),
   * we need to fix the last pointer */
  if (lastreql==req) {
    if (nextreql)
      for (lastreql=nextreql;lastreql->next;lastreql=lastreql->next)
        ; /* empty loop */
    else
      lastreql=NULL;
  }
  
  if (lastreqq==req) {
    if (nextreqq)
      for (lastreqq=nextreqq;lastreqq->next;lastreqq=lastreqq->next)
        ; /* empty loop */
    else
      lastreqq=NULL;
  }

  /* Check that the nick is still here.  If not, drop the request. */
  if (!(tnp=np=getnickbynumeric(req->reqnumeric))) {
    free(req);
    return;
  }

  if (rq_logfd != NULL) {
    now[0] = '\0';
    now_ts = time(NULL);

    if (req->cip->channel) {
      unique = countuniquehosts(req->cip->channel);
      total = rq_countchanusers(req->cip->channel);
    } else {
      unique = 0;
      total = 0;
    }

    strftime(now, sizeof(now), "%c", localtime(&now_ts));
    fprintf(rq_logfd, "%s: request (%s) for %s (%d unique users, "
            "%d total users) from %s: Request was %s (%c).\n", now,
            (req->what == QR_CSERVE) ? RQ_QNICK : RQ_SNICK,
            req->cip->name->content, unique, total, tnp->nick,
            (outcome == QR_OK) ? "accepted" : "denied", failcode);
    fflush(rq_logfd);
  }
  
  if (outcome==QR_OK) {
    if (req->what == QR_CSERVE) {
      /* Delete L, add Q.  Check that they both exist first, though. */

      if (!(lnp=getnickbynick(RQ_LNICK)) || !(qnp=getnickbynick(RQ_QNICK))) {
        sendnoticetouser(rqnick, tnp,
                         "Error: Cannot find %s and %s on the network. "
                         "Please request again later.", RQ_LNICK, RQ_QNICK);
        free(req);
        return;
      }

      if ( !IsAccount(lnp) ) {
        sendnoticetouser(rqnick, tnp, "Internal Error Occured: L is not authed. Contact #help");
        free(req);
        return;
      } 
 
      /* /msg Q ADDCHAN <channel> <flags> <owners nick> <channeltype> */
      sendmessagetouser(rqnick, qnp, "ADDCHAN %s +ap #%s upgrade",
                        req->cip->name->content,
                        np->authname);
  
      sendnoticetouser(rqnick, tnp, "Adding %s to channel, please wait...",
                        RQ_QNICK);
    } else if (req->what == QR_SPAMSCAN) {
      /* Add S */

      if (!(snp=getnickbynick(RQ_SNICK))) {
        sendnoticetouser(rqnick, tnp,
                         "Error: Cannot find %s on the network. "
                         "Please request again later.", RQ_SNICK);

        free(req);
        return;
      }

      sendnoticetouser(rqnick, tnp, "Requirements met, %s should be added. "
                        "Contact #help should further assistance be required.",
                        RQ_SNICK);

      /* auth */
      user = (sstring *)getcopyconfigitem("request", "user", "R", 30);
      password = (sstring *)getcopyconfigitem("request", "password", "bla", 30);
      sendmessagetouser(rqnick, snp, "AUTH %s %s", user->content, password->content);
      freesstring(user);
      freesstring(password);

      /* /msg S addchan <channel> default */
      //sendmessagetouser(rqnick, snp, "ADDCHAN %s default +op", req->cip->name->content);

{
    spamscan_channelprofile *cp;
    spamscan_channelsettings *cs;
    spamscan_channelext *ce;
    chanindex *sc_index;
    
    cs = spamscan_getchannelsettings(req->cip->name->content, 0);
    
    if ( !cs ) {
        cp = spamscan_getchannelprofile("Default", 0);
        
        if ( cp ) {
            cs = spamscan_getchannelsettings(req->cip->name->content, 1);
            
            if ( cs ) {
                cs->cp = cp;
                cs->addedby = spamscan_getaccountsettings("R", 0);
                cs->flags = SPAMSCAN_CF_IGNOREOPS;
                
                sc_index = findorcreatechanindex(req->cip->name->content);
                
                if ( sc_index ) {
                    ce = spamscan_getchanext(sc_index, 1);
                    ce->cs = cs;
                    
                    if ( s_nickname && !CFIsSuspended(cs) && sc_index->channel ) {
                        ce->joinedchan = 1;
                        localjoinchannel(s_nickname, sc_index->channel);
                        localgetops(s_nickname, sc_index->channel);
                    }
                }
                
                spamscan_insertchanneldb(cs);
            }
        }
    }
}

      /* we do not put the request into another queue, so free it here */
      free(req);
      
      return;
    }

    if (lastqreq)
      lastqreq->next=req;
    else
      lastqreq=nextqreq=req;

    req->next=NULL;

    rq_success++;

    /* Don't free, it's in new queue now */
  } else {
    /* Sort out the message.. */
    va_start(va, message);
    vsnprintf(msgbuf,511,message,va);
    va_end(va);

    sendnoticetouser(rqnick, tnp, "%s", msgbuf);
    /* This is a failure message.  Add disclaimer. */
    /*sendnoticetouser(rqnick, tnp, "Do not complain about this result in #help or #feds.");*/
    free(req);

    rq_failed++;
  }
}

/*
 * qr_checksize: 
 *  Checks that a channel is beeeeg enough for teh Q
 */

static int qr_checksize(chanindex *cip, int what, char *failcode) {
  chanstats *csp;
  channel *cp;
  nick *np, *qbot;
  int i , avg, tot=0, authedcount=0, count=0, uniquecount, avgcount;

  cp = cip->channel;
  
  if (cp == NULL)
    return 0; /* this shouldn't ever happen */

#if QR_DEBUG
  return 1;
#endif

  /* make sure we can actually add Q */
  if (what == QR_CSERVE) {
    qbot = getnickbynick(RQ_QNICK);

    if (!qbot)
      return 0;

    if (QR_MAXQCHANS != 0 && qbot->channels->cursi > QR_MAXQCHANS) {
      qr_a++;
      *failcode = 'A';
      return 0; /* no Q for you :p */
    }
  }

  /* make sure that there are enough authed users */
  for (i=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i] != nouser) {
      np = getnickbynumeric(cp->users->content[i]);
      
      if (IsAccount(np))
        authedcount++;
        
      count++;
    }
  }

  int authedpct;
  
  if (what == QR_CSERVE) {
    if ( count <= QR_AUTHEDPCT_SCALE ) {
      authedpct = QR_AUTHEDPCT_CSERVE;
    } else if ( count >= QR_AUTHEDPCT_SCALEMAX ) {
      authedpct = QR_AUTHEDPCT_CSERVEMIN;
    } else {
      authedpct = (QR_AUTHEDPCT_CSERVEMIN + (((QR_AUTHEDPCT_CSERVE - QR_AUTHEDPCT_CSERVEMIN) * (100 - (((count - QR_AUTHEDPCT_SCALE) * 100) / (QR_AUTHEDPCT_SCALEMAX - QR_AUTHEDPCT_SCALE)))) / 100));
    }
  }
  else {
    if ( count <= QR_AUTHEDPCT_SCALE ) {
      authedpct = QR_AUTHEDPCT_SPAMSCAN;
    } else if ( count >= QR_AUTHEDPCT_SCALEMAX ) {
      authedpct = QR_AUTHEDPCT_SPAMSCANMIN;
    } else {
      authedpct = (QR_AUTHEDPCT_SPAMSCANMIN + (((QR_AUTHEDPCT_SPAMSCAN - QR_AUTHEDPCT_SPAMSCANMIN) * (100 - (((count - QR_AUTHEDPCT_SCALE) * 100) / (QR_AUTHEDPCT_SCALEMAX - QR_AUTHEDPCT_SCALE)))) / 100));
    }
  }
  
  if (authedcount * 100 / count < authedpct) {
    qr_b++;
    *failcode = 'B';
    return 0; /* too few authed users */
  }

  if (!(csp=cip->exts[csext]))
    return 0;

  for (i=0;i<HISTORYDAYS;i++) {
    tot += csp->lastdays[i];
  }

  uniquecount = countuniquehosts(cp);
  avgcount = tot / HISTORYDAYS / 10;

  /* chan needs at least QR_MINUSERPCT% of the avg usercount, and can't have
   * more than QR_MAXUSERPCT% */
  if ( what == QR_CSERVE ) {
    if ((avgcount * QR_MINUSERSPCT / 100 > uniquecount)) {
      qr_c++;
      *failcode = 'C';
      return 0;
    }
    if ((avgcount * QR_MAXUSERSPCT / 100 < uniquecount)) {
      qr_e++; 
      *failcode = 'E';
      return 0;
    }
  }

  avg = (what == QR_CSERVE) ? QR_REQUIREDSIZE_CSERVE : QR_REQUIREDSIZE_SPAMSCAN;

  if (tot > (avg * 140))
    return 1;
  else {
    qr_d++;
    *failcode = 'D';
    return 0;
  }
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

void qr_handle_notice(nick *sender, char *message) {
  char *ch, *chop;
  chanindex *cip;
  requestrec *rrp1, *rrp2;
  nick *np;
  int delrequest = 0, state, who;
  requestrec *nextreq;

/*  logcp = findchannel("#qnet.request");
  
  if (logcp)
    sendmessagetochannel(rqnick, logcp, "%s: %s - %d %d %x %x", sender->nick, message, rlstate, rqstate, nextreql, nextreqq);
*/
  if (!ircd_strcmp(sender->nick, RQ_QNICK) && nextqreq) {
    /* Message from Q */
    if (!ircd_strcmp(message,"Done.")) {
      /* Q added the channel: delete from L and tell the user. */
      /* If L has conspired to vanish between the request and the outcome,
       * we have a chan with Q and L... too bad. */

      if ((np=getnickbynick(RQ_LNICK))) {
        sendmessagetouser(rqnick, np, "SENDCHANLEV %s %s",
                          nextqreq->cip->name->content, RQ_QNICK);

        sendmessagetouser(rqnick, np, "DELCHAN %s",
                          nextqreq->cip->name->content);
      }

      if ((np=getnickbynumeric(nextqreq->reqnumeric))) {
        sendnoticetouser(rqnick, np, "Request completed. %s added.", RQ_QNICK);
      }
      
      delrequest = 1;
    } else if (!ircd_strcmp(message,"That channel already exists.")) {
      if ((np=getnickbynumeric(nextqreq->reqnumeric))) {
        sendnoticetouser(rqnick, np,
                         "Your channel '%s' appears to have %s already "
                         "(it may be suspended).", nextqreq->cip->name->content, RQ_QNICK);

        qr_suspended++;
        
        delrequest = 1;
      }
    }

    /* For either of the two messages above we want to delete the request
     * at the head of the queue. */
    if (delrequest) {
      rrp1=nextqreq;
  
      nextqreq=nextqreq->next;
      if (!nextqreq)
        lastqreq=NULL;
  
      free(rrp1);
    }
  }

  if (!ircd_strcmp(sender->nick, RQ_LNICK) || !ircd_strcmp(sender->nick, RQ_QNICK)) {
    who = !ircd_strcmp(sender->nick, RQ_LNICK) ? QR_L : QR_Q;
    state = (who == QR_Q) ? rqstate : rlstate;
    nextreq = (who == QR_Q) ? nextreqq : nextreql;

    /* Message from L or Q */
    switch (state) {
      case QRLstate_IDLE:
        /* We're idle, do nothing */
        return;

      case QRLstate_AWAITINGCHAN:
        /* We're waiting for conformation of the channel name */
        if ((!ircd_strncmp(message,"Users for",9) && who == QR_L) ||
          (!ircd_strncmp(message,"Known users on",14) && who == QR_Q)
        ) {
          /* Looks like the right message.  Let's find a channel name */

          for (ch=message;*ch;ch++)
            if (*ch=='#')
              break;

          if (!*ch) {
            Error("qrequest",ERR_WARNING,
                  "Unable to parse channel name from L/Q message: %s",message);
            return;
          }

          /* chop off any remaining words */
          chop = ch;
          while (*(chop++)) {
            if (*chop == ' ') {
              *chop = '\0';
              break;
            }
          }

          if (!(cip=findchanindex(ch))) {
            Error("qrequest",ERR_WARNING,
                  "Unable to find channel from L/Q message: %s",ch);
            return;
          }

          if (cip==nextreq->cip) {
            /* Ok, this is the correct channel, everything is proceeding
             * exactly as I had forseen */
            if (who == QR_L)
              rlstate = QRLstate_AWAITINGUSER;
            else
              rqstate = QRLstate_AWAITINGUSER;

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
              for(rrp2=nextreq;rrp2;) {
                if (rrp2==rrp1)
                  break;

                Error("qrequest",ERR_WARNING,
                      "Lost response for channel %s; skipping.",
                      rrp2->cip->name->content);

                qr_result(rrp2, QR_FAILED, 'X',
                          "Sorry, an error occurred while processing your request.");

                rrp2 = nextreq = (who == QR_Q) ? nextreqq : nextreql;
              }

              if (rrp2) {
                /* We seem to be back in sync. */
                if (who == QR_L)
                  rlstate = QRLstate_AWAITINGUSER;
                else
                  rqstate = QRLstate_AWAITINGUSER;

                return;
              }
              /* Some form of hole in the space time continuum exists
               * if we get here.  Unclear how to proceed. */
              return;
            } else {
              /* No match - let's just ignore this completely */
              Error("qrequest",ERR_WARNING,
                    "Ignoring L/Q response for spurious channel %s",
                    cip->name->content);
              return;
            }
          }
        }
        break;

      case QRLstate_AWAITINGUSER:
        if ((!ircd_strncmp(message, "End of chanlev",14) && who == QR_L) ||
          (!ircd_strncmp(message, "End of list.",12) && who == QR_Q)) {
          /* Oh dear, we got to the end of the chanlev in this state.
           * This means that we didn't find the user.
           */

          qr_result(nextreq, QR_FAILED, 'X', 
                    "Error: You are not known on %s.",
                    nextreq->cip->name->content);

          /* need to reset nextreq .. just in case
           * qr_result has cleaned up records */

          nextreq = (who == QR_Q) ? nextreqq : nextreql;

          if (nextreq) {
            if (who == QR_L)
              rlstate = QRLstate_AWAITINGUSER;
            else
              rqstate = QRLstate_AWAITINGUSER;
          } else {
            if (who == QR_L)
              rlstate = QRLstate_IDLE;
            else
              rqstate = QRLstate_IDLE;
          }

          qr_nochanlev++;

          return;
        } else {
          /* Brutalise the message :-) */

          if (who == QR_Q) {
            while (*message == ' ')
              message++;
          }

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
            char failcode;
            /* They iz teh +n! */
            
            /* Note: We're checking for blocks kind of late, so the request
               system gets a chance to send other error messages first (like
              'no chanstats', 'not known on channel', etc.). This is required
              so that the user doesn't notice that he's being blocked. */
            if (qr_checksize(nextreq->cip, nextreq->what, &failcode) && !qr_blockcheck(nextreq)) {
              qr_result(nextreq, QR_OK, '-', "OK");
            } else {
              if (nextreq->what == QR_CSERVE) {
                qr_result(nextreq, QR_FAILED, failcode,
                          "Error: Sorry, Your channel '%s' does not meet the requirements "
                          "for %s. Please continue to use %s.", nextreq->cip->name->content, RQ_QNICK, RQ_LNICK);                          
              } else {
                qr_result(nextreq, QR_FAILED, failcode,
                          "Error: Sorry, Your channel '%s' does not require %s. Please try again in a few days.", nextreq->cip->name->content, RQ_SNICK);
              }

              qr_toosmall++;
            }
          } else {
            qr_result(nextreq, QR_FAILED, 'X', 
                      "Error: You don't hold the +n flag on %s.",
                      nextreq->cip->name->content);

            qr_notowner++;
          }

          /* OK, we found what we wanted so make sure we skip the rest */
          if (who == QR_L)
            rlstate = QRLstate_AWAITINGEND;
          else
            rqstate = QRLstate_AWAITINGEND;

          return;
        }
        break;

      case QRLstate_AWAITINGEND:
        if (!ircd_strncmp(message, "End of chanlev",14) ||
          !ircd_strncmp(message, "End of list.",12)) {
          /* Found end of list */

          if (nextreq) {
            if (who == QR_L)
              rlstate = QRLstate_AWAITINGCHAN;
            else
              rqstate = QRLstate_AWAITINGCHAN;
          } else {
            if (who == QR_L)
              rlstate = QRLstate_IDLE;
            else
              rqstate = QRLstate_IDLE;
          }

          return;
        }
        break;
    }
  }
}

/*
 * This function deals with requests from users for Q.
 * Some sanity checks are made and the request is
 * added to the queue.
 */

int qr_requestq(nick *rqnick, nick *sender, channel *cp, nick *lnick, nick *qnick) {
  chanindex *cip = cp->index;

  /* Check:
   *  - we have some form of channel stats for the channel
   *
   * Note that the actual channel stats will not be checked
   * until we're sure the user has +n on the channel.
   */

  if (rq_isspam(sender)) {
      sendnoticetouser(rqnick, sender, "Error: Do not flood the request system."
            " Try again in %s.", rq_longtoduration(rq_blocktime(sender)));
    
      return RQ_ERROR;
  }

  if (!cip->exts[csext]) {
    sendnoticetouser(rqnick, sender,
                     "Error: No historical record exists for %s.",
                     cip->name->content);

    qr_nohist++;

    return RQ_ERROR;
  }

  /* Request stats from L */
  sendmessagetouser(rqnick, lnick, "CHANLEV %s", cip->name->content);

  /* Sort out a request record */
  if (lastreql) {
    lastreql->next = (requestrec *)malloc(sizeof(requestrec));
    lastreql=lastreql->next;
  } else {
    lastreql=nextreql=(requestrec *)malloc(sizeof(requestrec));
  }

  lastreql->next = NULL;
  lastreql->cip = cip;
  lastreql->what = QR_CSERVE;
  lastreql->who = QR_L;
  lastreql->reqnumeric = sender->numeric;

  if (rlstate == QRLstate_IDLE)
    rlstate = QRLstate_AWAITINGCHAN;

  sendnoticetouser(rqnick, sender,
                   "Checking your %s access. "
                   "This may take a while, please be patient...", RQ_LNICK);
                   
  /* we don't know yet whether the request was successful */
  return RQ_UNKNOWN;
}

int qr_instantrequestq(nick *sender, channel *cp) {
  requestrec *fakerequest;
  chanfix *cf;
  regop *ro;
  int rocount, i;
  char failcode;
  regop *rolist[QR_TOPX];

  if (!qr_checksize(cp->index, QR_CSERVE, &failcode))
    return RQ_ERROR;

  cf = cf_findchanfix(cp->index);

  if (cf == NULL)
    return RQ_ERROR;
    
  rocount = cf_getsortedregops(cf, QR_TOPX, rolist);

  ro = NULL;

  for (i = 0; i < min(QR_TOPX, rocount); i++) {
    if (cf_cmpregopnick(rolist[i], sender)) {
      ro = rolist[i];
      break;
    }
  }

  /* not in top 5 - we don't have to worry about that error here
     as the L request code will detect it again and send the user
     an appropriate message */
  if (ro == NULL)
    return RQ_ERROR;

  /* allocate a fake request */
  fakerequest = (requestrec *)malloc(sizeof(requestrec));

  fakerequest->reqnumeric = sender->numeric;
  fakerequest->cip = cp->index;
  fakerequest->what = QR_CSERVE;
  fakerequest->who = QR_L; /* pretend that we asked L about the chanlev */

  /* add it to the queue */
  if (nextreql == NULL) {
    fakerequest->next = NULL;
    nextreql = fakerequest;
    lastreql = fakerequest;
  } else {
    fakerequest->next = nextreql;
    nextreql = fakerequest;
  }
  
  qr_result(fakerequest, QR_OK, '-', "OK");
  
  return RQ_OK;
}

int qr_requests(nick *rqnick, nick *sender, channel *cp, nick *lnick, nick *qnick) {
  chanindex *cip = cp->index;
  int who = 0;
  requestrec *nextreq, *lastreq;

  if (rq_isspam(sender)) {
      sendnoticetouser(rqnick, sender, "Error: Do not flood the request system."
          " Try again in %s.", rq_longtoduration(rq_blocktime(sender)));
    
      return RQ_ERROR;
  }

  /* check which service is on the channel */
  if (getnumerichandlefromchanhash(cp->users, lnick->numeric) != NULL) {
    /* we've found L */
    who = QR_L;

    /* Request stats from L */
    sendmessagetouser(rqnick, lnick, "CHANLEV %s", cip->name->content);

    if (rlstate == QRLstate_IDLE)
      rlstate = QRLstate_AWAITINGCHAN;
  } else if (getnumerichandlefromchanhash(cp->users, qnick->numeric) != NULL) {
    /* we've found Q */
    who = QR_Q;

    /* Request stats from Q */
    sendmessagetouser(rqnick, qnick, "CHANLEV %s", cip->name->content);

    if (rqstate == QRLstate_IDLE)
      rqstate = QRLstate_AWAITINGCHAN;
  } /* 'else' cannot happen as R has already checked whether the user has L or Q */

  lastreq = (who == QR_Q) ? lastreqq : lastreql;
  nextreq = (who == QR_Q) ? nextreqq : nextreql;

  /* Sort out a request record */
  if (lastreq) {
    lastreq->next = (requestrec *)malloc(sizeof(requestrec));
    lastreq=lastreq->next;
  } else {
    lastreq=nextreq=(requestrec *)malloc(sizeof(requestrec));
  }

  lastreq->next = NULL;
  lastreq->cip = cip;
  lastreq->what = QR_SPAMSCAN;
  lastreq->reqnumeric = sender->numeric;

  if (who == QR_Q) {
    nextreqq = nextreq;
    lastreqq = lastreq;
  } else {
    nextreql = nextreq;
    lastreql = lastreq;
  }

   sendnoticetouser(rqnick, sender,
                   "Checking your %s access. "
                   "This may take a while, please be patient...",
                   who == QR_Q ? RQ_QNICK : RQ_LNICK);

  return RQ_UNKNOWN;
}

void qr_initrequest(void) {
  nextreql=lastreql=NULL;
  nextreqq=lastreqq=NULL;
  nextqreq=lastqreq=NULL;
}

void qr_finirequest(void) {
  struct requestrec *rp;

  while (nextreqq) {
    rp=nextreqq;
    nextreqq=nextreqq->next;
    free(rp);
  }

  while (nextreql) {
    rp=nextreql;
    nextreql=nextreql->next;
    free(rp);
  }

  while (nextqreq) {
    rp=nextqreq;
    nextqreq=nextqreq->next;
    free(rp);
  }
}

void qr_requeststats(nick *rqnick, nick *np) {
  sendnoticetouser(rqnick, np, "- Suspended (Q):                  %d", qr_suspended);
  sendnoticetouser(rqnick, np, "- No chanstats (Q/S):             %d", qr_nohist);
  sendnoticetouser(rqnick, np, "- Too small (Q/S):                %d", qr_toosmall);
  sendnoticetouser(rqnick, np, "- User was not on chanlev (Q/S):  %d", qr_nochanlev);
  sendnoticetouser(rqnick, np, "- User was not the owner (Q/S):   %d", qr_notowner);
  sendnoticetouser(rqnick, np, "- A:                              %d", qr_a);
  sendnoticetouser(rqnick, np, "- B:                              %d", qr_b);
  sendnoticetouser(rqnick, np, "- C:                              %d", qr_c);
  sendnoticetouser(rqnick, np, "- D:                              %d", qr_d);
  sendnoticetouser(rqnick, np, "- E:                              %d", qr_e);

}
