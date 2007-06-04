/*
 * exists functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *exists_exe(struct searchNode *thenode, int type, void *theinput);
void exists_free(struct searchNode *thenode);

struct searchNode *exists_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "exists: this function is only valid for channel searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = exists_exe;
  thenode->free = exists_free;

  return thenode;
}

void *exists_exe(struct searchNode *thenode, int type, void *theinput) {
  chanindex *cip = (chanindex *)theinput;

  if (cip->channel == NULL)
    return falseval(type);
  return trueval(type);
}

void exists_free(struct searchNode *thenode) {
  free(thenode);
}

