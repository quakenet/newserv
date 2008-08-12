/*
 * REALNAME functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *realname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void realname_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *realname_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = realname_exe;
  thenode->free = realname_free;

  return thenode;
}

void *realname_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return np->realname->name->content;
}

void realname_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

