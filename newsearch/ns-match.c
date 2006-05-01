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

void *match_exe(struct searchNode *thenode, int type, void *theinput);
void match_free(struct searchNode *thenode);

struct searchNode *match_parse(int type, int argc, char **argv) {
  struct match_localdata *localdata;
  struct searchNode *thenode;
  struct searchNode *targnode, *patnode;

  if (argc<2) {
    parseError="match: usage: match source pattern";
    return NULL;
  }

  if (!(targnode = search_parse(type, argv[0])))
    return NULL;

  if (!(patnode = search_parse(type, argv[1]))) {
    (targnode->free)(targnode);
    return NULL;
  }

  localdata=(struct match_localdata *)malloc(sizeof (struct match_localdata));
    
  localdata->targnode=targnode;
  localdata->patnode=patnode;

  thenode = (struct searchNode *)malloc(sizeof (struct searchNode));

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = match_exe;
  thenode->free = match_free;

  return thenode;
}

void *match_exe(struct searchNode *thenode, int type, void *theinput) {
  struct match_localdata *localdata;
  char *pattern, *target;
  int ret;

  localdata = thenode->localdata;
  
  pattern = (char *)(localdata->patnode->exe) (localdata->patnode, RETURNTYPE_STRING, theinput);
  target  = (char *)(localdata->targnode->exe)(localdata->targnode,RETURNTYPE_STRING, theinput);

  ret = match2strings(pattern, target);

  switch(type) {
  default:
  case RETURNTYPE_INT:
  case RETURNTYPE_BOOL:
    return (void *)ret;
    
  case RETURNTYPE_STRING:
    return (ret ? "1" : "");
  }
}

void match_free(struct searchNode *thenode) {
  struct match_localdata *localdata;

  localdata=thenode->localdata;

  (localdata->patnode->free)(localdata->patnode);
  (localdata->targnode->free)(localdata->targnode);
  free(localdata);
  free(thenode);
}

