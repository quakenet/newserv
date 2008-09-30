#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "trusts.h"

extern DBAPIConn *trustsdb;
static trustmigration *migration;

static void tm_group(void *, unsigned int, char *, unsigned int, unsigned int, unsigned int, unsigned int, time_t, time_t, time_t, char *, char *, char *);
static void tm_host(void *tm, unsigned int id, char *host, unsigned int max, time_t lastseen);
static void tm_final(void *, int);

trustmigration *migration_start(TrustMigrationGroup, TrustMigrationHost, TrustMigrationFini, void *);
void migration_stop(trustmigration *);

struct callbackdata {
  void *tag;
  TrustDBMigrationCallback callback;
};

int trusts_migration_start(TrustDBMigrationCallback callback, void *tag) {
  struct callbackdata *cbd;

  if(migration)
    return 0;

  if(callback) {
    cbd = malloc(sizeof(struct callbackdata));
    if(!cbd)
      return 0;

    cbd->callback = callback;
    cbd->tag = tag;
  } else {
    cbd = NULL;
  }

  trustsdb->squery(trustsdb, "DELETE FROM ?", "T", "groups");
  trustsdb->squery(trustsdb, "DELETE FROM ?", "T", "hosts");

  migration = migration_start(tm_group, tm_host, tm_final, cbd);
  if(!migration) {
    free(cbd);
    return 0;
  }

  return 1;
}

void trusts_migration_stop(void) {
  if(!migration)
    return;

  migration_stop(migration);
}

static void tm_group(void *tag, unsigned int id, char *name, unsigned int trustedfor, unsigned int mode, unsigned int maxperident, unsigned int maxseen, time_t expires, time_t lastseen, time_t lastmaxuserreset, char *createdby, char *contact, char *comment) {
  if(id % 25 == 0)
    Error("trusts", ERR_INFO, "Migration currently at id: %d", id);

  trustsdb->squery(trustsdb, 
    "INSERT INTO ? (id, name, trustedfor, mode, maxperident, maxseen, expires, lastseen, lastmaxuserreset, createdby, contact, comment) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
    "Tusuuuutttsss", "groups", id, name, trustedfor, mode, maxperident, maxseen, expires, lastseen, lastmaxuserreset, createdby, contact, comment
  );
}

static void tm_host(void *tag, unsigned int id, char *host, unsigned int max, time_t lastseen) {
  trustsdb->squery(trustsdb, 
    "INSERT INTO ? (groupid, host, max, lastseen) VALUES (?, ?, ?, ?)",
    "Tusut", "hosts", id, host, max, lastseen
  );
}

static void tm_final(void *tag, int errcode) {
  struct callbackdata *cbd = tag;
  migration = NULL;

  if(errcode) {
    Error("trusts", ERR_ERROR, "Migration error: %d", errcode);
  } else {
    Error("trusts", ERR_INFO, "Migration completed.");
  }

  if(cbd) {
    cbd->callback(errcode, tag);
    free(cbd);
  }
}
