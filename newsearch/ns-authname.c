/*
 * AUTHNAME functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../nick/nick.h"

void *authname_exe(struct searchNode *thenode, int type, void *theinput);
void authname_free(struct searchNode *thenode);

struct searchNode *authname_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "authname: this function is only valid for nick searches.";
    return NULL;
  }

  thenode=(struct searchNode *)malloc(sizeof (struct searchNode));

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = authname_exe;
  thenode->free = authname_free;

  return thenode;
}

void *authname_exe(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  if (IsAccount(np))
    return np->authname;
  else
    return "[NULL]";

}

void authname_free(struct searchNode *thenode) {
  free(thenode);
}

