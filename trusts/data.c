#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../lib/sstring.h"
#include "../core/hooks.h"
#include "../core/nsmalloc.h"
#include "../lib/irc_string.h"
#include "trusts.h"

trustgroup *tglist;

void th_dbupdatecounts(trusthost *);
void tg_dbupdatecounts(trustgroup *);

static trusthost *th_getnextchildbyhost(trusthost *, trusthost *);

void trusts_freeall(void) {
  trustgroup *tg, *ntg;
  trusthost *th, *nth;

  for(tg=tglist;tg;tg=ntg) {
    ntg = tg->next;
    for(th=tg->hosts;th;th=nth) {
      nth = th->next;

      th_free(th);
    }

    tg_free(tg);
  }

  tglist = NULL;
}

trustgroup *tg_getbyid(unsigned int id) {
  trustgroup *tg;

  for(tg=tglist;tg;tg=tg->next)
    if(tg->id == id)
      return tg;

  return NULL;
}

void th_free(trusthost *th) {
  nsfree(POOL_TRUSTS, th);
}

static void th_updatechildren(trusthost *th) {
  trusthost *nth = NULL;

  th->children = NULL;

  for(;;) {
    nth = th_getnextchildbyhost(th, nth);
    if(!nth)
      break;

    nth->nextbychild = th->children;
    th->children = nth;
  }
}

void th_linktree(void) {
  trustgroup *tg;
  trusthost *th;

  /* ugh */
  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      th->parent = th_getsmallestsupersetbyhost(th->ip, th->mask);

  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      if(th->parent)
        th_updatechildren(th->parent);
}

trusthost *th_add(trustgroup *tg, unsigned int id, char *host, unsigned int maxusage, time_t lastseen) {
  uint32_t ip, mask;
  trusthost *th;

  if(!trusts_str2cidr(host, &ip, &mask))
    return NULL;

  th = nsmalloc(POOL_TRUSTS, sizeof(trusthost));
  if(!th)
    return NULL;

  th->id = id;
  th->maxusage = maxusage;
  th->lastseen = lastseen;
  th->ip = ip;
  th->mask = mask;

  th->users = NULL;
  th->group = tg;
  th->count = 0;

  th->parent = NULL;
  th->children = NULL;

  th->marker = 0;

  th->next = tg->hosts;
  tg->hosts = th;

  return th;
}

void tg_free(trustgroup *tg) {
  triggerhook(HOOK_TRUSTS_LOSTGROUP, tg);

  freesstring(tg->name);
  freesstring(tg->createdby);
  freesstring(tg->contact);
  freesstring(tg->comment);
  nsfree(POOL_TRUSTS, tg);
}

trustgroup *tg_add(unsigned int id, char *name, unsigned int trustedfor, int mode, unsigned int maxperident, unsigned int maxusage, time_t expires, time_t lastseen, time_t lastmaxuserreset, char *createdby, char *contact, char *comment) {
  trustgroup *tg = nsmalloc(POOL_TRUSTS, sizeof(trustgroup));
  if(!tg)
    return NULL;

  tg->name = getsstring(name, TRUSTNAMELEN);
  tg->createdby = getsstring(createdby, NICKLEN);
  tg->contact = getsstring(contact, CONTACTLEN);
  tg->comment = getsstring(comment, COMMENTLEN);
  if(!tg->name || !tg->createdby || !tg->contact || !tg->comment) {
    tg_free(tg);
    return NULL;
  }

  tg->id = id;
  tg->trustedfor = trustedfor;
  tg->mode = mode;
  tg->maxperident = maxperident;
  tg->maxusage = maxusage;
  tg->expires = expires;
  tg->lastseen = lastseen;
  tg->lastmaxuserreset = lastmaxuserreset;
  tg->hosts = NULL;

  tg->marker = 0;

  tg->count = 0;

  memset(tg->exts, 0, sizeof(tg->exts));

  tg->next = tglist;
  tglist = tg;

  triggerhook(HOOK_TRUSTS_NEWGROUP, tg);

  return tg;
}

