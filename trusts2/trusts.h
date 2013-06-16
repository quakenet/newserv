/*

 * trust.h:
 *  Top level data structures and function prototypes for the
 *  trusts service module
 */

#ifndef __TRUST_H
#define __TRUST_H

#include <string.h>

#include "../lib/irc_ipv6.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"
#include "../control/control.h"
#include "../patricia/patricia.h"
#include "../dbapi/dbapi.h"
#include "../patricianick/patricianick.h"
#include "../nick/nick.h"

#define TRUSTS_HASH_GROUPSIZE 500
#define TRUSTS_HASH_HOSTSIZE 1000
#define TRUSTS_HASH_IDENTSIZE 50

#define TRUSTS_MAXGROUPNAMELEN 20

/* node extensions */
extern int tgh_ext;
extern int tgb_ext;

/* nick extensions */
extern int tgn_ext;

extern int removeusers;
extern int trusts_loaded;

typedef struct trusthost_s {
        /* Details */
        unsigned long id;
        patricia_node_t* node;
        time_t startdate;
        time_t lastused;
        time_t expire;
        unsigned long maxused;

        /* Trust group */
        struct trustgroup_s* trustgroup;

        time_t created;
        time_t modified;

        /* Hash table */
        struct trusthost_s* nextbyid;
        struct trusthost_s* nextbygroupid;
} trusthost_t;

typedef struct trustgroup_s {
        /* Details */
        unsigned long id;

        unsigned long maxusage, currenton;
        unsigned long maxclones, maxperident;
        unsigned short maxperip;
        int enforceident;
        time_t startdate,lastused, expire;
        unsigned long ownerid;

        time_t created;
        time_t modified;

        /* Hash table */
        struct trustgroup_s* nextbyid;
} trustgroup_t;

typedef struct trustgroupidentcount_s {
        /* Ident */
        sstring *ident;

        /* Trust group */
        struct trustgroup_s* trustgroup;

        /* Counter */
        unsigned long currenton;

        /* Hash table */
        struct trustgroupidentcount_s* next;
} trustgroupidentcount_t;

/* trusts hash tables */
extern trustgroup_t *trustgroupidtable[TRUSTS_HASH_GROUPSIZE];
extern trustgroup_t *trustgroupnametable[TRUSTS_HASH_GROUPSIZE];
extern trusthost_t *trusthostidtable[TRUSTS_HASH_HOSTSIZE];
extern trusthost_t *trusthostgroupidtable[TRUSTS_HASH_HOSTSIZE];
extern trustgroupidentcount_t *trustgroupidentcounttable[TRUSTS_HASH_IDENTSIZE];

#define trusts_gettrustgroupidhash(x) ((x)%TRUSTS_HASH_GROUPSIZE)
#define trusts_gettrustgroupnamehash(x) ((crc32i(x))%TRUSTS_HASH_GROUPSIZE)
#define trusts_gettrusthostidhash(x) ((x)%TRUSTS_HASH_HOSTSIZE)
#define trusts_gettrusthostgroupidhash(x) ((x)%TRUSTS_HASH_HOSTSIZE)
#define trusts_gettrustgroupidenthash(x) ((crc32i(x))%TRUSTS_HASH_IDENTSIZE)

/* trusts_hash.c */
void trusts_hash_init();
void trusts_hash_fini();

void trusts_addtrusthosttohash(trusthost_t *newhost);
void trusts_removetrusthostfromhash(trusthost_t *t);

void trusts_addtrustgrouptohash(trustgroup_t *newgroup);
void trusts_removetrustgroupfromhash(trustgroup_t *t);

void trusts_addtrustgroupidenttohash(trustgroupidentcount_t *newident);
void trusts_removetrustgroupidentfromhash(trustgroupidentcount_t *t);

trustgroup_t* findtrustgroupbyid(int id);
trustgroup_t* findtrustgroupbyownerid(int ownerid);

