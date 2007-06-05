/*
 * size functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *size_exe(struct searchNode *thenode, void *theinput);
void size_free(struct searchNode *thenode);

struct searchNode *size_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "size: this function is only valid for channel searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = size_exe;
  thenode->free = size_free;

  return thenode;
}

void *size_exe(struct searchNode *thenode, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  
  if (cip->channel==NULL)
    return (void *)0;
  
  return (void *)((unsigned long)cip->channel->users->totalusers);
}

void size_free(struct searchNode *thenode) {
  free(thenode);
}
