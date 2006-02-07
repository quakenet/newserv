#ifndef __trojanscan_H
#define __trojanscan_H

#include "../core/config.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../lib/strlfunc.h"
#include "../localuser/localuserchannel.h"

#include <assert.h>
#include <mysql/mysql.h>
#include <mysql/mysql_version.h>
#include <pcre.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#define TROJANSCAN_VERSION "2.58"

#define TROJANSCAN_CLONE_MAX        150
#define TROJANSCAN_WATCHCLONE_MAX   100
#define TROJANSCAN_CLONE_TOTAL TROJANSCAN_CLONE_MAX + TROJANSCAN_WATCHCLONE_MAX

#define TROJANSCAN_POOLSIZE 1000
#define TROJANSCAN_MINPOOLSIZE 500 /* 500 */

#define TROJANSCAN_MINIMUM_HOSTS_BEFORE_POOL 5000 /* 500 */

#define TROJANSCAN_DEFAULT_MAXCHANS 750
#define TROJANSCAN_DEFAULT_CYCLETIME 800

#define TROJANSCAN_DEFAULT_MINIMUM_CHANNEL_SIZE 100 /* 100 */

#define TROJANSCAN_NICKCHANGE_ODDS 8
#define TROJANSCAN_INVISIBLE_ODDS 8

#define TROJANSCAN_DEFAULT_PARTTIME 100
#define TROJANSCAN_DEFAULT_MAXUSERS 20

#define TROJANSCAN_POOL_REGENERATION 3600

#define TROJANSCAN_HOST_POOL      0x00
#define TROJANSCAN_STEAL_HOST     0x01

#define TROJANSCAN_EPIDEMIC_MULTIPLIER 40

#define TROJANSCAN_HOST_MODE      TROJANSCAN_STEAL_HOST

#define TROJANSCAN_CAT "./trojanscan/cat.txt"

#define TROJANSCAN_CHANNEL "#qnet.sec.trj"
#define TROJANSCAN_OPERCHANNEL "#twilightzone"
#define TROJANSCAN_PEONCHANNEL "#qnet.trj"

#define TROJANSCAN_URL_PREFIX "http://trojanscan.quakenet.org/?"

#define TROJANSCAN_MMAX(a, b) a > b ? a : b
#define TROJANSCAN_MMIN(a, b) a > b ? b : a

#define TROJANSCAN_NORMAL_CLONES 0
#define TROJANSCAN_WATCH_CLONES  1

#define TROJANSCAN_QUERY_TEMP_BUF_SIZE 1040
#define TROJANSCAN_QUERY_BUF_SIZE 5040

#define TROJANSCAN_FIRST_OFFENSE 12
#define TROJANSCAN_IPLEN         20

#define TROJANSCAN_VERSION_DETECT "\001VERSION"
#define TROJANSCAN_CLONE_VERSION_REPLY "mIRC v6.16 Khaled Mardam-Bey"

typedef struct trojanscan_clones {
  int              remaining, sitting, index;
  nick             *clone;
  long             fakeip;
} trojanscan_clones;

typedef struct trojanscan_channels {
  sstring *name;
  unsigned exempt : 1;
} trojanscan_channels;

typedef struct trojanscan_worms {
  int id;
  sstring *name;
  unsigned glinehost : 1;
  unsigned glineuser : 1;
  unsigned monitor   : 1;
  unsigned datalen   : 1;
  unsigned hitpriv   : 1;
  unsigned hitchans  : 1;
  unsigned epidemic  : 1;
} trojanscan_worms;

typedef struct trojanscan_phrases {
  int id;
  pcre *phrase;
  pcre_extra *hint;
  trojanscan_worms *worm;
} trojanscan_phrases;

typedef struct trojanscan_db {
  int total_channels, total_phrases, total_worms, detections, glines;
  trojanscan_channels       *channels;
  trojanscan_phrases        *phrases;
  trojanscan_worms          *worms;
} trojanscan_db;

typedef struct trojanscan_prechannels {
  sstring *name;
  int size;
  struct trojanscan_clones *watch_clone;
  struct trojanscan_prechannels *next;
  unsigned exempt : 1;
} trojanscan_prechannels;

typedef struct trojanscan_realchannels {
  channel *chan;
  struct trojanscan_clones *clone;
  struct trojanscan_realchannels *next;
  void *schedule;
  char donotpart;
  int kickedout;
} trojanscan_realchannels;

