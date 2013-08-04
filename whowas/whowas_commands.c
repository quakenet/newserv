#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "whowas.h"

MODULE_VERSION("");

static int ww_cmdwhowas(void *source, int cargc, char **cargv) {
  nick *np = source;
  char *pattern;
  whowas *ww;
  char hostmask[WW_MASKLEN + 1];
  char timebuf[30];
  int matches = 0, limit = 500;

  if (cargc < 1)
    return CMD_USAGE;

  pattern = cargv[0];

  if (cargc > 1)
    limit = strtol(cargv[1], NULL, 10);

  for (ww = whowas_head; ww; ww = ww->next) {
    snprintf(hostmask, sizeof(hostmask), "%s!%s@%s", ww->nick, ww->ident, ww->host);

    if (match2strings(pattern, hostmask)) {
      matches++;

      if (matches <= limit) {
        strftime(timebuf, 30, "%d/%m/%y %H:%M:%S", localtime(&(ww->seen)));

        controlreply(np, "[%s] %s (%s): %s", timebuf, hostmask, ww->realname, ww->reason->content);
      } else if (matches == limit + 1) {
        controlreply(np, "--- More than %d matches, skipping the rest", limit);
      }
    }
  }

  controlreply(np, "--- Found %d entries.", matches);

  return CMD_OK;
}

void _init(void) {
  registercontrolhelpcmd("whowas", NO_OPER, 2, &ww_cmdwhowas, "Usage: whowas <mask> ?limit?\nLooks up information about recently disconnected users.");
}

void _fini(void) {
  deregistercontrolcmd("whowas", &ww_cmdwhowas);
}
