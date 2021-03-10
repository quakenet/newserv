#include <stdio.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../irc/irc.h"
#include "../localuser/localuser.h"
#include "../lib/flags.h"
#include "../lib/version.h"

MODULE_VERSION("");

/* we allow reauthing for flag changing, as well as testing purposes */
int au_auth(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  char *authname;
  int authts;
  int authid;
  flag_t flags;

  if(cargc < 3)
    return CMD_USAGE;

  if(sscanf(myserver->content, "%*s.quakenet.org") == 1) {
    controlreply(sender, "Sorry, you can't use this command on QuakeNet.");
    return CMD_ERROR;
  }

  if(cargc > 3) {
    if(setflags(&flags, AFLAG_ALL, cargv[3], accountflags, REJECT_UNKNOWN) != REJECT_NONE) {
      controlreply(sender, "Bad flag(s) supplied.");
      return CMD_ERROR;
    }
  } else {
    flags = 0;
  }

  authname = cargv[0];
  authts = atoi(cargv[1]);
  authid = atoi(cargv[2]);

  controlwall(NO_OPER, NL_OPERATIONS, "AUTH'ed at %s:%d:%d:%s", authname, authts, authid, printflags(flags, accountflags));

  localusersetaccount(sender, authname, authid, flags, authts);
  controlreply(sender, "Done.");

  return CMD_OK;
}

void _init() {
  registercontrolhelpcmd("auth", NO_OPERED, 3, au_auth, "Usage: auth <authname> <authts> <authid> ?accountflags?");
}

void _fini() {
  deregistercontrolcmd("auth", au_auth);
}

