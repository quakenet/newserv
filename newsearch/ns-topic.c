/*
 * topic functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct topic_localdata {
  const char *pattern;
};

void *topic_exe(struct searchNode *thenode, int type, void *theinput);
void topic_free(struct searchNode *thenode);

struct searchNode *topic_parse(int type, int argc, char **argv) {
  struct topic_localdata *localdata;
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "topic: this function is only valid for channel searches.";
    return NULL;
  }

  if (argc!=1) {
    parseError="topic: usage: (topic pattern)";
    return NULL;
  }

  if (!(localdata=(struct topic_localdata *)malloc(sizeof(struct topic_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  localdata->pattern = argv[0];

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = topic_exe;
  thenode->free = topic_free;

  return thenode;
}

void *topic_exe(struct searchNode *thenode, int type, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  struct topic_localdata *localdata;

  localdata = thenode->localdata;
  
  if ((cip->channel==NULL) || (cip->channel->topic==NULL) || 
    !match2strings(localdata->pattern, cip->channel->topic->content))
    return falseval(type);
  return trueval(type);
}

void topic_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

