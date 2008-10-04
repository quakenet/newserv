#include "trusts.h"

trustgroup_t *createtrustgroupfromdb(unsigned long id,
 unsigned long maxusage, unsigned long maxclones, unsigned long maxperident, unsigned short maxperip, int enforceident, time_t startdate, time_t lastused, time_t expire, unsigned long ownerid, int type, time_t created, time_t modified){
  trustgroup_t *tg = createtrustgroup(id, maxclones, maxperident, maxperip, enforceident, expire, ownerid,type);
  tg->maxusage = maxusage;
  // currenton
  tg->startdate = startdate;
  tg->lastused = lastused;
  tg->created = created;
  tg->modified = modified;
  return tg;
}
trustgroup_t *createtrustgroup(unsigned long id, unsigned long maxclones, unsigned long maxperident, unsigned short maxperip, int enforceident, time_t expire, unsigned long ownerid, int type) {
  trustgroup_t *tg = newtrustgroup();

  time_t timenow = getnettime();

  tg->id = id;
  tg->maxusage = 0;
  tg->currenton = 0;
  tg->maxclones = maxclones;
  tg->maxperident = maxperident;
  tg->maxperip = maxperip;
  tg->enforceident = enforceident;
  tg->startdate = timenow;
  tg->lastused = 0;
  tg->expire = expire;
  tg->ownerid = ownerid;
  tg->type = type;
  tg->created = timenow;
  tg->modified = timenow;
  trusts_addtrustgrouptohash(tg);
  return tg;
}

void trustgroup_free(trustgroup_t* t)
{
  trusts_removetrustgroupfromhash(t);
  freetrustgroup(t);
}

void trustgroup_expire (trustgroup_t *tg) {
  trusthost_t *thptr, *nthptr;
  /* set expire to 0 so we can call trusthost_expire without it bouncing back */
  tg->expire = 0;

  /* first expiry any trust hosts */
  int hash = trusts_gettrusthostgroupidhash(tg->id);
  thptr = trusthostgroupidtable[hash];
  while (thptr) {
    nthptr = thptr;
    thptr=thptr->nextbygroupid;
    if(nthptr->trustgroup == tg) {
      trusthost_expire(nthptr);
    }
  }

  /* secondly expire trust group */
  controlwall(NO_OPER, NL_TRUSTS, "expired trustgroup #%lu",tg->id); 
  trustsdb_deletetrustgroup(tg);
  trustgroup_free(tg);
}
