#include "trusts.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../core/nsmalloc.h"
#include "../lib/version.h"

MODULE_VERSION("");

int tgh_ext = -1;
int tgb_ext = -1;
int tgn_ext = -1;

unsigned long trusts_lasttrustgroupid;
unsigned long trusts_lasttrusthostid;
int trusts_loaded = 0;
int removeusers = 0;

static void trusts_status(int hooknum, void *arg);
void trustsfinishinit(int hooknum, void *arg);

void _init(void) {
  trusts_hash_init();

  tgh_ext = registernodeext("trusthost");
  if ( tgh_ext == -1 ) {
    Error("trusts", ERR_FATAL, "Could not register a required node extension (trusthost)");
    return;
  }

  tgn_ext = registernickext("trustnick");
  if ( tgn_ext == -1 ) {
    Error("trusts", ERR_FATAL, "Could not register a required nick extension (trustnick)");
    return;
  }

  registerhook(HOOK_TRUSTS_DBLOADED, trustsfinishinit);

  if ( !trusts_load_db()) {
    return;
  }
 
  if (trusts_loaded) 
    trustsfinishinit(HOOK_TRUSTS_DBLOADED, NULL);
}

void trustsfinishinit(int hooknum, void *arg) {
  registerhook(HOOK_NICK_NEWNICK, &trusts_hook_newuser);
  registerhook(HOOK_NICK_LOSTNICK, &trusts_hook_lostuser);

  registerhook(HOOK_CORE_STATSREQUEST, trusts_status);
}

void _fini(void) {
  trusthost_t *thptr;
  trustgroupidentcount_t *t;

  int i;

  deregisterhook(HOOK_TRUSTS_DBLOADED, trustsfinishinit);

  if ( trusts_loaded ) {
    deregisterhook(HOOK_NICK_NEWNICK, &trusts_hook_newuser);
    deregisterhook(HOOK_NICK_LOSTNICK, &trusts_hook_lostuser);

    deregisterhook(HOOK_CORE_STATSREQUEST, trusts_status);
  }

  for ( i = 0; i < TRUSTS_HASH_IDENTSIZE ; i++ ) {
    for ( t = trustgroupidentcounttable[i]; t; t = t-> next ) {
      if (t->ident) {
        freesstring(t->ident);
      }
    }
  }

  if (tgh_ext != -1)
    releasenodeext(tgh_ext);
  if (tgb_ext != -1)
    releasenodeext(tgb_ext);
  if (tgn_ext != -1)
    releasenickext(tgn_ext);

  /* @@@ CLOSE DB */

  trusts_hash_fini();

  nsfreeall(POOL_TRUSTS);
}

void increment_trust_ipnode(patricia_node_t *node) {
  patricia_node_t *parent;
  trusthost_t *tgh = NULL;
  time_t curtime = getnettime();
  parent = node;
  while (parent) {
    if(parent->exts && parent->exts[tgh_ext]) {
      /* update the trusted hosts themselves */
      tgh = (trusthost_t *)parent->exts[tgh_ext];
      tgh->lastused = curtime;
      if (tgh->node->usercount > tgh->maxused) { tgh->maxused = tgh->node->usercount; }

      /* update the trustgroup itself */
      tgh->trustgroup->currenton++;
      tgh->trustgroup->lastused = curtime;
      if (tgh->trustgroup->currenton > tgh->trustgroup->maxusage) { tgh->trustgroup->maxusage = tgh->trustgroup->currenton; }
    }
    parent = parent->parent;
  }
}

void decrement_trust_ipnode(patricia_node_t *node) {
  patricia_node_t *parent;
  trusthost_t *tgh = NULL;
  time_t curtime = getnettime();

  parent = node;
  while (parent) {
    if(parent->exts && parent->exts[tgh_ext]) {
      tgh = (trusthost_t *)parent->exts[tgh_ext];
      tgh->trustgroup->currenton--;
      tgh->lastused = curtime;

      tgh->trustgroup->lastused = curtime;
    }
    parent = parent->parent;
  }
}

static void trusts_status(int hooknum, void *arg) {
  if((long)arg > 10) {
    char message[100];
    int tgcount = 0, thcount = 0; 
    trustgroup_t *tg; trusthost_t* thptr; int i;

    for ( i = 0; i < TRUSTS_HASH_GROUPSIZE ; i++ ) {
      for ( tg = trustgroupidtable[i]; tg; tg = tg -> nextbyid ) {
        tgcount++;
      }
    }

    for ( i = 0; i < TRUSTS_HASH_HOSTSIZE ; i++ ) {
      for ( thptr = trusthostidtable[i]; thptr; thptr = thptr-> nextbyid ) {
        thcount++;
      }
    }
    snprintf(message, sizeof(message), "Trusts  :%7d groups, %7d hosts", tgcount, thcount);
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }
}

int trusts_ignore_np(nick *np) {
  if(SIsService(&serverlist[homeserver(np->numeric)])) {
    /* ANY user created by a server (nterface,fakeusers,Q) are ignored in relation to trusts */
    /* NOTE: we might need to review this if we ever used newserv to handle client/user connections in some way */
    return 1;
  }
  return 0;
}
