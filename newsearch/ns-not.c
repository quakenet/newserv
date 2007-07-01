/*
 * NOT functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void not_free(struct searchNode *thenode);
void *not_exe(struct searchNode *thenode, void *theinput);

struct searchNode *not_parse(int type, int argc, char **argv) {
  searchNode *thenode, *subnode;

  if (argc!=1) {
    parseError="not: usage: not (term)";
    return NULL;
  }
    
  /* Allocate our actual node */
  if (!(thenode=(searchNode *)malloc(sizeof(searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->exe          = not_exe;
  thenode->free         = not_free;

  subnode=search_parse(type, argv[0]); /* Propogate the search type */

  if (!subnode) {
    free(thenode);
    return NULL;
  }

  /* Our subnode needs to return a BOOL */  
  subnode=coerceNode(subnode, RETURNTYPE_BOOL);

  thenode->localdata=(void *)subnode;
      
  return thenode;
}

void not_free(struct searchNode *thenode) {
  struct searchNode *subnode;
  subnode=thenode->localdata;

  (subnode->free)(subnode);
  free(thenode);
}

void *not_exe(struct searchNode *thenode, void *theinput) {
  struct searchNode *subnode;

  subnode=thenode->localdata;

  if ((subnode->exe)(subnode, theinput)) {
    return (void *)0;
  } else {
    return (void *)1;
  }
}
