/*
 * ALL functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void all_free(searchCtx *ctx, struct searchNode *thenode);
void *all_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);

#define MAX_ITERATIONS 1000

struct all_localdata {
  searchNode *genfn;
  searchNode *lambdafn;
  int hitlimit;
};

struct searchNode *all_parse(searchCtx *ctx, int type, int argc, char **argv) {
  searchNode *thenode;
  struct all_localdata *localdata;
  
  if(argc < 2) {
    parseError = "all: usage: all (generatorfn x) (fn ... (var x) ...)";
    return NULL;
  }

  if(!(localdata=(struct all_localdata *)malloc(sizeof(struct all_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  localdata->hitlimit = 0;

  if(!(localdata->genfn=ctx->parser(ctx, type, argv[0]))) {
    free(localdata);
    return NULL;
  }

  localdata->lambdafn = ctx->parser(ctx, type, argv[1]);
  if(!(localdata->lambdafn = coerceNode(ctx, localdata->lambdafn, RETURNTYPE_BOOL))) {
    (localdata->genfn->free)(ctx, localdata->genfn);
    free(localdata);
    return NULL;
  }
  
  if(!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    (localdata->genfn->free)(ctx, localdata->genfn);
    (localdata->lambdafn->free)(ctx, localdata->lambdafn);
    free(localdata);
    return NULL;
  }
  
  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->localdata    = localdata;
  thenode->exe          = all_exe;
  thenode->free         = all_free;

  return thenode;
}

void all_free(searchCtx *ctx, struct searchNode *thenode) {
  struct all_localdata *localdata = thenode->localdata;
  
  if(localdata->hitlimit)
    ctx->reply(senderNSExtern, "Warning: your expression hit the maximum iteration count and was terminated early.");
  
  (localdata->genfn->free)(ctx, localdata->genfn);
  (localdata->lambdafn->free)(ctx, localdata->lambdafn);
  
  free(localdata);
  free(thenode);
}

void *all_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct all_localdata *localdata = thenode->localdata;
  int i;

  if(localdata->hitlimit)
    return (void *)0;

  for(i=0;i<MAX_ITERATIONS;i++) {
    if(!(localdata->genfn->exe)(ctx, localdata->genfn, theinput))
      return (void *)1;
    
    if(!(localdata->lambdafn->exe)(ctx, localdata->lambdafn, theinput))
      return (void *)0;
  }
  
  localdata->hitlimit = 1;
  return (void *)0;
}
