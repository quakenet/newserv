#include "trusts.h"

trustgroupidentcount_t *getnewtrustgroupidentcount(trustgroup_t *tg, char *ident) {
  trustgroupidentcount_t *tgic = newtrustgroupidentcount();
  tgic->ident = getsstring(ident,USERLEN);
  tgic->currenton = 1;
  tgic->trustgroup = tg;

  trusts_addtrustgroupidenttohash(tgic);
  return tgic;
}

void increment_ident_count(nick *np, trustgroup_t *tg) {
  trustgroupidentcount_t* identcnt;
  identcnt = findtrustgroupcountbyident(np->ident,tg);
  if(identcnt) {
    /* ident exists */
    if( tg->maxperident && (identcnt->currenton+1) > tg->maxperident) {
      trust_debug("NEWNICK TRUSTED BAD USER: Exceeding User (%s) Ident Limit (%d/%d)",np->ident, identcnt->currenton+1, tg->maxperident);
      //killuser(NULL, np, "USER: Exceeding User Ident Limit.");
    }
    identcnt->currenton++;
  } else {
    /* we have a new user to add */
    getnewtrustgroupidentcount( tg, np->ident );
  }
}

void decrement_ident_count(nick *np, trustgroup_t *tg) {
  trustgroupidentcount_t* identcnt;
  identcnt = findtrustgroupcountbyident(np->ident,tg);
  if(identcnt) {
    identcnt->currenton--;
    if(identcnt->currenton == 0) {
      trusts_removetrustgroupidentfromhash(identcnt);
      if (identcnt->ident) {
        freesstring(identcnt->ident);
      }
      freetrustgroupidentcount(identcnt);
    }
  }
}

