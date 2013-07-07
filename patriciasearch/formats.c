#include <stdio.h>
#include "patriciasearch.h"

void printnode_header(searchCtx *ctx, nick *sender, void *args) {
  ctx->reply(sender, "IP [usercount]");
}

void printnode(searchCtx *ctx, nick *sender, patricia_node_t *pn) {
  ctx->reply(sender,"%s [%lu]", IPtostr(pn->prefix->sin), pn->usercount);
}

