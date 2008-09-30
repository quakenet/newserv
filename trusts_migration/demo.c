#include "../core/error.h"
#include "../nick/nick.h"
#include "trusts_migration.h"

static DBAPIConn *tsdb;
static trustmigration *t;

static void tm_group(trustmigration *, unsigned int, char *, unsigned int, unsigned int, unsigned int, unsigned int, time_t, time_t, time_t, char *, char *, char *);
static void tm_host(trustmigration *tm, unsigned int id, char *host, unsigned int max, time_t lastseen);
static void tm_final(trustmigration *, int);

void _init(void) {
  tsdb = dbapi2open(NULL, "trusts_migration");
  if(!tsdb) {
    Error("trusts", ERR_WARNING, "Unable to connect to db -- not loaded.");
    return;
  }

  tsdb->createtable(tsdb, NULL, NULL,
    "CREATE TABLE ? (id INT PRIMARY KEY, name VARCHAR(100), trustedfor INT, mode INT, maxperident INT, maxseen INT, expires INT, lastseen INT, lastmaxuserreset INT, createdby VARCHAR(?), contact VARCHAR(?), comment VARCHAR(?))",
    "Tddd", "groups", NICKLEN, CONTACTLEN, COMMENTLEN
  );
  tsdb->createtable(tsdb, NULL, NULL, "CREATE TABLE ? (groupid INT, host VARCHAR(100), max INT, lastseen INT, PRIMARY KEY (groupid, host))", "T", "hosts");

  tsdb->squery(tsdb, "DELETE FROM ?", "T", "groups");
  tsdb->squery(tsdb, "DELETE FROM ?", "T", "hosts");

  t = trusts_migration_start(tm_group, tm_host, tm_final);
}

void _fini(void) {
  if(tsdb) {
    tsdb->close(tsdb);
    tsdb = NULL;
  } else {
    return;
  }

  if(t)
    trusts_migration_stop(t);
}

static void tm_group(trustmigration *tm, unsigned int id, char *name, unsigned int trustedfor, unsigned int mode, unsigned int maxperident, unsigned int maxseen, time_t expires, time_t lastseen, time_t lastmaxuserreset, char *createdby, char *contact, char *comment) {
  if(id % 25 == 0)
    Error("trusts_migration", ERR_INFO, "Currently at id: %d", id);

  tsdb->squery(tsdb, 
    "INSERT INTO ? (id, name, trustedfor, mode, maxperident, maxseen, expires, lastseen, lastmaxuserreset, createdby, contact, comment) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
    "Tusuuuutttsss", "groups", id, name, trustedfor, mode, maxperident, maxseen, expires, lastseen, lastmaxuserreset, createdby, contact, comment
  );
}

static void tm_host(trustmigration *tm, unsigned int id, char *host, unsigned int max, time_t lastseen) {
  tsdb->squery(tsdb, 
    "INSERT INTO ? (groupid, host, max, lastseen) VALUES (?, ?, ?, ?)",
    "Tusut", "hosts", id, host, max, lastseen
  );
}

static void tm_final(trustmigration *tm, int errcode) {
  t = NULL;

  if(errcode) {
    Error("trusts_migration", ERR_ERROR, "Error: %d", errcode);
  } else {
    Error("trusts_migration", ERR_INFO, "Operation completed.");
  }
}
