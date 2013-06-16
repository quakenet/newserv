#include "trusts2_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_tbid_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_tbid_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_tbid_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = tsns_tbid_exe;
  thenode->free = tsns_tbid_free;

  return thenode;
}
