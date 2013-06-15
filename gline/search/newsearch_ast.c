#include "../../lib/sstring.h"
#include "../../lib/strlfunc.h"
#include "../../lib/stringbuf.h"
#include <stdarg.h>
#include <string.h>
#include "gline_search.h"

int ast_glsearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, GLDisplayFunc display, HeaderFunc header, void *headerarg, int limit) {
  searchCtx ctx;
  searchASTCache cache;
  searchNode *search;
  char buf[1024];

  newsearch_ctxinit(&ctx, search_astparse, reply, wall, &cache, reg_glsearch, sender, display, limit);

  memset(&cache, 0, sizeof(cache));
  cache.tree = tree;

  buf[0] = '\0';
  reply(sender, "Parsing: %s", ast_printtree(buf, sizeof(buf), tree, reg_glsearch));
  search = ctx.parser(&ctx, (char *)tree);
  if(!search) {
    reply(sender, "Parse error: %s", parseError);
    return CMD_ERROR;
  }

  reply(sender, "Executing...");
  if(header)
    header(&ctx, sender, headerarg);
  glsearch_exe(search, &ctx);

  (search->free)(&ctx, search);

  return CMD_OK;
}

