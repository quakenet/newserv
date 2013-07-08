#include "gline_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *gsns_glbadchan_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void gsns_glbadchan_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *gsns_glbadchan_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = gsns_glbadchan_exe;
  thenode->free = gsns_glbadchan_free;

  return thenode;
}

void *gsns_glbadchan_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  gline *g = (gline *)theinput;

  if (g->flags & GLINE_BADCHAN)
    if (g->user)
      return g->user->content;
  return ""; 
}

void gsns_glbadchan_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
