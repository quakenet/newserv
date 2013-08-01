/* nick.h */

#ifndef __NICK_H
#define __NICK_H

#include "../lib/sstring.h"
#include "../irc/irc_config.h"
#include "../lib/flags.h"
#include "../lib/array.h"
#include "../server/server.h"
#include "../lib/base64.h"
#include "../lib/irc_ipv6.h"
#include "../patricia/patricia.h"

#include "../authext/authext.h"

#include <time.h>

#ifndef MAXNICKEXTS
#define MAXNICKEXTS       10
#endif

#define UMODE_INV       0x0001
#define UMODE_WALLOPS   0x0002
#define UMODE_DEBUG     0x0004
#define UMODE_OPER      0x0008
#define UMODE_SERVICE   0x0010
#define UMODE_XOPER     0x0020
#define UMODE_DEAF      0x0040  
#define UMODE_ACCOUNT   0x0080
#define UMODE_HIDECHAN  0x0100
#define UMODE_HIDEHOST  0x0200
#define UMODE_SETHOST   0x0400
#define UMODE_REGPRIV   0x0800
#define UMODE_HIDEIDLE  0x1000
#define UMODE_PARANOID  0x2000
#define UMODE_CLOAKED   0x4000

#define UMODE_ALL       0x7FFF

#define AFLAG_STAFF     0x0001
#define AFLAG_SUPPORT   0x0002
#define AFLAG_OPER      0x0004
#define AFLAG_ADMIN     0x0008
#define AFLAG_DEVELOPER 0x0010

#define AFLAG_ALL       0x001F

#define IsInvisible(x)    ((x)->umodes & UMODE_INV)
#define IsWallops(x)      ((x)->umodes & UMODE_WALLOPS)
#define IsDebug(x)        ((x)->umodes & UMODE_DEBUG)
#define IsOper(x)         ((x)->umodes & UMODE_OPER)
#define IsService(x)      ((x)->umodes & UMODE_SERVICE)
#define IsXOper(x)        ((x)->umodes & UMODE_XOPER)
#define IsDeaf(x)         ((x)->umodes & UMODE_DEAF)
#define IsAccount(x)      ((x)->umodes & UMODE_ACCOUNT)
#define IsHideChan(x)     ((x)->umodes & UMODE_HIDECHAN)
#define IsHideHost(x)     ((x)->umodes & UMODE_HIDEHOST)
#define IsSetHost(x)      ((x)->umodes & UMODE_SETHOST)
#define IsRegPriv(x)      ((x)->umodes & UMODE_REGPRIV)
#define IsHideIdle(x)     ((x)->umodes & UMODE_HIDEIDLE)
#define IsParanoid(x)     ((x)->umodes & UMODE_PARANOID)
#define IsCloaked(x)      ((x)->umodes & UMODE_CLOAKED)

#define SetInvisible(x)    ((x)->umodes |= UMODE_INV)
#define SetWallops(x)      ((x)->umodes |= UMODE_WALLOPS)
#define SetDebug(x)        ((x)->umodes |= UMODE_DEBUG)
#define SetOper(x)         ((x)->umodes |= UMODE_OPER)
#define SetService(x)      ((x)->umodes |= UMODE_SERVICE)
#define SetXOper(x)        ((x)->umodes |= UMODE_XOPER)
#define SetDeaf(x)         ((x)->umodes |= UMODE_DEAF)
#define SetAccount(x)      ((x)->umodes |= UMODE_ACCOUNT)
#define SetHideChan(x)     ((x)->umodes |= UMODE_HIDECHAN)
#define SetHideHost(x)     ((x)->umodes |= UMODE_HIDEHOST)
#define SetSetHost(x)      ((x)->umodes |= UMODE_SETHOST)
#define SetRegPriv(x)      ((x)->umodes |= UMODE_REGPRIV)
#define SetHideIdle(x)     ((x)->umodes |= UMODE_HIDEIDLE)
#define SetParanoid(x)     ((x)->umodes |= UMODE_PARANOID)
#define SetCloaked(x)      ((x)->umodes |= UMODE_CLOAKED)

#define ClearInvisible(x)    ((x)->umodes &= ~UMODE_INV)
#define ClearWallops(x)      ((x)->umodes &= ~UMODE_WALLOPS)
#define ClearDebug(x)        ((x)->umodes &= ~UMODE_DEBUG)
#define ClearOper(x)         ((x)->umodes &= ~UMODE_OPER)
#define ClearService(x)      ((x)->umodes &= ~UMODE_SERVICE)
#define ClearXOper(x)        ((x)->umodes &= ~UMODE_XOPER)
#define ClearDeaf(x)         ((x)->umodes &= ~UMODE_DEAF)
#define ClearAccount(x)      ((x)->umodes &= ~UMODE_ACCOUNT)
#define ClearHideChan(x)     ((x)->umodes &= ~UMODE_HIDECHAN)
#define ClearHideHost(x)     ((x)->umodes &= ~UMODE_HIDEHOST)
#define ClearSetHost(x)      ((x)->umodes &= ~UMODE_SETHOST)
#define ClearRegPriv(x)      ((x)->umodes &= ~UMODE_REGPRIV)
#define ClearHideIdle(x)     ((x)->umodes &= ~UMODE_HIDEIDLE)
#define ClearParanoid(x)     ((x)->umodes &= ~UMODE_PARANOID)
#define ClearCloaked(x)      ((x)->umodes &= ~UMODE_CLOAKED)

#define IsStaff(x)           ((x)->flags & AFLAG_STAFF)
#define IsDeveloper(x)       ((x)->flags & AFLAG_DEVELOPER)
#define IsSupport(x)         ((x)->flags & AFLAG_SUPPORT)
#define IsAdmin(x)           ((x)->flags & AFLAG_ADMIN)
#define IsOperFlag(x)        ((x)->flags & AFLAG_OPER)

