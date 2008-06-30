#ifndef __AUTHEXT_H
#define __AUTHEXT_H

#include "../lib/flags.h"

#define MAXAUTHNAMEEXTS 5

struct nick;

typedef struct authname {
  unsigned long userid;
  int usercount;
  unsigned int marker;
  struct nick *nicks;
  struct authname *next, *nextbyname;
  unsigned int namebucket;
  flag_t flags;
  /* These are extensions only used by other modules */
  void *exts[MAXAUTHNAMEEXTS];
} authname;

#define AUTHNAMEHASHSIZE  60000

extern authname *authnametable[AUTHNAMEHASHSIZE];

/* Allocators */
authname *newauthname(void);
void freeauthname (authname *hp);

/* EXT management */
int registerauthnameext(const char *name);
int findauthnameext(const char *name);
void releaseauthnameext(int index);

/* Actual user commands */
authname *findauthname(unsigned long userid);
authname *findauthnamebyname(const char *name);
authname *findorcreateauthname(unsigned long userid, const char *name);
void releaseauthname(authname *anp);

/* Marker */
unsigned int nextauthnamemarker(void);

authname *getauthbyname(const char *name);

#endif
