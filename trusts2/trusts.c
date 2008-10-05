#include "trusts.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "../core/nsmalloc.h"

int tgh_ext;
unsigned long trusts_lasttrustgroupid;
unsigned long trusts_lasttrusthostid;
unsigned long trusts_lasttrustblockid;
int trusts_loaded;
int removeusers = 0;

static void trusts_status(int hooknum, void *arg);
void trustsfinishinit(int hooknum, void *arg);

void _init(void) {
  trusts_hash_init();

  tgh_ext = registernodeext("trusthost");
  if ( !tgh_ext ) {
    Error("trusts", ERR_FATAL, "Could not register a required node extension");
    return;
  }

  if ( !trusts_load_db()) {
    return;
  }
 
  registerhook(HOOK_TRUSTS_DBLOADED, trustsfinishinit);

  if (trusts_loaded) 
    trustsfinishinit(HOOK_TRUSTS_DBLOADED, NULL);
}

void trustsfinishinit(int hooknum, void *arg) {
  Error("trusts",ERR_INFO,"Database loaded, finishing initialisation.");

  deregisterhook(HOOK_TRUSTS_DBLOADED, trustsfinishinit);

  registerhook(HOOK_NICK_NEWNICK, &trusts_hook_newuser);
  registerhook(HOOK_NICK_LOSTNICK, &trusts_hook_lostuser);

  registerhook(HOOK_CORE_STATSREQUEST, trusts_status);
}

void _fini(void) {
  trusthost_t *thptr;
  trustgroupidentcount_t *t;
  int i;
  for ( i = 0; i < TRUSTS_HASH_HOSTSIZE ; i++ ) {
    for ( thptr = trusthostidtable[i]; thptr; thptr = thptr-> nextbyid ) {
      derefnode(iptree,thptr->node);
    }
  }
  releasenodeext(tgh_ext); 

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

void trust_debug(char *format, ...) {
  char buf[512];
  va_list va;
  channel *debugcp = findchannel("#qnet.trusts");
  if(debugcp) {
     va_start(va, format);
     vsnprintf(buf, sizeof(buf), format, va);
     va_end(va);
    controlchanmsg(debugcp,buf);
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
