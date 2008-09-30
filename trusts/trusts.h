#ifndef __TRUSTS_H
#define __TRUSTS_H

#include <time.h>
#include "../lib/sstring.h"

#define MIGRATION_STOPPED -1

#define CONTACTLEN 100
#define COMMENTLEN 300
#define TRUSTNAMELEN 100
#define TRUSTHOSTLEN 100

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

typedef struct trusthost {
  sstring *host;
  unsigned int maxseen;
  time_t lastseen;

  struct trusthost *next;
} trusthost;

typedef struct trustgroup {
  unsigned int id;

  sstring *name;
  unsigned int trustedfor;
  int mode;
  unsigned int maxperident;
  unsigned int maxseen;
  time_t expires;
  time_t lastseen;
  time_t lastmaxuserreset;
  sstring *createdby, *contact, *comment;

  trusthost *hosts;

  struct trustgroup *next;
} trustgroup;

void trusts_reloaddb(void);

extern int trustsdbloaded;
extern trustgroup *tglist;

#endif
