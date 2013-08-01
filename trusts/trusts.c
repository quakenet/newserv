#include <stdio.h>
#include <string.h>
#include "../lib/version.h"
#include "../core/hooks.h"
#include "../core/error.h"
#include "../core/nsmalloc.h"
#include "../server/server.h"
#include "trusts.h"

MODULE_VERSION("");

void trusts_registerevents(void);
void trusts_deregisterevents(void);

static void statusfn(int, void *);
static void whoisfn(int, void *);

static sstring *tgextnames[MAXTGEXTS];

int trusts_thext, trusts_nextuserext;
int trustsdbloaded;

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

  registerhook(HOOK_CORE_STATSREQUEST, statusfn);
  registerhook(HOOK_CONTROL_WHOISREQUEST, &whoisfn);
  trusts_registerevents();
}

void _fini(void) {
  if(trusts_thext != -1) {
    releasenickext(trusts_thext);
    releasenickext(trusts_nextuserext);
  }

  deregisterhook(HOOK_CORE_STATSREQUEST, statusfn);
  deregisterhook(HOOK_CONTROL_WHOISREQUEST, &whoisfn);
  trusts_deregisterevents();

  nscheckfreeall(POOL_TRUSTS);
}

static void whoisfn(int hooknum, void *arg) {
  trusthost *th;
  char message[512];
  nick *np = (nick *)arg;

  if(!np)
    return;

  th = gettrusthost(np);

  if(!th)
    return;

  snprintf(message, sizeof(message), "Trustgroup: %s (#%d)", th->group->name->content, th->group->id);
  triggerhook(HOOK_CONTROL_WHOISREPLY, message);

  if (th->maxpernode > 0) {
    snprintf(message, sizeof(message), "Node      : %s", CIDRtostr(np->p_ipaddr, th->nodebits));
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);

    patricia_node_t *node;
    int usercount = 0;

    node = refnode(iptree, &(np->p_ipaddr), th->nodebits);
    usercount = node->usercount;
    derefnode(iptree, node);

    snprintf(message, sizeof(message), "Node      : Usage: %d/%d", usercount, th->maxpernode);
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);
  }

  if (th->group->trustedfor > 0) {
    snprintf(message, sizeof(message), "Trusthost : %s", CIDRtostr(th->ip, th->bits));
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);

    snprintf(message, sizeof(message), "Trustgroup : Usage: %d/%d", th->group->count, th->group->trustedfor);
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);
  }
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

int findtgext(const char *name) {
  int i;

  for(i=0;i<MAXTGEXTS;i++)
    if(tgextnames[i] && !strcmp(name, tgextnames[i]->content))
      return i;

  return -1;
}

int registertgext(const char *name) {
  int i;

  if(findtgext(name) != -1) {
    Error("trusts", ERR_WARNING, "Tried to register duplicate trust group extension: %s.", name);
    return -1;
  }

  for(i=0;i<MAXNICKEXTS;i++) {
    if(!tgextnames[i]) {
      tgextnames[i] = getsstring(name, 100);
      return i;
    }
  }

  Error("trusts", ERR_WARNING, "Tried to register too many trust group extensions: %s.", name);
  return -1;
}

void releasetgext(int index) {
  trustgroup *tg;

  freesstring(tgextnames[index]);
  tgextnames[index] = NULL;

  for(tg=tglist;tg;tg=tg->next)
    tg->exts[index] = NULL;
}

int trusts_fullyonline(void) {
  if(myhub == -1)
    return 0;

  return serverlist[myhub].linkstate == LS_LINKED;
}

