/*
 * topic functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *topic_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void topic_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *topic_parse(searchCtx *ctx, int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "topic: this function is only valid for channel searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = topic_exe;
  thenode->free = topic_free;

  return thenode;
}

void *topic_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  
  if ((cip->channel==NULL) || (cip->channel->topic==NULL))
    return "";
  else
    return cip->channel->topic->content;
}

void topic_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

