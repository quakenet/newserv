/*
 * AWAY functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *away_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void away_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *away_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = away_exe;
  thenode->free = away_free;

  return thenode;
}

void *away_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return (np->away) ? np->away->content : "";
}

void away_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

