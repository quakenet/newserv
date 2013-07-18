#ifndef __GLINES_H
#define __GLINES_H

#include "../lib/sstring.h"
#include "../nick/nick.h"
#include "../channel/channel.h"

#define SNIRCD_VERSION 134

#define MAXUSERGLINEUSERHITS        100
#define MAXUSERGLINECHANNELHITS     10

#define MAXGLINEDURATION            90 * 86400
#define MINGLINEREASONLEN           10

#define MAXGLINEUSERHITS     500
#define MAXGLINECHANNELHITS  50

#define MAXGLINELOG          500

#define GLINE_IGNORE_TRUST   1
#define GLINE_ALWAYS_NICK    2
#define GLINE_ALWAYS_USER    4
#define GLINE_SIMULATE       8

#define GLINE_HOSTMASK    1   /* Gline includes a host mask */
#define GLINE_IPMASK      2   /* Gline includes an CIDR mask */
#define GLINE_BADCHAN     4   /* Gline includes a badchan */
#define GLINE_REALNAME    8   /* Gline includes a realname */
#define GLINE_ACTIVATE    16  /* Gline should be activated */
#define GLINE_DEACTIVATE  32  /* Gline should be deactivated */
#define GLINE_DESTROY     64  /* Gline should be destroyed */
#define GLINE_ACTIVE      128 /* Gline is active */
#define GLINE_DESTROYED   256 /* Gline is destroyed */

/**
 * glist flags
 */
#define GLIST_COUNT    0x01 /* -c */
#define GLIST_EXACT    0x02 /* -x */
#define GLIST_FIND     0x04 /* -f */
#define GLIST_REASON   0x10 /* -r */
#define GLIST_OWNER    0x20 /* -o */
#define GLIST_REALNAME 0x40 /* -R */
#define GLIST_INACTIVE 0x80 /* -i */

#define GLSTORE_PATH_PREFIX   "data/glines"
#define GLSTORE_SAVE_FILES    5
#define GLSTORE_SAVE_INTERVAL 3600

/**
 * Interpret absolute/relative timestamps with same method as snircd
 * If the expiration value, interpreted as an absolute timestamp, is
 * more recent than 5 years in the past, we interpret it as an
 * absolute timestamp; otherwise, we assume it's relative and convert
 * it to an absolute timestamp.
 */
#define abs_expire(exp) ((exp) >= getnettime() - 157680000 ? (exp) : (exp) + getnettime())

#define gline_max(a, b) (((a)<(b)) ? (b) : (a))

typedef struct gline {
  sstring *nick;
  sstring *user;
  sstring *host;
  sstring *reason;
  sstring *creator;

  struct irc_in_addr ip;
  unsigned char bits;

  time_t expire;
  time_t lastmod;
  time_t lifetime;

  unsigned int flags;

  struct gline *next;
} gline;

typedef struct glinebuf {
  int id;
  sstring *comment;
  time_t flush;

  int merge;
  gline *glines;

  int userhits;
  int channelhits;
} glinebuf;

extern gline *glinelist;
extern glinebuf *glinebuflog[MAXGLINELOG];
extern int glinebuflogoffset;

/* glines.c */
gline *findgline(const char *);
void gline_propagate(gline *);
void gline_deactivate(gline *, time_t, int);
void gline_destroy(gline *, time_t, int);
void gline_activate(gline *agline, time_t lastmod, int propagate);
int glineequal(gline *, gline *);
int gline_match_mask(gline *gla, gline *glb);
int gline_match_nick(gline *gl, nick *np);
int gline_match_channel(gline *gl, channel *cp);
int isglinesane(gline *gl, const char **hint);
gline *glinedup(gline *gl);

/* glines_formats.c */
gline *makegline(const char *);
char *glinetostring(gline *g);

/* glines_util.c */
int glinebyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int duration, const char *reason, int flags, const char *creator);
int glinebynick(nick *np, int duration, const char *reason, int flags, const char *creator);
void glineunsetmask(const char *mask);

/* glines_buf.c */
void glinebufinit(glinebuf *gbuf, int merge);
gline *glinebufadd(glinebuf *gbuf, const char *mask, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime);
void glinebufaddbyip(glinebuf *gbuf, const char *user, struct irc_in_addr *ip, unsigned char bits, int flags, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime);
void glinebufaddbynick(glinebuf *gbuf, nick *, int flags, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime);
void glinebufcounthits(glinebuf *gbuf, int *users, int *channels, nick *spewto);
int glinebufchecksane(glinebuf *gbuf, nick *spewto, int overridesanity, int overridelimit, int spewhits);
void glinebufspew(glinebuf *gbuf, nick *spewto);
void glinebufcommit(glinebuf *gbuf, int propagate);
void glinebufabort(glinebuf *gbuf);
int glinebufundo(int id);
void glinebufcommentf(glinebuf *gbuf, const char *format, ...);
void glinebufcommentv(glinebuf *gbuf, const char *prefix, int cargc, char **cargv);

/* glines_alloc.c */
void freegline(gline *);
gline *newgline();
void removegline(gline *);

/* glines_handler.c */
int handleglinemsg(void *, int, char **);
void handleglinestats(int hooknum, void *arg);

/* glines_store.c */
int glstore_save(void);
int glstore_load(void);

#endif
