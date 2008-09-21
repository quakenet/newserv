/*
 * HOST functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *host_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void *host_exe_real(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void host_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *host_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode, *argsn;
  char *p;
  
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

  if (!(argsn=argtoconststr("host", ctx, argv[0], &p))) {
    free(thenode);
    return NULL;
  }
  
  /* Allow "host real" to match realhost */
  if (argc>0 && !ircd_strcmp(p,"real"))
    thenode->exe = host_exe_real;
    
  argsn->free(ctx, argsn);

  return thenode;
}

void *host_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  if (IsSetHost(np)) {
    return np->sethost->content;
  } else if (IsHideHost(np)) {
    sprintf(buf,"%s.%s",np->authname,HIS_HIDDENHOST);
    return buf;
  } else {
    return np->host->name->content;
  }
}

void *host_exe_real(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return np->host->name->content;
}

void host_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}
