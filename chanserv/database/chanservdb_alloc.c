/*
 * chanservdb_alloc.c:
 *  Handles allocation of the various chanserv structures.
 */

#include "../chanserv.h"
#include "../../core/nsmalloc.h"

regchan *getregchan() {
  regchan *rcp = nsmalloc(POOL_CHANSERVDB, sizeof(regchan));

  if (!rcp)
    return NULL;  
  
  tagregchan(rcp);

  return rcp;
}

void freeregchan(regchan *rcp) {
  verifyregchan(rcp);
  nsfree(POOL_CHANSERVDB, rcp);
}

reguser *getreguser() {
  reguser *rup = nsmalloc(POOL_CHANSERVDB, sizeof(reguser));

  if (!rup)
    return NULL;
  
  tagreguser(rup);
  
  return rup;
}

void freereguser(reguser *rup) {
  verifyreguser(rup);
  nsfree(POOL_CHANSERVDB, rup);
}

regchanuser *getregchanuser() {
  regchanuser *rcup = nsmalloc(POOL_CHANSERVDB, sizeof(regchanuser));

  if (!rcup)
    return NULL;

  tagregchanuser(rcup);

  return rcup;
}

void freeregchanuser(regchanuser *rcup) {
  verifyregchanuser(rcup);
  nsfree(POOL_CHANSERVDB, rcup);
}

regban *getregban() {
  regban *rbp = nsmalloc(POOL_CHANSERVDB, sizeof(regban));

  if (!rbp)
    return NULL;

  tagregchanban(rbp);
  
  return rbp;
}

void freeregban(regban *rbp) {
  verifyregchanban(rbp);
  nsfree(POOL_CHANSERVDB, rbp);
}

activeuser *getactiveuser() {
  activeuser *aup = nsmalloc(POOL_CHANSERVDB, sizeof(activeuser));

  if (!aup)
    return NULL;

  tagactiveuser(aup);
  
  aup->authattempts=0;
  aup->helloattempts=0;
  aup->entropyttl=0;
  
  return aup;
}

void freeactiveuser(activeuser *aup) {
  verifyactiveuser(aup);
  nsfree(POOL_CHANSERVDB, aup);
}

maildomain *getmaildomain() {
  maildomain *mdp = nsmalloc(POOL_CHANSERVDB, sizeof(maildomain));

  if (!mdp)
    return NULL;

  tagmaildomain(mdp);

  return mdp;
}

void freemaildomain(maildomain *mdp) {
  verifymaildomain(mdp);
  nsfree(POOL_CHANSERVDB, mdp);
}

maillock *getmaillock() {
  maillock *mlp = nsmalloc(POOL_CHANSERVDB, sizeof(maillock));

  if (!mlp)
    return NULL;

  tagmaillock(mlp);

  return mlp;
}

void freemaillock(maillock *mlp) {
  verifymaillock(mlp);

  freesstring(mlp->pattern);
  freesstring(mlp->reason);
  nsfree(POOL_CHANSERVDB, mlp);
}
