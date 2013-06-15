#ifndef _GLINE_H
#define _GLINE_H

#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../lib/sstring.h"
#include "../lib/flags.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../parser/parser.h"
#include "../localuser/localuserchannel.h"

#define MAX_USERS_RN 1

#define PASTWATCH       157680000       /* number of seconds in 5 years */

/*
 * If the expiration value, interpreted as an absolute timestamp, is
 * more recent than 5 years in the past, we interpret it as an
 * absolute timestamp; otherwise, we assume it's relative and convert
 * it to an absolute timestamp.  Either way, the output of this macro
 * is an absolute timestamp--not guaranteed to be a *valid* timestamp,
 * but you can't have everything in a macro ;)
 */
#define abs_expire(exp)                                                 \
  ((exp) >= getnettime() - PASTWATCH ? (exp) : (exp) + getnettime())

#define gline_max(a,b) (((a)<(b)) ? (b) : (a))

extern int gl_nodeext;

typedef struct gline {
  struct gline    *next;
  struct gline    *nextbynode;
  struct gline    *nextbynonnode;

  long             glineid;
  long             numeric;
 
  sstring         *nick;
  sstring         *user;
  sstring         *host;
  sstring         *reason;
  sstring         *creator;

  patricia_node_t* node;
 
  time_t           expires;
  time_t           lastmod;
  time_t           lifetime;
 
  unsigned int     flags;
} gline;

#define GLINE_NICKEXACT   0x000001  /* Gline includes an exact nick (no wildcards) */
#define GLINE_NICKMASK    0x000002  /* Gline includes a nick mask with wildcards */
#define GLINE_NICKANY     0x000004  /* Gline is *!.. */
#define GLINE_NICKNULL    0x000008  /* Gline has no nick */
#define GLINE_USEREXACT   0x000010  /* Gline includes an exact user (no wildcards) */
#define GLINE_USERMASK    0x000020  /* Gline includes a user mask with wildcards */
#define GLINE_USERANY     0x000040  /* Gline is ..!*@.. */
#define GLINE_USERNULL    0x000080  /* Gline has no user */
#define GLINE_HOSTEXACT   0x000100  /* Gline includes an exact host */
#define GLINE_HOSTMASK    0x000200  /* Gline includes a host mask */
#define GLINE_HOSTANY     0x000400  /* Gline is ..@* */
#define GLINE_HOSTNULL    0x000800  /* Gline has no host */
#define GLINE_BADCHAN     0x001000  /* Gline includes a badchan */
#define GLINE_REALNAME    0x002000  /* Gline includes a realname */
#define GLINE_IPMASK      0x004000  /* Gline includes an CIDR mask */
#define GLINE_FORCED      0x008000  /* Gline includes forced flag (!) */
#define GLINE_ACTIVATE    0x010000  /* Gline should be activated */
#define GLINE_DEACTIVATE  0x020000  /* Gline should be deactivated */
#define GLINE_ACTIVE      0x040000  /* Gline is active */
#define GLINE_HOST        0x080000
#define GLINE_IPWILD      0x100000
#define GLINE_BADMASK     0x200000

#define GlineIsBadChan(x)  ((x)->flags & GLINE_BADCHAN)
#define GlineIsRealName(x) ((x)->flags & GLINE_REALNAME)
#define GlineIsIpMask(x)   ((x)->flags & GLINE_IPMASK)
#define GlineIsForced(x)   ((x)->flags & GLINE_FORCED)

#define GlineCreator(x) ((x)->creator)
#define GlineExpires(x) ((x)->expires)
#define GlineLastMod(x) ((x)->lastmod)

#define GLIST_COUNT    0x01 /* -c */
#define GLIST_EXACT    0x02 /* -x */
#define GLIST_FIND     0x04 /* -f */
#define GLIST_REASON   0x10 /* -r */
#define GLIST_OWNER    0x20 /* -o */
#define GLIST_REALNAME 0x40 /* -R */

extern gline* glinelist;
extern gline* glinelistnonnode;
extern gline* badchanlist;
extern int glinecount;
extern int badchancount;
extern int rnglinecount;
extern int hostglinecount;
extern int ipglinecount;

int gline_glist(void* source, int cargc, char** cargv);
int gline_glgrep(void* source, int cargc, char** cargv);
int gline_glstats(void* source, int cargc, char** cargv);
int gline_ungline(void* source, int cargc, char** cargv);
int gline_rawglinefile(void* source, int cargc, char** cargv);
int gline_glinefile(void* source, int cargc, char** cargv);
int gline_unglinefile(void* source, int cargc, char** cargv);
int gline_saveglines(void* source, int cargc, char** cargv);
int handleglinemsg(void* source, int cargc, char** cargv);
int gline_gline(void* source, int cargc, char** cargv);
int gline_rngline(void* source, int cargc, char** cargv);
int gline_block(void* source, int cargc, char** cargv);

gline *newgline();
void freegline (gline *gl);

gline* gline_processmask(char *mask);
int gline_match ( gline *gla, gline *glb);
int gline_match_mask ( gline *gla, gline *glb);

char *glinetostring(gline *g);

gline* gline_add(long creatornum, sstring *creator, char *mask, char *reason, time_t expires, time_t lastmod, time_t lifetime);
int gline_setnick(nick *np, char *creator, int duration, char *reason, int forceidentonly );

#endif
