/*
 * KICK functionality 
 */

#include "newsearch.h"
#include "../localuser/localuserchannel.h"

#include <stdio.h>
#include <stdlib.h>

void *kick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void kick_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *kick_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;
  nick *np;
  
  if (argc!=1) {
    parseError="kick: usage: (kick target)";
    return NULL;
  }

  if ((np=getnickbynick(argv[0]))==NULL) {
    parseError="kick: unknown nickname";
    return NULL;
  }
  
  if (IsOper(np) || IsService(np)) {
    parseError="kick: can't kick opers or services";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = np;
  thenode->exe = kick_exe;
  thenode->free = kick_free;

  return thenode;
}

void *kick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np;
  chanindex *cip;

  np=thenode->localdata;
  cip=(chanindex *)theinput;

  if (cip->channel==NULL || getnumerichandlefromchanhash(cip->channel->users, np->numeric)==NULL)
    return (void *)0;
  
  localkickuser(NULL, cip->channel, np, "");
  return (void *)1;
}

void kick_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
