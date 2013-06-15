#include "trusts2_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_trusted_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_trusted_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_trusted_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = tsns_trusted_exe;
  thenode->free = tsns_trusted_free;

  return thenode;
}

void *tsns_trusted_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  patricia_node_t *node = (patricia_node_t *)theinput;

  if (node->exts[tgh_ext] == NULL)
    return (void *)0;

  return (void *)1;
}

void tsns_trusted_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

