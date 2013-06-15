#include "trusts2_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_tgstartdate_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_tgstartdate_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_tgstartdate_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = tsns_tgstartdate_exe;
  thenode->free = tsns_tgstartdate_free;

  return thenode;
}

void *tsns_tgstartdate_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  trustgroup_t *tg;
  patricia_node_t *node = (patricia_node_t *)theinput;

  if (ctx->searchcmd == reg_nodesearch) {
    if (node->exts[tgh_ext] != NULL) 
      return (void *)(((trusthost_t *)node->exts[tgh_ext])->trustgroup->startdate);
    else
      return (void *)0; /* will cast to a FALSE */
  } else if (ctx->searchcmd == reg_tgsearch) {
    tg = (trustgroup_t *)theinput;
    return (void *)(tg->startdate);
  } else {
    return NULL;
  }
}

void tsns_tgstartdate_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
