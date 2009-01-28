/* required modules: splitlist, chanfix(3) */

#include <stdio.h>
#include <string.h>
#include "request.h"
#include "lrequest.h"
#include "request_block.h"
#include "../localuser/localuser.h"

/* stats counters */
int lr_noregops = 0;
int lr_scoretoolow = 0;
int lr_top5 = 0;
int lr_floodattempts = 0;

#define min(a,b) ((a > b) ? b : a)

int lr_requestl(nick *svc, nick *np, channel *cp, nick *qnick) {
  chanfix *cf;
  regop *rolist[LR_TOPX], *ro;
  int i, rocount;

  if (strlen(cp->index->name->content) > LR_MAXCHANLEN) {
    sendnoticetouser(svc, np, "Sorry, your channel name is too long. You will have to "
          "create a channel with a name less than %d characters long.",
          LR_MAXCHANLEN + 1);

    return RQ_ERROR;
  }

  cf = cf_findchanfix(cp->index);

  if (cf == NULL) {
    sendnoticetouser(svc, np, "Sorry, your channel '%s' was created recently. "
          "Please try again in an hour.", cp->index->name->content);

    lr_noregops++;

    return RQ_ERROR;
  }

  rocount = cf_getsortedregops(cf, LR_TOPX, rolist);

  ro = NULL;

  for (i = 0; i < min(LR_TOPX, rocount); i++) {
    if (cf_cmpregopnick(rolist[i], np)) {
      ro = rolist[i];
      break;
    }
  }

  if (ro == NULL) {
    sendnoticetouser(svc, np, "Sorry, you must be one of the top %d ops "
          "for the channel '%s'.", LR_TOPX, cp->index->name->content);

    lr_top5++;

    return RQ_ERROR;
  }

  /* treat blocked users as if their score is too low */
  if (ro->score < LR_CFSCORE || rq_findblock(np->authname)) {
    if (rq_isspam(np)) {
      sendnoticetouser(svc, np, "Do not flood the request system. "
            "Try again in %s.", rq_longtoduration(rq_blocktime(np)));

      lr_floodattempts++;

      return RQ_ERROR;
    }

    sendnoticetouser(svc, np, "Sorry, you do not meet the "
          "%s request requirements; please try again in an hour, "
          "see http://www.quakenet.org/faq/faq.php?c=1&f=6#6", RQ_QNICK);

    lr_scoretoolow++;

    return RQ_ERROR;
  }

  
  sendmessagetouser(svc, qnick, "addchan %s #%s +jp upgrade %s", cp->index->name->content,
        np->authname, np->nick);

  sendnoticetouser(svc, np, "Success! %s has been added to '%s' "
        "(contact #help if you require further assistance).", 
        RQ_QNICK, cp->index->name->content);

  return RQ_OK;
}

void lr_requeststats(nick *rqnick, nick *np) {
  sendnoticetouser(rqnick, np, "- No registered ops (Q):          %d", lr_noregops);
  sendnoticetouser(rqnick, np, "- Score too low (Q):              %d", lr_scoretoolow);
  sendnoticetouser(rqnick, np, "- Not in top%d (Q):                %d", LR_TOPX, lr_top5);
  sendnoticetouser(rqnick, np, "- Floods (Q):                     %d", lr_floodattempts);
}
