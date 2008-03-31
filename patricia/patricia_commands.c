#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../control/control.h"

FILE* dumpip_logfp;
int nc_cmd_dumptree(void *source, int cargc, char **cargv);
int nc_cmd_nodecount(void *source, int cargc, char **cargv);

void _init() {
  if (!(dumpip_logfp = fopen("log/iplist", "w")))
    Error("dumpip", ERR_ERROR, "Failed to open log file!");
  registercontrolcmd("dumptree", 10, 2, &nc_cmd_dumptree);
  registercontrolcmd("nodecount", 10, 1, &nc_cmd_nodecount);
}

void _fini() {
  if (dumpip_logfp)
    fclose(dumpip_logfp);
  deregistercontrolcmd("dumptree", &nc_cmd_dumptree);
  deregistercontrolcmd("nodecount", &nc_cmd_nodecount);
}

int nc_cmd_dumptree(void *source, int cargc, char **cargv) {
  nick *np=(nick *)source; 
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *head, *node;
  unsigned int level=0;
  int i = 0;
 
  if (cargc < 1) {
    controlreply(np, "Syntax: dumptree <ipv4|ipv6|cidr4|cidr6>");
    return CMD_OK;
  }

  if (ipmask_parse(cargv[0], &sin, &bits) == 0) {
    controlreply(np, "Invalid mask.");
    return CMD_OK;
  }

  if (cargc>1) {
    level=strtoul(cargv[1],NULL,10);
  }

  head = refnode(iptree, &sin, bits);

  if (level < 10) { 
    PATRICIA_WALK(head, node)
    {
      switch (level) {
        case 0:
          controlreply(np,"%p: %s", node, IPtostr(node->prefix->sin)); 
          break;
        case 1:
          controlreply(np,"%p: prefix %p, bit %d, ref_count %d, IP: %s",node, node->prefix, 
                           node->prefix->bitlen, node->prefix->ref_count, IPtostr(node->prefix->sin));
          break;
        case 2:
          controlreply(np,"%p: bit: %d, usercount: %d, IP: %s", node, node->bit, node->usercount, IPtostr(node->prefix->sin));
          break;
        case 3: 
          controlreply(np,"%p: L: %p, R: %p", node, node->l, node->r);
          break;
        case 4: 
          controlreply(np,"%p: 0: %p, 1: %p, 2: %p, 3: %p, 4: %p", node, 
                          node->exts[0],  node->exts[1],  node->exts[2],  node->exts[3],  node->exts[4]);
          break; 
        default:
          if( i == 0 ) controlreply(np,"Invalid Level");
      }
      if ( i++ > 500) {
        controlreply(np,"too many... aborting...");
        break;
      }
    }
    PATRICIA_WALK_END;
  } else {
    PATRICIA_WALK_ALL(head, node)
    {
      switch (level) {
        case 10:
          controlreply(np,"%p: prefix: %p %s", node, node->prefix, node->prefix?IPtostr(node->prefix->sin):"");
          break;
        case 11:
          if(node->prefix) 
            controlreply(np,"%p: prefix bit: %d, ref_count %d, IP: %s",node,
                           node->prefix->bitlen, node->prefix->ref_count, IPtostr(node->prefix->sin));
          else
            controlreply(np,"%p: --", node);
          break;
        case 12:
          controlreply(np,"%p: bit: %d, usercount: %d, IP: %s", node, node->bit, node->usercount, node->prefix?IPtostr(node->prefix->sin):"");
          break;
        case 13:
          controlreply(np,"%p: L: %p, R: %p", node, node->l, node->r);
          break;
        case 14:
          controlreply(np,"%p%s 0: %p, 1: %p, 2: %p, 3: %p, 4: %p", node, node->prefix?"-":":",
                          node->exts[0],  node->exts[1],  node->exts[2],  node->exts[3],  node->exts[4]);
          break;
        default:
          if ( i == 0 ) controlreply(np,"Invalid Level");
      }
      if ( i++ > 500) {
        controlreply(np,"too many... aborting...");
        break;
      }
    }
    PATRICIA_WALK_END;
  }
  derefnode(iptree, head);

}

int nc_cmd_nodecount(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *head, *node;
  int count;

  if (cargc < 1) {
    controlreply(np, "Syntax: nodecount <ipv4|ipv6|cidr4|cidr6>");
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

