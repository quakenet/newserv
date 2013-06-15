#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "whowas.h"

static whowas *wwhead, *wwtail;
static int wwcount;

int ww_cmdwhowas(void *source, int cargc, char **cargv) {
  nick *np = source;
  char *pattern;
  whowas *ww;
  char hostmask[WW_MASKLEN+1];
  char timebuf[30];
  int matches = 0, limit = 500;

  if(cargc<1)
    return CMD_USAGE;

  pattern = cargv[0];

  if(cargc>1)
    limit = strtol(cargv[1], NULL, 10);

  for(ww=wwhead;ww;ww=ww->next) {
    snprintf(hostmask, sizeof(hostmask), "%s!%s@%s", ww->nick, ww->ident, ww->host);

    if (match2strings(pattern, hostmask)) {
      matches++;

      if(matches<=limit) {
        strftime(timebuf, 30, "%d/%m/%y %H:%M:%S", localtime(&(ww->seen)));

        controlreply(np, "[%s] %s (%s): %s", timebuf, hostmask, ww->realname, ww->reason->content);
      } else if(matches==limit+1) {
        controlreply(np, "--- More than %d matches, skipping the rest", limit);
      }
    }
  }

  controlreply(np, "--- Found %d entries.", matches);

  return CMD_OK;
}

void ww_handlequitorkill(int hooknum, void *arg) {
  void **args=arg;
  nick *np=args[0];
  char *reason=args[1];
  char *rreason;
  char resbuf[512];
  whowas *ww;
  time_t now;

  time(&now);

  /* Clean up old records. */
  while((ww = wwtail) && (ww->seen < now - WW_MAXAGE || wwcount >= WW_MAXENTRIES)) {
    wwtail = ww->prev;

    if (ww->prev)
      ww->prev->next = NULL;
    else
      wwhead = ww->prev;

    wwcount--;
    free(ww);
  }

  /* Create a new record. */
  ww = malloc(sizeof(whowas));
  strncpy(ww->nick, np->nick, NICKLEN);
  strncpy(ww->ident, np->ident, USERLEN);
  strncpy(ww->host, np->host->name->content, HOSTLEN);
  strncpy(ww->realname, np->realname->name->content, REALLEN);
  ww->seen = getnettime();

  if(hooknum==HOOK_NICK_KILL && (rreason=strchr(reason,' '))) {
    sprintf(resbuf,"Killed%s",rreason);
    reason=resbuf;
  }

  ww->reason = getsstring(reason, WW_REASONLEN);

  if(wwhead)
    wwhead->prev = ww;

  ww->next = wwhead;
  wwhead = ww;

  ww->prev = NULL;

  if(!wwtail)
    wwtail = ww;

  wwcount++;
}

void _init(void) {
  registerhook(HOOK_NICK_QUIT, ww_handlequitorkill);
  registerhook(HOOK_NICK_KILL, ww_handlequitorkill);

  registercontrolhelpcmd("whowas", NO_OPER, 2, &ww_cmdwhowas, "Usage: whowas <mask> ?limit?\nLooks up information about recently disconnected users.");
}

void _fini(void) {
  whowas *ww;

  deregisterhook(HOOK_NICK_QUIT, ww_handlequitorkill);
  deregisterhook(HOOK_NICK_KILL, ww_handlequitorkill);

  deregistercontrolcmd("whowas", &ww_cmdwhowas);

  while((ww = wwhead)) {
    wwhead = ww->next;
    free(ww);
  }
}
