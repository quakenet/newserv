#include "../core/hooks.h"
#include "trusts.h"

static void __counthandler(int hooknum, void *arg);

void trusts_newnick(nick *sender, int moving) {
  trusthost *th;
  void *arg[2];
  struct irc_in_addr ipaddress;

  ip_canonicalize_tunnel(&ipaddress, &sender->p_ipaddr);

  th = th_getbyhost(&ipaddress);

  settrusthost(sender, th);
  if(!th) {
    setnextbytrust(sender, NULL);
  } else {
    setnextbytrust(sender, th->users);
    th->users = sender;
    setipnodebits(sender, th->nodebits);
  }

  arg[0] = sender;
  arg[1] = (void *)(long)moving;

  /* sucks we have to do this, at least until we get priority hooks */
  __counthandler(HOOK_TRUSTS_NEWNICK, arg);

  triggerhook(HOOK_TRUSTS_NEWNICK, arg);
}

static void __newnick(int hooknum, void *arg) {
  trusts_newnick(arg, 0);
}

void trusts_lostnick(nick *sender, int moving) {
  nick *np, *lp;
  trusthost *th = gettrusthost(sender);
  void *arg[2];

  arg[0] = sender;
  arg[1] = (void *)(long)moving;

  __counthandler(HOOK_TRUSTS_LOSTNICK, arg);
  triggerhook(HOOK_TRUSTS_LOSTNICK, arg);

  if(!th)
    return;

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

static void __lostnick(int hooknum, void *arg) {
  trusts_lostnick(arg, 0);
}

static void __counthandler(int hooknum, void *arg) {
  time_t t = time(NULL);
  void **args = arg;
  trusthost *th = gettrusthost((nick *)args[0]);
  trustgroup *tg;

  if(!th)
    return;

  tg = th->group;
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
