#include "newsearch.h"
#include "../lib/sstring.h"
#include "../lib/strlfunc.h"
#include "../lib/stringbuf.h"
#include <stdarg.h>
#include <string.h>

/* at least we have some type safety... */
typedef union exprunion {
  searchASTExpr *expr;
  char *literal;
} exprunion;

typedef struct searchASTCache {
  searchASTExpr *tree;
  searchASTExpr *cache[AST_RECENT];
  int nextpos;
} searchASTCache;

/* comares either a string and a string or an expression and an expression */
static searchASTExpr *compareloc(searchASTExpr *expr, exprunion *loc) {
  if(expr->type == AST_NODE_LITERAL) {
    if(expr->u.literal == loc->literal)
      return expr;
  } else if(expr->type == AST_NODE_CHILD) {
    if(expr == loc->expr)
      return expr;
  } else {
    parseError = "static_parse_compare: bad node type";
    return NULL;
  }
  return NULL;
}

/* searches the abstract syntax tree for the supplied expression/string */
static searchASTExpr *treesearch(searchASTExpr *expr, exprunion *loc) {
  searchASTExpr *ret = compareloc(expr, loc);
  if(ret)
    return ret;
  
  if(expr->type == AST_NODE_CHILD) {
    int i;
    for(i=0;i<expr->u.child->argc;i++) {
      searchASTExpr *d = treesearch(expr->u.child->argv[i], loc);
      if(d)
        return d;
    }
  }
  return NULL;
}

/*
 * searches the AST cache, if this fails it goes and searches the tree.
 * the cache is hit most of the time and I guess makes it nearly O(1) amortised...
 */
searchASTExpr *cachesearch(searchASTCache *cache, exprunion *loc) {
  searchASTExpr *ret = compareloc(cache->tree, loc);
  int i;
  if(ret)
    return ret;

  for(i=0;i<AST_RECENT;i++) {
    if(!cache->cache[i])
      continue;
    ret = compareloc(cache->cache[i], loc);
    if(ret)
      return ret;
  }

  return treesearch(cache->tree, loc);
}

/* pushes an item into the cache */
static void cachepush(searchASTCache *cache, searchASTExpr *expr) {
  cache->cache[cache->nextpos] = expr;
  cache->nextpos = (cache->nextpos + 1) % AST_RECENT;
}

/* ast parser, the way we pass context around is very very hacky... */
searchNode *search_astparse(searchCtx *ctx, int type, char *loc) {
  searchASTCache *cache = ctx->arg;
  searchASTExpr *expr = cachesearch(cache, (exprunion *)&loc);
  searchNode *node;
  char **v;
  int i;

  if(!expr) {
    parseError = "WARNING: AST parsing failed";
    return NULL;
  }

  switch(expr->type) {
    case AST_NODE_LITERAL:
      if (!(node=(searchNode *)malloc(sizeof(searchNode)))) {
        parseError = "malloc: could not allocate memory for this search.";
        return NULL;
      }
      node->localdata  = getsstring(expr->u.literal,512);
      node->returntype = RETURNTYPE_CONST | RETURNTYPE_STRING;
      node->exe        = literal_exe;
      node->free       = literal_free;
      return node;
    case AST_NODE_CHILD:
      v = (char **)malloc(expr->u.child->argc * sizeof(char *));
      if(!v) {
        parseError = "malloc: could not allocate memory for this search.";
        return NULL;
      }
      for(i=0;i<expr->u.child->argc;i++) {
        searchASTExpr *child = expr->u.child->argv[i];

        cachepush(cache, child);
        switch(child->type) {
          case AST_NODE_LITERAL:
            v[i] = child->u.literal;
            break;
          case AST_NODE_CHILD:
            v[i] = (char *)child;
            break;
          default:
            parseError = "static_parse: bad child node type";
            free(v);
            return NULL;
        }
      }

      node = expr->u.child->fn(ctx, type, expr->u.child->argc, v);
      free(v);
      return node;
   default:
      parseError = "static_parse: bad node type";
      return NULL;
  }
}

