/*
 * NICK functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct nick_localdata {
  nick *np;
  int type;
};

void *nick_exe(struct searchNode *thenode, void *theinput);
void nick_free(struct searchNode *thenode);

struct searchNode *nick_parse(int type, int argc, char **argv) {
  struct nick_localdata *localdata;
  struct searchNode *thenode;

  if (!(localdata=(struct nick_localdata *)malloc(sizeof(struct nick_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
    
  switch (type) {
  case SEARCHTYPE_CHANNEL:
    if (argc!=1) {
      parseError="nick: usage: (nick target)";
      free(localdata);
      return NULL;
    }
    if ((localdata->np=getnickbynick(argv[0]))==NULL) {
      parseError="nick: unknown nickname";
      free(localdata);
      return NULL;
    }
    localdata->type = type;
    break;

  case SEARCHTYPE_NICK:
    if (argc) {
      parseError="nick: usage: (match (nick) target)";
      free(localdata);
      return NULL;
    }
    localdata->type = type;
    localdata->np = NULL;
    break;

  default:
    parseError="nick: unsupported search type";
    free(localdata);
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  if (type == SEARCHTYPE_CHANNEL)
    thenode->returntype = RETURNTYPE_BOOL;
  else
    thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = localdata;
  thenode->exe = nick_exe;
  thenode->free = nick_free;

  return thenode;
}

void *nick_exe(struct searchNode *thenode, void *theinput) {
  struct nick_localdata *localdata;
  nick *np;
  chanindex *cip;

  localdata = thenode->localdata;

  switch (localdata->type) {
  case SEARCHTYPE_CHANNEL:
    cip = (chanindex *)theinput;

    if (cip->channel==NULL || getnumerichandlefromchanhash(cip->channel->users, localdata->np->numeric)==NULL)
      return (void *)0;
      
    return (void *)1;
  
  default:
  case SEARCHTYPE_NICK:
    np = (nick *)theinput;

    return np->nick;
  }
}

void nick_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

