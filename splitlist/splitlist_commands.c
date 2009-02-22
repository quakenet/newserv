/* oper commands for the splitlist */

#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../splitlist/splitlist.h"
#include "../control/control.h"
#include "../lib/version.h"

MODULE_VERSION("");

int spcmd_splitlist(void *source, int cargc, char **cargv);
int spcmd_splitdel(void *source, int cargc, char **cargv);

void _init(void) {
  registercontrolhelpcmd("splitlist", NO_STAFF, 0, &spcmd_splitlist, "Usage: splitlist\nLists servers currently split from the netowkr");
  registercontrolcmd("splitdel", 10, 1, &spcmd_splitdel);
}

void _fini(void) {
  deregistercontrolcmd("splitlist", &spcmd_splitlist);
  deregistercontrolcmd("splitdel", &spcmd_splitdel);
}

/* todo: add RELINK status */
int spcmd_splitlist(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int i;
  splitserver srv;

  if (splitlist.cursi == 0) {
    controlreply(np, "There currently aren't any registered splits.");

    return CMD_OK;
  }

  controlreply(np, "Server Status Split for");

  for (i = 0; i < splitlist.cursi; i++) {
    srv = ((splitserver*)splitlist.content)[i];

    controlreply(np, "%s M.I.A. %s", srv.name->content, longtoduration(getnettime() - srv.ts, 1));
  }

  controlreply(np, "--- End of splitlist");
  
  return CMD_OK;
}

int spcmd_splitdel(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int i, count;
  splitserver srv;

  if (cargc < 1) {
    controlreply(np, "Syntax: splitdel <pattern>");

    return CMD_ERROR;
  }

  count = 0;

  for (i = splitlist.cursi - 1; i >= 0; i--) {
    srv = ((splitserver*)splitlist.content)[i];

    if (match2strings(cargv[0], srv.name->content)) {
      sp_deletesplit(srv.name->content); /* inefficient .. but it doesn't matter */
      count++;
    }
  }

  controlreply(np, "%d %s deleted.", count, count != 1 ? "splits" : "split");

  return CMD_OK;
}
