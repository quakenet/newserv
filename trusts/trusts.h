#ifndef __TRUSTS_H
#define __TRUSTS_H

#include <time.h>
#include "../lib/sstring.h"

#define MIGRATION_STOPPED -1

#define CONTACTLEN 100
#define COMMENTLEN 300

struct trustmigration;

typedef void (*TrustMigrationGroup)(void *, unsigned int, char *, unsigned int, unsigned int, unsigned int, unsigned int, time_t, time_t, time_t, char *, char *, char *);
typedef void (*TrustMigrationHost)(void *, unsigned int, char *, unsigned int, time_t);
typedef void (*TrustMigrationFini)(void *, int);

typedef struct trustmigration {
  int count, cur;
  void *schedule;
  void *tag;

  TrustMigrationGroup group;
  TrustMigrationHost host;
  TrustMigrationFini fini;
} trustmigration;

typedef void (*TrustDBMigrationCallback)(int, void *);

typedef struct trustgroup {
  unsigned int id;

  sstring *name;
  unsigned int trustedfor;
  int mode;
  unsigned int maxperident;
  time_t expires;
  time_t latseeen;
  time_t lastmaxuserreset;

  sstring *createdby, *contact, *comment;

  struct trustgroup *next;
} trustgroup;

typedef struct trusthost {
  unsigned int id;

  sstring *mask;
  unsigned int max;
  time_t lastseen;

  struct trusthost *next;
} trusthost;

#endif
