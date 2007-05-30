/*
 * HOSTMASK functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *hostmask_exe(struct searchNode *thenode, int type, void *theinput);
void *hostmask_exe_real(struct searchNode *thenode, int type, void *theinput);
void hostmask_free(struct searchNode *thenode);

struct searchNode *hostmask_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "hostmask: this function is only valid for nick searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  if (!(thenode->localdata = (void *)malloc(HOSTLEN+USERLEN+NICKLEN+3))) {
    /* couldn't malloc() memory for thenode->localdata, so free thenode to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(thenode);
    return NULL;
  }
  thenode->exe = hostmask_exe;
  thenode->free = hostmask_free;

  /* Allow "hostmask real" to match realhost */

  if (argc>0 && !ircd_strcmp(argv[0],"real")) {
    thenode->exe = hostmask_exe_real;
  }

  return thenode;
}

void *hostmask_exe(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  return visiblehostmask(np, buf);
}

void *hostmask_exe_real(struct searchNode *thenode, int type, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  if (type != RETURNTYPE_STRING) {
    return (void *)1;
  }

  sprintf(buf,"%s!%s@%s",np->nick,np->ident,np->host->name->content);

  return buf;
}

void hostmask_free(struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

