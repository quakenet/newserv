#include "patriciasearch.h"

#include <stdio.h>
#include <stdlib.h>

void *ps_users_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void ps_users_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *ps_users_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = ps_users_exe;
  thenode->free = ps_users_free;

  return thenode;
}

void *ps_users_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  patricia_node_t *pn = (patricia_node_t *)theinput;

  return (void *)(long)(pn->usercount);
}

void ps_users_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
