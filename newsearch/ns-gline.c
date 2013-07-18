/*
 * GLINE functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../control/control.h"
#include "../irc/irc.h" /* irc_send() */
#include "../lib/irc_string.h" /* IPtostr(), longtoduration(), durationtolong() */
#include "../lib/strlfunc.h"

/* used for *_free functions that need to warn users of certain things
   i.e. hitting too many users in a (kill) or (gline) - declared in newsearch.c */
extern nick *senderNSExtern;
static const char *defaultreason = "You (%u) have been g-lined for violating our terms of service";

void *gline_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void gline_free(searchCtx *ctx, struct searchNode *thenode);

struct gline_localdata {
  unsigned int marker;
  unsigned int duration;
  int count;
  char reason[NSMAX_REASON_LEN];
};

struct searchNode *gline_parse(searchCtx *ctx, int argc, char **argv) {
  struct gline_localdata *localdata;
  struct searchNode *thenode;

  if (!(localdata = (struct gline_localdata *) malloc(sizeof(struct gline_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  localdata->count = 0;
  if (ctx->searchcmd == reg_chansearch)
    localdata->marker = nextchanmarker();
  else if (ctx->searchcmd == reg_nicksearch)
    localdata->marker = nextnickmarker();
  else {
    free(localdata);
    parseError = "gline: invalid search type";
    return NULL;
  }

  /* default duration, default reason */
  if(argc == 0) {
    localdata->duration = NSGLINE_DURATION;
    strlcpy(localdata->reason, defaultreason, sizeof(localdata->reason));
  } else if(argc > 2) {
    free(localdata);
    parseError = "gline: invalid number of arguments";
    return NULL;
  } else {
    char *argzerop, *reasonp, *durationp;
    struct searchNode *durationsn, *reasonsn, *argzerosn;
    
    if (!(argzerosn=argtoconststr("gline", ctx, argv[0], &argzerop))) {
      free(localdata);
      return NULL;
    }

    if(argc == 1) {
      durationp = reasonp = NULL;
      durationsn = reasonsn = NULL;
      
      /* if we have a space it's a reason */
      if(strchr(argzerop, ' ')) {
        reasonsn = argzerosn;
        reasonp = argzerop;
      } else {
        durationsn = argzerosn;
        durationp = argzerop;
      }
    } else {
      durationsn = argzerosn;
      durationp = argzerop;
      
      if (!(reasonsn=argtoconststr("gline", ctx, argv[1], &reasonp))) {
        durationsn->free(ctx, durationsn);
        free(localdata);
        return NULL;
      }      
    }
    
    if(!reasonp) {
      strlcpy(localdata->reason, defaultreason, sizeof(localdata->reason));
    } else {
      strlcpy(localdata->reason, reasonp, sizeof(localdata->reason));
      reasonsn->free(ctx, reasonsn);
    }

    if(!durationp) {
      localdata->duration = NSGLINE_DURATION;
    } else {
      localdata->duration = durationtolong(durationp);
      durationsn->free(ctx, durationsn);
      
      if (localdata->duration == 0) {
        parseError = "gline duration invalid.";
        free(localdata);
        return NULL;
      }
    }
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = gline_exe;
  thenode->free = gline_free;

  return thenode;
}

void *gline_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct gline_localdata *localdata;
  nick *np;
  chanindex *cip;

  localdata = thenode->localdata;

  if (ctx->searchcmd == reg_chansearch) {
    cip = (chanindex *)theinput;
    cip->marker = localdata->marker;
    if (cip->channel != NULL)
      localdata->count += cip->channel->users->totalusers;
  }
  else {
    np = (nick *)theinput;
    np->marker = localdata->marker;
    localdata->count++;
  }

  return (void *)1;
}

static int glineuser(nick *np, struct gline_localdata *localdata, time_t ti) {
  char msgbuf[512];
  if (!IsOper(np) && !IsService(np) && !IsXOper(np)) {
    nssnprintf(msgbuf, sizeof(msgbuf), localdata->reason, np);
    if (np->host->clonecount <= NSMAX_GLINE_CLONES)
      irc_send("%s GL * +*@%s %u %jd :%s", mynumeric->content, IPtostr(np->p_ipaddr), localdata->duration, (intmax_t)ti, msgbuf);
    else
      irc_send("%s GL * +%s@%s %u %jd :%s", mynumeric->content, np->ident, IPtostr(np->p_ipaddr), localdata->duration, (intmax_t)ti, msgbuf);
    return 1;
  }
  
  return 0;
}

void gline_free(searchCtx *ctx, struct searchNode *thenode) {
  struct gline_localdata *localdata;
  nick *np, *nnp;
  chanindex *cip, *ncip;
  int i, j, safe=0;
  time_t ti = time(NULL);

  localdata = thenode->localdata;

  if (localdata->count > NSMAX_GLINE_LIMIT) {
    /* need to warn the user that they have just tried to twat half the network ... */
    ctx->reply(senderNSExtern, "Warning: your pattern matches too many users (%d) - nothing done.", localdata->count);
    free(localdata);
    free(thenode);
    return;
  }

  if (ctx->searchcmd == reg_chansearch) {
    for (i=0;i<CHANNELHASHSIZE;i++) {
      for (cip=chantable[i];cip;cip=ncip) {
        ncip = cip->next;
        if (cip != NULL && cip->channel != NULL && cip->marker == localdata->marker) {
          for (j=0;j<cip->channel->users->hashsize;j++) {
            if (cip->channel->users->content[j]==nouser)
              continue;
    
            if ((np=getnickbynumeric(cip->channel->users->content[j]))) {
              if(!glineuser(np, localdata, ti))
                safe++;
            }
          }
        }
      }
    }
  }
  else {
    for (i=0;i<NICKHASHSIZE;i++) {
      for (np=nicktable[i];np;np=nnp) {
        nnp = np->next;
        if (np->marker == localdata->marker) {
          if(!glineuser(np, localdata, ti))
            safe++;
        }
      }
    }
  }
  if (safe)
    ctx->reply(senderNSExtern, "Warning: your pattern matched privileged users (%d in total) - these have not been touched.", safe);
  /* notify opers of the action */
  ctx->wall(NL_GLINES, "%s/%s glined %d %s via %s for %s [%d untouched].", senderNSExtern->nick, senderNSExtern->authname, (localdata->count - safe), 
    (localdata->count - safe) != 1 ? "users" : "user", (ctx->searchcmd == reg_chansearch) ? "chansearch" : "nicksearch", longtoduration(localdata->duration, 1), safe);
  free(localdata);
  free(thenode);
}
