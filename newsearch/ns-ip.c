/*
 * ip functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *ip_exe(struct searchNode *thenode, void *theinput);
void ip_free(struct searchNode *thenode);

struct searchNode *ip_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "ip: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = ip_exe;
  thenode->free = ip_free;

  return thenode;
}

void *ip_exe(struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return (void *)IPtostr(np->p_ipaddr);
}

void ip_free(struct searchNode *thenode) {
  free(thenode);
}

