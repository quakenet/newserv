/*
 * IDENT functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *ident_exe(struct searchNode *thenode, int type, void *theinput);
void ident_free(struct searchNode *thenode);

struct searchNode *ident_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "ident: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = ident_exe;
  thenode->free = ident_free;

  return thenode;
}

void *ident_exe(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  return np->ident;
}

void ident_free(struct searchNode *thenode) {
  free(thenode);
}

