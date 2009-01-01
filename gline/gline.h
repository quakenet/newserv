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
  struct gline*    next;
  struct gline*    nextbynode;
  struct gline*    nextbynonnode;

  long             glineid;
  long             numeric;
 
  sstring*         nick;
  sstring*         user;
  sstring*         host;
  sstring*         reason;
  sstring*         creator;

  patricia_node_t* node;
 
  time_t           expires;
  time_t           lastmod;
  time_t           lifetime;
 
  unsigned int     flags;
} gline;

#define GLINE_NICKEXACT   0x00001  /* Gline includes an exact nick (no wildcards) */
#define GLINE_NICKMASK    0x00002  /* Gline includes a nick mask with wildcards */
#define GLINE_NICKANY     0x00004  /* Gline is *!.. */
#define GLINE_NICKNULL    0x00008  /* Gline has no nick */
#define GLINE_USEREXACT   0x00010  /* Gline includes an exact user (no wildcards) */
#define GLINE_USERMASK    0x00020  /* Gline includes a user mask with wildcards */
#define GLINE_USERANY     0x00040  /* Gline is ..!*@.. */
#define GLINE_USERNULL    0x00080  /* Gline has no user */
#define GLINE_HOSTEXACT   0x00100  /* Gline includes an exact host */
#define GLINE_HOSTMASK    0x00200  /* Gline includes a host mask */
#define GLINE_HOSTANY     0x00400  /* Gline is ..@* */
#define GLINE_HOSTNULL    0x00800  /* Gline has no host */
#define GLINE_BADCHAN     0x01000
#define GLINE_REALNAME    0x02000
#define GLINE_IPMASK      0x04000
#define GLINE_FORCED      0x08000
#define GLINE_ACTIVATE    0x10000
#define GLINE_DEACTIVATE  0x20000
#define GLINE_ACTIVE      0x40000
#define GLINE_HOST        0x80000

#define GlineIsBadChan(x)  ((x)->flags & GLINE_BADCHAN)
#define GlineIsRealName(x) ((x)->flags & GLINE_REALNAME)
#define GlineIsIpMask(x)   ((x)->flags & GLINE_IPMASK)
#define GlineIsForced(x)   ((x)->flags & GLINE_FORCED)

#define GlineNick(x)    ((x)->nick)
#define GlineUser(x)    ((x)->user)
#define GlineHost(x)    ((x)->host)
#define GlineReason(x)  ((x)->reason)
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

gline* gline_add(long creatornum, sstring *creator, char *mask, char *reason, time_t expires, time_t lastmod, time_t lifetime);

/*
int gline_add(char* mask, char* reason, char* creator, time_t expires, time_t lastmod, unsigned int flags, int propagate);
gline* make_gline(char* nick, char* user, char* host, char* reason, char* creator, time_t expires, time_t lastmod, unsigned int flags);
gline* gline_find(char* mask);
void gline_free(gline* g);
int check_if_ipmask(const char* mask);
void canon_userhost(char* mask, char** nick, char** user, char** host, char* def_user);
*/

#endif
