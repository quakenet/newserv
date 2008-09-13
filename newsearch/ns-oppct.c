/*
 * oppct functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *oppct_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void oppct_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *oppct_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = oppct_exe;
  thenode->free = oppct_free;

  return thenode;
}

void *oppct_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  int i;
  int ops;
  chanindex *cip = (chanindex *)theinput;
  
  if (cip->channel==NULL || cip->channel->users->totalusers==0)
    return (void *)0;
  
  ops=0;
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]!=nouser) {
      if (cip->channel->users->content[i] & CUMODE_OP) {
        ops++;
      }
    }
  }

  return (void *)(long)((ops * 100) / cip->channel->users->totalusers);
}

void oppct_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

