#ifndef __PATRICIANICKS_H
#define __PATRICIANICKS_H

#include "../nick/nick.h"

#define PATRICIANICK_HASHSIZE   5
#define pn_getidenthash(x)      ((crc32i(x)) % PATRICIANICK_HASHSIZE)
#define PATRICIANICK_MAXRESULTS 1000

typedef struct patricianick_s {
  nick *identhash[PATRICIANICK_HASHSIZE];
} patricianick_t;

extern int pnode_ext;
extern int pnick_ext;

void pn_hook_newuser(int hook, void *arg);
void pn_hook_lostuser(int hook, void *arg);
void pn_hook_movenodeuser(int hook, void *arg);

void addnicktonode(patricia_node_t *node, nick *nick);
void deletenickfromnode(patricia_node_t *node, nick *nick);
void freepatricianick(patricianick_t *pnp);
#endif
