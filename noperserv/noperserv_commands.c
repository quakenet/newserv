#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"

int controlkill(void *sender, int cargc, char **cargv);
int controlopchan(void *sender, int cargc, char **cargv);
int controlkick(void *sender, int cargc, char **cargv);

void _init() {
  registercontrolhelpcmd("kill", NO_OPER, 2, &controlkill, "Usage: kill <nick> ?reason?\nKill specified user with given reason (or 'Killed').");
  registercontrolhelpcmd("kick", NO_OPER, 3, &controlkick, "Usage: kick <channel> <user> ?reason?\nKick a user from the given channel, with given reason (or 'Kicked').");
}

void _fini() {
  deregistercontrolcmd("kill", controlkill); 
  deregistercontrolcmd("kick", controlkick);
}

int controlkick(void *sender, int cargc, char **cargv) {
  nick *np=(nick *)sender;
  channel *cp;
  nick *target;

  if (cargc<2)
    return CMD_USAGE;

  if ((cp=findchannel(cargv[0]))==NULL) {
    controlreply(np,"Couldn't find that channel.");
    return CMD_ERROR;
  }

  if ((target=getnickbynick(cargv[1]))==NULL) {
    controlreply(np,"Sorry, couldn't find that user");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_KICKS, "%s/%s sent KICK for %s!%s@%s on %s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content,cp->index->name->content, (cargc>2)?cargv[2]:"Kicked");
  localkickuser(NULL, cp, target, (cargc>2)?cargv[2]:"Kicked");
  controlreply(sender, "KICK sent.");

  return CMD_OK;
}

/* NO checking to see if it's us or anyone important */
int controlkill(void *sender, int cargc, char **cargv) {
  nick *target;
  nick *np = (nick *)sender;
 
  if (cargc<1)
    return CMD_USAGE;  

  if ((target=getnickbynick(cargv[0]))==NULL) {
    controlreply(np,"Sorry, couldn't find that user.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_KILLS, "%s/%s sent KILL for %s!%s@%s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content, (cargc>1)?cargv[1]:"Killed");
  killuser(NULL, target, (cargc>1)?cargv[1]:"Killed");
  controlreply(np, "KILL sent.");

  return CMD_OK;
}
