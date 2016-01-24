#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "rbl.h"

MODULE_VERSION("");

static int rbl_cmdlookuprbl(void *source, int cargc, char **cargv) {
  nick *sender = source;
  rbl_instance *rbl;
  struct irc_in_addr ip;

  if (cargc < 1)
    return CMD_USAGE;

  if (!ipmask_parse(cargv[0], &ip, NULL)) {
    controlreply(sender, "Invalid IP address.");
    return CMD_ERROR;
  }

  for (rbl = rbl_instances; rbl; rbl = rbl->next) {
    char reason[255];
    if (RBL_LOOKUP(rbl, &ip, reason, sizeof(reason)) <= 0)
      continue;

     controlreply(sender, "RBL: %s (%s)", rbl->name->content, reason);
  }

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int rbl_cmdlistrbl(void *source, int cargc, char **cargv) {
  nick *sender = source;
  rbl_instance *rbl;

  for (rbl = rbl_instances; rbl; rbl = rbl->next)
    controlreply(sender, "%s", rbl->name->content);

  controlreply(sender, "End of list.");

  return CMD_OK;
}

void _init(void) {
  registercontrolhelpcmd("lookuprbl", NO_OPER, 1, &rbl_cmdlookuprbl, "Usage: lookuprbl <IP>\nLooks up whether RBL records exist for the specified IP address.");
  registercontrolhelpcmd("listrbl", NO_OPER, 0, &rbl_cmdlistrbl, "Usage: listrbl\nLists RBLs.");
}

void _fini(void) {
  deregistercontrolcmd("lookuprbl", &rbl_cmdlookuprbl);
  deregistercontrolcmd("listrbl", &rbl_cmdlistrbl);
}
