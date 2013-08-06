/* 
 * NOperserv v0.01
 *
 * A replacement for Germania's ageing Operservice2
 *
 * Copyright (C) 2005 Chris Porter.
 */

#include "../localuser/localuser.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../lib/version.h"
#include "../authext/authext.h"
#include "../control/control.h"
#include "../control/control_db.h"
#include "../control/control_policy.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

MODULE_VERSION("");

#define FLAGBUFLEN 100

#define NO_FOUND_NICKNAME 1
#define NO_FOUND_AUTHNAME 2

/* @test */
static int noperserv_hello(void *sender, int cargc, char **cargv) {
  authname *newaccount = NULL;
  no_autheduser *au;
  nick *np = (nick *)sender, *np2, *target = NULL;

  if(cargc == 0) {
    newaccount = np->auth;
  } else {
    if(cargv[0][0] == '#') {
      authname *a = getauthbyname(cargv[0] + 1);
      if(!a) {
        controlreply(np, "Cannot find anyone with that authname on the network.");
        return CMD_ERROR;
      }
      newaccount = a;
    } else {
      target = getnickbynick(cargv[0]);
      if(!target) {
        controlreply(np, "Supplied nickname is not on the network.");
        return CMD_ERROR;
      }
      newaccount = target->auth;
    }
  }
  if(!newaccount) {
    controlreply(np, "Supplied user is not authed with the network.");
    return CMD_ERROR;
  }
  au = noperserv_get_autheduser(newaccount);
  if(au) {
    controlreply(np, "Authname already registered.");  
    return CMD_ERROR;
  }

  au = noperserv_new_autheduser(newaccount->userid, newaccount->name);
  if(!au) {
    controlreply(np, "Memory allocation error.");
    return CMD_ERROR;
  }

  if(noperserv_get_autheduser_count() == 1) {
    au->authlevel = NO_FIRST_USER_LEVEL;
    au->noticelevel = NO_FIRST_USER_DEFAULT_NOTICELEVEL;
  } else {
    au->authlevel = NO_DEFAULT_LEVEL;
    au->noticelevel = NO_DEFAULT_NOTICELEVEL;
  }

  noperserv_update_autheduser(au);

  for(np2=newaccount->nicks;np2;np2=np2->nextbyauthname) {
    controlreply(np2, "An account has been created for you (auth %s).", au->authname->name);
    if(NOGetAuthLevel(au))
      controlreply(np2, "User flags: %s", printflags(NOGetAuthLevel(au), no_userflags));
    controlreply(np2, "Notice flags: %s", printflags(NOGetNoticeLevel(au), no_noticeflags));
  }

  if(np->auth==newaccount) { /* send a message to the person who HELLO'ed if we haven't already been told */
    controlreply(np, "Account created for auth %s.", au->authname->name);
    if(NOGetAuthLevel(au))
      controlreply(np, "User flags: %s", printflags(NOGetAuthLevel(au), no_userflags));
    controlreply(np, "Notice flags: %s", printflags(NOGetNoticeLevel(au), no_noticeflags));
    controlreply(np, "Instructions sent to all authed users.");
  } else if(newaccount->nicks && newaccount->nicks->next) { /* if we have already been told, tell the user it was sent to more than themselves */
    controlreply(np, "Instructions sent to all authed users.");
  }

  controlwall(NO_OPERED, NL_MANAGEMENT, "%s/%s just HELLO'ed: %s", np->nick, np->authname, au->authname->name);
  return CMD_OK;
}

static no_autheduser *noperserv_autheduser_from_command(nick *np, char *command, int *typefound, char **returned) {
  no_autheduser *au;
  authname *anp;
  if(command[0] == '#') {
    anp = findauthnamebyname(command + 1);
    if(!anp) {
      controlreply(np, "Authname not found.");
      return NULL;
    }
    au = noperserv_get_autheduser(anp);
    if(!au) {
      controlreply(np, "Authname not found.");
    } else {
      *typefound = NO_FOUND_AUTHNAME;
      *returned = au->authname->name;
      return au;
    }
  } else {
    nick *np2 = getnickbynick(command);
    if(!np2) {
      controlreply(np, "Nickname not on the network.");
      return NULL;
    }
    if(!IsAccount(np2)) {
      controlreply(np, "User is not authed with the network.");
      return NULL;
    }
    au = NOGetAuthedUser(np2);
    if(!au) {
      controlreply(np, "User does not have an account.");
    } else {
      *typefound = NO_FOUND_NICKNAME;
      *returned = np2->nick;
      return au;
    }
  }

  return NULL;
}

