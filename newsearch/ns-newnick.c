/*
 * newnick functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *newnick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void newnick_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *newnick_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = newnick_exe;
  thenode->free = newnick_free;

  return thenode;
}

void *newnick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  whowas *ww = (whowas *)np->next;

  if (ww->type == WHOWAS_RENAME)
    return "";
  
  return (void *)ww->newnick->content;
}

void newnick_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