trustgroupidentcount_t* findtrustgroupcountbyident(char *ident, trustgroup_t *t);

trustgroupidentcount_t *getnewtrustgroupidentcount(trustgroup_t *tg, char *ident);

extern unsigned long trusts_lasttrustgroupid, trusts_lasttrusthostid;

/* trusts alloc */
trustgroup_t *newtrustgroup();
void freetrustgroup (trustgroup_t *trustgroup);
trusthost_t *newtrusthost();
void freetrusthost (trusthost_t *trusthost);
trustgroupidentcount_t *newtrustgroupidentcount();
void freetrustgroupidentcount (trustgroupidentcount_t *trustgroupidentcount);

/* trusts db */
int trusts_load_db(void);
void trusts_create_tables(void);
void trusts_cleanup_db(void);
void trusts_loadtrustgroups(DBConn *dbconn, void *arg);
void trusts_loadtrustgroupsmax(DBConn *dbconn, void *arg);
void trusts_loadtrusthosts(DBConn *dbconn, void *arg);
void trusts_loadtrusthostsmax(DBConn *dbconn, void *arg);
void trustsdb_addtrustgroup(trustgroup_t *t);
void trustsdb_updatetrustgroup(trustgroup_t *t);
void trustsdb_deletetrustgroup(trustgroup_t *t);
void trustsdb_addtrusthost(trusthost_t *th);
void trustsdb_updatetrusthost(trusthost_t *th);
void trustsdb_deletetrusthost(trusthost_t *th);
void trustsdb_logmessage(trustgroup_t *tg, unsigned long userid, int type, char *message);

/* trusts handlers */
void trusts_hook_newuser(int hook, void *arg);
void trusts_hook_lostuser(int hook, void *arg);

/* trusts groups */

trustgroup_t *createtrustgroupfromdb(unsigned long id, unsigned long maxusage, unsigned long maxclones, unsigned long maxperident, unsigned short maxperip, int enforceident, time_t startdate, time_t lastused, time_t expire, unsigned long ownerid, time_t created, time_t modified);
trustgroup_t *createtrustgroup(unsigned long id, unsigned long maxclones, unsigned long maxperident, unsigned short maxperip, int enforceident, time_t expire, unsigned long ownerid);
void trustgroup_expire (trustgroup_t *tg);
void trustgroup_free(trustgroup_t* t);

/* trusts hosts */
trusthost_t *createtrusthostfromdb(unsigned long id, patricia_node_t* node, time_t startdate, time_t lastused, time_t expire, unsigned long maxused, trustgroup_t* trustgroup, time_t created, time_t modified);
trusthost_t *createtrusthost(unsigned long id, patricia_node_t* node, time_t expire, trustgroup_t *trustgroup);
void trusthost_free(trusthost_t* t);
void trusthost_addcounters(trusthost_t* tgh);
trusthost_t* trusthostadd(patricia_node_t *node, trustgroup_t* tg, time_t expire);
void trusthost_expire ( trusthost_t *th);

/* trusts idents */
void increment_ident_count(nick *np, trustgroup_t *tg);
void decrement_ident_count(nick *np, trustgroup_t *tg);

/* trusts */
void decrement_trust_ipnode(patricia_node_t *node);
void increment_trust_ipnode(patricia_node_t *node);
int trusts_ignore_np(nick *np);

/* trusts cmds */
void trusts_cmdinit();
void trusts_cmdfini();

int trust_groupadd(void *source, int cargc, char **cargv);
int trust_groupmodify(void *source, int cargc, char **cargv);
int trust_groupdel(void *source, int cargc, char **cargv);
int trust_del(void *source, int cargc, char **cargv);
int trust_add(void *source, int cargc, char **cargv);
int trust_comment(void *source, int cargc, char **cargv);

int trust_stats(void *source, int cargc, char **cargv);
int trust_dump(void *source, int cargc, char **cargv);
int trust_dotrustlog(void *source, int cargc, char **cargv);

#endif

