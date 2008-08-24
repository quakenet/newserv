/*
 * length functionality - returns the length of a string
 */

#include "newsearch.h"

#include "../lib/irc_string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void length_free(searchCtx *ctx, struct searchNode *thenode);
void *length_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);

struct searchNode *length_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode, *childnode;

  if (!(thenode = (struct searchNode *)malloc(sizeof(struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  
  thenode->localdata  = NULL;
  thenode->returntype = RETURNTYPE_INT;
  thenode->exe  = length_exe;
  thenode->free = length_free;
  
  if (argc != 1) {
    parseError = "length: usage: length <string>";
    free(thenode);
    return NULL;
  }

  childnode = ctx->parser(ctx, argv[0]);
  if (!(thenode->localdata = coerceNode(ctx, childnode, RETURNTYPE_STRING))) {
    length_free(ctx, thenode);
    return NULL;
  }

  return thenode;
}

void length_free(searchCtx *ctx, struct searchNode *thenode) {
  struct searchNode *anode;
  
  if ((anode=thenode->localdata))
    (anode->free)(ctx, anode);
  
  free(thenode);
}

void *length_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  char *strval;
  struct searchNode *anode=thenode->localdata;  

  strval=(char *)(anode->exe)(ctx, anode, theinput);
  
  return (void *)strlen(strval);
}