typedef struct trojanscan_userflags {
  unsigned unauthed   : 1;
  unsigned cat        : 1;
  unsigned website    : 1;
  unsigned staff      : 1;
  unsigned teamleader : 1;
  unsigned developer  : 1;
} trojanscan_userflags;

typedef union trojanscan_userlevel {
  char number;
  struct trojanscan_userflags values;
} trojanscan_userlevel;

typedef struct trojanscan_flagmodification {
  char addition;
  char flag;
} trojanscan_flagmodification;

typedef struct trojanscan_rejoinlist {
  sstring *channel;
  trojanscan_clones *clone;
  void *schedule;
  struct trojanscan_realchannels *rp;
  struct trojanscan_rejoinlist *next;
} trojanscan_rejoinlist;

typedef struct trojanscan_inchannel {
  sstring *channel;
  struct trojanscan_clones *watch_clone;
} trojanscan_inchannel;

typedef struct trojanscan_templist {
  struct trojanscan_clones *watch_clone;
  char active;  /* required as copy of trojanscan_inchannel could be nil unfortunatly */
} trojanscan_templist;

typedef MYSQL_RES trojanscan_database_res;
typedef MYSQL_ROW trojanscan_database_row;
typedef MYSQL trojanscan_database_connection;

#define TROJANSCAN_FLAG_MASK      "%s%s%s%s%s"

#define TrojanscanIsCat(x)        (x.values.cat)
#define TrojanscanIsWebsite(x)    (x.values.website)
#define TrojanscanIsStaff(x)      (x.values.staff)
#define TrojanscanIsDeveloper(x)  (x.values.developer)
#define TrojanscanIsTeamLeader(x) (x.values.teamleader)

#define TrojanscanIsLeastTeamLeader(x) ((TrojanscanIsTeamLeader(x) | TrojanscanIsDeveloper(x)))
#define TrojanscanIsLeastWebsite(x) ((TrojanscanIsWebsite(x) | TrojanscanIsDeveloper(x)))
#define TrojanscanIsLeastStaff(x) ((TrojanscanIsStaff(x) | TrojanscanIsTeamLeader(x) | TrojanscanIsDeveloper(x)))

#define TrojanscanFlagsInfo(flags) TrojanscanIsDeveloper(flags) ? "d" : "", TrojanscanIsTeamLeader(flags) ? "t" : "", TrojanscanIsStaff(flags) ? "s" : "", TrojanscanIsWebsite(flags) ? "w" : "", TrojanscanIsCat(flags) ? "c" : ""

#define TROJANSCAN_ACL_UNAUTHED   0x01
#define TROJANSCAN_ACL_STAFF      0x02
#define TROJANSCAN_ACL_OPER       0x04
#define TROJANSCAN_ACL_DEVELOPER  0x08
#define TROJANSCAN_ACL_CAT        0x10
#define TROJANSCAN_ACL_WEBSITE    0x20
#define TROJANSCAN_ACL_TEAMLEADER 0x40

int trojanscan_status(void *sender, int cargc, char **cargv);
int trojanscan_showcommands(void *sender, int cargc, char **cargv);
int trojanscan_help(void *sender, int cargc, char **cargv);
int trojanscan_hello(void *sender, int cargc, char **cargv);
int trojanscan_userjoin(void *sender, int cargc, char **cargv);
int trojanscan_changelev(void *sender, int cargc, char **cargv);
int trojanscan_deluser(void *sender, int cargc, char **cargv);
int trojanscan_rehash(void *sender, int cargc, char **cargv);
int trojanscan_mew(void *sender, int cargc, char **cargv);
int trojanscan_cat(void *sender, int cargc, char **cargv);
int trojanscan_reschedule(void *sender, int cargc, char **cargv);
int trojanscan_chanlist(void *sender, int cargc, char **cargv);
int trojanscan_whois(void *sender, int cargc, char **cargv);
int trojanscan_listusers(void *sender, int cargc, char **cargv);

char trojanscan_getmtfrommessagetype(int input);

void trojanscan_handlemessages(nick *target, int messagetype, void **args);
void trojanscan_clonehandlemessages(nick *target, int messagetype, void **args);
void trojanscan_mainchanmsg(char *message, ...);
void trojanscan_peonchanmsg(char *message, ...);
void trojanscan_reply(nick *target, char *message, ... );
void trojanscan_privmsg_chan_or_nick(channel *cp, nick *np, char *text, ...);

