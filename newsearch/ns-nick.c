/*
 * NICK functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct nick_localdata {
  nick *np;
};

void *nick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void nick_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *nick_parse(searchCtx *ctx, int argc, char **argv) {
  struct nick_localdata *localdata;
  struct searchNode *thenode;

  if (!(localdata=(struct nick_localdata *)malloc(sizeof(struct nick_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
    
  if (ctx->searchcmd == reg_chansearch) {
    struct searchNode *nickname;
    char *p;
    
    if (argc!=1) {
      parseError="nick: usage: (nick target)";
      free(localdata);
      return NULL;
    }
    
    if (!(nickname=argtoconststr("nick", ctx, argv[0], &p))) {
      free(localdata);
      return NULL;
    }
    
    localdata->np=getnickbynick(p);
    (nickname->free)(ctx, nickname);
    if (localdata->np==NULL) {
      parseError="nick: unknown nickname";
      free(localdata);
      return NULL;
    }
  } else if (ctx->searchcmd == reg_nicksearch) {
    if (argc) {
      parseError="nick: usage: (match (nick) target)";
      free(localdata);
      return NULL;
    }
    localdata->np = NULL;
  } else {
    parseError="nick: invalid search command";
    free(localdata);
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  if (ctx->searchcmd == reg_chansearch)
    thenode->returntype = RETURNTYPE_BOOL;
  else
    thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = localdata;
  thenode->exe = nick_exe;
  thenode->free = nick_free;

  return thenode;
}

void *nick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct nick_localdata *localdata;
  nick *np;
  chanindex *cip;

  localdata = thenode->localdata;

  if (ctx->searchcmd == reg_chansearch) {
    cip = (chanindex *)theinput;

    if (cip->channel==NULL || getnumerichandlefromchanhash(cip->channel->users, localdata->np->numeric)==NULL)
      return (void *)0;
      
    return (void *)1;
  } else { 
    np = (nick *)theinput;

    return np->nick;
  }
}

void nick_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

