#include <stdio.h>
#include <stdint.h>

#include "../core/error.h"
#include "trusts_migration.h"

DBAPIConn *tsdb;

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
  printf("id: %d, name: %s, trusted for: %u mode: %u maxperident: %u maxseen: %u expires: %jd lastseen: %jd lastmaxuserreset: %jd createdby: %s contact: %s comment: %s\n", id, name, trustedfor, mode, maxperident, maxseen, (intmax_t)expires, (intmax_t)lastseen, (intmax_t)lastmaxuserreset, createdby, contact, comment);
}

static void tm_host(trustmigration *tm, unsigned int id, char *host, unsigned int max, time_t lastseen) {
  printf("  id: %d, host: %s, max: %d, last seen: %jd\n", id, host, max, (intmax_t)lastseen);
}

static void tm_final(trustmigration *tm, int errcode) {
  t = NULL;

  if(errcode) {
    printf("error :(: %d\n", errcode);
  } else {
    printf("finished!\n");
  }
}
