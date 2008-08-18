#ifndef __regexgline_H
#define __regexgline_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <pcre.h>
#include <string.h>
#include <strings.h>

#include "../core/config.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"
#include "../lib/sstring.h"
#include "../core/schedule.h"

#define RG_QUERY_BUF_SIZE         5120
#define RG_MAX_CASUALTIES_DEFAULT 5000
#define RG_MAX_SPEW_DEFAULT       300
#define RG_REGEXGLINE_MAX         512
#define RG_REASON_MAX             255
#define RG_EXPIRY_BUFFER          200
#define RG_MASKLEN                HOSTLEN + USERLEN + NICKLEN + REALLEN + 5 /* includes NULL terminator */
#define RG_PCREFLAGS              PCRE_CASELESS
#define RG_MIN_MASK_LEN           5
#define RG_MAX_PER_GLINE_DEFAULT  5
#define RG_MINIMUM_DELAY_TIME     5
#define RG_MAXIMUM_RAND_TIME      15
#define RG_EXPIRY_TIME_DEFAULT    1800
#define RG_NETWORK_WIDE_MAX_GLINES_PER_8_SEC 625*8 /* 5000 / 8 */

#define RGStringise(x)            #x
#define RGBuildHostname(buf, np)  snprintf(buf, sizeof(buf), "%s!%s@%s\r%s", np->nick, np->ident, np->host->name->content, np->realname->name->content);
#define RGMasksEqual(a, b)        !strcasecmp(a, b)

typedef struct rg_struct {
  int              id;        /* database id */
  sstring          *mask;     /* gline mask */
  sstring          *setby;    /* who it's set by */
  sstring          *reason;   /* reason for gline */
  time_t            expires;  /* when it expires */
  int               type;     /* gline type (user@ip or *@ip) */
  pcre             *regex;    /* pcre expression */
  pcre_extra       *hint;     /* pcre hint       */
  long             glineid;   /* gline ID */
  const char       *class;    /* class of gline */
  unsigned long    hits;      /* hits since we were loaded */
  struct rg_struct *next;     /* ... pointer to next item */
} rg_struct;

struct rg_struct *rg_list = NULL;

int rg_max_casualties, rg_max_spew, rg_expiry_time, rg_max_per_gline;
void *rg_schedule = NULL;

void rg_nick(int hooknum, void *arg);
void rg_lostnick(int hooknum, void *arg);
void rg_startup(void);

int rg_gline(void *source, int cargc, char **cargv);
int rg_delgline(void *source, int cargc, char **cargv);
int rg_glist(void *source, int cargc, char **cargv);
int rg_idlist(void *source, int cargc, char **cargv);
int rg_spew(void *source, int cargc, char **cargv);
int rg_sanitycheck(char *mask, int *count);

int rg_dbconnect(void);
void rg_dbload(void);

void rg_freestruct(struct rg_struct *rp);
struct rg_struct *rg_newstruct(time_t expires);
struct rg_struct *rg_newsstruct(unsigned long id, char *mask, char *setby, char *reason, char *expires, char *type, time_t iexpires, char *class);

void rg_displaygline(nick *np, struct rg_struct *rp);

void rg_checkexpiry(void *arg);

void rg_logevent(nick *np, char *event, char *details, ...);
void rg_loggline(struct rg_struct *rg, nick *np);

unsigned char rc_hexlookup[256] = {
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01,
                       0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
                       0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c,
                       0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff
                                  };

#endif
