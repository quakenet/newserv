/*
 * name functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *name_exe(struct searchNode *thenode, void *theinput);
void name_free(struct searchNode *thenode);

struct searchNode *name_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "name: this function is only valid for channel searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = name_exe;
  thenode->free = name_free;

  return thenode;
}

void *name_exe(struct searchNode *thenode, void *theinput) {
  chanindex *cip = (chanindex *)theinput;

  return cip->name->content;
}

void name_free(struct searchNode *thenode) {
  free(thenode);
}

