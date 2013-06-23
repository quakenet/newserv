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
#define CREATEDBYLEN NICKLEN + 1
#define TRUSTLOGLEN 200

#define MAXTGEXTS 5

#define MAXTRUSTEDFOR 5000
#define MAXDURATION 365 * 86400 * 20
#define MAXPERIDENT 1000
#define MAXPERNODE 1000

#define TABLES_REGULAR 0
#define TABLES_MIGRATION 1
#define TABLES_REPLICATION 2

#define CLEANUP_TH_INACTIVE 60

#define POLICY_GLINE_DURATION 18

#define TRUST_ENFORCE_IDENT 1 /* This must be 1 for compatibility with O. */
#define TRUST_NO_CLEANUP 2
#define TRUST_PROTECTED 4

#define TRUST_MIN_UNPRIVILEGED_BITS_IPV4 (96 + 32)
#define TRUST_MIN_UNPRIVILEGED_BITS_IPV6 32

#define TRUST_MIN_UNPRIVILEGED_NODEBITS_IPV4 (96 + 24)
#define TRUST_MIN_UNPRIVILEGED_NODEBITS_IPV6 48

struct trustmigration;

struct trusthost;

typedef struct trusthost {
  unsigned int id;

  struct irc_in_addr ip;
  unsigned char bits;
  unsigned int maxusage;
  time_t created;
  time_t lastseen;

  nick *users;
  struct trustgroup *group;

  unsigned int count;

  int maxpernode;
  int nodebits;

  struct trusthost *parent, *children;
  unsigned int marker;

  struct trusthost *nextbychild;
  struct trusthost *next;
} trusthost;

typedef struct trustgroup {
  unsigned int id;

  sstring *name;
  unsigned int trustedfor;
  int flags;
  unsigned int maxperident;
  unsigned int maxusage;
  time_t expires;
  time_t lastseen;
  time_t lastmaxusereset;
  sstring *createdby, *contact, *comment;

  trusthost *hosts;
  unsigned int count;

  unsigned int marker;

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
int trusts_fullyonline(void);

/* formats.c */
char *trusts_timetostr(time_t);
char *trusts_cidr2str(struct irc_in_addr *ip, unsigned char);
char *dumpth(trusthost *, int);
char *dumptg(trustgroup *, int);
int parseth(char *, trusthost *, unsigned int *, int);
int parsetg(char *, trustgroup *, int);
char *rtrim(char *);

/* data.c */
extern trustgroup *tglist;
trustgroup *tg_getbyid(unsigned int);
void th_free(trusthost *);
trusthost *th_add(trusthost *);
void tg_free(trustgroup *, int);
trustgroup *tg_add(trustgroup *);
trusthost *th_getbyhost(struct irc_in_addr *);
trusthost *th_getbyhostandmask(struct irc_in_addr *, uint32_t);
trusthost *th_getsmallestsupersetbyhost(struct irc_in_addr *, uint32_t);
trustgroup *tg_strtotg(char *);
void th_adjusthosts(trusthost *th, trusthost *, trusthost *);
void th_getsuperandsubsets(struct irc_in_addr *, uint32_t, trusthost **, trusthost **);
trusthost *th_getsubsetbyhost(struct irc_in_addr *ip, uint32_t mask);
trusthost *th_getnextsubsetbyhost(trusthost *th, struct irc_in_addr *ip, uint32_t mask);
void th_linktree(void);
unsigned int nexttgmarker(void);
unsigned int nextthmarker(void);
trusthost *th_getbyid(unsigned int);
int tg_modify(trustgroup *, trustgroup *);
int th_modify(trusthost *, trusthost *);

/* migration.c */
typedef void (*TrustMigrationGroup)(void *, trustgroup *);
typedef void (*TrustMigrationHost)(void *, trusthost *, unsigned int);
typedef void (*TrustMigrationFini)(void *, int);

/* trusts_db.c */
extern int trustsdbloaded;
int trusts_loaddb(void);
void trusts_closedb(int);
trustgroup *tg_new(trustgroup *);
trusthost *th_new(trustgroup *, char *);
void trustsdb_insertth(char *, trusthost *, unsigned int);
void trustsdb_inserttg(char *, trustgroup *);
trustgroup *tg_copy(trustgroup *);
trusthost *th_copy(trusthost *);
void tg_update(trustgroup *);
void tg_delete(trustgroup *);
void th_update(trusthost *);
void th_delete(trusthost *);
void trustlog(trustgroup *tg, const char *user, const char *format, ...);
void trustlogspewid(nick *np, unsigned int groupid, unsigned int limit);
void trustlogspewname(nick *np, const char *groupname, unsigned int limit);
void trustloggrep(nick *np, const char *pattern, unsigned int limit);

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

/* events.c */
void trusts_newnick(nick *, int);
void trusts_lostnick(nick *, int);

/* trusts_api.c */
int istrusted(nick *);
unsigned char getnodebits(struct irc_in_addr *ip);

#endif
