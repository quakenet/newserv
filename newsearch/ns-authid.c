/*
 * AUTHID functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *authid_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void authid_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *authid_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (ctx->type != SEARCHTYPE_NICK) {
    parseError = "authid: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = authid_exe;
  thenode->free = authid_free;

  return thenode;
}

void *authid_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  if (IsAccount(np) && np->auth)
    return (void *)(np->auth->userid);
  else
    return (void *)0; /* will cast to a FALSE */

}

void authid_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