int ast_nicksearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, NickDisplayFunc display, HeaderFunc header, void *headerarg, int limit) {
  searchCtx ctx;
  searchASTCache cache;
  searchNode *search;
  char buf[1024];

  memset(&cache, 0, sizeof(cache));
  cache.tree = tree;

  ctx.reply = reply;
  ctx.wall = wall;
  ctx.parser = search_astparse;
  ctx.arg = (void *)&cache;

  buf[0] = '\0';
  reply(sender, "Parsing: %s", ast_printtree(buf, sizeof(buf), tree));
  search = ctx.parser(&ctx, SEARCHTYPE_NICK, (char *)tree);
  if(!search) {
    reply(sender, "Parse error: %s", parseError);
    return CMD_ERROR;
  }

  reply(sender, "Executing...");
  if(header)  
    header(sender, headerarg);
  nicksearch_exe(search, &ctx, sender, display, limit);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int ast_chansearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, ChanDisplayFunc display, HeaderFunc header, void *headerarg, int limit) {
  searchCtx ctx;
  searchASTCache cache;
  searchNode *search;
  char buf[1024];

  ctx.reply = reply;
  ctx.wall = wall;
  ctx.parser = search_astparse;
  ctx.arg = (void *)&cache;

  buf[0] = '\0';
  reply(sender, "Parsing: %s", ast_printtree(buf, sizeof(buf), tree));
  search = ctx.parser(&ctx, SEARCHTYPE_CHANNEL, (char *)tree);
  if(!search) {
    reply(sender, "Parse error: %s", parseError);
    return CMD_ERROR;
  }

  reply(sender, "Executing...");
  if(header)  
    header(sender, headerarg);
  chansearch_exe(search, &ctx, sender, display, limit);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int ast_usersearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, UserDisplayFunc display, HeaderFunc header, void *headerarg, int limit) {
  searchCtx ctx;
  searchASTCache cache;
  searchNode *search;
  char buf[1024];

  memset(&cache, 0, sizeof(cache));
  cache.tree = tree;

  ctx.reply = reply;
  ctx.wall = wall;
  ctx.parser = search_astparse;
  ctx.arg = (void *)&cache;

  buf[0] = '\0';
  reply(sender, "Parsing: %s", ast_printtree(buf, sizeof(buf), tree));
  search = ctx.parser(&ctx, SEARCHTYPE_USER, (char *)tree);
  if(!search) {
    reply(sender, "Parse error: %s", parseError);
    return CMD_ERROR;
  }

  reply(sender, "Executing...");
  if(header)  
    header(sender, headerarg);
  usersearch_exe(search, &ctx, sender, display, limit);

  (search->free)(&ctx, search);

  return CMD_OK;
}


/* horribly inefficient -- don't call me very often! */
static char *ast_printtree_real(StringBuf *buf, searchASTExpr *expr) {
  char lbuf[256];
  if(expr->type == AST_NODE_CHILD) {    
    int i;
    sstring *command = getcommandname(searchTree, (void *)expr->u.child->fn);

    if(command) {
      snprintf(lbuf, sizeof(lbuf), "(%s", command->content);
    } else {
      snprintf(lbuf, sizeof(lbuf), "(%p", expr->u.child->fn);
    }
    sbaddstr(buf, lbuf);

    for(i=0;i<expr->u.child->argc;i++) {
      sbaddchar(buf, ' ');
      ast_printtree_real(buf, expr->u.child->argv[i]);
    }
    sbaddchar(buf, ')');

  } else if(expr->type == AST_NODE_LITERAL) {
    sbaddstr(buf, expr->u.literal);
  } else {
    sbaddstr(buf, "???");
  }

  return buf->buf;
}

char *ast_printtree(char *buf, size_t bufsize, searchASTExpr *expr) {
  StringBuf b;
  char *p;

  b.capacity = bufsize;
  b.len = 0;
  b.buf = buf;

  p = ast_printtree_real(&b, expr);
 
  sbterminate(&b);
  return p;
}
