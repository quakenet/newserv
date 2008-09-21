/*
 * NICKITER functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void nickiter_free(searchCtx *ctx, struct searchNode *thenode);
void *nickiter_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);

struct nickiter_localdata {
  int currentnick;
  struct searchVariable *variable;
  chanindex *lastchanindex;
};

struct searchNode *nickiter_parse(searchCtx *ctx, int argc, char **argv) {
  searchNode *thenode;
  struct nickiter_localdata *localdata;
  
  if(argc < 1) {
    parseError = "nickiter: usage: nickiter variable";
    return NULL;
  }

  if(!(localdata=(struct nickiter_localdata *)malloc(sizeof(struct nickiter_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  if(!(localdata->variable=var_register(ctx, argv[0], RETURNTYPE_STRING))) {
    free(localdata);
    return NULL;
  }
  
  if(!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }
  
  localdata->currentnick = 0;
  localdata->lastchanindex = NULL;

  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->localdata    = localdata;
  thenode->exe          = nickiter_exe;
  thenode->free         = nickiter_free;
  
  return thenode;
}

void nickiter_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

void *nickiter_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct nickiter_localdata *localdata = thenode->localdata;
  chanindex *cip = (chanindex *)theinput;
  nick *np;
  
  if(cip != localdata->lastchanindex) {
    localdata->lastchanindex = cip;
    localdata->currentnick = 0;
  }
  
  if(!cip->channel || !cip->channel->users)
    return (void *)0;
  
  while(localdata->currentnick < cip->channel->users->hashsize) {
    if(cip->channel->users->content[localdata->currentnick] != nouser) {
      np = getnickbynumeric(cip->channel->users->content[localdata->currentnick]);
      localdata->currentnick++;
      
      if(!np)
        continue;
      
      var_setstr(localdata->variable, np->nick);
      return (void *)1;
    }
    localdata->currentnick++;
  }
  
  return (void *)0;
}
