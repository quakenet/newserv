#include "../core/hooks.h"
#include "trusts.h"

static void __counthandler(int hooknum, void *arg);

static void __newnick(int hooknum, void *arg) {
  nick *sender = arg;
  uint32_t host;
  trusthost *th;

  host = irc_in_addr_v4_to_int(&sender->p_ipaddr);
  th = th_getbyhost(host);

  settrusthost(sender, th);
  if(!th) {
    setnextbytrust(sender, NULL);
    return;
  }

  setnextbytrust(sender, th->users);
  th->users = sender;

  /* sucks we have to do this, at least until we get priority hooks */
  __counthandler(HOOK_TRUSTS_NEWNICK, sender);
  triggerhook(HOOK_TRUSTS_NEWNICK, sender);
}

static void __lostnick(int hooknum, void *arg) {
  nick *sender = arg, *np, *lp;
  trusthost *th = gettrusthost(sender);

  if(!th)
    return;

  __counthandler(HOOK_TRUSTS_LOSTNICK, sender);
  triggerhook(HOOK_TRUSTS_LOSTNICK, sender);

  /*
   * we need to erase this nick from the trusthost list
   * stored in the ->nextbytrust (ext) pointers in each nick
   */

  for(np=th->users,lp=NULL;np;lp=np,np=nextbytrust(np)) {
    if(np != sender)
      continue;

    if(lp) {
      setnextbytrust(lp, nextbytrust(np));
    } else {
      th->users = nextbytrust(np);
    }

    break;
  }
}

static void __counthandler(int hooknum, void *arg) {
  time_t t = time(NULL);
  trusthost *th = gettrusthost((nick *)arg);
  trustgroup *tg = th->group;

  tg->lastseen = th->lastseen = t;
  if(hooknum == HOOK_TRUSTS_NEWNICK) {
    th->count++;
    if(th->count > th->maxusage)
      th->maxusage = th->count;

    tg->count++;
    if(tg->count > tg->maxusage)
      tg->maxusage = tg->count;
  } else {
    th->count--;
    tg->count--;
  }
}

static int hooksregistered;

static void __dbloaded(int hooknum, void *arg) {
  int i;
  nick *np;

  if(hooksregistered)
    return;
  hooksregistered = 1;

  registerhook(HOOK_NICK_NEWNICK, __newnick);
  registerhook(HOOK_NICK_LOSTNICK, __lostnick);

/*  registerhook(HOOK_TRUSTS_NEWNICK, __counthandler);
  registerhook(HOOK_TRUSTS_LOSTNICK, __counthandler);
*/

  /* we could do it by host, but hosts and ips are not bijective :( */
  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      __newnick(0, np);
}

static void __dbclosed(int hooknum, void *arg) {
  if(!hooksregistered)
    return;
  hooksregistered = 0;

  deregisterhook(HOOK_NICK_NEWNICK, __newnick);
  deregisterhook(HOOK_NICK_LOSTNICK, __lostnick);

/*
  deregisterhook(HOOK_TRUSTS_NEWNICK, __counthandler);
  deregisterhook(HOOK_TRUSTS_LOSTNICK, __counthandler);
*/
}

void trusts_deregisterevents(void) {
  deregisterhook(HOOK_TRUSTS_DB_LOADED, __dbloaded);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, __dbclosed);

  __dbclosed(0, NULL);
}

void trusts_registerevents(void) {
  registerhook(HOOK_TRUSTS_DB_LOADED, __dbloaded);
  registerhook(HOOK_TRUSTS_DB_CLOSED, __dbclosed);

  if(trustsdbloaded)
    __dbloaded(0, NULL);
}
