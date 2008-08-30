#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../newsearch/newsearch.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../localuser/localuserchannel.h"
#include "../lib/version.h"
#include "../core/modules.h"

MODULE_VERSION("");

int controlkill(void *sender, int cargc, char **cargv);
int controlkick(void *sender, int cargc, char **cargv);
int controlspewchan(void *sender, int cargc, char **cargv);
int controlspew(void *sender, int cargc, char **cargv);
int controlcompare(void *sender, int cargc, char **cargv);
int controlbroadcast(void *sender, int cargc, char **cargv);
int controlobroadcast(void *sender, int cargc, char **cargv);
int controlmbroadcast(void *sender, int cargc, char **cargv);
int controlsbroadcast(void *sender, int cargc, char **cargv);
int controlcbroadcast(void *sender, int cargc, char **cargv);

void _init() {
  registercontrolhelpcmd("kill", NO_OPER, 2, &controlkill, "Usage: kill <nick> ?reason?\nKill specified user with given reason (or 'Killed').");
  registercontrolhelpcmd("kick", NO_OPER, 3, &controlkick, "Usage: kick <channel> <user> ?reason?\nKick a user from the given channel, with given reason (or 'Kicked').");

  registercontrolhelpcmd("spewchan", NO_OPER, 1, &controlspewchan, "Usage: spewchan <pattern>\nShows all channels which are matched by the given pattern");
  registercontrolhelpcmd("spew", NO_OPER, 1, &controlspew, "Usage: spew <pattern>\nShows all userss which are matched by the given pattern");
  registercontrolhelpcmd("compare", NO_OPER, 255, &controlcompare, "Usage: compare <nick1> <nick2> ... <nickn>\nShows channels shared by supplied nicknames\nUsage: compare <channel1> <channel2> ... <channeln>\nShares nicks that share the supplied channels");

  registercontrolhelpcmd("broadcast", NO_OPER, 1, &controlbroadcast, "Usage: broadcast <text>\nSends a message to all users.");
  registercontrolhelpcmd("obroadcast", NO_OPER, 1, &controlobroadcast, "Usage: obroadcast <text>\nSends a message to all IRC operators.");
  registercontrolhelpcmd("mbroadcast", NO_OPER, 2, &controlmbroadcast, "Usage: mbroadcast <mask> <text>\nSends a message to all users matching the mask");
  registercontrolhelpcmd("sbroadcast", NO_OPER, 2, &controlsbroadcast, "Usage: sbroadcast <mask> <text>\nSends a message to all users on specific server(s).");
  registercontrolhelpcmd("cbroadcast", NO_OPER, 2, &controlcbroadcast, "Usage: cbroadcast <2 letter country> <text>\nSends a message to all users in the specified country (GeoIP must be loaded).");
}

