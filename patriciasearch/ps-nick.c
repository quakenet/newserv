#include "patriciasearch.h"

#include <stdio.h>
#include <stdlib.h>

void *ps_nick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void ps_nick_free(searchCtx *ctx, struct searchNode *thenode);

struct nick_localdata {
  nick *np;
};

struct searchNode *ps_nick_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;
  struct nick_localdata *localdata;

  if (!(localdata=(struct nick_localdata *)malloc(sizeof(struct nick_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  if (argc!=1) {
    parseError="nick: usage: (nick target)";
    free(localdata);
    return NULL;
  }
  if ((localdata->np=getnickbynick(argv[0]))==NULL) {
    parseError="nick: unknown nickname";
    free(localdata);
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = ps_nick_exe;
  thenode->free = ps_nick_free;

  return thenode;
}

void *ps_nick_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct nick_localdata *localdata;
  patricia_node_t *node;
  nick *np;
  patricianick_t *pnp;
  int i;

  localdata = thenode->localdata;
  node = (patricia_node_t *)theinput;

  if (pnode_ext && node->exts[pnode_ext] ) {
    pnp = node->exts[pnode_ext];
    for (i = 0; i < PATRICIANICK_HASHSIZE; i++) {
      for (np = pnp->identhash[i]; np; np=np->exts[pnick_ext]) {
        if (np == localdata->np)
          return (void *)1;
      }
    }
  }
  return (void *)0;
}

void ps_nick_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}