trusthost *th_getbyhost(uint32_t ip) {
  trustgroup *tg;
  trusthost *th, *result = NULL;
  uint32_t mask;

  for(tg=tglist;tg;tg=tg->next) {
    for(th=tg->hosts;th;th=th->next) {
      if((ip & th->mask) == th->ip) {
        if(!result || (th->mask > mask)) {
          mask = th->mask;
          result = th;
        }
      }
    }
  }

  return result;
}

trusthost *th_getbyhostandmask(uint32_t ip, uint32_t mask) {
  trustgroup *tg;
  trusthost *th;

  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      if((th->ip == ip) && (th->mask == mask))
        return th;

  return NULL;
}

/* returns the ip with the smallest prefix that is still a superset of the given host */
trusthost *th_getsmallestsupersetbyhost(uint32_t ip, uint32_t mask) {
  trustgroup *tg;
  trusthost *th, *result = NULL;
  uint32_t smask;

  for(tg=tglist;tg;tg=tg->next) {
    for(th=tg->hosts;th;th=th->next) {
      if(th->ip == (ip & th->mask)) {
        if((th->mask < mask) && (!result || (th->mask > smask))) {
          smask = th->mask;
          result = th;
        }
      }
    }
  }

  return result;
}

/* returns the first ip that is a subset it comes across */
trusthost *th_getsubsetbyhost(uint32_t ip, uint32_t mask) {
  trustgroup *tg;
  trusthost *th;

  for(tg=tglist;tg;tg=tg->next)
    for(th=tg->hosts;th;th=th->next)
      if((th->ip & mask) == ip)
        if(th->mask > mask)
          return th;

  return NULL;
}

/* NOT reentrant obviously */
static trusthost *th_getnextchildbyhost(trusthost *orig, trusthost *th) {
  if(!th) {
    trustgroup *tg;

    tg = tglist;
    for(tg=tglist;tg;tg=tg->next) {
      th = tg->hosts;
      if(th)
        break;
    }

    /* INVARIANT: tg => th */
    if(!tg)
      return NULL;

    if(th->parent == orig)
      return th;
  }

  for(;;) {
    if(th->next) {
      th = th->next;
    } else {
      if(!th->group->next)
        return NULL;
      th = th->group->next->hosts;
    }

    if(th->parent == orig)
      return th;
  }
}

void th_getsuperandsubsets(uint32_t ip, uint32_t mask, trusthost **superset, trusthost **subset) {
  *superset = th_getsmallestsupersetbyhost(ip, mask);
  *subset = th_getsubsetbyhost(ip, mask);
}

void trusts_flush(void) {
  trustgroup *tg;
  trusthost *th;
  time_t t = time(NULL);

  for(tg=tglist;tg;tg=tg->next) {
    if(tg->count > 0)
      tg->lastseen = t;

    tg_dbupdatecounts(tg);

    for(th=tg->hosts;th;th=th->next) {
      if(th->count > 0)
        th->lastseen = t;

      th_dbupdatecounts(th);
    }
  }
}

trustgroup *tg_strtotg(char *name) {
  unsigned long id;
  trustgroup *tg;

  /* legacy format */
  if(name[0] == '#') {
    id = strtoul(&name[1], NULL, 10);
    if(!id)
      return NULL;

    for(tg=tglist;tg;tg=tg->next)
      if(tg->id == id)
        return tg;
  }

  for(tg=tglist;tg;tg=tg->next)
    if(!match(name, tg->name->content))
      return tg;

  id = strtoul(name, NULL, 10);
  if(!id)
    return NULL;

  /* legacy format */
  for(tg=tglist;tg;tg=tg->next)
    if(tg->id == id)
      return tg;

  return NULL;
}

