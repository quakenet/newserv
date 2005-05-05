/*
 * NICK functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../nick/nick.h"

void *nick_exe(struct searchNode *thenode, int type, void *theinput);
void nick_free(struct searchNode *thenode);

struct searchNode *nick_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "nick: this function is only valid for nick searches.";
    return NULL;
  }

  thenode=(struct searchNode *)malloc(sizeof (struct searchNode));

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = nick_exe;
  thenode->free = nick_free;

  return thenode;
}

void *nick_exe(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  return np->nick;
}

void nick_free(struct searchNode *thenode) {
  free(thenode);
}

