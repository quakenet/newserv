#include <stdio.h>
#include "../core/hooks.h"
#include "../core/error.h"
#include "trusts.h"

int trusts_loaddb(void);
void trusts_closedb(void);
void trusts_registerevents(void);
void trusts_deregisterevents(void);

static void statusfn(int, void *);

static int loaded, unloaded;

int trusts_thext, trusts_nextuserext;

void _init(void) {
  trusts_thext = registernickext("trustth");
  if(trusts_thext == -1) {
    Error("trusts", ERR_ERROR, "Unable to register first nick extension.");
    return;
  }

  trusts_nextuserext = registernickext("trustnext");
  if(trusts_thext == -1) {
    releasenickext(trusts_thext);
    Error("trusts", ERR_ERROR, "Unable to register second nick extension.");
    return;
  }

  if(!trusts_loaddb())
    return;
  loaded = 1;

  registerhook(HOOK_CORE_STATSREQUEST, statusfn);
  trusts_registerevents();
}

void trusts_unload(void) {
  if(unloaded)
    return;

  unloaded = 1;

  if(trusts_thext != -1) {
    releasenickext(trusts_thext);
    releasenickext(trusts_nextuserext);
  }

  if(loaded) {
    deregisterhook(HOOK_CORE_STATSREQUEST, statusfn);
    trusts_deregisterevents();
  }

  trusts_closedb();
}

void _fini(void) {
  trusts_unload();
}

static void statusfn(int hooknum, void *arg) {
  if((long)arg > 10) {
    char message[100];
    int groupcount = 0, hostcount = 0, usercount = 0;
    trustgroup *tg;
    trusthost *th;

    for(tg=tglist;tg;tg=tg->next) {
      usercount+=tg->count;
      groupcount++;
      for(th=tg->hosts;th;th=th->next)
        hostcount++;
    }

    snprintf(message, sizeof(message), "Trusts  :%7d groups, %7d hosts, %7d users", groupcount, hostcount, usercount);
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }  
}
