/*
 * thastrust functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../trusts.h"

#include <stdlib.h>

void *thastrust_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void thastrust_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *thastrust_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if(!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe = thastrust_exe;
  thenode->free = thastrust_free;

  return thenode;
}

void *thastrust_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = theinput;
  trusthost *th = gettrusthost(np);

  if(!th)
    return (void *)0;

  return (void *)1;
}

void thastrust_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