static int noperserv_noticeflags(void *sender, int cargc, char **cargv) {
  nick *np2, *np = (nick *)sender;
  no_autheduser *au;

  if(cargc == 1) {
    if((cargv[0][0] == '+') || (cargv[0][0] == '-')) {
      int ret;
      au = NOGetAuthedUser(np);
      flag_t fwas = NOGetNoticeLevel(au), permittedchanges = noperserv_policy_permitted_noticeflags(au);
      
      ret = setflags(&au->noticelevel, permittedchanges, cargv[0], no_noticeflags, REJECT_DISALLOWED | REJECT_UNKNOWN);
      if(ret != REJECT_UNKNOWN) {
        if(ret == REJECT_DISALLOWED) {
          flag_t fnow = fwas;
          setflags(&fnow, NL_ALL, cargv[0], no_noticeflags, REJECT_NONE);
          if(fnow == fwas) {
            controlreply(np, "No changes made to existing flags.");
          } else {
            char ourflags[FLAGBUFLEN], ournoticeflags[FLAGBUFLEN];
            controlreply(np, "Flag alterations denied.");
          
            strlcpy(ourflags, printflags(NOGetAuthLevel(au), no_userflags), sizeof(ourflags));
            strlcpy(ournoticeflags, printflags(NOGetNoticeLevel(au), no_noticeflags), sizeof(ournoticeflags));
            controlwall(NO_OPER, NL_MANAGEMENT, "%s/%s (%s) attempted to NOTICEFLAGS (%s): %s", np->nick, np->authname, ourflags, ournoticeflags, printflagdiff(fwas, fnow, no_noticeflags));
            return CMD_ERROR;
          }
        } else if(ret == REJECT_NONE) {
          if(NOGetNoticeLevel(au) == fwas) {
            controlreply(np, "No changes made to existing flags.");
          } else {
            char ourflags[FLAGBUFLEN], ournoticeflags[FLAGBUFLEN], diff[FLAGBUFLEN * 2 + 1], finalflags[FLAGBUFLEN];
            noperserv_update_autheduser(au);
            controlreply(np, "Flag alterations complete.");

            strlcpy(ourflags, printflags(NOGetAuthLevel(au), no_userflags), sizeof(ourflags));
            strlcpy(ournoticeflags, printflags(fwas, no_noticeflags), sizeof(ournoticeflags));
            strlcpy(diff, printflagdiff(fwas, NOGetNoticeLevel(au), no_noticeflags), sizeof(diff));
            controlwall(NO_OPER, NL_MANAGEMENT, "%s/%s (%s) successfully used NOTICEFLAGS (%s): %s", np->nick, np->authname, ourflags, ournoticeflags, diff);

            strlcpy(finalflags, printflags(NOGetNoticeLevel(au), no_noticeflags), sizeof(finalflags));
            for(np2=au->authname->nicks;np2;np2=np2->nextbyauthname)
              if(np2 != np) {
                controlreply(np2, "!!! %s just used NOTICEFLAGS (%s): %s", np->nick, ournoticeflags, diff);
                controlreply(np2, "Your notice flags are %s", finalflags);
              }
          }
        }
      } else {
        controlreply(np, "Unknown flag(s) supplied.");
        return CMD_ERROR;
      }
    } else {
      int typefound;
      char *itemfound;
      au = noperserv_autheduser_from_command(np, cargv[0], &typefound, &itemfound);
      if(!au)
        return CMD_ERROR;

      if(au != NOGetAuthedUser(np)) {
        controlreply(np, "Notice flags for %s %s are: %s", typefound==NO_FOUND_NICKNAME?"user":"authname", itemfound, printflags(NOGetNoticeLevel(au), no_noticeflags));
        return CMD_OK;
      }
    }
  } else {
    au = NOGetAuthedUser(np);
  }

  if(!au) /* shouldn't happen */
    return CMD_ERROR;

  controlreply(np, "Your notice flags are: %s", printflags(NOGetNoticeLevel(au), no_noticeflags));

  return CMD_OK;
}

