#include "trusts_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_tgid_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_tgid_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_tgid_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = tsns_tgid_exe;
  thenode->free = tsns_tgid_free;

  return thenode;
}

void *tsns_tgid_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  patricia_node_t *node;
  trustgroup_t *tg;

  if (ctx->searchcmd == reg_nodesearch) {
      node = (patricia_node_t *)theinput;
      if (node->exts[tgh_ext] != NULL)
        return (void *)(((trusthost_t *)node->exts[tgh_ext])->trustgroup->id);
      else
        return (void *)0; /* will cast to a FALSE */
  } else if (ctx->searchcmd == reg_tgsearch) {
      tg = (trustgroup_t *)theinput;
      return (void *)(tg->id); 
  } else {
      return NULL;
  } 

}

void tsns_tgid_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
