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

struct searchNode *timestamp_parse(searchCtx *ctx, int type, int argc, char **argv) {
  struct searchNode *thenode;

  if ((type != SEARCHTYPE_NICK) && (type != SEARCHTYPE_CHANNEL)){
    parseError = "timestamp: this function is only valid for nick or channel searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = (void *)(long)type;
  thenode->exe = timestamp_exe;
  thenode->free = timestamp_free;

  return thenode;
}

void *timestamp_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  chanindex *cip = (chanindex *)theinput;
  
  if ((long)thenode->localdata == SEARCHTYPE_NICK) {
    return (void *)np->timestamp;
  } else {
    if (cip->channel)
      return (void *)cip->channel->timestamp;
    else
      return 0;
  }
}

void timestamp_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
