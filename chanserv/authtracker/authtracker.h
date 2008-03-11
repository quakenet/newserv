#ifndef AUTHTRACKER_H
#define AUTHTRACKER_H

#include "../../nick/nick.h"
#include "../../pqsql/pqsql.h"

#include <time.h>

extern PQModuleIdentifier authtrackerpq;

#define AT_NETSPLIT	0	/* User lost in netsplit */
#define AT_RESTART	1	/* Dangling session found at restart */

/* authtracker_query.c */
void at_logquit(unsigned long userid, time_t accountts, time_t time, char *reason);
void at_lognewsession(unsigned int userid, nick *np);
void at_finddanglingsessions();

/* authtracker_db.c */
void at_lostnick(unsigned int numeric, unsigned long userid, time_t accountts, time_t losttime, int reason);
int at_foundnick(unsigned int numeric, unsigned long userid, time_t accountts);
void at_serverback(unsigned int server);
void at_flushghosts();

/* authtracker_hooks.c */
unsigned long at_getuserid(nick *np);
void at_hookinit();
void at_hookfini();

/* authtracker.c */
void at_dbloaded(int hooknum, void *arg);


#endif
