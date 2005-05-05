/* new H accounts */
#ifndef HACCOUNT_H
#define HACCOUNT_H

#include <time.h>

#include "../lib/sstring.h"

#include "../nick/nick.h"

#include "hdef.h"
#include "hstat.h"

enum
{
    H_ALL_PRIVMSG  = 1 << 0,
    H_ALL_NOTICE   = 1 << 1,
    H_NOSPAM       = 1 << 2,
    H_AUTO_OP      = 1 << 3,
    H_AUTO_VOICE   = 1 << 4,
    H_EXPIRES      = 1 << 31
};

#define H_ACCFLAGS_DEFAULT 0

#define HACCOUNT_CONF_COUNT 4

typedef struct haccount_struct
{
    sstring *name;
    int flags;
    hlevel level;

    time_t last_activity;
    hstat_account *stats;

    struct haccount_struct *next;
} haccount;

extern struct haccount_struct *haccounts;

haccount *haccount_get_by_name(const char*);
haccount *haccount_add(const char*, hlevel);
int haccount_del(haccount*);

void haccount_clear_inactives(void);

void haccount_set_level(haccount*, hlevel);

const char *haccount_get_state(haccount*, int);
const char *haccount_get_sname(int);

int haccount_count(hlevel);

#endif
