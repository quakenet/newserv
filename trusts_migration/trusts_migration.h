#ifndef __TRUSTS_H
#define __TRUSTS_H

#include "../dbapi2/dbapi2.h"
#include <time.h>

struct trustmigration;

typedef void (*TrustMigrationGroup)(struct trustmigration *, unsigned int, char *, unsigned int, unsigned int, unsigned int, unsigned int, time_t, time_t, time_t, char *, char *, char *);
typedef void (*TrustMigrationHost)(struct trustmigration *, unsigned int, char *, unsigned int, time_t);
typedef void (*TrustMigrationFini)(struct trustmigration *, int);

typedef struct trustmigration {
  int count, cur;
  void *schedule;

  TrustMigrationGroup group;
  TrustMigrationHost host;
  TrustMigrationFini fini;
} trustmigration;

trustmigration *trusts_migration_start(TrustMigrationGroup, TrustMigrationHost, TrustMigrationFini);
void trusts_migration_stop(trustmigration *);

#define CONTACTLEN 100
#define COMMENTLEN 300

#endif
