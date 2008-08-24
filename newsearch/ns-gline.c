/*
 * GLINE functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  int len;
  char *p;

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
    parseError = "gline: invalid search type";
    return NULL;
  }

  switch (argc) {
  case 0:
    localdata->duration = NSGLINE_DURATION;
    strlcpy(localdata->reason, defaultreason, sizeof(localdata->reason));
    break;

  case 1:
    if (strchr(argv[0], ' ') == NULL) { /* duration specified */
      localdata->duration = durationtolong(argv[0]);
      /* error checking on gline duration */
      if (localdata->duration == 0)
        localdata->duration = NSGLINE_DURATION;
      strlcpy(localdata->reason, defaultreason, sizeof(localdata->reason));
    }
    else { /* reason specified */
      localdata->duration = NSGLINE_DURATION;

      p = argv[0];
      if(*p == '\"')
        p++;
      len = strlcpy(localdata->reason, p, sizeof(localdata->reason));
      if(len >= sizeof(localdata->reason)) {
        localdata->reason[sizeof(localdata->reason)-1] = '\0';
      } else {
        localdata->reason[len-1] = '\0';
      }
    }
    break;

  case 2:
    localdata->duration = durationtolong(argv[0]);
    /* error checking on gline duration */
    if (localdata->duration == 0)
      localdata->duration = NSGLINE_DURATION;

    p = argv[1];
    if(*p == '\"')
      p++;
    len = strlcpy(localdata->reason, p, sizeof(localdata->reason));
    if(len >= sizeof(localdata->reason)) {
      localdata->reason[sizeof(localdata->reason)-1] = '\0';
    } else {
      localdata->reason[len-1] = '\0';
    }

    break;
  default:
    free(localdata);
    parseError = "gline: invalid number of arguments";
    return NULL;
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
    localdata->count += cip->channel->users->totalusers;
  }
  else {
    np = (nick *)theinput;
    np->marker = localdata->marker;
    localdata->count++;
  }

  return (void *)1;
}

void gline_free(searchCtx *ctx, struct searchNode *thenode) {
  struct gline_localdata *localdata;
  nick *np, *nnp;
  chanindex *cip, *ncip;
  int i, j, safe=0;
  char msgbuf[512];
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
              if (!IsOper(np) && !IsService(np) && !IsXOper(np)) {
                nssnprintf(msgbuf, sizeof(msgbuf), localdata->reason, np);
                if (np->host->clonecount <= NSMAX_GLINE_CLONES)
                  irc_send("%s GL * +*@%s %u %jd :%s", mynumeric->content, IPtostr(np->p_ipaddr), localdata->duration, (intmax_t)ti, msgbuf);
                else
                  irc_send("%s GL * +%s@%s %u %jd :%s", mynumeric->content, np->ident, IPtostr(np->p_ipaddr), localdata->duration, (intmax_t)ti, msgbuf);
              }
              else
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
          if (!IsOper(np) && !IsService(np) && !IsXOper(np)) {
            nssnprintf(msgbuf, sizeof(msgbuf), localdata->reason, np);
            if (np->host->clonecount <= NSMAX_GLINE_CLONES)
              irc_send("%s GL * +*@%s %u %jd :%s", mynumeric->content, IPtostr(np->p_ipaddr), localdata->duration, (intmax_t)ti, msgbuf);
            else
              irc_send("%s GL * +%s@%s %u %jd :%s", mynumeric->content, np->ident, IPtostr(np->p_ipaddr), localdata->duration, (intmax_t)ti, msgbuf);
          }
          else
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
