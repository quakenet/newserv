#include "trusts_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_thmaxusage_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_thmaxusage_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_thmaxusage_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = tsns_thmaxusage_exe;
  thenode->free = tsns_thmaxusage_free;

  return thenode;
}

void *tsns_thmaxusage_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  trusthost_t *th;
  patricia_node_t *node = (patricia_node_t *)theinput;

  if (ctx->searchcmd == reg_nodesearch) {
    if (node->exts[tgh_ext] != NULL) 
      return (void *)(((trusthost_t *)node->exts[tgh_ext])->maxused);
    else
      return (void *)0; /* will cast to a FALSE */
  } else if (ctx->searchcmd == reg_thsearch) {
    th = (trusthost_t *)theinput;
    return (void *)(th->maxused);
  } else {
    return NULL;
  }
}

void tsns_thmaxusage_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
