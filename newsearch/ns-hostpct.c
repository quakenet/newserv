/*
 * hostpct functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *hostpct_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void hostpct_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *hostpct_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = hostpct_exe;
  thenode->free = hostpct_free;

  return thenode;
}

void *hostpct_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  int i;
  unsigned int marker;
  unsigned int hosts=0;
  nick *np;
  chanindex *cip = (chanindex *)theinput;

  if (cip->channel==NULL)
    return (void *)0;
  
  marker=nexthostmarker();
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
      
    if (!(np=getnickbynumeric(cip->channel->users->content[i])))
      continue;

    if (np->host->marker!=marker) {
      hosts++;
      np->host->marker=marker;
    }
  }
  
  return (void *)(long)((hosts * 100)/cip->channel->users->totalusers);
}

void hostpct_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

