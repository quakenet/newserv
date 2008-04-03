/*
 * EQ functionality
 */

#include "newsearch.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <stdlib.h>

struct eq_localdata {
  int type;
  int count;
  struct searchNode **nodes;
};

void eq_free(searchCtx *ctx, struct searchNode *thenode);
void *eq_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);

struct searchNode *eq_parse(searchCtx *ctx, int type, int argc, char **argv) {
  struct eq_localdata *localdata;
  struct searchNode *thenode;
  int i;

  if (!(localdata = (struct eq_localdata *)malloc(sizeof(struct eq_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  if (!(localdata->nodes = (struct searchNode **)malloc(sizeof(struct searchNode *) * argc))) {
    /* couldn't malloc() memory for localdata->nodes, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }
  localdata->count = 0;
  
  if (!(thenode = (struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata and localdata->nodes to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata->nodes);
    free(localdata);
    return NULL;
  }
  
  thenode->localdata  = localdata;
  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe  = eq_exe;
  thenode->free = eq_free;
  
  for (i=0;i<argc;i++) {
    /* Parse the node.. */
    localdata->nodes[i] = ctx->parser(ctx, type, argv[i]);
    
    /* Subsequent nodes get coerced to match the type of the first node */
    if (i)
      localdata->nodes[i]=coerceNode(ctx,localdata->nodes[i],localdata->type);

    /* If a node didn't parse, give up */    
    if (!localdata->nodes[i]) {
      eq_free(ctx, thenode);
      return NULL;
    }
    
    if (!i) {
      /* First arg determines the type */
      localdata->type = localdata->nodes[0]->returntype & RETURNTYPE_TYPE;
    }
    
    localdata->count++;
  }

  return thenode;
}

void eq_free(searchCtx *ctx, struct searchNode *thenode) {
  struct eq_localdata *localdata;
  int i;

  localdata=thenode->localdata;
  
  for (i=0;i<localdata->count;i++) {
    if (localdata->nodes[i])
      (localdata->nodes[i]->free)(ctx, localdata->nodes[i]);
  }
  
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *eq_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  int i;
  char *strval;
  int intval;
  int rval;
  struct eq_localdata *localdata;
  
  localdata=thenode->localdata;
  
  if (localdata->count==0)
    return (void *)1;

  switch (localdata->type) {
  case RETURNTYPE_INT:
    intval = (int)((long)(localdata->nodes[0]->exe)(ctx, localdata->nodes[0], theinput));
    for (i=1;i<localdata->count;i++) {
      if ((int)((long)(localdata->nodes[i]->exe)(ctx, localdata->nodes[i], theinput) != intval))
	return (void *)0;
    }
    return (void *)1;
    
  case RETURNTYPE_BOOL:
    intval = (int)((long)(localdata->nodes[0]->exe)(ctx, localdata->nodes[0], theinput));
    for (i=1;i<localdata->count;i++) {
      rval=(int)((long)(localdata->nodes[i]->exe)(ctx, localdata->nodes[i], theinput));
      if ((rval && !intval) || (!rval && intval)) { /* LOGICAL XOR GOES HERE FS */
	return (void *)0;
      }
    }
    return (void *)1;

  case RETURNTYPE_STRING:
    strval = (char *)(localdata->nodes[0]->exe)(ctx, localdata->nodes[0], theinput);
    for (i=1;i<localdata->count;i++) {
      if (ircd_strcmp(strval, (char *)(localdata->nodes[i]->exe)(ctx, localdata->nodes[i], theinput)))
	return (void *)0;
    }
    return (void *)1;

  default:
    return (void *)0;
  }
}

