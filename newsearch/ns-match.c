/*
 * MATCH functionality
 */

#include "newsearch.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <stdlib.h>

struct match_localdata {
  struct searchNode *targnode;
  struct searchNode *patnode;
};

void *match_exe(struct searchNode *thenode, void *theinput);
void match_free(struct searchNode *thenode);

struct searchNode *match_parse(int type, int argc, char **argv) {
  struct match_localdata *localdata;
  struct searchNode *thenode;
  struct searchNode *targnode, *patnode;

  if (argc<2) {
    parseError="match: usage: match source pattern";
    return NULL;
  }

  targnode = search_parse(type, argv[0]);
  if (!(targnode = coerceNode(targnode, RETURNTYPE_STRING)))
    return NULL;

  patnode = search_parse(type, argv[1]);
  if (!(patnode = coerceNode(patnode, RETURNTYPE_STRING))) {
    (targnode->free)(targnode);
    return NULL;
  }

  if (!(localdata=(struct match_localdata *)malloc(sizeof (struct match_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
    
  localdata->targnode=targnode;
  localdata->patnode=patnode;

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = match_exe;
  thenode->free = match_free;

  return thenode;
}

void *match_exe(struct searchNode *thenode, void *theinput) {
  struct match_localdata *localdata;
  char *pattern, *target;

  localdata = thenode->localdata;
  
  pattern = (char *)(localdata->patnode->exe) (localdata->patnode, theinput);
  target  = (char *)(localdata->targnode->exe)(localdata->targnode,theinput);

  return (void *)match2strings(pattern, target);
}

void match_free(struct searchNode *thenode) {
  struct match_localdata *localdata;

  localdata=thenode->localdata;

  (localdata->patnode->free)(localdata->patnode);
  (localdata->targnode->free)(localdata->targnode);
  free(localdata);
  free(thenode);
}

