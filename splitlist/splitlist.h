#ifndef __SPLITLIST_H
#define __SPLITLIST_H

#include <time.h>
#include "../server/server.h"
#include "../serverlist/serverlist.h"
#include "../lib/array.h"
#include "../lib/flags.h"

typedef struct {
  sstring        *name;      /* name of the server */
  time_t         ts;         /* timestamp of the split */
  flag_t         type;
} splitserver;

extern array splitlist;

void sp_deletesplit(const char *name);
int sp_countsplitservers(flag_t orflags);
/* I don't see why these are exported... */
/*
int sp_issplitserver(const char *name);
void sp_addsplit(const char *name, time_t ts);
*/

#endif /* __SPLITLIST_H */
