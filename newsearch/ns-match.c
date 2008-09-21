/*
 * MATCH functionality
 */

#include "newsearch.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <stdlib.h>

struct match_localdata {
  struct searchNode *targnode;
  struct searchNode *patnode;
};

void *match_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void match_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *match_parse(searchCtx *ctx, int argc, char **argv) {
  struct match_localdata *localdata;
  struct searchNode *thenode;
  struct searchNode *targnode, *patnode;

  if (argc<2) {
    parseError="match: usage: match source pattern";
    return NULL;
  }

  /* @fixme check this works with new parsing semantics */
  targnode = ctx->parser(ctx, argv[0]);
  if (!(targnode = coerceNode(ctx,targnode, RETURNTYPE_STRING)))
    return NULL;

  patnode = ctx->parser(ctx, argv[1]);
  if (!(patnode = coerceNode(ctx,patnode, RETURNTYPE_STRING))) {
    (targnode->free)(ctx, targnode);
    return NULL;
  }

  if (!(localdata=(struct match_localdata *)malloc(sizeof (struct match_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    (targnode->free)(ctx, targnode);
    (patnode->free)(ctx, patnode);
    return NULL;
  }
    
  localdata->targnode=targnode;
  localdata->patnode=patnode;

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    (targnode->free)(ctx, targnode);
    (patnode->free)(ctx, patnode);
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = match_exe;
  thenode->free = match_free;

  return thenode;
}

void *match_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct match_localdata *localdata;
  char *pattern, *target;

  localdata = thenode->localdata;
  
  pattern = (char *)(localdata->patnode->exe) (ctx, localdata->patnode, theinput);
  target  = (char *)(localdata->targnode->exe)(ctx, localdata->targnode,theinput);

  return (void *)(long)match2strings(pattern, target);
}

void match_free(searchCtx *ctx, struct searchNode *thenode) {
  struct match_localdata *localdata;

  localdata=thenode->localdata;

  (localdata->patnode->free)(ctx, localdata->patnode);
  (localdata->targnode->free)(ctx, localdata->targnode);
  free(localdata);
  free(thenode);
}

