#include "gline_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *gsns_glcreator_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void gsns_glcreator_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *gsns_glcreator_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = gsns_glcreator_exe;
  thenode->free = gsns_glcreator_free;

  return thenode;
}

void *gsns_glcreator_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  gline *g = (gline *)theinput;

  if (g->creator)
    return g->creator->content;
  return ""; 
}

void gsns_glcreator_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
