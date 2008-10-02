/*
 * tgroup functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../trusts.h"

#include <stdlib.h>

void *tgroup_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void tgroup_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *tgroup_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode, *arg;
  trustgroup *tg;
  char *p;
	
  if(argc < 1) {
    parseError = "tgroup: usage: tgroup <#id|name|id>";
    return NULL;
  }

  if(!(arg=argtoconststr("tgroup", ctx, argv[0], &p)))
    return NULL;

  tg = tg_strtotg(p);
  (arg->free)(ctx, arg);
  if(!tg) {
    parseError = "tgroup: unknown group";
    return NULL;
  }

  if(!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe = tgroup_exe;
  thenode->free = tgroup_free;
  thenode->localdata = tg;

  return thenode;
}

void *tgroup_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = theinput;
  trusthost *th = gettrusthost(np);
  trustgroup *tg = thenode->localdata;

  if(th && (th->group == tg))
    return (void *)1;

  return (void *)0;
}

void tgroup_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