#define SetStaff(x)          ((x)->flags |= AFLAG_STAFF)
#define SetDeveloper(x)      ((x)->flags |= AFLAG_DEVELOPER)
#define SetSupport(x)        ((x)->flags |= AFLAG_SUPPORT)
#define SetAdmin(x)          ((x)->flags |= AFLAG_ADMIN)
#define SetOperFlag(x)       ((x)->flags |= AFLAG_OPER)

#define ClearStaff(x)        ((x)->flags &= ~AFLAG_STAFF)
#define ClearDeveloper(x)    ((x)->flags &= ~AFLAG_DEVELOPER)
#define ClearSupport(x)      ((x)->flags &= ~AFLAG_SUPPORT)
#define ClearAdmin(x)        ((x)->flags &= ~AFLAG_ADMIN)
#define ClearOperFlag(x)     ((x)->flags &= ~AFLAG_OPER)

#define NickOnServiceServer(target)   SIsService(&serverlist[homeserver((target)->numeric)])

typedef struct host {
  sstring *name;
  int clonecount;
  unsigned int marker;
  struct nick *nicks;
  struct host *next;
} host;

typedef struct realname {
  sstring *name;
  int usercount;
  unsigned int marker;
  struct nick *nicks;
  struct realname *next;
} realname;

typedef struct nick {
  char nick[NICKLEN+1];
  long numeric;
  char ident[USERLEN+1];
  host *host;
  realname *realname;
  sstring *shident;  /* +h users: fake ident/host goes here */
  sstring *sethost;
  sstring *opername;
  flag_t umodes;
  char *authname;
  authname *auth; /* This requires User ID numbers to work */
  time_t timestamp;
  time_t accountts;
  sstring *away;
  patricia_node_t *ipnode;
  unsigned int cloak_count;
  struct nick *cloak_extra;
  unsigned int marker;
  struct nick *next;
  struct nick *nextbyhost;
  struct nick *nextbyrealname;
  struct nick *nextbyauthname;
  /* These are extensions only used by other modules */
  array *channels;
  void *exts[MAXNICKEXTS];
} nick;

#define p_ipaddr ipnode->prefix->sin

#define NICKHASHSIZE      60000
#define HOSTHASHSIZE      40000
#define REALNAMEHASHSIZE  40000

extern nick *nicktable[NICKHASHSIZE];
extern nick **servernicks[MAXSERVERS];
extern host *hosttable[HOSTHASHSIZE];
extern realname *realnametable[REALNAMEHASHSIZE];
extern const flag umodeflags[];
extern const flag accountflags[];
extern char *NULLAUTHNAME;

#define MAXNUMERIC 0x3FFFFFFF

#define homeserver(x)           (((x)>>18)&(MAXSERVERS-1))
#define gethandlebynumericunsafe(x)   (&(servernicks[homeserver(x)][(x)&(serverlist[homeserver(x)].maxusernum)]))
#define getnickbynumericunsafe(x)       (servernicks[homeserver(x)][(x)&(serverlist[homeserver(x)].maxusernum)])
#define getnickbynumericstr(x)  (getnickbynumeric(numerictolong((x),5)))
#define gethandlebynumeric(x)   ((serverlist[homeserver(x)].linkstate==LS_INVALID) ? NULL : \
                                 (gethandlebynumericunsafe(x)))
#define getnickbynumeric(x)     ((gethandlebynumeric(x)==NULL)?NULL: \
                                 ((*gethandlebynumeric(x)==NULL)?NULL: \
                                  (((*gethandlebynumeric(x))->numeric==(x&MAXNUMERIC))?(*gethandlebynumeric(x)):NULL)))

/* nickalloc.c functions */
realname *newrealname();
void freerealname(realname *rn);
nick *newnick();
void freenick(nick *np);
host *newhost();
void freehost(host *hp);

/* nick.c functions */
void handleserverchange(int hooknum, void *arg);
void deletenick(nick *np);
void addnicktohash(nick *np);
void removenickfromhash(nick *np);
nick *getnickbynick(const char *nick);
int registernickext(const char *name);
int findnickext(const char *name);
void releasenickext(int index);
char *visiblehostmask(nick *np, char *buf);
char *visibleuserhost(nick *np, char *buf);
int canseeuser(nick *np, nick *cloaked);
void addcloaktarget(nick *cloaked, nick *target);
void removecloaktarget(nick *target);
void clearcloaktargets(nick *cloaked);

/* nickhandlers.c functions */
int handlenickmsg(void *source, int cargc, char **cargv);
int handlequitmsg(void *source, int cargc, char **cargv);
int handlekillmsg(void *source, int cargc, char **cargv);
int handleusermodemsg(void *source, int cargc, char **cargv);
int handleaccountmsg(void *source, int cargc, char **cargv);
int handleprivmsg(void *source, int cargc, char **cargv);
int handleawaymsg(void *source, int cargc, char **cargv);
int handleaddcloak(void *source, int cargc, char **cargv);
int handleclearcloak(void *source, int cargc, char **cargv);

/* These functions have been replaced by macros 
nick **gethandlebynumeric(long numeric);
nick *getnickbynumeric(long numeric);
nick *getnickbynumericstr(char *numericstr);
*/
                    
/* nickhelpers.c functions */
void initnickhelpers();
void fininickhelpers();
host *findhost(const char *hostname);
host *findorcreatehost(const char *hostname);
void releasehost(host *hp);
realname *findrealname(const char *name);
realname *findorcreaterealname(const char *name);
void releaserealname(realname *rnp);

unsigned int nexthostmarker();
unsigned int nextrealnamemarker();
unsigned int nextnickmarker();

#endif
