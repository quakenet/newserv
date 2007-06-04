/*
 * hostpct functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct hostpct_localdata {
  long pct;
};

void *hostpct_exe(struct searchNode *thenode, int type, void *theinput);
void hostpct_free(struct searchNode *thenode);

struct searchNode *hostpct_parse(int type, int argc, char **argv) {
  struct hostpct_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "uniquehostpct: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="uniquehostpct: usage: (uniquehostpct number)";
    return NULL;
  }

  if (!(localdata=(struct hostpct_localdata *)malloc(sizeof(struct hostpct_localdata)))) {
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
  thenode->exe = hostpct_exe;
  thenode->free = hostpct_free;

  return thenode;
}

void *hostpct_exe(struct searchNode *thenode, int type, void *theinput) {
  int hostreq;
  int i;
  unsigned int marker;
  nick *np;
  chanindex *cip = (chanindex *)theinput;
  struct hostpct_localdata *localdata;

  localdata = thenode->localdata;
  
  if (cip->channel==NULL)
    return falseval(type);
  
  marker=nexthostmarker();
  
  hostreq=(cip->channel->users->totalusers * localdata->pct) / 100;
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
      
    if (!(np=getnickbynumeric(cip->channel->users->content[i])))
      continue;

    if (np->host->marker!=marker) {
      /* new unique host */
      if (--hostreq <= 0) {
        return trueval(type);
      }
      np->host->marker=marker;
    }
  }
  
  return falseval(type);
}

void hostpct_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

