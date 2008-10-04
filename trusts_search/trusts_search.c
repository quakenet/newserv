#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../control/control.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../parser/parser.h"
#include "../lib/splitline.h"
#include "../lib/version.h"
#include "../lib/stringbuf.h"
#include "../lib/strlfunc.h"
#include "../trusts2/trusts.h"
#include "trusts_search.h"

typedef void (*TGDisplayFunc)(struct searchCtx *, nick *, trustgroup_t *);
typedef void (*THDisplayFunc)(struct searchCtx *, nick *, trusthost_t *);

int do_tgsearch(void *source, int cargc, char **cargv);
int do_tgsearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);
void tgsearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, TGDisplayFunc display, int limit, patricia_node_t *subset);
int do_thsearch(void *source, int cargc, char **cargv);
int do_thsearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);
void thsearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, THDisplayFunc display, int limit, patricia_node_t *subset);

searchCmd *reg_tgsearch;
searchCmd *reg_thsearch;

TGDisplayFunc defaulttgfn = printtg;
THDisplayFunc defaultthfn = printth;

void _init() {
  reg_tgsearch = (searchCmd *)registersearchcommand("tgsearch",NO_OPER,do_tgsearch, printtg);
  reg_thsearch = (searchCmd *)registersearchcommand("thsearch",NO_OPER,do_thsearch, printth);

  regdisp(reg_tgsearch, "all", printtgfull, 0, "show trustgroup details, including hosts, excludes trust comments");
  regdisp(reg_tgsearch, "default", printtg, 0, "displays trust group id");
  regdisp(reg_thsearch, "default", printth, 0, "displays trust host id");
}

void _fini() {
  unregdisp( reg_tgsearch, "all", printtgfull);
  unregdisp(reg_tgsearch, "default", printtg);
  unregdisp(reg_thsearch, "default", printth);

  deregistersearchcommand( reg_tgsearch );
  deregistersearchcommand( reg_thsearch );
}

static void controlwallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  controlwall(NO_OPER, level, "%s", buf);
  va_end(ap);
}

int do_tgsearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = senderNSExtern = source;
  struct searchNode *search;
  int limit=500;
  int arg=0;
  TGDisplayFunc display=defaulttgfn;
  searchCtx ctx;
  int ret;
  patricia_node_t *subset = iptree->head;

  if (cargc<1)
    return CMD_USAGE;

  ret = parseopts(cargc, cargv, &arg, &limit, (void *)&subset, (void **)&display, reg_tgsearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  newsearch_ctxinit(&ctx, search_parse, reply, wall, NULL, reg_tgsearch, sender, display, limit);

  if (!(search = ctx.parser(&ctx, cargv[arg]))) {
    reply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }

  tgsearch_exe(search, &ctx, sender, display, limit, subset);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int do_tgsearch(void *source, int cargc, char **cargv) {
  return do_tgsearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void tgsearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, TGDisplayFunc display, int limit, patricia_node_t *subset) {
  int matches = 0;
  trustgroup_t *tg;
  int i;

  /* Get a marker value to mark "seen" channels for unique count */
  //nmarker=nextnodemarker();

  /* The top-level node needs to return a BOOL */
  search=coerceNode(ctx, search, RETURNTYPE_BOOL);

  for ( i = 0; i < TRUSTS_HASH_GROUPSIZE ; i++ ) {
    for ( tg = trustgroupidtable[i]; tg; tg = tg -> nextbyid ) {
      if ((search->exe)(ctx, search, tg)) {
      if (matches<limit)
        display(ctx, sender, tg);

      if (matches==limit)
        ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
      matches++;
    }
  }
  }
  ctx->reply(sender,"--- End of list: %d matches",
                matches);
}

int do_thsearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = senderNSExtern = source;
  struct searchNode *search;
  int limit=500;
  int arg=0;
  THDisplayFunc display=defaultthfn;
  searchCtx ctx;
  int ret;
  patricia_node_t *subset = iptree->head;

  if (cargc<1)
    return CMD_USAGE;

  ret = parseopts(cargc, cargv, &arg, &limit, (void *)&subset, (void **)&display, reg_tgsearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  newsearch_ctxinit(&ctx, search_parse, reply, wall, NULL, reg_thsearch, sender, display, limit);

  if (!(search = ctx.parser(&ctx, cargv[arg]))) {
    reply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }

  thsearch_exe(search, &ctx, sender, display, limit, subset);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int do_thsearch(void *source, int cargc, char **cargv) {
  return do_thsearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void thsearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, THDisplayFunc display, int limit, patricia_node_t *subset) {
  int matches = 0;
  trusthost_t *tgh;
  int i;

  /* Get a marker value to mark "seen" channels for unique count */
  //nmarker=nextnodemarker();

  /* The top-level node needs to return a BOOL */
  search=coerceNode(ctx, search, RETURNTYPE_BOOL);

  for ( i = 0; i < TRUSTS_HASH_HOSTSIZE ; i++ ) {
    for ( tgh = trusthostidtable[i]; tgh; tgh = tgh -> nextbyid ) {
      if ((search->exe)(ctx, search, tgh)) {
      if (matches<limit)
        display(ctx, sender, tgh);

      if (matches==limit)
        ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
      matches++;
    }
  }
  }
  ctx->reply(sender,"--- End of list: %d matches",
                matches);
}

