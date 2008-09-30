#include "../control/control.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "trusts.h"

int trusts_migration_start(TrustDBMigrationCallback, void *);
void trusts_migration_stop(void);
static void registercommands(void);
static void deregistercommands(void);

static int commandsregistered;

static void migrate_status(int errcode, void *tag) {
  long sender = (long)tag;
  nick *np = getnickbynumeric(sender);

  if(!np)
    return;

  if(!errcode) {
    controlreply(np, "Migration complete.");
    trusts_reloaddb();
  } else {
    controlreply(np, "Error %d occured during migration.", errcode);
    registercommands();
  }
}

static int trusts_cmdmigrate(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int ret;

  /* iffy but temporary */
  ret = trusts_migration_start(migrate_status, (void *)(sender->numeric));
  if(!ret) {
    controlreply(sender, "Migration started.");
    deregistercommands();
  } else {
    controlreply(sender, "Error %d starting migration.", ret);
  }

  return CMD_OK;
}

static trustgroup *tg_strtotg(char *name) {
  unsigned long id;
  trustgroup *tg;

  /* legacy format */
  if(name[0] == '#') {
    id = strtoul(&name[1], NULL, 10);
    if(id == ULONG_MAX)
      return NULL;

    for(tg=tglist;tg;tg=tg->next)
      if(tg->id == id)
        return tg;
  }

  for(tg=tglist;tg;tg=tg->next)
    if(tg->name && !match(name, tg->name->content))
      return tg;

  id = strtoul(name, NULL, 10);
  if(id == ULONG_MAX)
    return NULL;

  /* legacy format */
  for(tg=tglist;tg;tg=tg->next)
    if(tg->id == id)
      return tg;

  return NULL;
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

  controlreply(sender, "Name:            : %s", tg->name?tg->name->content:"(unknown)");
  controlreply(sender, "Trusted for      : %d", tg->trustedfor);
  controlreply(sender, "Clients per user : %d (%senforcing ident)", tg->maxperident, tg->mode?"":"not ");
  controlreply(sender, "Contact:         : %s", tg->contact?tg->contact->content:"(unknown)");
  controlreply(sender, "Expires in       : %s", (tg->expires>t)?longtoduration(tg->expires - t, 2):"(in the past)");
  controlreply(sender, "Last changed by  : %s", tg->createdby?tg->createdby->content:"(unknown)");
  controlreply(sender, "Comment:         : %s", tg->comment?tg->comment->content:"(unknown)");
  controlreply(sender, "ID:              : %u", tg->id);
  controlreply(sender, "Last used        : %s", (tg->lastseen==t)?"(now)":trusts_timetostr(tg->lastseen));
  controlreply(sender, "Max usage        : %d", tg->maxseen);
  controlreply(sender, "Last max reset   : %s", tg->lastmaxuserreset?trusts_timetostr(tg->lastmaxuserreset):"(never)");

  controlreply(sender, "Host                 Max        Last seen");

  for(th=tg->hosts;th;th=th->next)
    controlreply(sender, " %-20s %-10d %s", th->host?th->host->content:"??", th->maxseen, (th->lastseen==t)?"(now)":trusts_timetostr(th->lastseen));

  controlreply(sender, "End of list.");

  return CMD_OK;
}

static void dbloaded(int hooknum, void *arg) {
  registercommands();
}

void _init(void) {
  registerhook(HOOK_TRUSTS_DB_LOADED, dbloaded);

  if(trustsdbloaded)
    registercommands();
}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_DB_LOADED, dbloaded);
  deregistercommands();

  trusts_migration_stop();
}

static void registercommands(void) {
  if(!trustsdbloaded || commandsregistered)
    return;

  commandsregistered = 1;

  registercontrolhelpcmd("trustmigrate", NO_OPER, 0, trusts_cmdmigrate, "Usage: trustmigrate\nCopies trust data from O and reloads the database.");
  registercontrolhelpcmd("trustlist", NO_OPER, 1, trusts_cmdtrustlist, "Usage: trustlist <#id|name|id>\nShows trust data for the specified trust group.");
}

static void deregistercommands(void) {
  if(!commandsregistered)
    return;

  deregistercontrolcmd("trustmigrate", trusts_cmdmigrate);
  deregistercontrolcmd("trustlist", trusts_cmdtrustlist);
  commandsregistered = 0;
}
