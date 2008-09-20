#include <stdio.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../irc/irc.h"
#include "../localuser/localuser.h"

int au_auth(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  char *authname, *dummy;
  int authid;

  if(cargc < 2)
    return CMD_USAGE;

  if(sscanf(myserver->content, "%s.quakenet.org", dummy) == 1) {
    controlreply(sender, "Sorry, you can't use this command on QuakeNet.");
    return CMD_ERROR;
  }

  authname = cargv[0];
  authid = atoi(cargv[1]);

  controlwall(NO_OPER, NL_OPERATIONS, "AUTH'ed at %s:%d", authname, authid);

  localusersetaccount(sender, authname, authid, 0, 0);
  controlreply(sender, "Done.");

  return CMD_OK;
}

void _init() {
  registercontrolhelpcmd("auth", NO_OPER, 1, au_auth, "Usage: auth <authname> <authid>");
}

void _fini() {
  deregistercontrolcmd("auth", au_auth);
}

