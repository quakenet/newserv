/*
 * exists functionality 
 */

#include "../../newsearch/newsearch.h"
#include "../../lib/flags.h"
#include "../chanserv.h"

#include <stdio.h>
#include <stdlib.h>

void *qusers_exe(searchCtx *, struct searchNode *thenode, void *theinput);
void qusers_free(searchCtx *, struct searchNode *thenode);

struct qusers_localdata {
  flag_t setmodes;
  flag_t clearmodes;
};

struct searchNode *qusers_parse(searchCtx *, int type, int argc, char **argv) {
  struct searchNode *thenode;
  struct qusers_localdata *localdata;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "qusers: this function is only valid for channel searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->localdata=localdata=malloc(sizeof(struct qusers_localdata));
  thenode->returntype = RETURNTYPE_INT;
  thenode->exe = qusers_exe;
  thenode->free = qusers_free;

  if (argc==0) {
    localdata->setmodes=0;
    localdata->clearmodes=0;
  } else {
    localdata->setmodes=0;
    localdata->clearmodes=~0;
    
    setflags(&(localdata->setmodes), QCUFLAG_ALL, argv[0], rcuflags, REJECT_NONE);
    setflags(&(localdata->clearmodes), QCUFLAG_ALL, argv[0], rcuflags, REJECT_NONE);
    
    localdata->clearmodes = ~localdata->clearmodes;
  }

  return thenode;
}

void *qusers_exe(searchCtx *, struct searchNode *thenode, void *theinput) {
  chanindex *cip = (chanindex *)theinput;
  regchan *rcp;
  regchanuser *rcup;
  struct qusers_localdata *localdata=thenode->localdata;
  unsigned long i,count=0;
  
  if (!(rcp=cip->exts[chanservext]))
    return (void *)0;
  
  for (i=0;i<REGCHANUSERHASHSIZE;i++) {
    for (rcup=rcp->regusers[i];rcup;rcup=rcup->nextbychan) {
      if ((rcup->flags & localdata->setmodes) != localdata->setmodes)
        continue;
      
      if (rcup->flags & localdata->clearmodes)
        continue;
      
      count++;
    }
  }
  
  return (void *)count;
}

void qusers_free(searchCtx *, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

