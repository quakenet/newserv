#include "../core/hooks.h"
#include "../core/config.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../glines/glines.h"
#include "trusts.h"

static int countext, enforcepolicy;

static void policycheck(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  long moving = (long)args[1];
  trusthost *th = gettrusthost(np);
  trustgroup *tg;
  patricia_node_t *head, *node;
  int nodecount = 0;

  if(moving)
    return;

  if(!th)
    return;

  tg = th->group;

  head = refnode(iptree, &np->p_nodeaddr, th->nodebits);
  PATRICIA_WALK(head, node)
  {
    nodecount += node->usercount;
  }
  PATRICIA_WALK_END;
  derefnode(iptree, head);

  if(th->maxpernode && nodecount > th->maxpernode) {
    controlwall(NO_OPER, NL_TRUSTS, "Hard connection limit exceeded on subnet: %s (group: %s) %d connected, %d max.", trusts_cidr2str(&np->p_nodeaddr, th->nodebits), tg->name->content, nodecount, th->maxpernode);

    if(enforcepolicy)
      glinebynick(np, POLICY_GLINE_DURATION, "Too many connections from your host.", GLINE_IGNORE_TRUST);

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
    if((tg->mode == 1) && (np->ident[0] == '~')) {
      controlwall(NO_OPER, NL_TRUSTS, "Ident required: '%s' %s!%s@%s.", tg->name->content, np->nick, np->ident, np->host->name->content);

      if (enforcepolicy)
        glinebynick(np, POLICY_GLINE_DURATION, "IDENT required from your host.", GLINE_ALWAYS_USER|GLINE_IGNORE_TRUST);
    }

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

      if(identcount > tg->maxperident) {
        controlwall(NO_OPER, NL_TRUSTS, "Hard ident limit exceeded: '%s' %s!%s@%s, %d connected, %d max.", tg->name->content, np->nick, np->ident, np->host->name->content, identcount, tg->maxperident);

        if (enforcepolicy)
          glinebynick(np, POLICY_GLINE_DURATION, "Too many connections from your user.", GLINE_ALWAYS_USER|GLINE_IGNORE_TRUST);
      }
    }
  } else {
    if(tg->count < tg->maxusage)
      tg->exts[countext] = (void *)(long)tg->count;
  }
}

static int trusts_cmdtrustpolicy(void *source, int cargc, char **cargv) {
  nick *sender = source;

  if(cargc < 1) {
    controlreply(sender, "Trust policy enforcement is currently %s.", enforcepolicy?"enabled":"disabled");
    return CMD_OK;
  }

  enforcepolicy = atoi(cargv[0]);
  controlwall(NO_OPER, NL_TRUSTS, "%s %s trust policy enforcement.", controlid(sender), enforcepolicy?"enabled":"disabled");
  controlreply(sender, "Trust policy enforcement is now %s.", enforcepolicy?"enabled":"disabled");

  return CMD_OK;
}

void _init(void) {
  countext = registertgext("count");
  if(countext == -1)
    return;

  sstring *m;

  m = getconfigitem("trusts_policy", "enforcepolicy");
  if(m)
    enforcepolicy = atoi(m->content);

  registerhook(HOOK_TRUSTS_NEWNICK, policycheck);
  registerhook(HOOK_TRUSTS_LOSTNICK, policycheck);

  registercontrolhelpcmd("trustpolicy", NO_DEVELOPER, 1, trusts_cmdtrustpolicy, "Usage: trustpolicy ?1|0?\nEnables or disables policy enforcement. Shows current status when no parameter is specified.");
}

void _fini(void) {
  if(countext == -1)
    return;

  releasetgext(countext);

  deregisterhook(HOOK_TRUSTS_NEWNICK, policycheck);
  deregisterhook(HOOK_TRUSTS_LOSTNICK, policycheck);

  deregistercontrolcmd("trustpolicy", trusts_cmdtrustpolicy);
}
