/* server.h */

#ifndef __SERVER_H
#define __SERVER_H

#include "../lib/sstring.h"
#include "../irc/irc_config.h"

#define LS_INVALID   0   /* No server here */
#define LS_LINKED    1   /* Server fully linked */
#define LS_LINKING   2   /* This actual server is bursting */
#define LS_PLINKING  3   /* Some other server between here and there is bursting */
#define LS_SQUIT     4   /* This server is being deleted due to a SQUIT */

typedef struct {
  sstring   *name;
  sstring   *description;
  short      parent;
  short      linkstate;
  int        maxusernum;
} server;

extern server serverlist[MAXSERVERS];

int handleservermsg(void *source, int cargc, char **cargv);
int handleeobmsg(void *source, int cargc, char **argv);
int handlesquitmsg(void *source, int cargc, char **cargv);
void handledisconnect(int hooknum, void *arg);
void deleteserver(int servernum);
int findserver(const char *name);

#endif
