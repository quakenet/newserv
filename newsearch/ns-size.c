/*
 * size functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct size_localdata {
  long count;
};

void *size_exe(struct searchNode *thenode, int type, void *theinput);
void size_free(struct searchNode *thenode);

struct searchNode *size_parse(int type, int argc, char **argv) {
  struct size_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "size: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="size: usage: (size number)";
    return NULL;
  }

  if (!(localdata=(struct size_localdata *)malloc(sizeof(struct size_localdata)))) {
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
  thenode->exe = size_exe;
  thenode->free = size_free;

  return thenode;
}

void *size_exe(struct searchNode *thenode, int type, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  struct size_localdata *localdata;

  localdata = thenode->localdata;
  
  if ((cip->channel==NULL) || (cip->channel->users->totalusers < localdata->count))
    return falseval(type);
  return trueval(type);
}

void size_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

