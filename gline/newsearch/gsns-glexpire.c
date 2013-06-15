#include "gline_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *gsns_glexpire_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void gsns_glexpire_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *gsns_glexpire_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = gsns_glexpire_exe;
  thenode->free = gsns_glexpire_free;

  return thenode;
}

void *gsns_glexpire_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  gline *g = (gline *)theinput;

  return (void *)(g->expires);
}

void gsns_glexpire_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
