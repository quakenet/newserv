/*
 * pscan functionality 
 */

#include "../newsearch/newsearch.h"
#include "proxyscan.h"

#include <stdlib.h>

void *pscan_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void pscan_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *pscan_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe = pscan_exe;
  thenode->free = pscan_free;
  thenode->localdata = (void *)0;

  return thenode;
}

void *pscan_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  startnickscan((nick *)theinput);
  thenode->localdata = (void *)((int)thenode->localdata + 1);
  return (void *)1;
}

void pscan_free(searchCtx *ctx, struct searchNode *thenode) {
  ctx->reply(senderNSExtern, "proxyscan now scanning: %d nick(s).", (int)thenode->localdata);
  free(thenode);
}

