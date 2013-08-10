#ifndef __CHANFIX_H
#define __CHANFIX_H

#include "../channel/channel.h"

typedef struct chanfix {
  chanindex      *index;
  array          regops;
} chanfix;

typedef struct regop {
  int            type;       /* CFACCOUNT or CFHOST */
  unsigned long  hash;       /* hash of the user's account or host */
  sstring        *uh;        /* account or user@host if the user has enough points */
  time_t         lastopped;  /* when was he last opped */
  unsigned int   score;      /* chanfix score */
} regop;

extern int cfext;
extern int cfnext;

#define CFAUTOFIX 0
#define CFDEBUG 0 

#if CFDEBUG
#define CFSAMPLEINTERVAL 5
#define CFEXPIREINTERVAL 60
#define CFAUTOSAVEINTERVAL 60
#define CFREMEMBEROPS 10*24*60
#else
#define CFSAMPLEINTERVAL 300
#define CFEXPIREINTERVAL 3600
#define CFAUTOSAVEINTERVAL 3600
#define CFREMEMBEROPS 10*24*3600
#endif

/* we won't track scores for channels which have
   less users than this */
#define CFMINUSERS 4
/* if you lose a channel after 30 minutes then
   you really don't need a channel at all */
#define CFMINSCORE 6
/* chanfix won't ever reop more users than this */
#define CFMAXOPS 10
/* where we store our chanfix data */
#define CFSTORAGE "data/chanfix"
/* how many chanfix files we have */
#define CFSAVEFILES 5
/* maximum number of servers which may be split */
#define CFMAXSPLITSERVERS 10

/* track user by account */
#define CFACCOUNT 0x1
/* track user by ident@host */
#define CFHOST    0x2

/* channel was fixed */
#define CFX_FIXED 0
/* no chanfix information available */
#define CFX_NOCHANFIX 1
/* channel was fixed but less than CFMAXOPS were opped */
#define CFX_FIXEDFEWOPS 2
/* nobody could be found for a reop */
#define CFX_NOUSERSAVAILABLE 3

regop *cf_findregop(nick *np, chanindex *cip, int type);
chanfix *cf_findchanfix(chanindex *cip);
nick *cf_findnick(regop *ro, chanindex *cip);
int cf_wouldreop(nick *np, channel *cp);
int cf_fixchannel(channel *cp);
int cf_getsortedregops(chanfix *cf, int max, regop **list);
int cf_cmpregopnick(regop *ro, nick *np);

#endif /* __CHANFIX_H */
