#include "../chanserv.h"
#include "chanserv_newsearch.h"
#include <stdio.h>
#include <stdarg.h>

static void chanservmessagewrapper(nick *np, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  chanservsendmessage(np, "%s", buf);
}

static void chanservwallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  chanservwallmessage("%s", buf);
}

int cs_donicksearch(void *source, int cargc, char **cargv) {
  nick *sender=source;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "nicksearch");
    return CMD_ERROR;
  }

  do_nicksearch_real(chanservmessagewrapper, chanservwallwrapper, source, cargc, cargv);

  chanservstdmessage(sender, QM_DONE);  
  return CMD_OK;
}

int cs_dochansearch(void *source, int cargc, char **cargv) {
  nick *sender=source;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "chansearch");
    return CMD_ERROR;
  }

  do_chansearch_real(chanservmessagewrapper, chanservwallwrapper, source, cargc, cargv);

  chanservstdmessage(sender, QM_DONE);  
  return CMD_OK;
}

int cs_dousersearch(void *source, int cargc, char **cargv) {
  nick *sender=source;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "usersearch");
    return CMD_ERROR;
  }

  do_usersearch_real(chanservmessagewrapper, chanservwallwrapper, source, cargc, cargv);

  chanservstdmessage(sender, QM_DONE);  
  return CMD_OK;
}

int cs_dospewemailtwo(void *source, int cargc, char **cargv) {
  searchASTExpr *tree;

  if(cargc < 1)
    return CMD_USAGE;

  tree = NSASTNode(match_parse, NSASTNode(qemail_parse), NSASTLiteral(cargv[0]));
  return ast_usersearch(tree, chanservmessagewrapper, source, chanservwallwrapper, printauth, 500);
}

int cs_dospewdbtwo(void *source, int cargc, char **cargv) {
  searchASTExpr *tree;

  if(cargc < 1)
    return CMD_USAGE;

  tree =
    NSASTNode(or_parse,
      NSASTNode(match_parse, NSASTNode(qusername_parse), NSASTLiteral(cargv[0])),
      NSASTNode(match_parse, NSASTNode(qsuspendreason_parse), NSASTLiteral(cargv[0])),
      NSASTNode(match_parse, NSASTNode(qemail_parse), NSASTLiteral(cargv[0])),
      NSASTNode(match_parse, NSASTNode(qlasthost_parse), NSASTLiteral(cargv[0])),
    );
  return ast_usersearch(tree, chanservmessagewrapper, source, chanservwallwrapper, printauth, 500);
}

