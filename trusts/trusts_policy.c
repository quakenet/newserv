#include "../core/hooks.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
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

  if(!th)
    return;

  tg = th->group;

  if(th->maxpernode && np->ipnode->usercount > th->maxpernode) {
    controlwall(NO_OPER, NL_TRUSTS, "Hard connection limit exceeded on IP: %s (group: %s) %d connected, %d max.", IPtostr(np->p_ipaddr), tg->name->content, np->ipnode->usercount, th->maxpernode);
    return;
  }

  /*
   * the purpose of this logic is to avoid spam like this:
   * WARNING: tgX exceeded limit: 11 connected vs 10 max
   * (goes back down to 10)
   * WARNING: tgX exceeded limit: 11 connected vs 10 max
   */

  if(hooknum == HOOK_TRUSTS_NEWNICK) {
    if(tg->trustedfor && tg->count > tg->trustedfor) {
/*
      if(tg->count > (long)tg->exts[countext]) {

        tg->exts[countext] = (void *)(long)tg->count;
*/
        controlwall(NO_OPER, NL_TRUSTS, "Hard connection limit exceeded: '%s', %d connected, %d max.", tg->name->content, tg->count, tg->trustedfor);
      }
/*
    }
*/
    if((tg->mode == 1) && (np->ident[0] == '~'))
      controlwall(NO_OPER, NL_TRUSTS, "Ident required: '%s' %s!%s@%s.", tg->name->content, np->nick, np->ident, np->host->name->content);

    if(tg->maxperident > 0) {
      int identcount = 0;
      trusthost *th2;
      nick *tnp;

      for(th2=tg->hosts;th2;th2=th2->next) {
        for(tnp=th2->users;tnp;tnp=nextbytrust(tnp)) {
          if(!ircd_strcmp(tnp->ident, np->ident))
            identcount++;
        }
      }

      if(identcount > tg->maxperident)
        controlwall(NO_OPER, NL_TRUSTS, "Hard ident limit exceeded: '%s' %s!%s@%s, %d connected, %d max.", tg->name->content, np->nick, np->ident, np->host->name->content, identcount, tg->maxperident);
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
