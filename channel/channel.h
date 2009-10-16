/* channel.h */

#ifndef __CHANNEL_H
#define __CHANNEL_H

#include "../lib/flags.h"
#include "../lib/sstring.h"
#include "../irc/irc_config.h"
#include "../lib/array.h"
#include "../lib/sstring.h"
#include "../core/hooks.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "../chanindex/chanindex.h"
#include "../bans/bans.h"

#include <time.h>
#include <stdlib.h>

#define CHANMODE_NOEXTMSG   0x0001
#define CHANMODE_TOPICLIMIT 0x0002
#define CHANMODE_SECRET     0x0004
#define CHANMODE_PRIVATE    0x0008
#define CHANMODE_INVITEONLY 0x0010
#define CHANMODE_LIMIT      0x0020
#define CHANMODE_KEY        0x0040
#define CHANMODE_MODERATE   0x0080
#define CHANMODE_NOCOLOUR   0x0100
#define CHANMODE_NOCTCP     0x0200
#define CHANMODE_REGONLY    0x0400
#define CHANMODE_DELJOINS   0x0800
#define CHANMODE_NOQUITMSG  0x1000
#define CHANMODE_NONOTICE   0x2000
#define CHANMODE_MODNOAUTH  0x4000
#define CHANMODE_SINGLETARG 0x8000

#define CHANMODE_ALL        0xFFFF
#define CHANMODE_DEFAULT    0x2203	/* +ntCN */

#define IsNoExtMsg(x)   ((x)->flags & CHANMODE_NOEXTMSG)
#define IsTopicLimit(x) ((x)->flags & CHANMODE_TOPICLIMIT)
#define IsSecret(x)     ((x)->flags & CHANMODE_SECRET)
#define IsPrivate(x)    ((x)->flags & CHANMODE_PRIVATE)
#define IsInviteOnly(x) ((x)->flags & CHANMODE_INVITEONLY)
#define IsLimit(x)      ((x)->flags & CHANMODE_LIMIT)
#define IsKey(x)        ((x)->flags & CHANMODE_KEY)
#define IsModerated(x)  ((x)->flags & CHANMODE_MODERATE)
#define IsNoColour(x)   ((x)->flags & CHANMODE_NOCOLOUR)
#define IsNoCTCP(x)     ((x)->flags & CHANMODE_NOCTCP)
#define IsRegOnly(x)    ((x)->flags & CHANMODE_REGONLY)
#define IsDelJoins(x)   ((x)->flags & CHANMODE_DELJOINS)
#define IsNoQuitMsg(x)  ((x)->flags & CHANMODE_NOQUITMSG)
#define IsNoNotice(x)   ((x)->flags & CHANMODE_NONOTICE)
#define IsModNoAuth(x)  ((x)->flags & CHANMODE_MODNOAUTH)
#define IsSingleTarg(x) ((x)->flags & CHANMODE_SINGLETARG)

#define SetNoExtMsg(x)   ((x)->flags |= CHANMODE_NOEXTMSG)
#define SetTopicLimit(x) ((x)->flags |= CHANMODE_TOPICLIMIT)
#define SetSecret(x)     ((x)->flags |= CHANMODE_SECRET)
#define SetPrivate(x)    ((x)->flags |= CHANMODE_PRIVATE)
#define SetInviteOnly(x) ((x)->flags |= CHANMODE_INVITEONLY)
#define SetLimit(x)      ((x)->flags |= CHANMODE_LIMIT)
#define SetKey(x)        ((x)->flags |= CHANMODE_KEY)
#define SetModerated(x)  ((x)->flags |= CHANMODE_MODERATE)
#define SetNoColour(x)   ((x)->flags |= CHANMODE_NOCOLOUR)
#define SetNoCTCP(x)     ((x)->flags |= CHANMODE_NOCTCP)
#define SetRegOnly(x)    ((x)->flags |= CHANMODE_REGONLY)
#define SetDelJoins(x)   ((x)->flags |= CHANMODE_DELJOINS)
#define SetNoQuitMsg(x)  ((x)->flags |= CHANMODE_NOQUITMSG)
#define SetNoNotice(x)   ((x)->flags |= CHANMODE_NONOTICE)
#define SetModNoAuth(x)  ((x)->flags |= CHANMODE_MODNOAUTH)
#define SetSingleTarg(x) ((x)->flags |= CHANMODE_SINGLETARG)

