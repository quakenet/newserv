/*
 * qsuspended functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../chanserv.h"

#include <stdlib.h>

void *qsuspended_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void qsuspended_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *qsuspended_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe = qsuspended_exe;
  thenode->free = qsuspended_free;

  return thenode;
}

void *qsuspended_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  authname *ap = (authname *)theinput;
  reguser *rup = ap->exts[chanservext];
  if(!rup || !UHasSuspension(rup))
    return NULL;

  return (void *)1;
}

void qsuspended_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

