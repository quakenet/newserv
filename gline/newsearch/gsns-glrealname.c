#include "gline_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *gsns_glrealname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void gsns_glrealname_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *gsns_glrealname_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = gsns_glrealname_exe;
  thenode->free = gsns_glrealname_free;

  return thenode;
}

void *gsns_glrealname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  gline *g = (gline *)theinput;

  if (GlineIsRealName(g))
    if (g->user)
      return g->user->content + 2;
  return ""; 
}

void gsns_glrealname_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
