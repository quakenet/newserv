#include "trusts_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_tgenforceident_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_tgenforceident_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_tgenforceident_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = tsns_tgenforceident_exe;
  thenode->free = tsns_tgenforceident_free;

  return thenode;
}

void *tsns_tgenforceident_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  trustgroup_t *tg;
  patricia_node_t *node = (patricia_node_t *)theinput;

  if (ctx->searchcmd == reg_nodesearch) {
    if (node->exts[tgh_ext] != NULL) 
      if (((trusthost_t *)node->exts[tgh_ext])->trustgroup->enforceident)
        return (void *)1;
  } else if (ctx->searchcmd == reg_tgsearch) {
    tg = (trustgroup_t *)theinput;
    if (tg->enforceident)
      return (void *)1;
  } else {
    return NULL;
  }

  return (void *)0;
}

void tsns_tgenforceident_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