void trojanscan_connect(void *arg);
void trojanscan_dojoin(void *arg);
void trojanscan_dopart(void *arg);
void trojanscan_donickchange(void *arg);
void trojanscan_registerclones(void *arg);
void trojanscan_fill_channels(void *arg);
void trojanscan_rehash_schedule(void *arg);
void trojanscan_rejoin_channel(void *arg);

void trojanscan_read_database(int first_time);
void trojanscan_free_database(void);

struct trojanscan_realchannels *trojanscan_allocaterc(char *chan);
void trojanscan_join(trojanscan_realchannels *rc);
struct trojanscan_clones *trojanscan_selectclone(char type);
void trojanscan_free_channels(void);
int trojanscan_add_ll(struct trojanscan_prechannels **head, struct trojanscan_prechannels *newitem);
int trojanscan_keysort(const void *v1, const void *v2);

int trojanscan_user_level_by_authname(char *authname);
int trojanscan_user_id_by_authname(char *authname);
struct trojanscan_worms *trojanscan_find_worm_by_id(int id);
char *trojanscan_getuser(int id);

int trojanscan_minmaxrand(float min, float max);
char *trojanscan_iptostr(char *buf, int buflen, unsigned int ip);

int trojanscan_database_connect(char *dbhost, char *dbuser, char *dbpass, char *db, unsigned int port);
void trojanscan_database_close(void);
void trojanscan_database_escape_string(char *dest, char *source, size_t length);
int trojanscan_database_query(char *format, ...);
int trojanscan_database_num_rows(trojanscan_database_res *res);
trojanscan_database_res *trojanscan_database_store_result();
trojanscan_database_row trojanscan_database_fetch_row(trojanscan_database_res *res);
void trojanscan_database_free_result(trojanscan_database_res *res);
nick *trojanscan_selectuser(void);

int trojanscan_is_not_octet(char *begin, int length);
void trojanscan_genreal(char *ptc, char size);
char trojanscan_genchar(int ty);
void trojanscan_gennick(char *ptc, char size);
void trojanscan_genident(char *ptc, char size);
void trojanscan_genhost(char *ptc, char size, long *fakeip);

int trojanscan_generatepool(void);
void trojanscan_watch_clone_update(struct trojanscan_prechannels *hp, int count);
void trojanscan_repool(void *arg);

void trojanscan_generatehost(char *buf, int maxsize, long *fakeip);
void trojanscan_generatenick(char *buf, int maxsize);
void trojanscan_generateident(char *buf, int maxsize);
void trojanscan_generaterealname(char *buf, int maxsize);

sstring *trojanscan_getsstring(char *string, int length);
int trojanscan_strip_codes(char *buf, size_t max, char *original);
int trojanscan_isip(char *host);

struct trojanscan_clones trojanscan_swarm[TROJANSCAN_CLONE_TOTAL];
struct trojanscan_db trojanscan_database;

sstring *trojanscan_hostpool[TROJANSCAN_POOLSIZE], *trojanscan_tailpool[TROJANSCAN_POOLSIZE];
struct trojanscan_inchannel *trojanscan_chans = NULL;

unsigned int trojanscan_cycletime, trojanscan_maxchans, trojanscan_part_time, trojanscan_activechans = 0, trojanscan_tailpoolsize = 0, trojanscan_hostpoolsize = 0, trojanscan_channumber = 0, trojanscan_maxusers;
int trojanscan_watchclones_count = 0;

int trojanscan_errorcode;
struct trojanscan_realchannels *trojanscan_realchanlist;
char trojanscan_hostmode;

void *trojanscan_schedule, *trojanscan_connect_schedule;
void *trojanscan_initialschedule, *trojanscan_rehashschedule, *trojanscan_poolschedule, *trojanscan_cloneschedule;

int trojanscan_minchansize, trojanscan_min_hosts;
int trojanscan_swarm_created = 0;

nick *trojanscan_nick;
CommandTree *trojanscan_cmds; 
trojanscan_database_connection trojanscan_sql;
struct trojanscan_rejoinlist *trojanscan_schedulerejoins = NULL;

#endif  
