/*
 * NOT functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void not_free(searchCtx *ctx, struct searchNode *thenode);
void *not_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);

struct searchNode *not_parse(searchCtx *ctx, int argc, char **argv) {
  searchNode *thenode, *subnode;

  if (argc!=1) {
    parseError="not: usage: not (term)";
    return NULL;
  }
    
  /* Allocate our actual node */
  if (!(thenode=(searchNode *)malloc(sizeof(searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->exe          = not_exe;
  thenode->free         = not_free;

  subnode=ctx->parser(ctx, argv[0]); /* Propogate the search type */

  if (!subnode) {
    free(thenode);
    return NULL;
  }

  /* Our subnode needs to return a BOOL */  
  subnode=coerceNode(ctx, subnode, RETURNTYPE_BOOL);
  if(!subnode) {
    free(thenode);
    return NULL;
  }
  
  thenode->localdata=(void *)subnode;
      
  return thenode;
}

void not_free(searchCtx *ctx, struct searchNode *thenode) {
  struct searchNode *subnode;
  subnode=thenode->localdata;

  (subnode->free)(ctx, subnode);
  free(thenode);
}

void *not_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct searchNode *subnode;

  subnode=thenode->localdata;

  if ((subnode->exe)(ctx, subnode, theinput)) {
    return (void *)0;
  } else {
    return (void *)1;
  }
}
