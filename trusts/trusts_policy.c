#include "../core/hooks.h"
#include "../control/control.h"
#include "trusts.h"

static int countext;

static void policycheck(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  long moving = (long)args[1];
  trusthost *th = gettrusthost(np);
  trustgroup *tg;

  if(moving)
    return;

  if(!th) {
    if(np->ipnode->usercount > 5)
      controlwall(NO_OPER, NL_TRUSTS, "Hard limit exceeded on IP: %s (untrusted) %d connected, 5 max.", IPtostr(np->p_ipaddr), np->ipnode->usercount);
    return;
  }

  tg = th->group;
  /*
   * the purpose of this logic is to avoid spam like this:
   * WARNING: tgX exceeded limit: 11 connected vs 10 max
   * (goes back down to 10)
   * WARNING: tgX exceeded limit: 11 connected vs 10 max
   */

  if(hooknum == HOOK_TRUSTS_NEWNICK) {
    if(tg->count > tg->maxusage) {
      if(tg->count > (long)tg->exts[countext]) {
        tg->exts[countext] = (void *)(long)tg->count;

        controlwall(NO_OPER, NL_TRUSTS, "Hard limit exceeded: '%s', %d connected, %d max.", tg->name->content, tg->count, tg->maxusage);
      }
    }
  } else {
    if(tg->count < tg->maxusage)
      tg->exts[countext] = (void *)(long)tg->count;
  }
}

void _init(void) {
  countext = registertgext("count");
  if(countext == -1)
    return;

  registerhook(HOOK_TRUSTS_NEWNICK, policycheck);
  registerhook(HOOK_TRUSTS_LOSTNICK, policycheck);
}

void _fini(void) {
  if(countext == -1)
    return;

  releasetgext(countext);

  deregisterhook(HOOK_TRUSTS_NEWNICK, policycheck);
  deregisterhook(HOOK_TRUSTS_LOSTNICK, policycheck);
}
