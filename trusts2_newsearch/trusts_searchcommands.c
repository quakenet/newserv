#include "trusts_newsearch.h"
#include "../newsearch/newsearch.h"

static void tsnsmessagewrapper(nick *np, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  controlreply(np, "%s", buf);
}

static void tsnswallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  controlwall(NO_OPER, level, "%s", buf);
}

int tsns_dotrustlist(void *source, int cargc, char **cargv) {
  searchASTExpr tree;
  searchASTExpr nodes[2];
  
  if(cargc < 1) {
    controlreply(source,"Syntax: trustlist <#groupid>");
    return CMD_ERROR;
  }

  if(cargv[0][0]== '#'){
    tree = NSASTNode(eq_parse, NSASTNode(tsns_tgid_parse), NSASTLiteral(&cargv[0][1]));
  } else {
    tree = NSASTNode(eq_parse, NSASTNode(tsns_tgid_parse), NSASTLiteral(cargv[0]));
  }

//  nodes[0] = NSASTNode(tsns_tgid_parse);
//  nodes[1] = cargv[0];
//  tree =
//      NSASTManualNode(eq_parse, 2, nodes 
//    );
  return ast_tgsearch(&tree, tsnsmessagewrapper , source, tsnswallwrapper, printtgfull, NULL, NULL, 1);
}

int tsns_dotrustdenylist(void *source, int cargc, char **cargv) {
  searchASTExpr tree;

  tree = NSASTNode(gt_parse, NSASTNode(tsns_tbid_parse), NSASTLiteral("0"));

  if(cargc == 1){ /* just assume -private */
    return ast_nodesearch(&tree, tsnsmessagewrapper , source, tsnswallwrapper, printtrust_blockprivate, NULL, NULL, 500);
  } else {
    return ast_nodesearch(&tree, tsnsmessagewrapper , source, tsnswallwrapper, printtrust_block, NULL, NULL, 500);
  }
}
