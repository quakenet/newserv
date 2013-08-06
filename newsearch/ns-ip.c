/*
 * ip functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *ip_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void ip_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *ip_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = ip_exe;
  thenode->free = ip_free;

  return thenode;
}

void *ip_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return (void *)IPtostr(np->ipaddress);
}

void ip_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

