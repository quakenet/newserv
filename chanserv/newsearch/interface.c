#include "../chanserv.h"
#include "../../lib/stringbuf.h"
#include "chanserv_newsearch.h"
#include <stdio.h>
#include <stdarg.h>

static char *concatargs(int cargc, char **cargv) {
  static char bigbuf[1024];
  StringBuf b;
  int i;

  sbinit(&b, bigbuf, sizeof(bigbuf));
  for(i=0;i<cargc;i++) {
    sbaddstr(&b, cargv[i]);
    sbaddchar(&b, ' ');
  }
  sbterminate(&b);

  return bigbuf;
}

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

  cs_log(source, "NICKSEARCH %s", concatargs(cargc, cargv));
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

  cs_log(source, "CHANSEARCH %s", concatargs(cargc, cargv));
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

  cs_log(source, "USERSEARCH %s", concatargs(cargc, cargv));
  do_usersearch_real(chanservmessagewrapper, chanservwallwrapper, source, cargc, cargv);

  chanservstdmessage(sender, QM_DONE);  
  return CMD_OK;
}

void showheader(void *source, void *header) {
  long iheader = (long)header;

  chanservstdmessage(source, iheader);
}

int cs_dospewemail(void *source, int cargc, char **cargv) {
  searchASTExpr tree;

  if(cargc < 1)
    return CMD_USAGE;

  cs_log(source, "SPEWEMAIL %s", cargv[0]);

  tree = NSASTNode(match_parse, NSASTNode(qemail_parse), NSASTLiteral(cargv[0]));
  return ast_usersearch(&tree, chanservmessagewrapper, source, chanservwallwrapper, printauth, showheader, (void *)QM_SPEWHEADER, 2000);
}

int cs_dospewdb(void *source, int cargc, char **cargv) {
  searchASTExpr tree;

  if(cargc < 1)
    return CMD_USAGE;

  cs_log(source, "SPEWDB %s", cargv[0]);

  tree =
    NSASTNode(or_parse,
      NSASTNode(match_parse, NSASTNode(qusername_parse), NSASTLiteral(cargv[0])),
      NSASTNode(match_parse, NSASTNode(qsuspendreason_parse), NSASTLiteral(cargv[0])),
      NSASTNode(match_parse, NSASTNode(qemail_parse), NSASTLiteral(cargv[0])),
      NSASTNode(match_parse, NSASTNode(qlasthost_parse), NSASTLiteral(cargv[0])),
    );
  return ast_usersearch(&tree, chanservmessagewrapper, source, chanservwallwrapper, printauth, showheader, (void *)QM_SPEWHEADER, 2000);
}

