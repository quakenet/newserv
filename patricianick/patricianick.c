/* patricianick.c */

#include "patricianick.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../core/nsmalloc.h"
#include "../control/control.h"
#include "../core/schedule.h"

#include <stdio.h>
#include <string.h>

#define ALLOCUNIT	1000

patricianick_t *freepatricianicks;
int pnode_ext;
int pnick_ext;

int pn_cmd_dumpnodenicks(void *source, int cargc, char **cargv);

void _init() {
  nick *np, *nnp;
  int i;

  pnode_ext = registernodeext("patricianick");
  if ( pnode_ext == -1 ) {
    Error("patricianick", ERR_FATAL, "Could not register a required node extension");
    return;
  }

  pnick_ext = registernickext("patricianick");
  if ( pnick_ext == -1) { 
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

  registercontrolcmd("dumpnodenicks", NO_DEVELOPER, 1, &pn_cmd_dumpnodenicks);
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

  deregistercontrolcmd("dumpnodenicks", &pn_cmd_dumpnodenicks);
}

patricianick_t *getpatricianick() {
  int i;
  patricianick_t *pnp;

  if (freepatricianicks==NULL) {
    freepatricianicks=(patricianick_t *)nsmalloc(POOL_PATRICIANICK, ALLOCUNIT*sizeof(patricianick_t));
    for(i=0;i<ALLOCUNIT-1;i++) {
      freepatricianicks[i].np=(nick *)&(freepatricianicks[i+1]);
    }
    freepatricianicks[ALLOCUNIT-1].np=NULL;
  }

  pnp=freepatricianicks;
  freepatricianicks=(patricianick_t *)pnp->np;

  pnp->marker = 0;
  pnp->np = NULL;
  return pnp;
}

void addnicktonode(patricia_node_t *node, nick *np) {
  if ( !(node->exts[pnode_ext]) ) {
    node->exts[pnode_ext] = getpatricianick();
  }
 
  np->exts[pnick_ext] = ((patricianick_t *)node->exts[pnode_ext])->np;
  ((patricianick_t *)node->exts[pnode_ext])->np = np;
}

void deletenickfromnode(patricia_node_t *node, nick *np) {
  nick **tnp;
  int found =0; 

  for ( tnp = &(((patricianick_t *)node->exts[pnode_ext])->np); *tnp; tnp = (nick **)(&((*tnp)->exts[pnick_ext])) ) {
    if ( *tnp == np ) { 
      *tnp = np->exts[pnick_ext];
      found = 1;
      break;
    }
  }

  if (!found) {
    return; /* err */
  }

  if (!(((patricianick_t *)node->exts[pnode_ext])->np)) {
    freepatricianick(node->exts[pnode_ext]);
    node->exts[pnode_ext]= NULL;
  }

}

void freepatricianick(patricianick_t *pnp) {
  pnp->np=(nick *)freepatricianicks;
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

int pn_cmd_dumpnodenicks(void *source, int cargc, char **cargv) {
  nick *np=(nick *)source;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *head, *node;
  patricianick_t *pnp;
  nick *npp;
  int limit = 0;

  if (cargc < 1) {
    controlreply(np, "Syntax: dumpnodenicks <ipv4|ipv6|cidr4|cidr6>");
    return CMD_OK;
  }

  if (ipmask_parse(cargv[0], &sin, &bits) == 0) {
    controlreply(np, "Invalid mask.");
    return CMD_OK;
  }

  head = refnode(iptree, &sin, bits);

  PATRICIA_WALK(head, node)
  {
    pnp = node->exts[pnode_ext];
    if (pnp ) {
      npp = pnp->np;
      while(npp) {
        limit++;

        if ( limit < PATRICIANICK_MAXRESULTS )
          controlreply(np, "%s!%s@%s%s%s (%s)", npp->nick, npp->ident, npp->host->name->content, IsAccount(npp) ? "/" : "", npp->authname, IPtostr(node->prefix->sin));
        else if ( limit == PATRICIANICK_MAXRESULTS )
          controlreply(np, "Too many Results, truncated..");
        npp=npp->exts[pnick_ext];
      }    
    }
  }
  PATRICIA_WALK_END;
  controlreply(np, "Nick Count: %d", limit);

  return CMD_OK;
}

