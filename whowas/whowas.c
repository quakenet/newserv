#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "whowas.h"

static whowas *ww_records;
static int ww_count;

int ww_cmdwhowas(void *source, int cargc, char **cargv) {
  nick *np = source;
  char *pattern;
  whowas *ww;
  char hostmask[WW_MASKLEN+1];
  char timebuf[30];
  int matches = 0;

  if(cargc<1)
    return CMD_USAGE;

  pattern = cargv[0];

  for(ww=ww_records;ww;ww=ww->next) {
    snprintf(hostmask, sizeof(hostmask), "%s!%s@%s", ww->nick, ww->ident, ww->host);

    if (match2strings(pattern, hostmask)) {
      strftime(timebuf, 30, "%d/%m/%y %H:%M:%S", localtime(&(ww->seen)));

      controlreply(np, "[%s] %s (%s): %s", timebuf, hostmask, ww->realname, ww->reason);

      matches++;

      if(matches>1000) {
        controlreply(np, "--- Too many matching entries. Aborting...");
        break;
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
  while((ww = ww_records) && (ww->seen < now - WW_MAXAGE || ww_count >= WW_MAXENTRIES)) {
    ww_records = ww->next;
    ww_count--;
    free(ww);
  }

  /* Create a new record. */
  ww = malloc(sizeof(whowas));
  strncpy(ww->nick, np->nick, NICKLEN);
  strncpy(ww->ident, np->ident, USERLEN);
  strncpy(ww->host, np->host->name->content, HOSTLEN);
  strncpy(ww->realname, np->realname->name->content, REALLEN);
  ww->seen = getnettime();

  if (hooknum==HOOK_NICK_KILL && (rreason=strchr(reason,' '))) {
    sprintf(resbuf,"Killed%s",rreason);
    reason=resbuf;
  }

  strncpy(ww->reason, reason, WW_REASONLEN);

  ww->next = ww_records;
  ww_records = ww;
  ww_count++;
}

void _init(void) {
  registerhook(HOOK_NICK_QUIT, ww_handlequitorkill);
  registerhook(HOOK_NICK_KILL, ww_handlequitorkill);

  registercontrolhelpcmd("whowas", NO_OPER, 1, &ww_cmdwhowas, "Usage: whowas <mask>\nLooks up information about recently disconnected users.");
}

void _fini(void) {
  whowas *ww;

  deregisterhook(HOOK_NICK_QUIT, ww_handlequitorkill);
  deregisterhook(HOOK_NICK_KILL, ww_handlequitorkill);

  deregistercontrolcmd("whowas", &ww_cmdwhowas);

  while((ww = ww_records)) {
    ww_records = ww->next;
    free(ww);
  }
}
