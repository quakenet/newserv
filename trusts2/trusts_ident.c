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
      controlwall(NO_OPER, NL_TRUSTS, "TG %lu: Exceeded Ident Limit (%d/%d) for %s!%s@%s (%s)",tg->id, identcnt->currenton+1, tg->maxperident, np->nick, np->ident, np->host->name->content, removeusers == 1 ? "disconnected": "ignored");
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

