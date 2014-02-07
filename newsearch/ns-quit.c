/*
 * quit functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *quit_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void quit_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *quit_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = quit_exe;
  thenode->free = quit_free;

  return thenode;
}

void *quit_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  whowas *ww = (whowas *)np->next;

  if (ww->type != WHOWAS_QUIT)
    return (void *)0;
  
  return (void *)1;
}

void quit_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

