/*
 * HOST functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *host_exe(struct searchNode *thenode, int type, void *theinput);
void *host_exe_real(struct searchNode *thenode, int type, void *theinput);
void host_free(struct searchNode *thenode);

struct searchNode *host_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "host: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  if (!(thenode->localdata = (void *)malloc(HOSTLEN+1))) {
    /* couldn't malloc() memory for thenode->localdata, so free thenode to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(thenode);
    return NULL;
  }
  thenode->exe = host_exe;
  thenode->free = host_free;

  /* Allow "host real" to match realhost */

  if (argc>0 && !ircd_strcmp(argv[0],"real")) {
    thenode->exe = host_exe_real;
  }

  return thenode;
}

void *host_exe(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  if (IsSetHost(np)) {
    return np->sethost->content;
  } else if (IsHideHost(np)) {
    sprintf(buf,"%s.%s",np->authname,HIS_HIDDENHOST);
    return buf;
  } else {
    return np->host->name->content;
  }
}

void *host_exe_real(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  return np->host->name->content;
}

void host_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}
