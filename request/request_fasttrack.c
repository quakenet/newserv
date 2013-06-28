#include <stdio.h>
#include <string.h>
#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "request_fasttrack.h"

typedef struct rq_fasttrack {
  unsigned long userid;

  unsigned int targets;
  time_t refill_time;

  struct rq_fasttrack *next;
} rq_fasttrack;

static rq_fasttrack *ftlist;

/* our fast-track extension */
int rqnext;

static void rq_cleanup_fasttrack(void *arg);
static void rqhook_account(int hook, void *arg);

int rq_initfasttrack(void) {
  rqnext = registernickext("request_fasttrack");
  if(rqnext < 0)
    return 0;

  registerhook(HOOK_NICK_NEWNICK, &rqhook_account);
  registerhook(HOOK_NICK_ACCOUNT, &rqhook_account);

  schedulerecurring(time(NULL)+1, 0, 3600, rq_cleanup_fasttrack, NULL);

  return 1;
}

void rq_finifasttrack(void) {
  rq_fasttrack *ft;

  deregisterhook(HOOK_NICK_NEWNICK, &rqhook_account);
  deregisterhook(HOOK_NICK_ACCOUNT, &rqhook_account);

  for(ft=ftlist;ft;ft=ft->next)
    free(ft);

  releasenickext(rqnext);
}

static void rqhook_account(int hook, void *arg) {
  nick *np = (nick *)arg;
  rq_fasttrack *ft;

  /* Auth might be null for the newnick hook. */
  if(!np->auth)
    return;

  /* Try to find an existing fasttrack record for this user. */
  for(ft=ftlist;ft;ft=ft->next) {
    if(np->auth->userid==ft->userid) {
      np->exts[rqnext] = ft;
      break;
    }
  }
}

static void rq_cleanup_fasttrack(void *arg) {

}

static rq_fasttrack *rq_getfasttrack(nick *np) {
  rq_fasttrack *ft;

  /* Use an existing fast-track record if the nick has one. */
  if(np->exts[rqnext])
    return np->exts[rqnext];

  if(!np->auth)
    return NULL;

  ft = malloc(sizeof(rq_fasttrack));

  if(!ft)
    return NULL;

  ft->userid = np->auth->userid;
  ft->targets = 0;
  ft->refill_time = 0;

  np->exts[rqnext] = ft;

  return ft;
}

int rq_tryfasttrack(nick *np) {
  rq_fasttrack *ft = rq_getfasttrack(np);

  /* Don't fast-track if we can't find a fast-track record. */
  if(!ft)
    return 0;

  /* Refill targets if necessary. */
  if(getnettime() > ft->refill_time) {
    ft->targets = RQ_FASTTRACK_TARGETS;
    ft->refill_time = getnettime() + RQ_FASTTRACK_TIMEOUT;
  }

  /* Check if we have a free target. */
  if(ft->targets==0)
    return 0;

  ft->targets--;
  return 1;
}

