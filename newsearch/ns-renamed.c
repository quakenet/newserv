/*
 * renamed functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *renamed_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void renamed_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *renamed_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = renamed_exe;
  thenode->free = renamed_free;

  return thenode;
}

void *renamed_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  whowas *ww = (whowas *)np->next;

  if (ww->type != WHOWAS_RENAME)
    return (void *)0;
  
  return (void *)1;
}

void renamed_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

