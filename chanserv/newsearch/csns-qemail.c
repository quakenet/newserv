/*
 * qemail functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../chanserv.h"

#include <stdlib.h>

void *qemail_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void qemail_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *qemail_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->exe = qemail_exe;
  thenode->free = qemail_free;

  return thenode;
}

void *qemail_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  authname *ap = (authname *)theinput;
  reguser *rup = ap->exts[chanservaext];
  if(!rup || !rup->email)
    return "";

  return rup->email->content;
}

void qemail_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

