#include <stdio.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../irc/irc.h"
#include "../localuser/localuser.h"
#include "../lib/flags.h"

/* we allow reauthing for flag changing, as well as testing purposes */
int au_auth(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  char *authname;
  int authid;
  flag_t flags;

  if(cargc < 2)
    return CMD_USAGE;

  if(sscanf(myserver->content, "%*s.quakenet.org") == 1) {
    controlreply(sender, "Sorry, you can't use this command on QuakeNet.");
    return CMD_ERROR;
  }

  if(cargc > 2) {
    if(setflags(&flags, AFLAG_ALL, cargv[2], accountflags, REJECT_UNKNOWN) != REJECT_NONE) {
      controlreply(sender, "Bad flag(s) supplied.");
      return CMD_ERROR;
    }
  } else {
    flags = 0;
  }

  authname = cargv[0];
  authid = atoi(cargv[1]);

  controlwall(NO_OPER, NL_OPERATIONS, "AUTH'ed at %s:%d:%s", authname, authid, printflags(flags, accountflags));

  localusersetaccount(sender, authname, authid, flags, 0);
  controlreply(sender, "Done.");

  return CMD_OK;
}

void _init() {
  registercontrolhelpcmd("auth", NO_OPERED, 3, au_auth, "Usage: auth <authname> <authid> ?accountflags?");
}

void _fini() {
  deregistercontrolcmd("auth", au_auth);
}

