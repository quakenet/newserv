#include "trusts2_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_istrusted_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_istrusted_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_istrusted_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = tsns_istrusted_exe;
  thenode->free = tsns_istrusted_free;

  return thenode;
}

void *tsns_istrusted_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = theinput;

  trusthost_t *tgh = np->exts[tgn_ext];

  if (!tgh)
    return (void *)0;

  return (void *)1;
}

void tsns_istrusted_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
