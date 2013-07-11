#ifndef __GLINES_H
#define __GLINES_H

#include "../lib/sstring.h"
#include "../nick/nick.h"
#include "../channel/channel.h"

#define SNIRCD_13
#undef SNIRCD_14

#define MAXGLINEUSERS        100

#define MAXUSERGLINEDURATION (90 * 86400)
#define MINUSERGLINEREASON 10

#define GLINE_IGNORE_TRUST   1
#define GLINE_ALWAYS_NICK    2
#define GLINE_ALWAYS_USER    4
#define GLINE_NO_LIMIT       8
#define GLINE_SIMULATE       16

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
#define GLINE_ACTIVATE    0x004000  /* Gline should be activated */
#define GLINE_DEACTIVATE  0x008000  /* Gline should be deactivated */
#define GLINE_ACTIVE      0x010000  /* Gline is active */
#define GLINE_IPMASK      0x020000  /* Gline includes an CIDR mask */
#define GLINE_HOST        0x040000
#define GLINE_BADMASK     0x100000

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

extern gline *glinelist;

typedef struct gline_params {
  int duration;
  const char *reason;
  const char *creator;
} gline_params;

typedef void (*gline_callback)(const char *, int, void *);

/* glines.c */
void glinesetmask(const char *mask, int duration, const char *reason, const char *creator);
void glineunsetmask(const char *mask);

int glinesuggestbyip(const char *, struct irc_in_addr *, unsigned char, int, gline_callback callback, void *uarg);
int glinesuggestbynick(nick *, int, gline_callback callback, void *uarg);
int glinebyip(const char *, struct irc_in_addr *, unsigned char, int, const char *, int, const char *);
int glinebynick(nick *, int, const char *, int, const char *);

gline *gline_add(const char *creator, const char *mask, const char *reason, time_t expire, time_t lastmod, time_t lifetime);
char *glinetostring(gline *g);
gline *gline_find(const char *);
gline *makegline(const char *);
void gline_propagate(gline *);
gline *gline_deactivate(gline *, time_t, int);
gline *gline_activate(gline *agline, time_t lastmod, int propagate);
int glineequal(gline *, gline *);
int gline_count_hits(gline *gl);
int gline_match_mask(gline *gla, gline *glb);
int gline_match_nick(gline *gl, nick *np);
int gline_match_channel(gline *gl, channel *cp);

/* glines_alloc.c */
void freegline(gline *);
gline *newgline();
void removegline(gline *);

/* glines_handler.c */
int handleglinemsg(void *, int, char **);
void handleglinestats(int hooknum, void *arg);

#endif
