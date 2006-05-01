#ifndef __SPLITLIST_H
#define __SPLITLIST_H

#include <time.h>
#include "../server/server.h"
#include "../lib/array.h"

typedef struct {
  sstring        *name;      /* name of the server */
  time_t         ts;         /* timestamp of the split */
} splitserver;

extern array splitlist;

int sp_countsplitservers(void);
int sp_issplitserver(const char *name);
void sp_deletesplit(const char *name);
void sp_addsplit(const char *name, time_t ts);
int sp_countsplitservers(void);

#endif /* __SPLITLIST_H */
