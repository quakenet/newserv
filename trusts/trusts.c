#include "trusts.h"
#include "../core/hooks.h"
#include <stdio.h>
#include <time.h>

trustgroup *tglist;

int trusts_loaddb(void);
void trusts_closedb(void);
static void statusfn(int, void *);

void _init(void) {
  if(!trusts_loaddb())
    return;

  registerhook(HOOK_CORE_STATSREQUEST, statusfn);
}

void _fini(void) {
  trusts_closedb();

  deregisterhook(HOOK_CORE_STATSREQUEST, statusfn);
}

char *trusts_timetostr(time_t t) {
  static char buf[100];

  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&t));

  return buf;
}

static void statusfn(int hooknum, void *arg) {
  if((long)arg > 10) {
    char message[100];
    int groupcount = 0, hostcount = 0;
    trustgroup *tg;
    trusthost *th;

    for(tg=tglist;tg;tg=tg->next) {
      groupcount++;
      for(th=tg->hosts;th;th=th->next)
        hostcount++;
    }

    snprintf(message, sizeof(message), "Trusts  : %d trust groups, %d hosts", groupcount, hostcount);
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }  
}