void _fini() {
  deregistercontrolcmd("obroadcast", controlobroadcast);
  deregistercontrolcmd("sbroadcast", controlsbroadcast);
  deregistercontrolcmd("mbroadcast", controlmbroadcast);
  deregistercontrolcmd("broadcast", controlbroadcast);

  deregistercontrolcmd("spew", controlspew);
  deregistercontrolcmd("spewchan", controlspewchan);

  deregistercontrolcmd("kill", controlkill);
  deregistercontrolcmd("kick", controlkick);

  deregistercontrolcmd("compare", controlcompare);
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

  if(IsService(target) && SIsService(&serverlist[homeserver(target->numeric)])) {
    controlreply(np,"Sorry, cannot kick network services.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_KICKKILLS, "%s/%s sent KICK for %s!%s@%s on %s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content,cp->index->name->content, (cargc>2)?cargv[2]:"Kicked");
  localkickuser(NULL, cp, target, (cargc>2)?cargv[2]:"Kicked");
  controlreply(sender, "KICK sent.");

  return CMD_OK;
}

int controlkill(void *sender, int cargc, char **cargv) {
  nick *target;
  nick *np = (nick *)sender;

  if (cargc<1)
    return CMD_USAGE;

  if ((target=getnickbynick(cargv[0]))==NULL) {
    controlreply(np,"Sorry, couldn't find that user.");
    return CMD_ERROR;
  }

  if(IsService(target) && SIsService(&serverlist[homeserver(target->numeric)])) {
    controlreply(np,"Sorry, cannot kill network services.");
    controlwall(NO_OPER, NL_KICKKILLS, "%s/%s attempted to KILL service %s!%s@%s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content, (cargc>1)?cargv[1]:"Killed");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_KICKKILLS, "%s/%s sent KILL for %s!%s@%s (%s)", np->nick, np->authname, target->nick, target->ident, target->host->name->content, (cargc>1)?cargv[1]:"Killed");
  killuser(NULL, target, (cargc>1)?cargv[1]:"Killed");
  controlreply(np, "KILL sent.");

  return CMD_OK;
}

int controlspew(void *sender, int cargc, char **cargv) {
  searchASTExpr tree;

  if(cargc < 1)
    return CMD_USAGE;

  tree = NSASTNode(match_parse, NSASTNode(hostmask_parse), NSASTLiteral(cargv[0]));
  return ast_nicksearch(&tree, controlreply, sender, controlnswall, printnick, NULL, NULL, 500);
}

int controlspewchan(void *sender, int cargc, char **cargv) {
  searchASTExpr tree;

  if(cargc < 1)
    return CMD_USAGE;

  tree = NSASTNode(match_parse, NSASTNode(name_parse), NSASTLiteral(cargv[0]));
  return ast_chansearch(&tree, controlreply, sender, controlnswall, printchannel, NULL, NULL, 500);
}

int controlcompare(void *sender, int cargc, char **cargv) {
  searchASTExpr nodes[cargc], literals[cargc], tree;
  int i;
  void *displayfn;
  ASTFunc execfn;
  parseFunc parsefn;
  
  if(cargc < 2)
    return CMD_USAGE;

  if(cargv[0][0] == '#') {
    execfn = (ASTFunc)ast_nicksearch;
    displayfn = printnick;
    parsefn = channel_parse;
  } else {
    execfn = (ASTFunc)ast_chansearch;
    displayfn = printchannel;
    parsefn = nick_parse;
  }
    
  for(i=0;i<cargc;i++) {
    literals[i] = NSASTLiteral(cargv[i]);
    nodes[i] = NSASTManualNode(parsefn, 1, &literals[i]);
  }
  tree = NSASTManualNode(and_parse, cargc, nodes);
  
  return (execfn)(&tree, controlreply, sender, controlnswall, displayfn, NULL, NULL, 500);
}

int controlbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;

  if (cargc<1)
    return CMD_USAGE;

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent a broadcast: %s", np->nick, np->authname, cargv[0]);

  irc_send("%s O $* :(Broadcast) %s", longtonumeric(mynick->numeric,5), cargv[0]);

  controlreply(np, "Message broadcasted.");

  return CMD_OK;
}

int controlmbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;

  if (cargc<2)
    return CMD_USAGE;

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent an mbroadcast to %s: %s", np->nick, np->authname, cargv[0], cargv[1]);

  irc_send("%s O $@%s :(mBroadcast) %s", longtonumeric(mynick->numeric,5), cargv[0], cargv[1]);

  controlreply(np, "Message mbroadcasted.");

  return CMD_OK;
}

int controlsbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  if(cargc<2)
    return CMD_USAGE;

  controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent an sbroadcast to %s: %s", np->nick, np->authname, cargv[0], cargv[1]);

  irc_send("%s O $%s :(sBroadcast) %s", longtonumeric(mynick->numeric,5), cargv[0], cargv[1]);

  controlreply(np, "Message sbroadcasted.");

  return CMD_OK;
}

int controlobroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  searchASTExpr tree;
  int ret;
  char message[512];
  
  if(cargc<1)
    return CMD_USAGE;

  snprintf(message, sizeof(message), "(oBroadcast) %s", cargv[0]);

  tree = NSASTNode(and_parse, NSASTNode(modes_parse, NSASTLiteral("+o")), NSASTNode(notice_parse, NSASTLiteral(message)));
  if((ret=ast_nicksearch(&tree, controlreply, sender, controlnswall, NULL, NULL, NULL, -1)) == CMD_OK) {
    controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent an obroadcast: %s", np->nick, np->authname, cargv[0]);
    controlreply(np, "Message obroadcasted.");
  } else {
    controlreply(np, "An error occured.");
  }

  return ret;
}

int controlcbroadcast(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  int ret;
  searchASTExpr tree;
  char message[512];
  
  if(cargc < 2)
    return CMD_USAGE;

  snprintf(message, sizeof(message), "(cBroadcast) %s", cargv[1]);
  tree = NSASTNode(and_parse, NSASTNode(country_parse, NSASTLiteral(cargv[0])), NSASTNode(notice_parse, NSASTLiteral(message)));
  if((ret=ast_nicksearch(&tree, controlreply, sender, controlnswall, NULL, NULL, NULL, -1)) == CMD_OK) {
    controlwall(NO_OPER, NL_BROADCASTS, "%s/%s sent a cbroadcast %s: %s", np->nick, np->authname, cargv[0], cargv[1]);
    controlreply(np, "Message cbroadcasted.");
  } else {
    controlreply(np, "An error occured.");
  }

  return ret;
}
