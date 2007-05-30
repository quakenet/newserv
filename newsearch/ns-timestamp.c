/*
 * TIMESTAMP functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *timestamp_exe(struct searchNode *thenode, int type, void *theinput);
void *timestamp_exe_real(struct searchNode *thenode, int type, void *theinput);
void timestamp_free(struct searchNode *thenode);

struct searchNode *timestamp_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "timestamp: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = timestamp_exe;
  thenode->free = timestamp_free;

  return thenode;
}

void *timestamp_exe(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;

  if (type != RETURNTYPE_INT)
    return (void *)1;

  return (void *)np->timestamp;
}

void timestamp_free(struct searchNode *thenode) {
  free(thenode);
}
