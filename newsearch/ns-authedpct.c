/*
 * authedpct functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct authedpct_localdata {
  long pct;
};

void *authedpct_exe(struct searchNode *thenode, int type, void *theinput);
void authedpct_free(struct searchNode *thenode);

struct searchNode *authedpct_parse(int type, int argc, char **argv) {
  struct authedpct_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "authedpct: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="authedpct: usage: (authedpct number)";
    return NULL;
  }

  if (!(localdata=(struct authedpct_localdata *)malloc(sizeof(struct authedpct_localdata)))) {
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
  thenode->exe = authedpct_exe;
  thenode->free = authedpct_free;

  return thenode;
}

void *authedpct_exe(struct searchNode *thenode, int type, void *theinput) {
  int i;
  int j=0;
  nick *np;
  chanindex *cip = (chanindex *)theinput;
  struct authedpct_localdata *localdata;

  localdata = thenode->localdata;
  
  if (!cip->channel)
    return falseval(type);
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
    
    if ((np=getnickbynumeric(cip->channel->users->content[i])) && IsAccount(np))
      j++;
  }
  
  if (((j * 100) / cip->channel->users->totalusers) >= localdata->pct)
    return trueval(type);
  else
    return falseval(type);
}

void authedpct_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

