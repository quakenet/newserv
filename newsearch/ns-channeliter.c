/*
 * AND functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_CHANS 500

void channeliter_free(searchCtx *ctx, struct searchNode *thenode);
void *channeliter_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);

struct channeliter_localdata {
  int currentchannel;
  struct searchVariable *variable;
  nick *lastnick;
};

struct searchNode *channeliter_parse(searchCtx *ctx, int argc, char **argv) {
  searchNode *thenode;
  struct channeliter_localdata *localdata;
  
  if(argc < 1) {
    parseError = "channeliter: usage: channeliter variable";
    return NULL;
  }

  if(!(localdata=(struct channeliter_localdata *)malloc(sizeof(struct channeliter_localdata)))) {
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
  
  localdata->currentchannel = 0;
  localdata->lastnick = NULL;

  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->localdata    = localdata;
  thenode->exe          = channeliter_exe;
  thenode->free         = channeliter_free;
  
  return thenode;
}

void channeliter_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

void *channeliter_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct channeliter_localdata *localdata = thenode->localdata;
  nick *np = (nick *)theinput;
  
  if(np != localdata->lastnick) {
    localdata->lastnick = np;
    localdata->currentchannel = 0;
  }
  
  if(np->channels->cursi > MAX_CHANS)
    return (void *)0;
  
  if(localdata->currentchannel >= np->channels->cursi)
    return (void *)0;
  
  var_setstr(localdata->variable, ((channel **)np->channels->content)[localdata->currentchannel++]->index->name->content);
  
  return (void *)1;
}
