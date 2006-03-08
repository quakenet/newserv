/*
 * GT functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct lt_localdata {
  int count;
  struct searchNode **nodes;
};

void lt_free(struct searchNode *thenode);
void *lt_exe(struct searchNode *thenode, int type, void *theinput);

struct searchNode *lt_parse(int type, int argc, char **argv) {
  struct lt_localdata *localdata;
  struct searchNode *thenode;
  int i;

  localdata = (struct lt_localdata *)malloc(sizeof(struct lt_localdata));
  localdata->nodes = (struct searchNode **)malloc(sizeof(struct searchNode *) * argc);
  localdata->count = 0;
  
  thenode = (struct searchNode *)malloc(sizeof(struct searchNode));
  
  thenode->localdata  = localdata;
  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe  = lt_exe;
  thenode->free = lt_free;
  
  for (i=0;i<argc;i++) {
    if (!(localdata->nodes[i] = search_parse(type, argv[i]))) {
      lt_free(thenode);
      return NULL;
    }
    localdata->count++;
  }

  return thenode;
}

void lt_free(struct searchNode *thenode) {
  struct lt_localdata *localdata;
  int i;

  localdata=thenode->localdata;
  
  for (i=0;i<localdata->count;i++) {
    (localdata->nodes[i]->free)(localdata->nodes[i]);
  }
  
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *lt_exe(struct searchNode *thenode, int type, void *theinput) {
  int i;
  char *strval;
  int intval;
  int rval;
  struct lt_localdata *localdata;
  
  localdata=thenode->localdata;
  
  if (localdata->count==0)
    return trueval(type);

  switch (localdata->nodes[0]->returntype & RETURNTYPE_TYPE) {
  case RETURNTYPE_INT:
    intval = (int)(localdata->nodes[0]->exe)(localdata->nodes[0], RETURNTYPE_INT, theinput);
    for (i=1;i<localdata->count;i++) {
      if ((int)(localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_INT, theinput) < intval)
	return falseval(type);
    }

    return trueval(type);
    
  case RETURNTYPE_STRING:
    strval = (char *)(localdata->nodes[0]->exe)(localdata->nodes[0], RETURNTYPE_STRING, theinput);
    for (i=1;i<localdata->count;i++) {
      if (ircd_strcmp(strval, (char *)(localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_STRING, theinput)) < 0)
	return falseval(type);
    }
    
    return trueval(type);

  default:
    return falseval(type);
  }
}

