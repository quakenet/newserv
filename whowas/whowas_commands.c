#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "whowas.h"

MODULE_VERSION("");

static int whowas_cmdwhowas(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *pattern;
  whowas *ww;
  nick *np;
  int i;
  char hostmask[WW_MASKLEN + 1];
  int matches = 0, limit = 500;

  if (cargc < 1)
    return CMD_USAGE;

  pattern = cargv[0];

  if (cargc > 1)
    limit = strtol(cargv[1], NULL, 10);

  for (i = whowasoffset; i < whowasoffset + WW_MAXENTRIES; i++) {
    ww = &whowasrecs[i % WW_MAXENTRIES];

    if (ww->type == WHOWAS_UNUSED)
      continue;

    np = &ww->nick;
    snprintf(hostmask, sizeof(hostmask), "%s!%s@%s", np->nick, np->ident, np->host->name->content);

    if (match2strings(pattern, hostmask)) {
      matches++;

      if (matches <= limit)
        controlreply(sender, "%s", whowas_format(ww));
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

  controlreply(sender, "%s", whowas_format(ww));
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
