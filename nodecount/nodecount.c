#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"
#include "../control/control.h"

int nc_cmd_nodecount(void *source, int cargc, char **cargv);

void _init(void) {
  registercontrolcmd("nodecount", 10, 1, &nc_cmd_nodecount);
}

void _fini(void) {
  deregistercontrolcmd("nodecount", &nc_cmd_nodecount);
}

int nc_cmd_nodecount(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *head, *node;
  int count;

  if (cargc < 1) {
    controlreply(np, "Syntax: nodecount <IP>");
    return CMD_OK;
  }
  
  if (ipmask_parse(cargv[0], &sin, &bits) == 0) {
    controlreply(np, "Invalid mask.");

    return CMD_OK;
  }

  head = refnode(iptree, &sin, bits);

  count = 0;
  
  PATRICIA_WALK(head, node) {
    count += node->usercount;
  } PATRICIA_WALK_END;

  derefnode(iptree, head);
  
  controlreply(np, "%d user(s) found.", count);

  return CMD_OK;
}

