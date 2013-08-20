/*
 * CHANNEL functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../channel/channel.h"

void *channel_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void channel_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *channel_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode, *convsn;
  chanindex *cip;
  char *p;
  
  if (argc<1) {
    parseError = "channel: usage: channel <channel name>";
    return NULL;
  }

  if (!(convsn=argtoconststr("channel", ctx, argv[0], &p)))
    return NULL;

  cip=findorcreatechanindex(p);
  convsn->free(ctx, convsn);

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = cip;
  thenode->exe = channel_exe;
  thenode->free = channel_free;

  return thenode;
}

void *channel_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  chanindex *cip = thenode->localdata;
  channel *cp;
  whowas *ww;
  int i;

  if (ctx->searchcmd == reg_nicksearch) {
    cp = cip->channel;

    if (!cp)
      return (void *)0;

    if (getnumerichandlefromchanhash(cp->users, np->numeric))
      return (void *)1;
  } else {
    ww = (whowas *)np->next; /* Eww. */

    for (i = 0; i < WW_MAXCHANNELS; i++)
      if (ww->channels[i] == cip)
        return (void *)1;
  }

  return (void *)0;
}

void channel_free(searchCtx *ctx, struct searchNode *thenode) {
  releasechanindex(thenode->localdata);
  free(thenode);
}