void th_adjusthosts(trusthost *th, trusthost *superset, trusthost *subset) {
  /*
   * First and foremost, CIDR doesn't allow hosts to cross boundaries, i.e. everything with a smaller prefix
   * is entirely contained with the prefix that is one smaller.
   * e.g. 0.0.0.0/23, 0.0.0.128/23, you can't have a single prefix for 0.0.0.64-0.0.0.192, instead
   * you have two, 0.0.0.64/26 and 0.0.0.128/26.
   *
   * This makes the code MUCH easier as the entire thing is one huge set/tree.
   *
   * Four cases here:
   * 1: host isn't covered by any existing hosts.
   * 2: host is covered by a less specific one only, e.g. adding 0.0.0.1/32, while 0.0.0.0/24 already exists.
   * 3: host is covered by a more specific one only, e.g. adding 0.0.0.0/24 while 0.0.0.1/32 already exists
   *    (note there might be more than one more specific host, e.g. 0.0.0.1/32 and 0.0.0.2/32).
   * 4: covered by more and less specific cases, e.g. adding 0.0.0.0/24 to: { 0.0.0.1/32, 0.0.0.2/32, 0.0.0.0/16 }.
   *
   * CASE 1
   * ------
   *
   * !superset && !subset
   *
   * Scan through the host hash and add any clients which match our host, this is exactly the same as case 3
   * but without needing to check (though checking doesn't hurt), so we'll just use the code for that.
   *
   * CASE 2
   * ------
   *
   * superset && !subset
   *
   * We have the less specific host in 'superset', we know it is the only one so pull out clients in it's
   * ->users list matching our new host.
   * No need to look for extra hosts in the main nick hash as they're all covered already.
   *
   * CASE 3
   * ------
   *
   * !superset && subset
   *
   * We have one host in 'subset', but there might be more than one, we don't care though!
   * We can scan the entire host hash and pull out any hosts that match us and don't have
   * a trust group already, this ignores any with a more specific prefix.
   *
   * CASE 4
   * ------
   *
   * superset && subset
   *
   * Here we first fix up the ones less specific then us, so we just perform what we did for case 2,
   * then we perform what we did for case 3.
   *
   * So in summary:
   *   CASE 1: DO 3
   *   CASE 2: (work)
   *   CASE 3: (work)
   *   CASE 4: DO 2; DO 3
   * Or:
   *   if(2 || 4)     : DO 2
   *   if(1 || 3 || 4): DO 3
   */

  /* we let the compiler do the boolean minimisation for clarity reasons */

  if((superset && !subset) || (superset && subset)) { /* cases 2 and 4 */
    nick *np, *nnp;
    for(np=superset->users;np;np=nnp) {
      nnp = nextbytrust(np);
      if((irc_in_addr_v4_to_int(&np->p_ipaddr) & th->mask) == th->ip) {
        trusts_lostnick(np, 1);
        trusts_newnick(np, 1);
      }
    }
  }

  if((!superset && !subset) || (!superset && subset) || (superset && subset)) { /* cases 1, 3 and 4 */
    nick *np;
    int i;

    for(i=0;i<NICKHASHSIZE;i++)
      for(np=nicktable[i];np;np=np->next)
        if(!gettrusthost(np) && ((irc_in_addr_v4_to_int(&np->p_ipaddr) & th->mask) == th->ip))
          trusts_newnick(np, 1);
  }
}

unsigned int nexttgmarker(void) {
  static unsigned int tgmarker = 0;
  trustgroup *tg;

  tgmarker++;
  if(!tgmarker) {
    /* If we wrapped to zero, zap the marker on all groups */
    for(tg=tglist;tg;tg=tg->next)
      tg->marker=0;

    tgmarker++;
  }

  return tgmarker;
}

unsigned int nextthmarker(void) {
  static unsigned int thmarker = 0;
  trustgroup *tg;
  trusthost *th;

  thmarker++;
  if(!thmarker) {
    /* If we wrapped to zero, zap the marker on all hosts */
    for(tg=tglist;tg;tg=tg->next)
      for(th=tg->hosts;th;th=th->next)
        th->marker=0;

    thmarker++;
  }

  return thmarker;
}
