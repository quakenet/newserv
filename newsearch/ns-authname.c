/*
 * AUTHNAME functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *authname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void authname_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *authname_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (ctx->type != SEARCHTYPE_NICK) {
    parseError = "authname: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = authname_exe;
  thenode->free = authname_free;

  return thenode;
}

void *authname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  if (IsAccount(np))
    return np->authname;
  else
    return ""; /* will cast to a FALSE */

}

void authname_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

