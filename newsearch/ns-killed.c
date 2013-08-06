/*
 * quit functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *killed_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void killed_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *killed_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = killed_exe;
  thenode->free = killed_free;

  return thenode;
}

void *killed_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  whowas *ww = (whowas *)np->next;

  if (ww->type != WHOWAS_KILL)
    return (void *)0;
  
  return (void *)1;
}

void killed_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

