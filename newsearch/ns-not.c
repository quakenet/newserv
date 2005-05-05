/*
 * NOT functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void not_free(struct searchNode *thenode);
void *not_exe(struct searchNode *thenode, int type, void *theinput);

struct searchNode *not_parse(int type, int argc, char **argv) {
  searchNode *thenode, *subnode;

  if (argc!=1) {
    parseError="not: usage: not (term)";
    return NULL;
  }
    
  /* Allocate our actual node */
  thenode=(searchNode *)malloc(sizeof(searchNode));

  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->exe          = not_exe;
  thenode->free         = not_free;

  subnode=search_parse(type, argv[0]); /* Propogate the search type */

  if (!subnode) {
    free(thenode);
    return NULL;
  }

  thenode->localdata=(void *)subnode;
      
  return thenode;
}

void not_free(struct searchNode *thenode) {
  struct searchNode *subnode;
  subnode=thenode->localdata;

  (subnode->free)(subnode);
  free(thenode);
}

void *not_exe(struct searchNode *thenode, int type, void *theinput) {
  void *ret;
  struct searchNode *subnode;

  subnode=thenode->localdata;

  ret = (subnode->exe)(subnode, RETURNTYPE_BOOL, theinput);

  switch (type) {
    
  case RETURNTYPE_INT:
  case RETURNTYPE_BOOL:
    if (ret==NULL) {
      return (void *)1;
    } else {
      return NULL;
    }
    
  case RETURNTYPE_STRING:
    if (ret==NULL) {
      return "1";
    } else {
      return "";
    }
  }
  
  return NULL;
}
