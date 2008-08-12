/*
 * services functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *services_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void services_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *services_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = services_exe;
  thenode->free = services_free;

  return thenode;
}

void *services_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  nick *np;
  int i;
  unsigned long count=0;
  
  if (cip->channel == NULL)
    return (void *)0;

  for(i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
      
    if (!(np=getnickbynumeric(cip->channel->users->content[i])))
      continue;

    if (IsService(np))
      count++;
  }
  
  return (void *)count;
}

void services_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

