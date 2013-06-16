#include <stdio.h>

#include "../../newsearch/newsearch.h"
#include "../../control/control.h"
#include "../../lib/stringbuf.h"
#include "../trusts.h"

void printtrust_group(searchCtx *ctx, nick *sender, patricia_node_t *node) {
  trusthost_t *tgh = node->exts[tgh_ext];
  trustgroup_t *tg;

  if (tgh) {
    tg = tgh->trustgroup;
    ctx->reply(sender,"%s/%d | [%lu] | %lu/%lu", IPtostr(node->prefix->sin), irc_bitlen(&(node->prefix->sin),(node->prefix->bitlen)), tg->id, tg->currenton, tg->maxusage); 
  } else { 
    ctx->reply(sender,"%s/%d | <none>", IPtostr(node->prefix->sin), irc_bitlen(&(node->prefix->sin),(node->prefix->bitlen)));
  }
}
