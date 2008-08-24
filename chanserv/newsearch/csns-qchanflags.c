/*
 * qchanflags functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../../lib/flags.h"
#include "../chanserv.h"

#include <stdio.h>
#include <stdlib.h>

void *qchanflags_exe(searchCtx *, struct searchNode *thenode, void *theinput);
void qchanflags_free(searchCtx *, struct searchNode *thenode);

struct qchanflags_localdata {
  flag_t setmodes;
  flag_t clearmodes;
};

struct searchNode *qchanflags_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;
  struct qchanflags_localdata *localdata;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->localdata=localdata=malloc(sizeof(struct qchanflags_localdata));
  thenode->returntype = RETURNTYPE_INT;
  thenode->exe = qchanflags_exe;
  thenode->free = qchanflags_free;

  if (argc==0) {
    localdata->setmodes=0;
    localdata->clearmodes=0;
  } else {
    localdata->setmodes=0;
    localdata->clearmodes=~0;
    
    setflags(&(localdata->setmodes), QCFLAG_ALL, argv[0], rcflags, REJECT_NONE);
    setflags(&(localdata->clearmodes), QCFLAG_ALL, argv[0], rcflags, REJECT_NONE);
    
    localdata->clearmodes = ~localdata->clearmodes;
  }

  return thenode;
}

void *qchanflags_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  regchan *rcp;
  struct qchanflags_localdata *localdata=thenode->localdata;
  
  if (!(rcp=cip->exts[chanservext]))
    return (void *)0;
  
  if (((rcp->flags & localdata->setmodes) != localdata->setmodes) || (rcp->flags & localdata->clearmodes))
    return (void *)0;

  return (void *)1;
}

void qchanflags_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

