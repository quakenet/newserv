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
  registercontrolhelpcmd("kill",NO_OPER,2,&controlkill,"Usage: kill nick <reason>\nKill specificed user.");
  registercontrolhelpcmd("opchan",NO_OPER,2,&controlopchan,"Usage: opchan channel nick\nGive user +o on channel.");
  registercontrolhelpcmd("kick",NO_OPER,3,&controlkick,"Usage: kick channel user <reason>\nKick a user from the given channel");
}

void _fini() {
  deregistercontrolcmd("kill",controlkill); 
  deregistercontrolcmd("opchan",controlopchan);
  deregistercontrolcmd("kick",controlkick);
}

int controlkick(void *sender, int cargc, char **cargv) {
  nick *np=(nick *)sender;
  nick *victim;
  channel *cp;
  modechanges changes;
  nick *target;

  if (cargc<2) {
    controlreply(sender,"Usage: kick channel user <reason>");
    return CMD_ERROR;
  }

  if ((cp=findchannel(cargv[0]))!=NULL) {
    if (cargv[1][0]=='#') {
      if (!(target=getnickbynumericstr(cargv[1]+1))) {
        controlreply(np,"Sorry, couldn't find numeric %s",cargv[0]+1);
        return CMD_ERROR;
      }
    } else {
      if ((target=getnickbynick(cargv[1]))==NULL) {
        controlreply(np,"Sorry, couldn't find that user");
        return CMD_ERROR;
      }
    }

    if(cargc > 2) {
      irc_send("%s K %s %s :%s",mynumeric->content,cp->index->name->content,longtonumeric(target->numeric,5),cargv[2]);
    } else {
      irc_send("%s K %s %s :Kicked",mynumeric->content,cp->index->name->content,longtonumeric(target->numeric,5));
    }
    delnickfromchannel(cp, target->numeric, 1);

    controlreply(sender,"Put Kick for %s from %s.", target->nick, cp->index->name->content);
    controlwall(NO_OPER, NL_KICKS, "%s/%s sent KICK for %s!%s@%s from %s", np->nick, np->authname, target->nick, target->ident, target->host->name->content,cp->index->name->content);

  } else {
    controlreply(np,"Couldn't find channel %s.",cargv[0]);
    return;
  }

  return CMD_OK;
}

int controlopchan(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  nick *victim;
  channel *cp;
  modechanges changes;
  nick *target;
  unsigned long *lp;

  if (cargc<2) {
    controlreply(sender,"Usage: opchan channel user");
    return CMD_ERROR;
  }
  
  if ((cp=findchannel(cargv[0]))!=NULL) {
    if (cargv[1][0]=='#') {
      if (!(target=getnickbynumericstr(cargv[1]+1))) {
        controlreply(sender,"Sorry, couldn't find numeric %s",cargv[0]+1);
        return CMD_ERROR;
      }
    } else {
      if ((target=getnickbynick(cargv[1]))==NULL) {
        controlreply((nick *)sender,"Sorry, couldn't find that user");
        return CMD_ERROR;
      }
    }

    if ((lp=getnumerichandlefromchanhash(cp->users,target->numeric))==NULL) {
      controlreply((nick *)sender,"Sorry, User not on channel");
      return CMD_ERROR;
    }

    (*lp)|=CUMODE_OP;
    irc_send("%s OM %s +o %s",mynumeric->content,cp->index->name->content,longtonumeric(target->numeric,5));
    controlreply(sender,"Put mode +o %s on %s.", target->nick, cp->index->name->content);  
  } else {
    controlreply(sender,"Couldn't find channel %s.",cargv[0]);
    return;
  }
  
  return CMD_OK;
}

int controlkill(void *sender, int cargc, char **cargv) {
  nick *target;
  char buf[BUFSIZE];
  int i;
  nick *np = (nick *)sender;
 
  if (cargc<1) {
    controlreply(np,"Usage: kill <user> <reason>");
    return CMD_ERROR;
  }
  
  if (cargv[0][0]=='#') {
    if (!(target=getnickbynumericstr(cargv[0]+1))) {
      controlreply(np,"Sorry, couldn't find numeric %s",cargv[0]+1);
      return CMD_ERROR;
    }
  } else {
    if ((target=getnickbynick(cargv[0]))==NULL) {
      controlreply(np,"Sorry, couldn't find that user");
      return CMD_ERROR;
    }
  }

  killuser(NULL, target, (cargc>1)?cargv[1]:"Killed");
  controlreply(np,"KILL sent.");
  controlwall(NO_OPER, NL_KILLS, "%s/%s sent KILL for %s!%s@%s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content,(cargc>1)?cargv[1]:"Killed");

  return CMD_OK;
}
