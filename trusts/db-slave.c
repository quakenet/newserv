#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "trusts.h"

extern DBAPIConn *trustsdb;

void createtrusttables(int);
void trusts_replication_complete(int);

void trusts_replication_createtables(void) {
  createtrusttables(TABLES_REPLICATION);
  trustsdb->squery(trustsdb, "DELETE FROM ?", "T", "replication_groups");
  trustsdb->squery(trustsdb, "DELETE FROM ?", "T", "replication_hosts");
}

static void tr_complete(const DBAPIResult *r, void *tag) {
  if(!r) {
    trusts_replication_complete(1);
  } else {
    if(!r->success) {
      Error("trusts_slave", ERR_ERROR, "A error occured executing the rename table query.");
      trusts_replication_complete(2);
    } else {
      Error("trusts_slave", ERR_INFO, "Migration table copying complete.");
      trusts_replication_complete(0);
    }
    r->clear(r);
  }
}

void trusts_replication_swap(void) {
  trusts_closedb(0);

  Error("trusts_slave", ERR_INFO, "Copying tables...");

  trustsdb->squery(trustsdb, "BEGIN TRANSACTION", "");
  trustsdb->squery(trustsdb, "DROP TABLE ?", "T", "groups");
  trustsdb->squery(trustsdb, "ALTER TABLE ? RENAME TO groups", "T", "replication_groups");
  trustsdb->squery(trustsdb, "DROP TABLE ?", "T", "hosts");
  trustsdb->squery(trustsdb, "ALTER TABLE ? RENAME TO hosts", "T", "replication_hosts");
  trustsdb->query(trustsdb, tr_complete, NULL, "COMMIT", "");
}
