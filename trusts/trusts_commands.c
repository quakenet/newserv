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

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustmigrate", NO_DEVELOPER, 0, trusts_cmdmigrate, "Usage: trustmigrate\nCopies trust data from O and reloads the database.");
  registercontrolhelpcmd("trustlist", NO_OPER, 1, trusts_cmdtrustlist, "Usage: trustlist <#id|name|id>\nShows trust data for the specified trust group.");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustmigrate", trusts_cmdmigrate);
  deregistercontrolcmd("trustlist", trusts_cmdtrustlist);
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
