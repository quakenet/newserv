#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "trusts.h"

extern DBAPIConn *trustsdb;
static trustmigration *migration;

static void tm_group(void *, unsigned int, char *, unsigned int, unsigned int, unsigned int, unsigned int, time_t, time_t, time_t, char *, char *, char *);
static void tm_host(void *tm, unsigned int id, char *host, unsigned int max, time_t lastseen);
static void tm_final(void *, int);

trustmigration *migration_start(TrustMigrationGroup, TrustMigrationHost, TrustMigrationFini, void *);
void migration_stop(trustmigration *);
void createtrusttables(int migration);

struct callbackdata {
  void *tag;
  TrustDBMigrationCallback callback;
};

int trusts_migration_start(TrustDBMigrationCallback callback, void *tag) {
  struct callbackdata *cbd;

  if(migration)
    return 1;

  if(callback) {
    cbd = malloc(sizeof(struct callbackdata));
    if(!cbd)
      return 2;

    cbd->callback = callback;
    cbd->tag = tag;
  } else {
    cbd = NULL;
  }

  createtrusttables(1);
  trustsdb->squery(trustsdb, "DELETE FROM ?", "T", "migration_groups");
  trustsdb->squery(trustsdb, "DELETE FROM ?", "T", "migration_hosts");

  migration = migration_start(tm_group, tm_host, tm_final, cbd);
  if(!migration) {
    free(cbd);
    return 3;
  }

  return 0;
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
    "Tusuuuutttsss", "migration_groups", id, name, trustedfor, mode, maxperident, maxseen, expires, lastseen, lastmaxuserreset, createdby, contact, comment
  );
}

static void tm_host(void *tag, unsigned int id, char *host, unsigned int max, time_t lastseen) {
  trustsdb->squery(trustsdb, 
    "INSERT INTO ? (groupid, host, max, lastseen) VALUES (?, ?, ?, ?)",
    "Tusut", "migration_hosts", id, host, max, lastseen
  );
}

static void tm_complete(const DBAPIResult *r, void *tag) {
  struct callbackdata *cbd = tag;
  int errcode = 0;

  if(!r) {
    errcode = MIGRATION_STOPPED;
  } else {
    if(!r->success) {
      Error("trusts", ERR_ERROR, "A error occured executing the rename table query.");
      errcode = 100;
    } else {
      Error("trusts", ERR_INFO, "Migration table copying complete.");
    }
    r->clear(r);
  }

  if(cbd) {
    cbd->callback(errcode, cbd->tag);
    free(cbd);
  }
}

static void tm_final(void *tag, int errcode) {
  struct callbackdata *cbd = tag;
  migration = NULL;

  if(errcode) {
    Error("trusts", ERR_ERROR, "Migration error: %d", errcode);
    if(cbd) {
      cbd->callback(errcode, cbd->tag);
      free(cbd);
    }
  } else {
    Error("trusts", ERR_INFO, "Migration completed, copying tables...");
/*
    trustsdb->query(trustsdb, cbd?tm_complete:NULL, cbd,
                    "BEGIN TRANSACTION; DROP TABLE ?; ALTER TABLE ? RENAME TO ?; DROP TABLE ?; ALTER TABLE ? RENAME TO ?; COMMIT;",
                    "TTsTTs", "groups", "migration_groups", "groups", "hosts", "migration_hosts", "hosts");
*/
/*
    trustsdb->query(trustsdb, cbd?tm_complete:NULL, cbd,
                    "BEGIN TRANSACTION; DELETE FROM ?; INSERT INTO ? SELECT * FROM ?; DELETE FROM ?; INSERT INTO ? SELECT * FROM ?; COMMIT;",
                    "TTTTTT", "groups", "groups", "migration_groups", "hosts", "hosts", "migration_hosts");
*/
/*
    trustsdb->query(trustsdb, cbd?tm_complete:NULL, cbd,
                    "DELETE FROM ?; INSERT INTO ? SELECT * FROM ?; DELETE FROM ?; INSERT INTO ? SELECT * FROM ?;",
                    "TTTTTT", "groups", "groups", "migration_groups", "hosts", "hosts", "migration_hosts");
*/
    trustsdb->squery(trustsdb, "BEGIN TRANSACTION", "");
    trustsdb->squery(trustsdb, "DROP TABLE ?", "T", "groups");
    trustsdb->squery(trustsdb, "ALTER TABLE ? RENAME TO ?", "Ts", "migration_groups", "groups");
    trustsdb->squery(trustsdb, "DROP TABLE ?", "T", "hosts");
    trustsdb->squery(trustsdb, "ALTER TABLE ? RENAME TO ?", "Ts", "migration_hosts", "hosts");
    trustsdb->query(trustsdb, tm_complete, cbd, "COMMIT", "");
  }
}
