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
#include "patriciasearch.h"

CommandTree *pnodeOutputTree;
searchCmd *reg_nodesearch;

int do_pnodesearch(void *source, int cargc, char **cargv);

NodeDisplayFunc defaultpnodefn = printnode;

void regpnodedisp(const char *name, NodeDisplayFunc handler) {
  addcommandtotree(pnodeOutputTree, name, 0, 0, (CommandHandler)handler);
}

void unregpnodedisp(const char *name, NodeDisplayFunc handler) {
  deletecommandfromtree(pnodeOutputTree, name, (CommandHandler)handler);
}

void _init() {
  pnodeOutputTree=newcommandtree();

  reg_nodesearch = (searchCmd *)registersearchcommand("nodesearch",NO_OPER,do_pnodesearch, printnode);

  registersearchterm(reg_nodesearch, "users",ps_users_parse);
  registersearchterm(reg_nodesearch, "nick",ps_nick_parse);
}

void _fini() {
  destroycommandtree(pnodeOutputTree);

  deregistersearchcommand( reg_nodesearch );
}

static void controlwallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  controlwall(NO_OPER, level, "%s", buf);
  va_end(ap);
}

int do_pnodesearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = senderNSExtern = source;
  struct searchNode *search;
  int limit=500;
  int arg=0;
  NodeDisplayFunc display=defaultpnodefn;
  searchCtx ctx;
  int ret;
  patricia_node_t *subset = iptree->head;

  if (cargc<1)
    return CMD_USAGE;

  ret = parseopts(cargc, cargv, &arg, &limit, (void *)&subset, (void **)&display, reg_nodesearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  newsearch_ctxinit(&ctx, search_parse, reply, wall, NULL, reg_nodesearch);

  if (!(search = ctx.parser(&ctx, cargv[arg]))) {
    reply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }

  pnodesearch_exe(search, &ctx, sender, display, limit, subset);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int do_pnodesearch(void *source, int cargc, char **cargv) {
  return do_pnodesearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void pnodesearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, NodeDisplayFunc display, int limit, patricia_node_t *subset) {
  int matches = 0;
  patricia_node_t *node;

  /* Get a marker value to mark "seen" channels for unique count */
  //nmarker=nextnodemarker();

  /* The top-level node needs to return a BOOL */
  search=coerceNode(ctx, search, RETURNTYPE_BOOL);

  PATRICIA_WALK(subset, node) {
    if ((search->exe)(ctx, search, node)) {
      if (matches<limit)
        display(ctx, sender, node);

      if (matches==limit)
        ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
      matches++;
    }
  }
  PATRICIA_WALK_END;

  ctx->reply(sender,"--- End of list: %d matches",
                matches);
}


