#include <string.h>
#include <stdio.h>
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../core/config.h"
#include "rbl.h"

MODULE_VERSION("");

rbl_instance *rbl_instances = NULL;

int registerrbl(const char *name, const rbl_ops *ops, void *udata) {
  rbl_instance *instance;

  instance = malloc(sizeof(*instance));

  if (!instance)
    return -1;

  instance->name = getsstring(name, 255);
  instance->ops = ops;
  instance->udata = udata;

  instance->next = rbl_instances;
  rbl_instances = instance;

  RBL_REFRESH(instance);

  return 0;
}

void deregisterrblbyops(const rbl_ops *ops) {
  rbl_instance **pnext, *rbl;

  for (pnext = &rbl_instances; *pnext; pnext = &((*pnext)->next)) {
    rbl = *pnext;

    if (rbl->ops == ops) {
      RBL_DTOR(rbl);
      *pnext = rbl->next;
      freesstring(rbl->name);
      free(rbl);

      if (!*pnext)
        break;
    }
  }
}

static void rbl_sched_refresh(void *uarg) {
  rbl_instance *rbl;

  for (rbl = rbl_instances; rbl; rbl = rbl->next)
    RBL_REFRESH(rbl);
}

static void rbl_whois_cb(int hooknum, void *arg) {
  rbl_instance *rbl;
  nick *np = (nick *)arg;

  for (rbl = rbl_instances; rbl; rbl = rbl->next) {
    char reason[255], message[255];
    if (RBL_LOOKUP(rbl, &np->ipaddress, reason, sizeof(reason)) <= 0)
      continue;

     snprintf(message, sizeof(message), "RBL       : %s (%s)", rbl->name->content, reason);
     triggerhook(HOOK_CONTROL_WHOISREPLY, message);
  }
}

void _init(void) {
  schedulerecurring(time(NULL)+300, 0, 300, rbl_sched_refresh, NULL);
  registerhook(HOOK_CONTROL_WHOISREQUEST, &rbl_whois_cb);
}

void _fini(void) {
  deleteallschedules(rbl_sched_refresh);
  deregisterhook(HOOK_CONTROL_WHOISREQUEST, &rbl_whois_cb);
}