/* @test */
static int noperserv_deluser(void *sender, int cargc, char **cargv) {
  nick *np2, *np = (nick *)sender;
  no_autheduser *target /* target user */, *au = NOGetAuthedUser(np); /* user executing command */
  char *userreturned = NULL; /* nickname or authname of the target, pulled from the db */
  int typefound; /* whether it was an authname or a username */
  char targetflags[FLAGBUFLEN], ourflags[FLAGBUFLEN], deleteduser[NOMax(ACCOUNTLEN, NICKLEN) + 1];

  if(cargc != 1)
    return CMD_USAGE;
  
  target = noperserv_autheduser_from_command(np, cargv[0], &typefound, &userreturned);
  if(!target)
    return CMD_ERROR;

  strlcpy(targetflags, printflags(NOGetAuthLevel(target), no_userflags), sizeof(targetflags));
  strlcpy(ourflags, printflags(NOGetAuthLevel(au), no_userflags), sizeof(ourflags));

  /* we have to copy it as it might point to an autheduser, which we're about to delete */
  strlcpy(deleteduser, userreturned, sizeof(deleteduser));

  /* we have to check if target != au, because if successful policy_modification_permitted just returns the flags we're allowed
     to modify, if we have no flags we won't be able to delete ourselves */
  if((target != au) && !noperserv_policy_permitted_modifications(au, target)) {
    controlreply(np, "Deletion denied."); 
    controlwall(NO_OPER, NL_MANAGEMENT, "%s/%s (%s) attempted to DELUSER %s (%s)", np->nick, np->authname, ourflags, target->authname->name, targetflags);

    return CMD_ERROR;
  }

  for(np2=target->authname->nicks;np2;np2=np2->nextbyauthname)
    if(np2 != np)
      controlreply(np2, "!!! %s/%s (%s) just DELUSERed you.", np->nick, np->authname, ourflags);

  noperserv_delete_autheduser(target);

  controlwall(NO_OPER, NL_MANAGEMENT, "%s/%s (%s) successfully used DELUSER on %s (%s)", np->nick, np->authname, ourflags, deleteduser, targetflags);

  if(target == au) {
    controlreply(np, "You have been deleted.");
  } else {
    controlreply(np, "%s %s deleted.", typefound==NO_FOUND_AUTHNAME?"Auth":"User", deleteduser);
  }

  return CMD_OK;
}

/* @test */
/* this command needs LOTS of checking */
static int noperserv_userflags(void *sender, int cargc, char **cargv) {
  nick *np2, *np = (nick *)sender;
  no_autheduser *au = NOGetAuthedUser(np), *target = NULL;
  char *flags = NULL, *nicktarget = NULL;
  int typefound;

  if(cargc == 0) {
    target = au;
  } else if(cargc == 1) {
    if((cargv[0][0] == '+') || (cargv[0][0] == '-')) { /* modify our own */
      flags = cargv[0];
      target = au;
    } else { /* viewing someone elses */
      nicktarget = cargv[0];
    }
  } else if(cargc == 2) {
    nicktarget = cargv[0];
    flags = cargv[1];
  } else {
    return CMD_USAGE;
  }

  if(nicktarget) {
    target = noperserv_autheduser_from_command(np, nicktarget, &typefound, &nicktarget);
    if(!target)
      return CMD_ERROR;
  }

  if(flags) {
    int ret;
    flag_t permitted = noperserv_policy_permitted_modifications(au, target), fwas = NOGetAuthLevel(target), fours = NOGetAuthLevel(au);

    ret = setflags(&target->authlevel, permitted, flags, no_userflags, REJECT_DISALLOWED | REJECT_UNKNOWN);
    if(ret != REJECT_UNKNOWN) {
      if(ret == REJECT_DISALLOWED) {
        flag_t fnow = fwas;
        setflags(&fnow, NO_ALL_FLAGS, flags, no_userflags, REJECT_NONE);
        if(fnow == fwas) {
          controlreply(np, "No changes made to existing flags.");
        } else {
          char targetflags[FLAGBUFLEN], ourflags[FLAGBUFLEN];
          controlreply(np, "Flag alterations denied.");

          strlcpy(targetflags, printflags(fwas, no_userflags), sizeof(targetflags));
          strlcpy(ourflags, printflags(fours, no_userflags), sizeof(ourflags));

          controlwall(NO_OPER, NL_MANAGEMENT, "%s/%s (%s) attempted to use USERFLAGS on %s (%s): %s", np->nick, np->authname, ourflags, target->authname->name, targetflags, printflagdiff(fwas, fnow, no_userflags));
          return CMD_ERROR;
        }
      } else if(ret == REJECT_NONE) {
        if(NOGetAuthLevel(target) == fwas) {
          controlreply(np, "No changes made to existing flags.");
        } else {
          char targetflags[FLAGBUFLEN], ourflags[FLAGBUFLEN], finalflags[FLAGBUFLEN];

          noperserv_policy_update_noticeflags(fwas, target);          
          noperserv_update_autheduser(target);

          controlreply(np, "Flag alterations complete.");

          strlcpy(targetflags, printflags(fwas, no_userflags), sizeof(targetflags));
          strlcpy(ourflags, printflags(fours, no_userflags), sizeof(ourflags));

          controlwall(NO_OPER, NL_MANAGEMENT, "%s/%s (%s) successfully used USERFLAGS on %s (%s): %s", np->nick, np->authname, ourflags, target->authname->name, targetflags, printflagdiff(fwas, NOGetAuthLevel(target), no_userflags));

          strlcpy(finalflags, printflags(NOGetAuthLevel(target), no_userflags), sizeof(finalflags));
          for(np2=target->authname->nicks;np2;np2=np2->nextbyauthname)
            if(np2 != np) {
              controlreply(np2, "!!! %s/%s (%s) just used USERFLAGS on you (%s): %s", np->nick, np->authname, ourflags, targetflags, printflagdiff(fwas, NOGetAuthLevel(target), no_userflags));
              controlreply(np2, "Your user flags are now: %s", finalflags);
              controlreply(np2, "Your notice flags are now: %s", printflags(target->noticelevel, no_noticeflags));
            }
        }
      }
    } else {
      controlreply(np, "Unknown flag(s) supplied.");
      return CMD_ERROR;
    }
  }

  if(target != au) {
    controlreply(np, "User flags for %s %s: %s", typefound==NO_FOUND_AUTHNAME?"auth":"user", nicktarget, printflags(NOGetAuthLevel(target), no_userflags));
    controlreply(np, "Notice flags for %s %s: %s", typefound==NO_FOUND_AUTHNAME?"auth":"user", nicktarget, printflags(target->noticelevel, no_noticeflags));
  } else {
    controlreply(np, "Your user flags are: %s", printflags(NOGetAuthLevel(target), no_userflags));
    controlreply(np, "Your notice flags are: %s", printflags(target->noticelevel, no_noticeflags));
  }

  return CMD_OK;
}

