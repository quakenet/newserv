/*
 * services functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct services_localdata {
  long count;
};

void *services_exe(struct searchNode *thenode, int type, void *theinput);
void services_free(struct searchNode *thenode);

struct searchNode *services_parse(int type, int argc, char **argv) {
  struct services_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "services: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="services: usage: (services number)";
    return NULL;
  }

  if (!(localdata=(struct services_localdata *)malloc(sizeof(struct services_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  localdata->count = strtoul(argv[0],NULL,10);

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = services_exe;
  thenode->free = services_free;

  return thenode;
}

void *services_exe(struct searchNode *thenode, int type, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  nick *np;
  struct services_localdata *localdata;
  int i;

  localdata = thenode->localdata;

  long count = localdata->count;

  if (cip->channel == NULL)
    return falseval(type);

  for(i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
      
    if (!(np=getnickbynumeric(cip->channel->users->content[i])))
      continue;
    if (IsService(np) && !--count)
      return trueval(type); /* channel found */
  }
  
  return falseval(type); /* channel not found */
}

void services_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

