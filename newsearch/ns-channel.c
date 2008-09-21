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
  channel *cp;
  char *p;
  
  if (argc<1) {
    parseError = "channel: usage: channel <channel name>";
    return NULL;
  }

  if (!(convsn=argtoconststr("channel", ctx, argv[0], &p)))
    return NULL;

  cp=findchannel(p);
  convsn->free(ctx, convsn);
  if (!cp) {
    parseError = "channel: unknown channel";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = cp;
  thenode->exe = channel_exe;
  thenode->free = channel_free;

  return thenode;
}

void *channel_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  channel *cp = thenode->localdata;

  if (getnumerichandlefromchanhash(cp->users, np->numeric)) {
    return (void *)1;
  } else {
    return (void *)0;
  }
}

void channel_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

