/* patricianick.c */

#include "patricianick.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../core/nsmalloc.h"
#include "../control/control.h"
#include "../core/schedule.h"
#include "../lib/version.h"

#include <stdio.h>
#include <string.h>

#define ALLOCUNIT	100

MODULE_VERSION("")

patricianick_t *freepatricianicks;
int pnode_ext;
int pnick_ext;

int pn_cmd_nodeuserlist(void *source, int cargc, char **cargv);

void _init() {
  nick *np, *nnp;
  int i;

  pnode_ext = registernodeext("patricianick");
  if (pnode_ext == -1) {
    Error("patricianick", ERR_FATAL, "Could not register a required node extension");
    return;
  }

  pnick_ext = registernickext("patricianick");
  if (pnick_ext == -1) {
    Error("patricianick", ERR_FATAL, "Could not register a required nick extension");
    return;
  }

  for (i=0;i<NICKHASHSIZE;i++)
    for (np=nicktable[i];np;np=nnp) {
      nnp=np->next;
      addnicktonode(np->ipnode, np);
    }

  registerhook(HOOK_NICK_NEWNICK, &pn_hook_newuser);
  registerhook(HOOK_NICK_LOSTNICK, &pn_hook_lostuser);
  registerhook(HOOK_NICK_MOVENODE, &pn_hook_nodemoveuser);

  registercontrolhelpcmd("nodeuserlist", NO_OPER, 1, &pn_cmd_nodeuserlist, "Usage: nodeuserlist <ipv4|ipv6|cidr4|cidr6>\nLists all users on a given IP address or CIDR range.");
}

void _fini() {
  nsfreeall(POOL_PATRICIANICK);

  if (pnode_ext != -1) {
    releasenodeext(pnode_ext);
  }

  if (pnick_ext != -1) {
    releasenickext(pnick_ext);
  }

  deregisterhook(HOOK_NICK_NEWNICK, &pn_hook_newuser);
  deregisterhook(HOOK_NICK_LOSTNICK, &pn_hook_lostuser);
  deregisterhook(HOOK_NICK_MOVENODE, &pn_hook_nodemoveuser);

  deregistercontrolcmd("nodeuserlist", &pn_cmd_nodeuserlist);
}

patricianick_t *getpatricianick() {
  int i;
  patricianick_t *pnp;

  if (freepatricianicks==NULL) {
    freepatricianicks=(patricianick_t *)nsmalloc(POOL_PATRICIANICK, ALLOCUNIT*sizeof(patricianick_t));
    for(i=0;i<ALLOCUNIT-1;i++) {
      freepatricianicks[i].identhash[0]=(nick *)&(freepatricianicks[i+1]);
    }
    freepatricianicks[ALLOCUNIT-1].identhash[0]=NULL;
  }

  pnp=freepatricianicks;
  freepatricianicks=(patricianick_t *)pnp->identhash[0];

  memset(pnp, 0, sizeof(patricianick_t));
  return pnp;
}

void addnicktonode(patricia_node_t *node, nick *np) {
  unsigned long hash;

  patricia_ref_prefix(node->prefix);

  if (!(node->exts[pnode_ext])) {
    node->exts[pnode_ext] = getpatricianick();
  }

  hash = pn_getidenthash(np->ident);
  np->exts[pnick_ext] = ((patricianick_t *)node->exts[pnode_ext])->identhash[hash];
  ((patricianick_t *)node->exts[pnode_ext])->identhash[hash] = np;
}

void deletenickfromnode(patricia_node_t *node, nick *np) {
  nick **tnp;
  int found, i;

  found = 0;

  for (tnp = &(((patricianick_t *)node->exts[pnode_ext])->identhash[pn_getidenthash(np->ident)]); *tnp; tnp = (nick **)(&((*tnp)->exts[pnick_ext]))) {
    if (*tnp == np) {
      *tnp = np->exts[pnick_ext];
      found = 1;
      break;
    }
  }

  if (!found) {
    Error("patricianick", ERR_ERROR, "Could not remove %s!%s from %s", np->nick, np->ident, IPtostr(node->prefix->sin));
    return;
  }

  for (i = 0; i < PATRICIANICK_HASHSIZE; i++) {
    if (((patricianick_t *)node->exts[pnode_ext])->identhash[i]) {
      return;
    }
  }

  freepatricianick(node->exts[pnode_ext]);
  node->exts[pnode_ext]= NULL;
}

void freepatricianick(patricianick_t *pnp) {
  pnp->identhash[0]=(nick *)freepatricianicks;
  freepatricianicks=pnp;
}

void pn_hook_newuser(int hook, void *arg) {
  nick *np = (nick *)arg;

  addnicktonode(np->ipnode, np);
}

void pn_hook_lostuser(int hook, void *arg) {
  nick *np = (nick *)arg;

  deletenickfromnode(np->ipnode, np);
}

void pn_hook_nodemoveuser(int hook, void *arg) {
  nick *np = ((void **)arg)[0];
  patricia_node_t *oldnode = ((void **)arg)[1];

  deletenickfromnode(oldnode, np);
  addnicktonode(np->ipnode, np);
}

int pn_cmd_nodeuserlist(void *source, int cargc, char **cargv) {
  nick *np=(nick *)source;
  struct irc_in_addr sin;
  unsigned char bits;
  unsigned int count, i;
  patricia_node_t *head, *node;
  patricianick_t *pnp;
  nick *npp;

  if (cargc < 1) {
    return CMD_USAGE;
  }

  if (ipmask_parse(cargv[0], &sin, &bits) == 0) {
    controlreply(np, "Invalid mask.");
    return CMD_ERROR;
  }

  head = refnode(iptree, &sin, bits);
  count = 0;

  PATRICIA_WALK(head, node)
  {
    pnp = node->exts[pnode_ext];
    if (pnp) {
      if (count < PATRICIANICK_MAXRESULTS) {
        for (i = 0; i < PATRICIANICK_HASHSIZE; i++) {
          for (npp = pnp->identhash[i]; npp; npp=npp->exts[pnick_ext]) {
            controlreply(np, "%s!%s@%s%s%s (%s)", npp->nick, npp->ident, npp->host->name->content, IsAccount(npp) ? "/" : "", npp->authname, IPtostr(node->prefix->sin));
          }
        }

        count += node->usercount;

        if (count >= PATRICIANICK_MAXRESULTS) {
          controlreply(np, "Too many results, output truncated");
        }
      } else {
        count += node->usercount;
      }
    }
  }
  PATRICIA_WALK_END;
  derefnode(iptree, head);

  controlreply(np, "Total users on %s: %d", cargv[0], count);
  return CMD_OK;
}
