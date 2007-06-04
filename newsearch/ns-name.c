/*
 * name functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct name_localdata {
  const char *pattern;
};

void *name_exe(struct searchNode *thenode, int type, void *theinput);
void name_free(struct searchNode *thenode);

struct searchNode *name_parse(int type, int argc, char **argv) {
  struct name_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "name: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="name: usage: (name pattern)";
    return NULL;
  }

  if (!(localdata=(struct name_localdata *)malloc(sizeof(struct name_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  localdata->pattern = argv[0];

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = name_exe;
  thenode->free = name_free;

  return thenode;
}

void *name_exe(struct searchNode *thenode, int type, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  struct name_localdata *localdata;

  localdata = thenode->localdata;
  
  if ((cip->channel==NULL) || !match2strings(localdata->pattern, cip->name->content))
    return falseval(type);
  return trueval(type);
}

void name_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

