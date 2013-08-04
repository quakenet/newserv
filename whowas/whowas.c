#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "whowas.h"

MODULE_VERSION("");

whowas *whowas_head = NULL, *whowas_tail = NULL;
int whowas_count = 0;

static void ww_handlequitorkill(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *reason = args[1];
  char *rreason;
  char resbuf[512];
  whowas *ww;
  time_t now;

  time(&now);

  /* Clean up old records. */
  while ((ww = whowas_tail) && (ww->seen < now - WW_MAXAGE || whowas_count >= WW_MAXENTRIES)) {
    whowas_tail = ww->prev;

    if (ww->prev)
      ww->prev->next = NULL;
    else
      whowas_head = ww->prev;

    whowas_count--;
    free(ww);
  }

  /* Create a new record. */
  ww = malloc(sizeof(whowas));
  strncpy(ww->nick, np->nick, NICKLEN);
  strncpy(ww->ident, np->ident, USERLEN);
  strncpy(ww->host, np->host->name->content, HOSTLEN);
  strncpy(ww->realname, np->realname->name->content, REALLEN);
  ww->seen = getnettime();

  if (hooknum == HOOK_NICK_KILL && (rreason = strchr(reason, ' '))) {
    sprintf(resbuf, "Killed%s", rreason);
    reason = resbuf;
  }

  ww->reason = getsstring(reason, WW_REASONLEN);

  if (whowas_head)
    whowas_head->prev = ww;

  ww->next = whowas_head;
  whowas_head = ww;

  ww->prev = NULL;

  if (!whowas_tail)
    whowas_tail = ww;

  whowas_count++;
}

void _init(void) {
  registerhook(HOOK_NICK_QUIT, ww_handlequitorkill);
  registerhook(HOOK_NICK_KILL, ww_handlequitorkill);
}

void _fini(void) {
  whowas *ww;

  deregisterhook(HOOK_NICK_QUIT, ww_handlequitorkill);
  deregisterhook(HOOK_NICK_KILL, ww_handlequitorkill);

  while ((ww = whowas_head)) {
    whowas_head = ww->next;
    free(ww);
  }
}
