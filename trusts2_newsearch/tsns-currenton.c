#include "trusts_newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *tsns_tgcurrenton_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tsns_tgcurrenton_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tsns_tgcurrenton_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = tsns_tgcurrenton_exe;
  thenode->free = tsns_tgcurrenton_free;

  return thenode;
}

void *tsns_tgcurrenton_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  trustgroup_t *tg;

  tg = (trustgroup_t *)theinput;
  return (void *)(tg->currenton);
}

void tsns_tgcurrenton_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
