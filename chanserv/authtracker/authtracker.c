
#include "../chanserv.h"
#include "authtracker.h"
#include "../../core/nsmalloc.h"
#include "../../core/hooks.h"
#include "../../core/error.h"

void at_newnick(int, void *);

void _init() {
  at_finddanglingsessions();
}

void _fini() {
  at_hookfini();
  nsfreeall(POOL_AUTHTRACKER);
}

void at_dbloaded(int hooknum, void *arg) {
  unsigned int i;
  nick *np;
  
  if (!(chanserv_init_status == CS_INIT_READY)) {
    registerhook(HOOK_CHANSERV_RUNNING, at_dbloaded);
    return;
  }
  
  if (hooknum)
    deregisterhook(HOOK_CHANSERV_RUNNING, at_dbloaded);
  
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      at_newnick(0, np);
    }
  }

  Error("authtracker",ERR_INFO,"Authtracker running");
  at_flushghosts();  
  at_hookinit();
}
