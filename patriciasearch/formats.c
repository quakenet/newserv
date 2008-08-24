#include <stdio.h>
#include "patriciasearch.h"

void printnode(searchCtx *ctx, nick *sender, patricia_node_t *pn) {
  ctx->reply(sender,"%s [%lu]", IPtostr(pn->prefix->sin), pn->usercount);
}

