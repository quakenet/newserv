/*
 * LT functionality
 */

#include "newsearch.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <stdlib.h>

struct lt_localdata {
  int type;
  int count;
  struct searchNode **nodes;
};

void lt_free(struct searchNode *thenode);
void *lt_exe(struct searchNode *thenode, void *theinput);

struct searchNode *lt_parse(int type, int argc, char **argv) {
  struct lt_localdata *localdata;
  struct searchNode *thenode;
  int i;

  if (!(localdata = (struct lt_localdata *)malloc(sizeof(struct lt_localdata)))) {
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
  thenode->exe  = lt_exe;
  thenode->free = lt_free;
  
  for (i=0;i<argc;i++) {
    /* Parse the node.. */
    localdata->nodes[i] = search_parse(type, argv[i]);
    
    /* Subsequent nodes get coerced to match the type of the first node */
    if (i)
      localdata->nodes[i]=coerceNode(localdata->nodes[i],localdata->type);

    /* If a node didn't parse, give up */    
    if (!localdata->nodes[i]) {
      lt_free(thenode);
      return NULL;
    }
    
    if (!i) {
      /* First arg determines the type */
      localdata->type = localdata->nodes[0]->returntype & RETURNTYPE_TYPE;
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
    if (localdata->nodes[i])
      (localdata->nodes[i]->free)(localdata->nodes[i]);
  }
  
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *lt_exe(struct searchNode *thenode, void *theinput) {
  int i;
  char *strval;
  int intval;
  struct lt_localdata *localdata;
  
  localdata=thenode->localdata;
  
  if (localdata->count==0)
    return (void *)1;

  switch (localdata->type) {
  case RETURNTYPE_INT:
  case RETURNTYPE_BOOL:
    intval = (int)((long)(localdata->nodes[0]->exe)(localdata->nodes[0], theinput));
    for (i=1;i<localdata->count;i++) {
      if ((int)((long)(localdata->nodes[i]->exe)(localdata->nodes[i], theinput) <= intval))
	return (void *)0;
    }
    return (void *)1;
    
  case RETURNTYPE_STRING:
    strval = (char *)(localdata->nodes[0]->exe)(localdata->nodes[0], theinput);
    for (i=1;i<localdata->count;i++) {
      if (ircd_strcmp(strval, (char *)(localdata->nodes[i]->exe)(localdata->nodes[i], theinput)) >= 0)
	return (void *)0;
    }
    return (void *)1;

  default:
    return (void *)0;
  }
}

