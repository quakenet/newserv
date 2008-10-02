#ifndef __TRUSTS_H
#define __TRUSTS_H

#include <time.h>
#include <stdint.h>
#include "../nick/nick.h"
#include "../lib/sstring.h"

#define MIGRATION_STOPPED -1
#define MIGRATION_LASTERROR -2

#define CONTACTLEN 100
#define COMMENTLEN 300
#define TRUSTNAMELEN 100
#define TRUSTHOSTLEN 100
#define MAXTGEXTS 5

struct trustmigration;

struct trusthost;

typedef struct trusthost {
  unsigned int id;

  uint32_t ip, mask;
  unsigned int maxusage;
  time_t lastseen;

  nick *users;
  struct trustgroup *group;

  unsigned int count;

  struct trusthost *next;
} trusthost;

typedef struct trustgroup {
  unsigned int id;

  sstring *name;
  unsigned int trustedfor;
  int mode;
  unsigned int maxperident;
  unsigned int maxusage;
  time_t expires;
  time_t lastseen;
  time_t lastmaxuserreset;
  sstring *createdby, *contact, *comment;

  trusthost *hosts;
  unsigned int count;

  struct trustgroup *next;

  void *exts[MAXTGEXTS];
} trustgroup;

#define nextbytrust(x) (nick *)((x)->exts[trusts_nextuserext])
#define gettrusthost(x) (trusthost *)((x)->exts[trusts_thext])
#define setnextbytrust(x, y) (x)->exts[trusts_nextuserext] = (y)
#define settrusthost(x, y) (x)->exts[trusts_thext] = (y)

/* trusts.c */
extern int trusts_thext, trusts_nextuserext;
int findtgext(const char *);
int registertgext(const char *);
void releasetgext(int);

/* db.c */
extern int trustsdbloaded;
int trusts_loaddb(void);
void trusts_closedb(int);

/* formats.c */
char *trusts_timetostr(time_t);
int trusts_parsecidr(const char *, uint32_t *, short *);
int trusts_str2cidr(const char *, uint32_t *, uint32_t *);
char *trusts_cidr2str(uint32_t, uint32_t);

/* data.c */
extern trustgroup *tglist;
trustgroup *tg_getbyid(unsigned int);
void th_free(trusthost *);
int th_add(trustgroup *, unsigned int, char *, unsigned int, time_t);
void tg_free(trustgroup *);
int tg_add(unsigned int, char *, unsigned int, int, unsigned int, unsigned int, time_t, time_t, time_t, char *, char *, char *);
trusthost *th_getbyhost(uint32_t host);
trustgroup *tg_strtotg(char *name);

/* migration.c */
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

/* db-migration.c */

typedef void (*TrustDBMigrationCallback)(int, void *);

#endif
