#include "../control/control.h"
#include "../nick/nick.h"
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
}

static void deregistercommands(void) {
  if(!commandsregistered)
    return;

  deregistercontrolcmd("trustmigrate", trusts_cmdmigrate);
  commandsregistered = 0;
}
