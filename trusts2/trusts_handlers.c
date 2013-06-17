#include "trusts.h"

void trusts_hook_newuser(int hook, void *arg) {
  /*TODO: add subnet clone warnings somewhere */
  nick *np = (nick *)arg;
  trusthost_t *tgh = NULL;
  trustgroup_t *tg = NULL;
  patricia_node_t *parent;

  if (trusts_ignore_np(np)) {
    return;
  }

  if(np->ipnode->exts[tgh_ext]) {
    /* we have a new user on a trust group host */
    tgh = (trusthost_t *)np->ipnode->exts[tgh_ext];
    /* check if it has expired */ 
    if(tgh->expire && (tgh->expire <= getnettime())) {
      trusthost_expire(tgh);
      tgh = NULL;
    } else {
      tg = tgh->trustgroup;
    } 
  }

  if (!tgh) {
    /* recurse to see if a parent node is trusted */
    parent = np->ipnode->parent;
    while (parent) {
      if(parent->exts)
        if( parent->exts[tgh_ext]) {
          /* falls under parent trust */
          tgh = (trusthost_t *)parent->exts[tgh_ext];
          if(tgh->expire && (tgh->expire <= getnettime())) {
            trusthost_expire(tgh);
            tgh = NULL;
          } else {
            tg = tgh->trustgroup;
            break; 
          }
        }
      parent = parent->parent;
    }
  }

  if(tgh) {
    /* we have a trusthost - check it */
    tg = tgh->trustgroup;
    if(((((int)(np->ipnode->usercount))) > tg->maxperip) && tg->maxperip ) {
      /* user exceed ip trust limit - disconnect */
      controlwall(NO_OPER, NL_TRUSTS, "TG %lu: Exceeding IP limit (%d / %d) for %s!%s@%s (%s)", tg->id, (((int)(np->ipnode->usercount))), tg->maxperip, np->nick, np->ident, np->host->name->content, removeusers == 1 ? "disconnected": "ignored");
      //killuser(NULL, np, "USER: Exceeding IP Limit.");
    }
    if( tg->maxclones >0 ) {
      if( (tg->currenton + 1) > tg->maxclones) {
        /* user exceeds trust group limit - disconnect */
        //killuser(NULL, np, "USER: Exceeding Trustgroup Limit.");
        controlwall(NO_OPER, NL_TRUSTS, "TG %lu: Exceeding trustgroup limit (%d / %d) for %s!%s@%s (%s)",tg->id, (tg->currenton + 1), tg->maxclones, np->nick, np->ident, np->host->name->content, removeusers == 1 ? "disconnected": "ignored");
      }
    }
    if ( np->ident[0] == '~')  {
      /* non-ident user */
      if (tg->enforceident ) {
        controlwall(NO_OPER, NL_TRUSTS, "TG %lu: Ident Required for %s!%s@%s (%s)", tg->id, np->nick, np->ident, np->host->name->content, removeusers == 1 ? "disconnected": "ignored");
        //killuser(NULL, np, "USER: Ident Required From Your Host.");
        /*TODO: add short gline here - ~*@%s - "IDENTD required from your host", "MissingIDENT" */
      }
    } else {
      /* ident user */
      /*TODO: need to tidy out ident currenton */
      increment_ident_count(np, tg);
    }
    /* Trust Checks Passed: OK - increment counters */
    increment_trust_ipnode(np->ipnode);

    /* set nick extension for user for future use */
    np->exts[tgn_ext] = tgh; 

    return;
  }
  /* non trusted user - OK */
}


void trusts_hook_lostuser(int hook, void *arg) {
  nick *np = (nick *)arg;
  trusthost_t *tgh = NULL;
  trustgroup_t *tg = NULL;
  patricia_node_t *parent;

  if(!np) {
    Error("nodecount", ERR_ERROR, "np was NULL");
  }
  if(!np->ipnode) {
    Error("nodecount", ERR_ERROR, "np->ipnode was NULL");
  }
  if(!np->ipnode->exts) {
    Error("nodecount", ERR_ERROR, "np->ipnode->exts was NULL");
  }

  if (trusts_ignore_np(np)) {
    return;
  }

  decrement_trust_ipnode(np->ipnode);

  if(np->ipnode->exts[tgh_ext]) {
    tgh = (trusthost_t *)np->ipnode->exts[tgh_ext];
  } else {
    parent = np->ipnode->parent;
    while (parent) {
      if(parent->exts)
        if( parent->exts[tgh_ext]) {
          /* falls under parent trust */
          tgh = (trusthost_t *)parent->exts[tgh_ext];
          break;
        }
      parent = parent->parent;
    }
  }
  if(tgh) {
    tg = tgh->trustgroup;
    if ( np->ident[0] != '~')  {
      decrement_ident_count(np, tg);
    }
  }

  /* clear nick extension */
  np->exts[tgn_ext] = NULL;
}

