/*
 * GT functionality
 */

#include "newsearch.h"

#include "../lib/irc_string.h"
#include <stdio.h>
#include <stdlib.h>

struct gt_localdata {
  int count;
  struct searchNode **nodes;
};

void gt_free(struct searchNode *thenode);
void *gt_exe(struct searchNode *thenode, int type, void *theinput);

struct searchNode *gt_parse(int type, int argc, char **argv) {
  struct gt_localdata *localdata;
  struct searchNode *thenode;
  int i;

  if (!(localdata = (struct gt_localdata *)malloc(sizeof(struct gt_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  if (!(localdata->nodes = (struct searchNode **)malloc(sizeof(struct searchNode *) * argc))) {
    /* couldn't malloc() memory for localdata->nodes, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }
  localdata->count = 0;
  
  if (!(thenode = (struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata and localdata->nodes to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata->nodes);
    free(localdata);
    return NULL;
  }
  
  thenode->localdata  = localdata;
  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe  = gt_exe;
  thenode->free = gt_free;
  
  for (i=0;i<argc;i++) {
    if (!(localdata->nodes[i] = search_parse(type, argv[i]))) {
      gt_free(thenode);
      return NULL;
    }
    localdata->count++;
  }

  return thenode;
}

void gt_free(struct searchNode *thenode) {
  struct gt_localdata *localdata;
  int i;

  localdata=thenode->localdata;
  
  for (i=0;i<localdata->count;i++) {
    (localdata->nodes[i]->free)(localdata->nodes[i]);
  }
  
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *gt_exe(struct searchNode *thenode, int type, void *theinput) {
  int i;
  char *strval;
  int intval;
  struct gt_localdata *localdata;
  
  localdata=thenode->localdata;
  
  if (localdata->count==0)
    return trueval(type);

  switch (localdata->nodes[0]->returntype & RETURNTYPE_TYPE) {
  case RETURNTYPE_INT:
    intval = (int)(localdata->nodes[0]->exe)(localdata->nodes[0], RETURNTYPE_INT, theinput);
    for (i=1;i<localdata->count;i++) {
      if ((int)(localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_INT, theinput) > intval)
	return falseval(type);
    }

    return trueval(type);
    
  case RETURNTYPE_STRING:
    strval = (char *)(localdata->nodes[0]->exe)(localdata->nodes[0], RETURNTYPE_STRING, theinput);
    for (i=1;i<localdata->count;i++) {
      if (ircd_strcmp(strval, (char *)(localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_STRING, theinput)) > 0)
	return falseval(type);
    }
    
    return trueval(type);

  default:
    return falseval(type);
  }
}