#define ClearNoExtMsg(x)   ((x)->flags &= ~CHANMODE_NOEXTMSG)
#define ClearTopicLimit(x) ((x)->flags &= ~CHANMODE_TOPICLIMIT)
#define ClearSecret(x)     ((x)->flags &= ~CHANMODE_SECRET)
#define ClearPrivate(x)    ((x)->flags &= ~CHANMODE_PRIVATE)
#define ClearInviteOnly(x) ((x)->flags &= ~CHANMODE_INVITEONLY)
#define ClearLimit(x)      ((x)->flags &= ~CHANMODE_LIMIT)
#define ClearKey(x)        ((x)->flags &= ~CHANMODE_KEY)
#define ClearModerated(x)  ((x)->flags &= ~CHANMODE_MODERATE)
#define ClearNoColour(x)   ((x)->flags &= ~CHANMODE_NOCOLOUR)
#define ClearNoCTCP(x)     ((x)->flags &= ~CHANMODE_NOCTCP)
#define ClearRegOnly(x)    ((x)->flags &= ~CHANMODE_REGONLY)
#define ClearDelJoins(x)   ((x)->flags &= ~CHANMODE_DELJOINS)
#define ClearNoQuitMsg(x)  ((x)->flags &= ~CHANMODE_NOQUITMSG)
#define ClearNoNotice(x)   ((x)->flags &= ~CHANMODE_NONOTICE)
#define ClearModNoAuth(x)  ((x)->flags &= ~CHANMODE_MODNOAUTH)
#define ClearSingleTarg(x) ((x)->flags &= ~CHANMODE_SINGLETARG)

#define     CUMODE_OP      0x80000000
#define     CUMODE_VOICE   0x40000000

#define     CU_NUMERICMASK 0x3FFFFFFF
#define     CU_MODEMASK    0xC0000000

#define     CU_NOUSERMASK  0x0003FFFF

/* Maximum allowed hash search depth */

#define     CUHASH_DEPTH   10

#define  MAGIC_REMOTE_JOIN_TS 1270080000

#define MODECHANGE_MODES   0x00000001
#define MODECHANGE_USERS   0x00000002
#define MODECHANGE_BANS    0x00000004

typedef struct chanuserhash {
  unsigned short  hashsize;
  unsigned short  totalusers;
  unsigned long  *content;
} chanuserhash;
  
typedef struct channel {
  chanindex      *index;
  time_t          timestamp;
  sstring        *topic;
  time_t          topictime;
  flag_t          flags;
  sstring        *key;
  int             limit;
  chanban        *bans;
  chanuserhash   *users;
} channel;

extern unsigned long nouser;
extern const flag cmodeflags[];

/* functions from channel.c */
int addnicktochannel(channel *cp, long numeric);
void delnickfromchannel(channel *cp, long numeric, int updateuser);
void delchannel(channel *cp);
channel *createchannel(char *name);
channel *findchannel(char *name);
void addchanneltohash(channel *cp);
void removechannelfromhash(channel *cp);
void addordelnick(int hooknum, void *arg);
void onconnect(int hooknum, void *arg);
unsigned int countuniquehosts(channel *cp);
void clean_key(char *s);

/* functions from channelhandlers.c */
int handleburstmsg(void *source, int cargc, char **cargv);
int handlejoinmsg(void *source, int cargc, char **cargv);
int handlecreatemsg(void *source, int cargc, char **cargv);
int handlepartmsg(void *source, int cargc, char **cargv);
int handlekickmsg(void *source, int cargc, char **cargv);
int handletopicmsg(void *source, int cargc, char **cargv);
int handlemodemsg(void *source, int cargc, char **cargv);
int handleclearmodemsg(void *source, int cargc, char **cargv);
void handlewhoischannels(int hooknum, void *arg);

/* functions from chanuserhash.c */
void rehashchannel(channel *cp);
int addnumerictochanuserhash(chanuserhash *cuh, long numeric);
unsigned long *getnumerichandlefromchanhash(chanuserhash *cuh, long numeric);

/* functions from channelalloc.c */
void initchannelalloc();
channel *newchan();
void freechan(channel *cp);
chanuserhash *newchanuserhash(int numbuckets);
void freechanuserhash(chanuserhash *cuhp);

/* functions from channelbans.c */
int setban(channel *cp, const char *ban);
int clearban(channel *cp, const char *ban, int optional);
void clearallbans(channel *cp);
int nickmatchban(nick *np, chanban *bp, int visibleonly);
int nickbanned(nick *np, channel *cp, int visibleonly);

/* functions from channelindex.c */
void initchannelindex();
chanindex *findchanindex(const char *name);
chanindex *findorcreatechanindex(const char *name);
void releasechanindex(chanindex *cip);
int registerchanext(const char *name);
int findchanext(const char *name);
void releasechanext(int index);
unsigned int nextchanmarker();

#endif
