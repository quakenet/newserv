/*
 * AUTHTS functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *authts_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void authts_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *authts_parse(searchCtx *ctx, int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "authts: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = authts_exe;
  thenode->free = authts_free;

  return thenode;
}

void *authts_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  if (IsAccount(np))
    return (void *)(np->accountts);
  else
    return (void *)0; /* will cast to a FALSE */

}

void authts_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
