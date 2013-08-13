/*
 * MESSAGE functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *message_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void message_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *message_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = message_exe;
  thenode->free = message_free;

  return thenode;
}

void *message_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return (np->message) ? np->message->content : "";
}

void message_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

