/*
 * AND functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void and_free(struct searchNode *thenode);
void *and_exe(struct searchNode *thenode, int type, void *theinput);

struct and_localdata {
  int count;
  searchNode **nodes;
};

struct searchNode *and_parse(int type, int argc, char **argv) {
  searchNode *thenode, *subnode;
  struct and_localdata *localdata;
  int i;

  /* Set up our local data - a list of nodes to AND together */
  if (!(localdata=(struct and_localdata *)malloc(sizeof(struct and_localdata)))) {
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
  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata->nodes and localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata->nodes);
    free(localdata);
    return NULL;
  }

  thenode->returntype   = RETURNTYPE_BOOL;
  thenode->localdata    = localdata;
  thenode->exe          = and_exe;
  thenode->free         = and_free;

  for (i=0;i<argc;i++) {
    subnode=search_parse(type, argv[i]); /* Propogate the search type */
    if (subnode) {
      localdata->nodes[localdata->count++] = subnode;
    } else {
      and_free(thenode);
      return NULL;
    }
  }
      
  return thenode;
}

void and_free(struct searchNode *thenode) {
  struct and_localdata *localdata;
  int i;

  localdata=thenode->localdata;
  for (i=0;i<localdata->count;i++) {
    (localdata->nodes[i]->free)(localdata->nodes[i]);
  }
  
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *and_exe(struct searchNode *thenode, int type, void *theinput) {
  int i;
  void *ret;
  struct and_localdata *localdata;
  
  localdata=thenode->localdata;

  for (i=0;i<localdata->count;i++) {
    ret = (localdata->nodes[i]->exe)(localdata->nodes[i], RETURNTYPE_BOOL, theinput);
    if (ret == NULL) {
      switch (type) {
	
      case RETURNTYPE_INT:
      case RETURNTYPE_BOOL:
	return NULL;

      case RETURNTYPE_STRING:
	return "";
      }
    }
  }

  switch (type) {
  case RETURNTYPE_INT:
  case RETURNTYPE_BOOL:
    return (void *)1;
 
  case RETURNTYPE_STRING:
    return "1";
  }

  return NULL;
}
