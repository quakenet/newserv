
#ifndef __CHANSTATS_H
#define __CHANSTATS_H

#include <time.h>
#include "../lib/sstring.h"
#include "../channel/channel.h"
#include "../parser/parser.h"

#define SAVEINTERVAL            3600
#define SAMPLEINTERVAL          360
#define SAMPLEHISTORY           10
#define HISTORYDAYS             14

/* The main stats structure.  Everything except the "lastsamples" array needs to be saved/restored */

typedef struct chanstats {
  chanindex        *index;                       /* Channel index pointer */
  unsigned short    lastdays[HISTORYDAYS];       /* Average user counts for the last 14 days, stored as (average users * 10) */
  unsigned char     lastdaysamples[HISTORYDAYS]; /* Number of samples taken on each of the last 14 days */
  unsigned short    lastmax[HISTORYDAYS];        /* Max users seen on each of the last 14 days */
  unsigned short    lastsamples[SAMPLEHISTORY];  /* Number of users for the last 10 samples (i.e. 1 hour) */
  unsigned char     todaysamples;                /* Number of samples taken today */
  unsigned int      todayusers;                  /* Total of all of today's samples */
  unsigned short    todaymax;                    /* Max users seen today */
  time_t            lastsampled;                 /* When this channel was last sampled */
  struct chanstats *nextsorted;                  /* Next channel in sorted order */
} chanstats;

/* These sample counts need to be saved and restored */
extern unsigned int totaltodaysamples;
extern unsigned int totallastdaysamples[HISTORYDAYS];

extern int csext;

extern int uponehour;
extern unsigned int lastdaysamples[HISTORYDAYS];
extern unsigned int todaysamples;

extern time_t chanstats_lastsample;

/* chanstatshash.c */
chanstats *findchanstats(const char *channame);
chanstats *findchanstatsifexists(const char *channame);

/* chanstatsalloc.c */
void cstsfreeall();
chanstats *getchanstats();
void freechanstats(chanstats *csp);

/* chansearch.c */
void initchansearch();
void finichansearch();
void regchansearchfunc(const char *name, int args, CommandHandler handler);
void unregchansearchfunc(const char *name, CommandHandler handler);
int dochansearch(void *source, int cargc, char **cargv);
  
#endif
