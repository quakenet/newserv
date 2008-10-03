#include <stdio.h>
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "trusts.h"

static void registercommands(int, void *);
static void deregistercommands(int, void *);

static int trusts_cmdtrustlist(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg;
  trusthost *th;
  time_t t;

  if(cargc < 1)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't find a trustgroup with that id.");
    return CMD_ERROR;
  }

  t = time(NULL);

  /* abusing the ternary operator a bit :( */
  controlreply(sender, "Name:            : %s", tg->name->content);
  controlreply(sender, "Trusted for      : %d", tg->trustedfor);
  controlreply(sender, "Currently using  : %d", tg->count);
  controlreply(sender, "Clients per user : %d (%senforcing ident)", tg->maxperident, tg->mode?"":"not ");
  controlreply(sender, "Contact:         : %s", tg->contact->content);
  controlreply(sender, "Expires in       : %s", (tg->expires>t)?longtoduration(tg->expires - t, 2):"(the past -- BUG)");
  controlreply(sender, "Last changed by  : %s", tg->createdby->content);
  controlreply(sender, "Comment:         : %s", tg->comment->content);
  controlreply(sender, "ID:              : %u", tg->id);
  controlreply(sender, "Last used        : %s", (tg->count>0)?"(now)":((tg->lastseen>0)?trusts_timetostr(tg->lastseen):"(never)"));
  controlreply(sender, "Max usage        : %d", tg->maxusage);
  controlreply(sender, "Last max reset   : %s", tg->lastmaxuserreset?trusts_timetostr(tg->lastmaxuserreset):"(never)");

  controlreply(sender, "Host                 Current    Max        Last seen");

  for(th=tg->hosts;th;th=th->next)
    controlreply(sender, " %-20s %-10d %-10d %s", trusts_cidr2str(th->ip, th->mask), th->count, th->maxusage, (th->count>0)?"(now)":((th->lastseen>0)?trusts_timetostr(th->lastseen):"(never)"));

  controlreply(sender, "End of list.");

  return CMD_OK;
}

static int trusts_cmdtrustadd(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *host;
  uint32_t ip, mask;
  trusthost *th, *superset, *subset;

  if(cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  host = cargv[1];
  if(!trusts_str2cidr(host, &ip, &mask)) {
    controlreply(sender, "Invalid host.");
    return CMD_ERROR;
  }

  /* OKAY! Lots of checking here!
   *
   * Need to check:
   *   - host isn't already covered by given group (reject if it is)
   *   - host doesn't already exist exactly already (reject if it does)
   *   - host is more specific than an existing one (warn if it is, fix up later)
   *   - host is less specific than an existing one (warn if it is, don't need to do anything special)
   */

  for(th=tg->hosts;th;th=th->next) {
    if(th->ip == (ip & th->mask)) {
      controlreply(sender, "This host (or part of it) is already covered in the given group.");
      return CMD_ERROR;
    }
  }

  if(th_getbyhostandmask(ip, mask)) {
    controlreply(sender, "This host already exists in another group with the same mask.");
    return CMD_ERROR;
  }

  /* this function will set both to NULL if it's equal, hence the check above */
  th_getsuperandsubsets(ip, mask, &superset, &subset);
  if(superset) {
    /* a superset exists for us, we will be more specific than one existing host */

    controlreply(sender, "Warning: this host already exists in another group, but this new host will override it as it has a smaller prefix.");
  }
  if(subset) {
    /* a subset of us exists, we will be less specific than some existing hosts */

    controlreply(sender, "Warning: this host already exists in at least one other group, the new host has a larger prefix and therefore will not override those hosts.");
  }
  if(superset || subset)
    controlreply(sender, "Adding anyway...");

  th = th_new(tg, host);
  if(!th) {
    controlreply(sender, "An error occured adding the host to the group.");
    return CMD_ERROR;
  }

  controlreply(sender, "Host added.");
  /* TODO: controlwall */

  return CMD_OK;
}

static int trusts_cmdtrustgroupadd(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *name, *contact, *comment, createdby[ACCOUNTLEN + 2];
  unsigned int howmany, maxperident, enforceident;
  time_t howlong;
  trustgroup *tg;

  if(cargc < 6)
    return CMD_USAGE;

  name = cargv[0];
  howmany = strtoul(cargv[1], NULL, 10);
  if(!howmany || (howmany > 50000)) {
    controlreply(sender, "Bad value maximum number of clients.");
    return CMD_ERROR;
  }

  howlong = durationtolong(cargv[2]);
  if((howlong <= 0) || (howlong > 365 * 86400 * 20)) {
    controlreply(sender, "Invalid duration supplied.");
    return CMD_ERROR;
  }

  maxperident = strtoul(cargv[3], NULL, 10);
  if(!howmany || (maxperident > 1000)) {
    controlreply(sender, "Bad value for max per ident.");
    return CMD_ERROR;
  }

  if(cargv[4][0] != '1' && cargv[4][0] != '0') {
    controlreply(sender, "Bad value for enforce ident (use 0 or 1).");
    return CMD_ERROR;
  }
  enforceident = cargv[4][0] == '1';

  contact = cargv[5];

  if(cargc < 7) {
    comment = "(no comment)";
  } else {
    comment = cargv[6];
  }

  /* don't allow #id or id forms */
  if((name[0] == '#') || strtoul(name, NULL, 10)) {
    controlreply(sender, "Invalid trustgroup name.");
    return CMD_ERROR;
  }

  tg = tg_strtotg(name);
  if(tg) {
    controlreply(sender, "A group with that name already exists");
    return CMD_ERROR;
  }

  snprintf(createdby, sizeof(createdby), "#%s", sender->authname);

  tg = tg_new(name, howmany, enforceident, maxperident, howlong + time(NULL), createdby, contact, comment);
  if(!tg) {
    controlreply(sender, "An error occured adding the trustgroup.");
    return CMD_ERROR;
  }

  controlreply(sender, "Group added.");
  /* TODO: controlwall */

  return CMD_OK;
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustlist", NO_OPER, 1, trusts_cmdtrustlist, "Usage: trustlist <#id|name|id>\nShows trust data for the specified trust group.");
  registercontrolhelpcmd("trustgroupadd", NO_OPER, 6, trusts_cmdtrustgroupadd, "Usage: trustgroupadd <name> <howmany> <howlong> <maxperident> <enforceident> <contact> ?comment?");
  registercontrolhelpcmd("trustadd", NO_OPER, 2, trusts_cmdtrustadd, "Usage: trustadd <#id|name|id> <host>");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustlist", trusts_cmdtrustlist);
  deregistercontrolcmd("trustgroupadd", trusts_cmdtrustgroupadd);
  deregistercontrolcmd("trustadd", trusts_cmdtrustadd);
}

void _init(void) {
  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if(trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  deregistercommands(0, NULL);
}
