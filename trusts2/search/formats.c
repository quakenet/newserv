#include <stdio.h>

#include "../../newsearch/newsearch.h"
#include "../../control/control.h"
#include "../../lib/stringbuf.h"
#include "../trusts.h"

char *trusts_timetostr(time_t t) {
  static char buf[100];
 
  if ( t == 0 ) {
    return "<none>";
  }

  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));

  return buf;
}

void printtg(searchCtx *ctx, nick *sender, trustgroup_t *tg) {
  ctx->reply(sender,"%lu", tg->id);
}

void printth(searchCtx *ctx, nick *sender, trusthost_t *th) {
  ctx->reply(sender,"%lu: %s", th->id, IPtostr(((patricia_node_t *)th->node)->prefix->sin));
}

void printtgfull(searchCtx *ctx, nick *sender, trustgroup_t *g) {
  trusthost_t* thptr;
  trusthost_t *tgh = NULL;
  patricia_node_t *parent;

  ctx->reply(sender,"Trustgroup ID   : %lu", g->id);
  ctx->reply(sender,"Max Connections : %lu, Max Per Ident: %lu, Max Per IP: %lu", g->maxclones, g->maxperident, g->maxperip);
  ctx->reply(sender,"Curent Usage    : %lu/%lu", g->currenton,g->maxusage);
  ctx->reply(sender,"Enforce Ident   : %d", g->enforceident);
  ctx->reply(sender,"Start Date      : %s", trusts_timetostr(g->startdate));
  ctx->reply(sender,"Last Used       : %s", trusts_timetostr(g->lastused));
  ctx->reply(sender,"Expiry          : %s", trusts_timetostr(g->expire));

  ctx->reply(sender,"Owner           : %lu", g->ownerid);

  ctx->reply(sender,"Trust Hosts:");
  ctx->reply(sender,"ID      Host             Current    Max   Last seen           Expiry");
  int hash = trusts_gettrusthostgroupidhash(g->id);
  for (thptr = trusthostgroupidtable[hash]; thptr; thptr = thptr->nextbygroupid ) {
    if(thptr->trustgroup == g)
      ctx->reply(sender, "%-5lu %15s/%d   %-10lu %-5lu %s %s",
                         thptr->id,
                         IPtostr(((patricia_node_t *)thptr->node)->prefix->sin),
                         irc_bitlen(&(((patricia_node_t *)thptr->node)->prefix->sin),((patricia_node_t *)thptr->node)->prefix->bitlen),
                         thptr->node->usercount,
                         thptr->maxused,
                         trusts_timetostr(thptr->lastused),
                         trusts_timetostr(thptr->expire));

    parent = ((patricia_node_t *)thptr->node)->parent;
    while (parent) {
      if(parent->exts)
        if( parent->exts[tgh_ext]) {
          tgh = (trusthost_t *)parent->exts[tgh_ext];
          ctx->reply(sender, "- Parent Trust Group: %lu", tgh->trustgroup->id);
        }
      parent = parent->parent;
    }
  }
}
