
#include "../chanserv.h"
#include "authtracker.h"
#include "../../core/nsmalloc.h"
#include "../../core/hooks.h"
#include "../../core/error.h"

void at_newnick(int, void *);
DBModuleIdentifier authtrackerdb;

void _init() {
  authtrackerdb = dbgetid();

  chanservaddcommand("dumpauthtracker",QCMD_DEV,1,at_dumpdb,"Shows servers with dangling authtracker entries.\n","");
  at_finddanglingsessions();
}

void _fini() {
  at_hookfini();
  nsfreeall(POOL_AUTHTRACKER);
  
  chanservremovecommand("dumpauthtracker",at_dumpdb);

  dbfreeid(authtrackerdb);
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
