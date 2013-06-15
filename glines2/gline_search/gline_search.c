#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../control/control.h"
#include "../../irc/irc_config.h"
#include "../../lib/irc_string.h"
#include "../../parser/parser.h"
#include "../../lib/splitline.h"
#include "../../lib/version.h"
#include "../../lib/stringbuf.h"
#include "../../lib/strlfunc.h"
#include "../gline.h"
#include "gline_search.h"
#include "../../lib/version.h"

MODULE_VERSION("")

int do_glsearch(void *source, int cargc, char **cargv);

searchCmd *reg_glsearch;

GLDisplayFunc defaultglfn = printgl;
HeaderFunc defaultglhfn = printgl_header;

void _init() {
  reg_glsearch = (searchCmd *)registersearchcommand("glsearch",NO_OPER,do_glsearch, printgl);

  regdisp(reg_glsearch, "default", printgl, printgl_header, 0, "displays gline info");
}

void _fini() {
  unregdisp(reg_glsearch, "default", printgl);

  deregistersearchcommand( reg_glsearch );
}

static void controlwallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  controlwall(NO_OPER, level, "%s", buf);
  va_end(ap);
}

int do_glsearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = senderNSExtern = source;
  int limit=500;
  int arg=0;
  GLDisplayFunc display=defaultglfn;
  HeaderFunc header=defaultglhfn;
  int ret;
  patricia_node_t *subset = iptree->head;
  parsertree *tree;

  if (cargc<1) {
    reply( sender, "Usage: [flags] <criteria>");
    reply( sender, "For help, see help glsearch");
    return CMD_OK;
  }

  ret = parseopts(cargc, cargv, &arg, &limit, (void *)&subset, (void **)&display, (void **)&header, reg_glsearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  tree = parse_string(reg_glsearch, cargv[arg]);
  if(!tree) {
    displaystrerror(reply, sender, cargv[arg]);
    return CMD_ERROR;
  }

  ast_glsearch(tree->root, reply, sender, wall, display, header, NULL, limit);

  parse_free(tree);

  return CMD_OK;
}

int do_glsearch(void *source, int cargc, char **cargv) {
  return do_glsearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void glsearch_exe(struct searchNode *search, searchCtx *ctx) {
  int matches = 0;
  gline *g, *sg;
  int i;
  nick *sender = ctx->sender;
  senderNSExtern = sender;
  GLDisplayFunc display = ctx->displayfn;
  int limit = ctx->limit;
  time_t curtime = getnettime();

  /* Get a marker value to mark "seen" channels for unique count */
  //nmarker=nextnodemarker();

  /* The top-level node needs to return a BOOL */
  search=coerceNode(ctx, search, RETURNTYPE_BOOL);

  for (g = glinelist; g; g = sg) {
    sg = g->next;

    if (g->lifetime <= curtime) {
      removegline(g);
      continue;
    } else if (g->expires <= curtime) {
      g->flags &= ~GLINE_ACTIVE;
    }

    if ((search->exe)(ctx, search, g)) {
      if (matches<limit)
        display(ctx, sender, g);

      if (matches==limit)
        ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
      matches++;
    }
  }
  ctx->reply(sender,"--- End of list: %d matches",
                matches);
}
