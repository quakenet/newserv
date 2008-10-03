#include <stdio.h>
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "trusts.h"

int trusts_migration_start(TrustDBMigrationCallback, void *);
void trusts_migration_stop(void);
static void registercommands(int, void *);
static void deregistercommands(int, void *);

static void migrate_status(int errcode, void *tag) {
  long sender = (long)tag;
  nick *np = getnickbynumeric(sender);

  if(!np)
    return;

  if(!errcode || errcode == MIGRATION_LASTERROR) {
    if(!errcode) {
       controlreply(np, "Migration complete.");
       controlreply(np, "Attempting to reload database. . .");
    } else {
      controlreply(np, "An error occured after the database was unloaded, attempting reload. . .");
    }
    if(trusts_loaddb()) {
      controlreply(np, "Database reloaded successfully.");
    } else {
      controlreply(np, "An error occured, please reload the module manually.");
    }
  } else {
    controlreply(np, "Error %d occured during migration, commands reregistered.", errcode);
    registercommands(0, NULL);
  }
}

static int trusts_cmdmigrate(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int ret;

  /* iffy but temporary */
  ret = trusts_migration_start(migrate_status, (void *)(sender->numeric));
  if(!ret) {
    controlreply(sender, "Migration started, commands deregistered.");
    deregistercommands(0, NULL);
  } else {
    controlreply(sender, "Error %d starting migration.", ret);
  }

  return CMD_OK;
}

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

  controlreply(sender, "Name:            : %s", tg->name->content);
  controlreply(sender, "Trusted for      : %d", tg->trustedfor);
  controlreply(sender, "Currently using  : %d", tg->count);
  controlreply(sender, "Clients per user : %d (%senforcing ident)", tg->maxperident, tg->mode?"":"not ");
  controlreply(sender, "Contact:         : %s", tg->contact->content);
  controlreply(sender, "Expires in       : %s", (tg->expires>t)?longtoduration(tg->expires - t, 2):"(in the past)");
  controlreply(sender, "Last changed by  : %s", tg->createdby->content);
  controlreply(sender, "Comment:         : %s", tg->comment->content);
  controlreply(sender, "ID:              : %u", tg->id);
  controlreply(sender, "Last used        : %s", (tg->count>0)?"(now)":trusts_timetostr(tg->lastseen));
  controlreply(sender, "Max usage        : %d", tg->maxusage);
  controlreply(sender, "Last max reset   : %s", tg->lastmaxuserreset?trusts_timetostr(tg->lastmaxuserreset):"(never)");

  controlreply(sender, "Host                 Current    Max        Last seen");

  for(th=tg->hosts;th;th=th->next)
    controlreply(sender, " %-20s %-10d %-10d %s", trusts_cidr2str(th->ip, th->mask), th->count, th->maxusage, (th->count>0)?"(now)":trusts_timetostr(th->lastseen));

  controlreply(sender, "End of list.");

  return CMD_OK;
}

static int trusts_cmdtrustadd(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *host;
  uint32_t ip, mask;
  trusthost *th;

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

  th = th_getbyhost(ip);
  if(th) {
    if(mask == th->mask) {
      controlreply(sender, "This host already exists with the same mask.");
      return CMD_ERROR;
    }
    if(mask > th->mask) {
      /* this mask is a closer fit */

      controlreply(sender, "Warning: this host will override another (%s), as it has smaller prefix (group: %s).", trusts_cidr2str(th->ip, th->mask), th->group->name->content);
      controlreply(sender, "Adding anyway...");
    }
  } else {
    th = th_getsupersetbyhost(ip, mask);
    if(th) {
      controlreply(sender, "Warning: this host is already covered by a smaller prefix (%s), which will remain part of that group: %s", trusts_cidr2str(th->ip, th->mask), th->group->name->content);
      controlreply(sender, "Adding anyway...");
    }
  }

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

  registercontrolhelpcmd("trustmigrate", NO_DEVELOPER, 0, trusts_cmdmigrate, "Usage: trustmigrate\nCopies trust data from O and reloads the database.");
  registercontrolhelpcmd("trustlist", NO_OPER, 1, trusts_cmdtrustlist, "Usage: trustlist <#id|name|id>\nShows trust data for the specified trust group.");
  registercontrolhelpcmd("trustgroupadd", NO_OPER, 6, trusts_cmdtrustgroupadd, "Usage: trustgroupadd <name> <howmany> <howlong> <maxperident> <enforceident> <contact> ?comment?");
  registercontrolhelpcmd("trustadd", NO_OPER, 2, trusts_cmdtrustadd, "Usage: trustadd <#id|name|id> <host>");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustmigrate", trusts_cmdmigrate);
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

  trusts_migration_stop();

  deregistercommands(0, NULL);
}
