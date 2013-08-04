#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "whowas.h"

MODULE_VERSION("");

static void whowas_spewrecord(whowas *ww, nick *np) {
  char timebuf[30];
  char hostmask[WW_MASKLEN + 1];

  snprintf(hostmask, sizeof(hostmask), "%s!%s@%s", ww->nick, ww->ident, ww->host);
  strftime(timebuf, 30, "%d/%m/%y %H:%M:%S", localtime(&(ww->seen)));

  if (ww->type == WHOWAS_RENAME)
    controlreply(np, "[%s] NICK %s (%s) -> %s", timebuf, hostmask, ww->realname, ww->newnick->content);
  else
    controlreply(np, "[%s] %s %s (%s): %s", timebuf, (ww->type == WHOWAS_QUIT) ? "QUIT" : "KILL", hostmask, ww->realname, ww->reason->content);
}

static int whowas_cmdwhowas(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *pattern;
  whowas *ww;
  char hostmask[WW_MASKLEN + 1];
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

      if (matches <= limit)
        whowas_spewrecord(ww, sender);
      else if (matches == limit + 1)
        controlreply(sender, "--- More than %d matches, skipping the rest", limit);
    }
  }

  controlreply(sender, "--- Found %d entries.", matches);

  return CMD_OK;
}

static int whowas_cmdwhowaschase(void *source, int cargc, char **cargv) {
  nick *sender = source;
  whowas *ww;

  if (cargc < 1)
    return CMD_USAGE;

  ww = whowas_chase(cargv[0], 900);

  if (!ww) {
    controlreply(sender, "No whowas record found.");
    return CMD_OK;
  }

  whowas_spewrecord(ww, sender);
  controlreply(sender, "Done.");

  return CMD_OK;
}

void _init(void) {
  registercontrolhelpcmd("whowas", NO_OPER, 2, &whowas_cmdwhowas, "Usage: whowas <mask> ?limit?\nLooks up information about recently disconnected users.");
  registercontrolhelpcmd("whowaschase", NO_OPER, 2, &whowas_cmdwhowaschase, "Usage: whowaschase <nick>\nFinds most-recent whowas record for a nick.");
}

void _fini(void) {
  deregistercontrolcmd("whowas", &whowas_cmdwhowas);
  deregistercontrolcmd("whowaschase", &whowas_cmdwhowaschase);
}
