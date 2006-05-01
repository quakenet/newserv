/*
 * EQ functionality
 */

#include "newsearch.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <stdlib.h>

struct eq_localdata {
  int count;
  struct searchNode **nodes;
};

void eq_free(struct searchNode *thenode);
void *eq_exe(struct searchNode *thenode, int type, void *theinput);

struct searchNode *eq_parse(int type, int argc, char **argv) {
  struct eq_localdata *localdata;
  struct searchNode *thenode;
  int i;

  localdata = (struct eq_localdata *)malloc(sizeof(struct eq_localdata));
  localdata->nodes = (struct searchNode **)malloc(sizeof(struct searchNode *) * argc);
  localdata->count = 0;
  
  thenode = (struct searchNode *)malloc(sizeof(struct searchNode));
  
  thenode->localdata  = localdata;
  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe  = eq_exe;
  thenode->free = eq_free;
  
  for (i=0;i<argc;i++) {
    if (!(localdata->nodes[i] = search_parse(type, argv[i]))) {
      eq_free(thenode);
      return NULL;
    }
    localdata->count++;
  }

  return thenode;
}

void eq_free(struct searchNode *thenode) {
  struct eq_localdata *localdata;
  int i;

  localdata=thenode->localdata;
  
  for (i=0;i<localdata->count;i++) {
    (localdata->nodes[i]->free)(localdata->nodes[i]);
  }
  
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *eq_exe(struct searchNode *thenode, int type, void *theinput) {
  int i;
  char *strval;
  int intval;
  int rval;
  struct eq_localdata *localdata;
  
  localdata=thenode->localdata;
  
  if (localdata->count==0)
    return trueval(type);

  switch (localdata->nodes[0]->returntype & RETURNTYPE_TYPE) {
  case RETURNTYPE_INT:
    intval = (int)(localdata->nodes[0]->exe)(localdata->nodes[0], RETURNTYPE_INT, theinput);
    for (i=1;i<localdata->count;i++) {
      if ((int)(localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_INT, theinput) != intval)
	return falseval(type);
    }

    return trueval(type);
    
  case RETURNTYPE_BOOL:
    intval = (int)(localdata->nodes[0]->exe)(localdata->nodes[0], RETURNTYPE_BOOL, theinput);
    for (i=1;i<localdata->count;i++) {
      rval=(int)(localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_BOOL, theinput);
      if ((rval && !intval) || (!rval && intval)) { /* LOGICAL XOR GOES HERE FS */
	return falseval(type);
      }
    }

    return trueval(type);

  case RETURNTYPE_STRING:
    strval = (char *)(localdata->nodes[0]->exe)(localdata->nodes[0], RETURNTYPE_STRING, theinput);
    for (i=1;i<localdata->count;i++) {
      if (ircd_strcmp(strval, (char *)(localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_STRING, theinput)))
	return falseval(type);
    }
    
    return trueval(type);

  default:
    return falseval(type);
  }
}

