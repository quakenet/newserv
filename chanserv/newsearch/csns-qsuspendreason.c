/*
 * qsuspendreason functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../chanserv.h"

#include <stdlib.h>

void *qsuspendreason_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void qsuspendreason_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *qsuspendreason_parse(searchCtx *ctx, int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_USER) {
    parseError = "qsuspendreason: this function is only valid for user searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->exe = qsuspendreason_exe;
  thenode->free = qsuspendreason_free;

  return thenode;
}

void *qsuspendreason_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  authname *ap = (authname *)theinput;
  reguser *rup = ap->exts[chanservext];
  if(!rup || !UHasSuspension(rup) || !rup->suspendreason)
    return "";

  return rup->suspendreason->content;
}

void qsuspendreason_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

