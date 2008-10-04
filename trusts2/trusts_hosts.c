#include "trusts.h"

trusthost_t *createtrusthostfromdb(unsigned long id, patricia_node_t* node, time_t startdate, time_t lastused, time_t expire, unsigned long maxused, trustgroup_t* trustgroup, time_t created, time_t modified){
  trusthost_t *th = createtrusthost(id,node,expire,trustgroup);

  th->startdate=startdate;
  th->lastused = lastused;
  th->maxused = maxused;
  th->created = created;
  th->modified = modified;
  return th;
}

trusthost_t *createtrusthost(unsigned long id, patricia_node_t* node, time_t expire, trustgroup_t *trustgroup) {
  trusthost_t *th = newtrusthost();
  memset(th, 0, sizeof(trusthost_t));

  time_t timenow = getnettime();

  th->id = id;
  th->node = node;
  th->expire = expire;
  th->trustgroup = trustgroup;
  th->created = timenow;
  th->modified = timenow;
  th->lastused = 0;
  trusts_addtrusthosttohash(th);
  return th;
}

void trusthost_free(trusthost_t* t)
{
  trusts_removetrusthostfromhash(t);
  derefnode(iptree,t->node);
  freetrusthost(t);
}

trusthost_t* trusthostadd(patricia_node_t *node, trustgroup_t* tg, time_t expire) {
  trusthost_t* tgh = createtrusthost(++trusts_lasttrusthostid, node, expire, tg);
  patricia_node_t *inode;
  nick *np;
  patricianick_t *pnp;
  int i;
  time_t timenow;

  if(!tgh) {
    return NULL;
  }

  timenow = getnettime();
  if(node->usercount >0){
    tgh->lastused = timenow;
    tg->currenton += node->usercount;
    tg->lastused = timenow;
  }
  tgh->expire = expire;

  PATRICIA_WALK(node, inode)
  {
    pnp = inode->exts[pnode_ext];
    if (pnp ) {
      for (i = 0; i < PATRICIANICK_HASHSIZE; i++) {
        for (np = pnp->identhash[i]; np; np=np->exts[pnick_ext]) {
          increment_ident_count(np, tg);
        }
      }
    }
  }
  PATRICIA_WALK_END;

  node->exts[tgh_ext] = tgh;

  return tgh;
}

void trusthost_addcounters(trusthost_t* tgh) {
  patricia_node_t *inode;
  nick *np;
  patricianick_t *pnp;
  int i;

  trustgroup_t* tg = tgh->trustgroup;
  if(tgh->node->usercount >0){
    tg->currenton += tgh->node->usercount;
  }

  PATRICIA_WALK(tgh->node, inode)
  {
    pnp = inode->exts[pnode_ext];
    if (pnp ) {
      for (i = 0; i < PATRICIANICK_HASHSIZE; i++) {
        for (np = pnp->identhash[i]; np; np=np->exts[pnick_ext]) {
          increment_ident_count(np, tg);
        }
      }
    }
  }
  PATRICIA_WALK_END;

  tgh->node->exts[tgh_ext] = tgh;
}


void trusthost_expire( trusthost_t *th) {
  trustgroup_t *tg = th->trustgroup;

  if(tg->expire && (tg->expire <= getnettime())) {
    /* check if trustgroup also expires */
    trustgroup_expire(tg);
  } else {
    controlwall(NO_OPER, NL_TRUSTS, "%s/%d expired from trustgroup #%lu",IPtostr(th->node->prefix->sin),irc_bitlen(&(th->node->prefix->sin),th->node->prefix->bitlen),tg->id);
    trustsdb_deletetrusthost(th->node->exts[tgh_ext]);
    trusthost_free(th->node->exts[tgh_ext]);
    th->node->exts[tgh_ext] = NULL;
  }
}
