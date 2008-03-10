/*
 * chanservdb_alloc.c:
 *  Handles allocation of the various chanserv structures.
 */

#include "../chanserv.h"
#include "../../core/nsmalloc.h"

#include <stdlib.h>

#define ALLOCUNIT 100

regchan     *csfreechans;
reguser     *csfreeusers;
regchanuser *csfreechanusers;
regban      *csfreeregbans;
activeuser  *csfreeactiveusers;
maildomain  *csfreemaildomains;

void *csmallocs;

void chanservallocinit() {
  csfreechans=NULL;
  csfreeusers=NULL;
  csfreechanusers=NULL;
  csfreeregbans=NULL;
  csfreeactiveusers=NULL;
}

regchan *getregchan() {
  int i;
  regchan *rcp;
  
  if (csfreechans==NULL) {
    csfreechans=(regchan *)nsmalloc(POOL_CHANSERVDB,ALLOCUNIT*sizeof(regchan));
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
    csfreeusers=(reguser *)nsmalloc(POOL_CHANSERVDB,ALLOCUNIT*sizeof(reguser));
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
    csfreechanusers=(regchanuser *)nsmalloc(POOL_CHANSERVDB,ALLOCUNIT*sizeof(regchanuser));
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

regban *getregban() {
  int i;
  regban *rbp;
  
  if (csfreeregbans==NULL) {
    csfreeregbans=(regban *)nsmalloc(POOL_CHANSERVDB,ALLOCUNIT*sizeof(regban));
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
    csfreeactiveusers=(activeuser *)nsmalloc(POOL_CHANSERVDB,ALLOCUNIT*sizeof(activeuser));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreeactiveusers[i].next=&(csfreeactiveusers[i+1]);
    }
    csfreeactiveusers[ALLOCUNIT-1].next=NULL;
  }
  
  aup=csfreeactiveusers;
  csfreeactiveusers=aup->next;
  
  tagactiveuser(aup);
  
  aup->authattempts=0;
  aup->helloattempts=0;
  aup->entropyttl=0;
  
  return aup;
}

void freeactiveuser(activeuser *aup) {
  verifyactiveuser(aup);
  aup->next=csfreeactiveusers;
  csfreeactiveusers=aup;
}

maildomain *getmaildomain() {
  int i;
  maildomain *mdp;

  if (csfreemaildomains==NULL) {
    csfreemaildomains=(maildomain *)nsmalloc(POOL_CHANSERVDB,ALLOCUNIT*sizeof(maildomain));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      csfreemaildomains[i].nextbyname=&(csfreemaildomains[i+1]);
    }
    csfreemaildomains[ALLOCUNIT-1].nextbyname=NULL;
  }

  mdp=csfreemaildomains;
  csfreemaildomains=mdp->nextbyname;

  tagmaildomain(mdp);

  return mdp;
}

void freemaildomain(maildomain *mdp) {
  verifymaildomain(mdp);
  mdp->nextbyname=csfreemaildomains;
  csfreemaildomains=mdp;
}

maillock *getmaillock() {
  /* we don't create too many of these */
  maillock *mlp = nsmalloc(POOL_CHANSERVDB,sizeof(maillock));
  tagmaillock(mlp);

  return mlp;
}

void freemaillock(maillock *mlp) {
  verifymaillock(mlp);

  freesstring(mlp->pattern);
  freesstring(mlp->reason);
  free(mlp);
}
