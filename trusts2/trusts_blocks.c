#include "trusts.h"

static trustblock_t *trustblocklist = NULL;

trustblock_t *createtrustblock(unsigned long id, patricia_node_t* node, unsigned long ownerid, time_t expire, char *reason_private, char *reason_public) {
  trustblock_t *tb = newtrustblock();
  memset(tb, 0, sizeof(trustblock_t));

  tb->startdate = getnettime();

  tb->id = id;
  tb->node = node;
  tb->ownerid = ownerid;
  tb->expire = expire;
  tb->reason_private = getsstring(reason_private,512);
  tb->reason_public = getsstring(reason_public,512);

  tb->next=trustblocklist;
  trustblocklist = tb; 
  return tb;
}

trustblock_t *createtrustblockfromdb(unsigned long id, patricia_node_t* node, unsigned long ownerid, time_t expire, time_t startdate, char *reason_private, char *reason_public) {
  trustblock_t *tb = createtrustblock(id,node,ownerid, expire, reason_private, reason_public);

  tb->startdate = startdate;
  return tb;
}

void trustblock_free(trustblock_t* t)
{
  trustblock_t *st, *pst=NULL;
  for (st = trustblocklist; st; st=st->next) {
    if (st == t ) {
      break;
    }
    pst = st;
  }
 
  if (st) {
    if (pst) {
      pst->next = st->next;
    } 

    derefnode(iptree,st->node);
    if (st->reason_public ) {
      freesstring(st->reason_public);
    }
    if (st->reason_private) {
      freesstring(st->reason_private);
    }
    freetrustblock(st);
  }
}

void trustblock_expire( trustblock_t *tb) {
    controlwall(NO_OPER, NL_TRUSTS, "trustblock on %s/%d expired",IPtostr(tb->node->prefix->sin),irc_bitlen(&(tb->node->prefix->sin),tb->node->prefix->bitlen));
    trustsdb_deletetrustblock(tb->node->exts[tgb_ext]);
    trustblock_free(tb->node->exts[tgb_ext]);
    tb->node->exts[tgb_ext] = NULL;
}

void trustblock_freeall() {
  trustblock_t *tb, *ptb=NULL;
  tb=trustblocklist;
  while(tb) {
    ptb=tb;
    tb=tb->next;

    derefnode(iptree,ptb->node);
    if (ptb->reason_public ) {
      freesstring(ptb->reason_public);
    }
    if (ptb->reason_private) {
      freesstring(ptb->reason_private);
    }
    ptb->node->exts[tgb_ext] = NULL;
  }
}
