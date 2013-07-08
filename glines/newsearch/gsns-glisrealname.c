#include "gline_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *gsns_glisrealname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void gsns_glisrealname_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *gsns_glisrealname_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = gsns_glisrealname_exe;
  thenode->free = gsns_glisrealname_free;

  return thenode;
}

void *gsns_glisrealname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  gline *g = (gline *)theinput;

  if (g->flags & GLINE_REALNAME)
    return (void *)1;
  return (void *)0;
}

void gsns_glisrealname_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
