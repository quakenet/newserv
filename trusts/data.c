#include <stdlib.h>

#include "../lib/sstring.h"
#include "../nick/nick.h" /* NICKLEN */
#include "trusts.h"

trustgroup *tglist;

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
  freesstring(th->host);
  free(th);
}

int th_add(trustgroup *tg, char *host, unsigned int maxseen, time_t lastseen) {
  trusthost *th = malloc(sizeof(trusthost));
  if(!th)
    return 0;

  th->host = getsstring(host, TRUSTHOSTLEN);
  if(!th->host) {
    th_free(th);
    return 0;
  }
  th->maxseen = maxseen;
  th->lastseen = lastseen;

  th->next = tg->hosts;
  tg->hosts = th;

  return 1;
}

void tg_free(trustgroup *tg) {
  freesstring(tg->name);
  freesstring(tg->createdby);
  freesstring(tg->contact);
  freesstring(tg->comment);
  free(tg);
}

int tg_add(unsigned int id, char *name, unsigned int trustedfor, int mode, unsigned int maxperident, unsigned int maxseen, time_t expires, time_t lastseen, time_t lastmaxuserreset, char *createdby, char *contact, char *comment) {
  trustgroup *tg = malloc(sizeof(trustgroup));
  if(!tg)
    return 0;

  tg->name = getsstring(name, TRUSTNAMELEN);
  tg->createdby = getsstring(createdby, NICKLEN);
  tg->contact = getsstring(contact, CONTACTLEN);
  tg->comment = getsstring(comment, COMMENTLEN);
  if(!tg->name || !tg->createdby || !tg->contact || !tg->comment) {
    tg_free(tg);
    return 0;
  }

  tg->id = id;
  tg->trustedfor = trustedfor;
  tg->mode = mode;
  tg->maxperident = maxperident;
  tg->maxseen = maxseen;
  tg->expires = expires;
  tg->lastseen = lastseen;
  tg->lastmaxuserreset = lastmaxuserreset;

  tg->next = tglist;
  tglist = tg;

  return 1;
}
