#include "../lib/sstring.h"
#include "../lib/strlfunc.h"
#include "../lib/stringbuf.h"
#include <stdarg.h>
#include <string.h>
#include "patriciasearch.h"

int ast_nodesearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, NodeDisplayFunc display, HeaderFunc header, void *headerarg, int limit) {
  searchCtx ctx;
  searchASTCache cache;
  searchNode *search;
  char buf[1024];

  newsearch_ctxinit(&ctx, search_astparse, reply, wall, &cache, reg_nodesearch, sender, display, limit);

  memset(&cache, 0, sizeof(cache));
  cache.tree = tree;

  buf[0] = '\0';
  reply(sender, "Parsing: %s", ast_printtree(buf, sizeof(buf), tree, reg_nodesearch));
  search = ctx.parser(&ctx, (char *)tree);
  if(!search) {
    reply(sender, "Parse error: %s", parseError);
    return CMD_ERROR;
  }

  reply(sender, "Executing...");
  if(header)
    header(&ctx, sender, headerarg);
  pnodesearch_exe(search, &ctx, iptree->head);

  (search->free)(&ctx, search);

  return CMD_OK;
}

