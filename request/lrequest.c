/* required modules: splitlist, chanfix(3) */

#include <stdio.h>
#include <string.h>
#include "request.h"
#include "lrequest.h"
#include "request_block.h"
#include "request_fasttrack.h"
#include "../localuser/localuser.h"

/* stats counters */
int lr_top5 = 0;
int lr_notargets = 0;

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

  if(cf) {
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
  }

  /* treat blocked users as if they're out of targets */
  if(rq_findblock(np->authname) || !rq_tryfasttrack(np)) {
    sendnoticetouser(svc, np, "Sorry, you may not request %s for another "
      "channel at this time. Please try again in an hour.", rq_qnick->content);

    lr_notargets++;

    return RQ_ERROR;
  }

  sendmessagetouser(svc, qnick, "addchan %s #%s +jp upgrade %s", cp->index->name->content,
        np->authname, np->nick);

  sendnoticetouser(svc, np, "Success! %s has been added to '%s' "
        "(contact #help if you require further assistance).", 
        rq_qnick->content, cp->index->name->content);

  return RQ_OK;
}

void lr_requeststats(nick *rqnick, nick *np) {
  sendnoticetouser(rqnick, np, "- Too many requests (Q):          %d", lr_notargets);
  sendnoticetouser(rqnick, np, "- Not in top%d (Q):                %d", LR_TOPX, lr_top5);
}
