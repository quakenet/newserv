/*
 * REALNAME functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *realname_exe(struct searchNode *thenode, int type, void *theinput);
void realname_free(struct searchNode *thenode);

struct searchNode *realname_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "realname: this function is only valid for nick searches.";
    return NULL;
  }

  thenode=(struct searchNode *)malloc(sizeof (struct searchNode));

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = realname_exe;
  thenode->free = realname_free;

  return thenode;
}

void *realname_exe(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  return np->realname->name->content;
}

void realname_free(struct searchNode *thenode) {
  free(thenode);
}

