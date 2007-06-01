#ifndef __AUTHEXT_H
#define __AUTHEXT_H

#define MAXAUTHNAMEEXTS 5

struct nick;

typedef struct authname {
  unsigned long userid;
  int usercount;
  unsigned int marker;
  struct nick *nicks;
  struct authname *next;
  /* These are extensions only used by other modules */
  void *exts[MAXAUTHNAMEEXTS];
} authname;

#define AUTHNAMEHASHSIZE  60000

extern authname *authnametable[AUTHNAMEHASHSIZE];

/* Allocators */
authname *newauthname();
void freeauthname (authname *hp);

/* EXT management */
int registerauthnameext(const char *name);
int findauthnameext(const char *name);
void releaseauthnameext(int index);

/* Actual user commands */
authname *findauthname(unsigned long userid);
authname *findorcreateauthname(unsigned long userid);
void releaseauthname(authname *anp);

/* Marker */
unsigned int nextauthnamemarker();

#endif
