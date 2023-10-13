#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../localuser/localuserchannel.h"
#include "../lib/version.h"
#include "../core/modules.h"

MODULE_VERSION("");

int controlraw(void *sender, int cargc, char **cargv);

void _init() {
  registercontrolhelpcmd("raw", NO_DEVELOPER, 2, &controlraw, "Usage: raw <type> <text>\ntype is one of client,server,raw\nUSE THIS COMMAND WITH CAUTION, YOU WILL MOST LIKELY CORRUPT NEWSERV STATE.");
}

void _fini() {
  deregistercontrolcmd("raw", controlraw);
}

int controlraw(void *sender, int cargc, char **cargv) {
  nick *np=(nick *)sender;
  if (cargc<2)
    return CMD_USAGE;

  controlreply(sender, "Sending Raw.. %s", cargv[1]);
  if (strcmp(cargv[0], "server") == 0) {
    irc_send("%s %s",mynumeric->content,cargv[1]);
    controlwall(NO_DEVELOPER, NL_OPERATIONS, "%s/%s sent SERVER RAW for %s", np->nick, np->authname, cargv[1]); 
  } else if (strcmp( cargv[0], "client") == 0) {
    controlwall(NO_DEVELOPER, NL_OPERATIONS, "%s/%s sent CLIENT RAW for %s", np->nick, np->authname, cargv[1]);
    irc_send("%s %s",longtonumeric(mynick->numeric,5),cargv[1]);
  } else if (strcmp( cargv[0], "raw") == 0) {
    controlwall(NO_DEVELOPER, NL_OPERATIONS, "%s/%s sent RAW for %s", np->nick, np->authname, cargv[1]);
    irc_send("%s", cargv[1]);
  } else {
    controlreply(sender, "Invalid Mode - server or client");
    return CMD_ERROR;
  }

  controlreply(sender, "RAW sent.");

  return CMD_OK;
}