void _init() {
  registercontrolhelpcmd("hello", NO_OPERED | NO_AUTHED, 1, &noperserv_hello, "Syntax: HELLO ?nickname|#authname?\nCreates an account on the service for the specified nick, or if one isn't supplied, your nickname.");
  registercontrolhelpcmd("userflags", NO_ACCOUNT, 2, &noperserv_userflags,
    "Syntax: USERFLAGS <nickname|#authname> ?modifications?\n"
    " Views and modifies user permissions.\n"
    " If no nickname or authname is supplied, you are substituted for it.\n"
    " If no flags are supplied, flags are just displayed instead of modified."
    " Flags:\n"
    "  +o: Operator\n"
    "  +s: Staff member\n"
    "  +S: Security team member\n"
    "  +d: NOperserv developer\n"
    "  +t: Trust queue worker\n"
    "  +Y: Relay\n"
    " Additional flags may show up in SHOWCOMMANDS but are not userflags as such:\n"
    "  +r: Authed user\n"
    "  +R: Registered NOperserv user\n"
    "  +O: Must be /OPER'ed\n"
    "  +L: Legacy command\n"
  );
  registercontrolhelpcmd("noticeflags", NO_ACCOUNT, 1, &noperserv_noticeflags,
    "Syntax: NOTICEFLAGS ?(nickname|#authname)|flags?\n"
    " This command can view and modify your own notice flags, and view that of other users.\n"
    " Flags:\n"
    "  +m: Management (hello, password, userflags, noticeflags)\n"
    "  +t: Trusts\n"
    "  +k: KICK/KILL commands\n"
    "  +g: GLINE commands\n"
    "  +G: automated gline messages\n"
    "  +h: Shows when glines are set by code (hits)\n"
    "  +c: Clone information\n"
    "  +C: CLEARCHAN command\n"
    "  +f: FAKEUSER commands\n"
    "  +b: BROADCAST commands\n"
    "  +o: Operation commands, such as insmod, rmmod, die, etc\n"
    "  +O: /OPER\n"
    "  +I: Misc commands (resync, etc)\n"
    "  +n: Sends notices instead of privmsgs\n"
    "  +A: Every single command sent to the service (spammy)\n"
  );

  registercontrolhelpcmd("deluser", NO_OPERED | NO_ACCOUNT, 2, &noperserv_deluser, "Syntax: DELUSER <nickname|#authname>\nDeletes the specified user.");
}

void _fini() {
  deregistercontrolcmd("noticeflags", &noperserv_noticeflags);
  deregistercontrolcmd("userflags", &noperserv_userflags);
  deregistercontrolcmd("noticeflags", &noperserv_noticeflags);
  deregistercontrolcmd("hello", &noperserv_hello);
  deregistercontrolcmd("deluser", &noperserv_deluser);
}

