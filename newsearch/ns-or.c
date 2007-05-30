/*
 * OR functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void or_free(struct searchNode *thenode);
void *or_exe(struct searchNode *thenode, int type, void *theinput);

struct or_localdata {
  int count;
  searchNode **nodes;
};

struct searchNode *or_parse(int type, int argc, char **argv) {
  searchNode *thenode, *subnode;
  struct or_localdata *localdata;
  int i;

  /* Set up our local data - a list of nodes to OR together */
  if (!(localdata=(struct or_localdata *)malloc(sizeof(struct or_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  if (!(localdata->nodes=(searchNode **)malloc(argc * sizeof(searchNode *)))) {
    /* couldn't malloc() memory for localdata->nodes, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }
  localdata->count=0;

  /* Allocate our actual node */
  if (!(thenode=(searchNode *)malloc(sizeof(searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata->nodes and localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata->nodes);
    free(localdata);
    return NULL;
  }

  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->localdata    = localdata;
  thenode->exe          = or_exe;
  thenode->free         = or_free;

  for (i=0;i<argc;i++) {
    subnode=search_parse(type, argv[i]); /* Propogate the search type */
    if (subnode) {
      localdata->nodes[localdata->count++] = subnode;
    } else {
      or_free(thenode);
      return NULL;
    }
  }
      
  return thenode;
}

void or_free(struct searchNode *thenode) {
  struct or_localdata *localdata;
  int i;

  localdata=thenode->localdata;
  for (i=0;i<localdata->count;i++) {
    (localdata->nodes[i]->free)(localdata->nodes[i]);
  }
  
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *or_exe(struct searchNode *thenode, int type, void *theinput) {
  int i;
  void *ret;
  struct or_localdata *localdata;
  
  localdata=thenode->localdata;

  for (i=0;i<localdata->count;i++) {
    ret = (localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_BOOL, theinput);
    if (ret) {
      switch (type) {
      case RETURNTYPE_STRING:
	return "1";
     
      case RETURNTYPE_INT:
      case RETURNTYPE_BOOL:
      default:
	return (void *)1;
      }
    }
  }

  switch(type) {
  case RETURNTYPE_STRING:
    return "";

  case RETURNTYPE_INT:
  case RETURNTYPE_BOOL:
  default:
    return NULL;
  }
}
