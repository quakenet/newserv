/*
 * CHANNELS functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *channels_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void channels_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *channels_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = channels_exe;
  thenode->free = channels_free;

  return thenode;
}

void *channels_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return (void *)(long)np->channels->cursi;
}

void channels_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
