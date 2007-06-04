/*
 * size functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct namelen_localdata {
  long count;
};

void *namelen_exe(struct searchNode *thenode, int type, void *theinput);
void namelen_free(struct searchNode *thenode);

struct searchNode *namelen_parse(int type, int argc, char **argv) {
  struct namelen_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "namelen: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="namelen: usage: (namelen number)";
    return NULL;
  }

  if (!(localdata=(struct namelen_localdata *)malloc(sizeof(struct namelen_localdata)))) {
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
  thenode->exe = namelen_exe;
  thenode->free = namelen_free;

  return thenode;
}

void *namelen_exe(struct searchNode *thenode, int type, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  struct namelen_localdata *localdata;

  localdata = thenode->localdata;
  
  if ((cip->channel==NULL) || (cip->name->length < localdata->count))
    return falseval(type);
  return trueval(type);
}

void namelen_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

