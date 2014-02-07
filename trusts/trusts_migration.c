#include "../lib/version.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../core/config.h"
#include "trusts.h"

MODULE_VERSION("");

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

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustmigrate", NO_DEVELOPER, 0, trusts_cmdmigrate, "Usage: trustmigrate\nCopies trust data from O and reloads the database.");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustmigrate", trusts_cmdmigrate);
}

static int loaded = 0;
void _init(void) {
  sstring *m;

  m = getconfigitem("trusts", "master");
  if(!m || (atoi(m->content) != 1)) {
    Error("trusts_migrationr", ERR_ERROR, "Not a master server, not loaded.");
    return;
  }

  loaded = 1;

  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if(trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  if(!loaded)
    return;

  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  trusts_migration_stop();

  deregistercommands(0, NULL);
}
