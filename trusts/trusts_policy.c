#include "../core/hooks.h"
#include "../control/control.h"
#include "trusts.h"

static void policycheck(int hooknum, void *arg) {
  nick *np = arg;
  trusthost *th = gettrusthost(np);
  trustgroup *tg = th->group;

  if(tg->count > tg->maxusage)
    controlwall(NO_OPER, NL_TRUSTS, "Hard limit exceeded: '%s', %d connected, %d max.", tg->name->content, tg->count, tg->maxusage);
}

void _init(void) {
  registerhook(HOOK_TRUSTS_NEWNICK, policycheck);
}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_NEWNICK, policycheck);
}
