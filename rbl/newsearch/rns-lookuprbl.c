/*
 * rbl functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../rbl.h"

#include <stdlib.h>
#include <string.h>

void *rbl_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void rbl_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *rbl_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode, *arg;
  rbl_instance *rbl;
  char *p;
	
  if(argc < 1) {
    parseError = "rbl: usage: rbl <rbl-name>";
    return NULL;
  }

  if(!(arg=argtoconststr("rbl", ctx, argv[0], &p)))
    return NULL;

  for (rbl = rbl_instances; rbl; rbl = rbl->next)
    if (strcmp(rbl->name->content, p) == 0)
      break;

  if (!rbl) {
    parseError = "rbl: unknown rbl name";
    return NULL;
  }

  if(!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->exe = rbl_exe;
  thenode->free = rbl_free;
  thenode->localdata = rbl;

  return thenode;
}

void *rbl_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = theinput;
  rbl_instance *rbl = thenode->localdata;

  if (RBL_LOOKUP(rbl, &np->ipaddress, NULL, 0) > 0)
    return (void *)1;

  return (void *)0;
}

void rbl_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

