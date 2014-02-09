/*
 * qusername functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../chanserv.h"

#include <stdlib.h>

void *qusername_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void qusername_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *qusername_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->exe = qusername_exe;
  thenode->free = qusername_free;

  return thenode;
}

void *qusername_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  authname *ap = (authname *)theinput;
  reguser *rup = ap->exts[chanservaext];
  if(!rup)
    return "";

  return rup->username;
}

void qusername_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

