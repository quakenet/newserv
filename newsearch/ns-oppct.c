/*
 * oppct functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct oppct_localdata {
  long pct;
};

void *oppct_exe(struct searchNode *thenode, int type, void *theinput);
void oppct_free(struct searchNode *thenode);

struct searchNode *oppct_parse(int type, int argc, char **argv) {
  struct oppct_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "oppct: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="oppct: usage: (oppct number)";
    return NULL;
  }

  if (!(localdata=(struct oppct_localdata *)malloc(sizeof(struct oppct_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  localdata->pct = strtoul(argv[0],NULL,10);

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = oppct_exe;
  thenode->free = oppct_free;

  return thenode;
}

void *oppct_exe(struct searchNode *thenode, int type, void *theinput) {
  int nonop;
  int i;
  chanindex *cip = (chanindex *)theinput;
  struct oppct_localdata *localdata;

  localdata = thenode->localdata;
  
  if (cip->channel==NULL)
    return falseval(type);
  
  nonop=(cip->channel->users->totalusers)-((cip->channel->users->totalusers*localdata->pct)/100);
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]!=nouser) {
      if (!(cip->channel->users->content[i] & CUMODE_OP)) {
        if (!nonop--) {
          return falseval(type);
        }
      }
    }
  }

  return trueval(type);
}

void oppct_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

