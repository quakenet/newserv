/*
 * TIMESTAMP functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *timestamp_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void timestamp_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *timestamp_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (ctx->type != SEARCHTYPE_NICK) {
    parseError = "timestamp: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = timestamp_exe;
  thenode->free = timestamp_free;

  return thenode;
}

void *timestamp_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return (void *)np->timestamp;
}

void timestamp_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
