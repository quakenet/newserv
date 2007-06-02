/*
 * chanservdb_alloc.c:
 *  Handles allocation of the various chanserv structures.
 */

#include "../chanserv.h"
#include <stdlib.h>

#define ALLOCUNIT 100

regchan     *csfreechans;
reguser     *csfreeusers;
regchanuser *csfreechanusers;
nicklist    *csfreenicklists;
regban      *csfreeregbans;
activeuser  *csfreeactiveusers;

void *csmallocs;

void chanservallocinit() {
  csfreechans=NULL;
  csfreeusers=NULL;
  csfreechanusers=NULL;
  csfreenicklists=NULL;
  csfreeregbans=NULL;
  csfreeactiveusers=NULL;
  csmallocs=NULL;
}

void *csmalloc(size_t size) {
  void **mem;
  
  /* Get the requested memory, with one extra pointer at the beginning */
  mem=(void **)malloc(size+sizeof(void *));

  /* Set the first word to point at the last thing we got */
  *mem=csmallocs;

  /* Now set the "last chunk" pointer to the address of this one */
  csmallocs=(void *)mem;
  
  /* return the rest of the memory to the caller */
  return (void *)(mem+1);
}

/*
 * csfreeall():
 *  Free all the memory we allocated for chanserv structures.
 */

void csfreeall() {
  void *vp,**vh;

  vp=csmallocs;

  while (vp!=NULL) {
    vh=(void **)vp;
    vp=*vh;
    free ((void *)vh);
  }
}

regchan *getregchan() {
  int i;
  regchan *rcp;
  
  if (csfreechans==NULL) {
    csfreechans=(regchan *)csmalloc(ALLOCUNIT*sizeof(regchan));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreechans[i].index=(chanindex *)&(csfreechans[i+1]);
    }
    csfreechans[ALLOCUNIT-1].index=NULL;
  }

  rcp=csfreechans;
  csfreechans=(regchan *)rcp->index;
  
  tagregchan(rcp);

  return rcp;
}

void freeregchan(regchan *rcp) {
  verifyregchan(rcp);
  rcp->index=(chanindex *)csfreechans;
  csfreechans=rcp;
}

reguser *getreguser() {
  int i;
  reguser *rup;

  if (csfreeusers==NULL) {
    csfreeusers=(reguser *)csmalloc(ALLOCUNIT*sizeof(reguser));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreeusers[i].nextbyname=&(csfreeusers[i+1]);
    }
    csfreeusers[ALLOCUNIT-1].nextbyname=NULL;
  }
  
  rup=csfreeusers;
  csfreeusers=rup->nextbyname;
  
  tagreguser(rup);
  
  return rup;
}

void freereguser(reguser *rup) {
  verifyreguser(rup);
  rup->nextbyname=csfreeusers;
  csfreeusers=rup;
}

regchanuser *getregchanuser() {
  int i;
  regchanuser *rcup;

  if (csfreechanusers==NULL) {
    csfreechanusers=(regchanuser *)csmalloc(ALLOCUNIT*sizeof(regchanuser));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreechanusers[i].nextbyuser=&(csfreechanusers[i+1]);
    }
    csfreechanusers[ALLOCUNIT-1].nextbyuser=NULL;
  }

  rcup=csfreechanusers;
  csfreechanusers=rcup->nextbyuser;

  tagregchanuser(rcup);

  return rcup;
}

void freeregchanuser(regchanuser *rcup) {
  verifyregchanuser(rcup);
  rcup->nextbyuser=csfreechanusers;
  csfreechanusers=rcup;
}

nicklist *getnicklist() {
  int i;
  nicklist *nlp;
  
  if (csfreenicklists==NULL) {
    csfreenicklists=(nicklist *)csmalloc(ALLOCUNIT*sizeof(nicklist));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreenicklists[i].next=&(csfreenicklists[i+1]);
    }
    csfreenicklists[ALLOCUNIT-1].next=NULL;
  }
  
  nlp=csfreenicklists;
  csfreenicklists=nlp->next;
  
  return nlp;
}

void freenicklist(nicklist *nlp) {
  nlp->next=csfreenicklists;
  csfreenicklists=nlp;
}

regban *getregban() {
  int i;
  regban *rbp;
  
  if (csfreeregbans==NULL) {
    csfreeregbans=(regban *)csmalloc(ALLOCUNIT*sizeof(regban));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreeregbans[i].next=&(csfreeregbans[i+1]);
    }
    csfreeregbans[ALLOCUNIT-1].next=NULL;
  }
  
  rbp=csfreeregbans;
  csfreeregbans=rbp->next;

  tagregchanban(rbp);
  
  return rbp;
}

void freeregban(regban *rbp) {
  verifyregchanban(rbp);
  rbp->next=csfreeregbans;
  csfreeregbans=rbp;
}

activeuser *getactiveuser() {
  int i;
  activeuser *aup;

  if (csfreeactiveusers==NULL) {
    csfreeactiveusers=(activeuser *)csmalloc(ALLOCUNIT*sizeof(activeuser));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreeactiveusers[i].next=&(csfreeactiveusers[i+1]);
    }
    csfreeactiveusers[ALLOCUNIT-1].next=NULL;
  }
  
  aup=csfreeactiveusers;
  csfreeactiveusers=aup->next;
  
  tagactiveuser(aup);
  
  aup->rup=NULL;
  aup->authattempts=0;
  
  return aup;
}

void freeactiveuser(activeuser *aup) {
  verifyactiveuser(aup);
  aup->next=csfreeactiveusers;
  csfreeactiveusers=aup;
}
