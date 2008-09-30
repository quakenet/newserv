#include "../core/error.h"
#include "trusts.h"

trustgroup *tglist;

int trusts_loaddb(void);
void trusts_closedb(void);
void trusts_reloaddb(void);
int trusts_migration_start(TrustDBMigrationCallback, void *);
void trusts_migration_stop(void);

void _init(void) {
  if(!trusts_loaddb())
    return;

  trusts_migration_start(NULL, NULL);
}

void _fini(void) {
  trusts_migration_stop();
  trusts_closedb();
}
