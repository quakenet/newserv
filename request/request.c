 /* shroud's service request */

#include <stdio.h>
#include <string.h>
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../lib/version.h"
#include "../control/control.h"
#include "../splitlist/splitlist.h"
#include "request.h"
#include "request_block.h"
#include "lrequest.h"
#include "sqrequest.h"
#include "user.h"

MODULE_VERSION("");

nick *rqnick;
CommandTree *rqcommands;

void rq_registeruser(void);
void rq_handler(nick *target, int type, void **args);

int rqcmd_showcommands(void *user, int cargc, char **cargv);
int rqcmd_request(void *user, int cargc, char **cargv);
int rqcmd_requestspamscan(void *user, int cargc, char **cargv);
int rqcmd_addblock(void *user, int cargc, char **cargv);
int rqcmd_delblock(void *user, int cargc, char **cargv);
int rqcmd_listblocks(void *user, int cargc, char **cargv);
int rqcmd_stats(void *user, int cargc, char **cargv);
int rqcmd_requestop(void *user, int cargc, char **cargv);

int rqcmd_adduser(void *user, int cargc, char **cargv);
int rqcmd_deluser(void *user, int cargc, char **cargv);
int rqcmd_changelev(void *user, int cargc, char **cargv);
int rqcmd_userlist(void *user, int cargc, char **cargv);

#define min(a,b) ((a > b) ? b : a)

/* stats counters */
int rq_count = 0;
int rq_failed = 0;
int rq_success = 0;
int rq_blocked = 0;

/* log fd */
FILE *rq_logfd;

static int extloaded = 0;

void _init(void) {
  if(!rq_initblocks())
    return;

  extloaded = 1;

  rqcommands = newcommandtree();

  addcommandtotree(rqcommands, "showcommands", RQU_ANY, 1, &rqcmd_showcommands);
  addcommandtotree(rqcommands, "requestbot", RQU_ANY, 1, &rqcmd_request);
  addcommandtotree(rqcommands, "requestspamscan", RQU_ANY, 1, &rqcmd_requestspamscan);
  addcommandtotree(rqcommands, "requestop", RQU_ANY, 2, &rqcmd_requestop);

  addcommandtotree(rqcommands, "addblock", RQU_ACCOUNT, 3, &rqcmd_addblock);
  addcommandtotree(rqcommands, "delblock", RQU_ACCOUNT, 1, &rqcmd_delblock);
  addcommandtotree(rqcommands, "listblocks", RQU_ACCOUNT, 1, &rqcmd_listblocks);
  addcommandtotree(rqcommands, "stats", RQU_ACCOUNT, 1, &rqcmd_stats);

  addcommandtotree(rqcommands, "adduser", RQU_OPER, 2, &rqcmd_adduser);
  addcommandtotree(rqcommands, "deluser", RQU_OPER, 1, &rqcmd_deluser);
  addcommandtotree(rqcommands, "changelev", RQU_OPER, 2, &rqcmd_changelev);
  addcommandtotree(rqcommands, "userlist", RQU_OPER, 1, &rqcmd_userlist);
  
  qr_initrequest();
  ru_load();

  rq_logfd = fopen(RQ_LOGFILE, "a");
  
  scheduleoneshot(time(NULL) + 1, (ScheduleCallback)&rq_registeruser, NULL);
}

void _fini(void) {
  if(!extloaded)
    return;

  deregisterlocaluser(rqnick, NULL);

  deletecommandfromtree(rqcommands, "showcommands", &rqcmd_showcommands);
  deletecommandfromtree(rqcommands, "requestbot", &rqcmd_request);
  deletecommandfromtree(rqcommands, "requestspamscan", &rqcmd_requestspamscan);
  deletecommandfromtree(rqcommands, "requestop", &rqcmd_requestop);

  deletecommandfromtree(rqcommands, "addblock", &rqcmd_addblock);
  deletecommandfromtree(rqcommands, "delblock", &rqcmd_delblock);
  deletecommandfromtree(rqcommands, "listblocks", &rqcmd_listblocks);
  deletecommandfromtree(rqcommands, "stats", &rqcmd_stats);

  deletecommandfromtree(rqcommands, "adduser", &rqcmd_adduser);
  deletecommandfromtree(rqcommands, "deluser", &rqcmd_deluser);
  deletecommandfromtree(rqcommands, "changelev", &rqcmd_changelev);
  deletecommandfromtree(rqcommands, "userlist", &rqcmd_userlist);

  destroycommandtree(rqcommands);

  rq_finiblocks();
  qr_finirequest();
  ru_persist();

  if (rq_logfd != NULL)
    fclose(rq_logfd);

  deleteallschedules((ScheduleCallback)&rq_registeruser);
}

void rq_registeruser(void) {
  channel *cp;

  rqnick = registerlocaluserflags(RQ_REQUEST_NICK, RQ_REQUEST_USER, RQ_REQUEST_HOST,
                             RQ_REQUEST_REAL, RQ_REQUEST_AUTH, 1780711, 0,
                             UMODE_ACCOUNT | UMODE_SERVICE | UMODE_OPER,
                             rq_handler);

  cp = findchannel(RQ_TLZ);

  if (cp == NULL)
    localcreatechannel(rqnick, RQ_TLZ);
  else
    localjoinchannel(rqnick, cp);
}

char *rq_longtoduration(unsigned long interval) {
  static char buf[100];

  strncpy(buf, longtoduration(interval, 0), sizeof(buf));

  /* chop off last character if it's a space */
  if (buf[strlen(buf)] == ' ')
    buf[strlen(buf)] = '\0';

  return buf;
}

void rq_handler(nick *target, int type, void **params) {
  Command* cmd;
  nick* user;
  char* line;
  int cargc;
  char* cargv[30];

  switch (type) {
    case LU_PRIVMSG:
    case LU_SECUREMSG:
      user = params[0];
      line = params[1];
      cargc = splitline(line, cargv, 30, 0);

      if (cargc == 0)
        return;

      cmd = findcommandintree(rqcommands, cargv[0], 1);

      if (cmd == NULL) {
        sendnoticetouser(rqnick, user, "Unknown command.");

        return;
      }

      if ((cmd->level & RQU_OPER) && !IsOper(user)) {
        sendnoticetouser(rqnick, user, "Sorry, this command is not "
              "available to you.");

        return;
      }

      if ((cmd->level & RQU_ACCOUNT) && (!IsAccount(user) || ru_getlevel(user) == 0) && !IsOper(user)) {
        sendnoticetouser(rqnick, user, "Sorry, this command is not "
              "available to you.");

	return;
      }

      if (cargc - 1 > cmd->maxparams)
        rejoinline(cargv[cmd->maxparams], cargc - cmd->maxparams);

      /* handle the command */
      cmd->handler((void*)user, min(cargc - 1, cmd->maxparams), &(cargv[1]));

      break;
    case LU_KILLED:
      scheduleoneshot(time(NULL) + 5, (ScheduleCallback)&rq_registeruser, NULL);

      break;
    case LU_PRIVNOTICE:
      qr_handle_notice(params[0], params[1]);

      break;
  }
}

int rqcmd_showcommands(void *user, int cargc, char **cargv) {
  nick* np = (nick*)user;
  int n, i;
  Command* cmdlist[50];

  n = getcommandlist(rqcommands, cmdlist, 50);

  sendnoticetouser(rqnick, np, "Available commands:");
  sendnoticetouser(rqnick, np, "-------------------");

  for (i = 0; i < n; i++) {
    if ((cmdlist[i]->level & RQU_OPER) && !IsOper(np))
      continue;
 
    if ((cmdlist[i]->level & RQU_ACCOUNT) && !(IsOper(np) || (IsAccount(np) && ru_getlevel(np) > 0)))
      continue;

    sendnoticetouser(rqnick, np, "%s", cmdlist[i]->command->content);
  }

  sendnoticetouser(rqnick, np, "End of SHOWCOMMANDS");

  return 0;
}

int rq_genericrequestcheck(nick *np, char *channelname, channel **cp, nick **qnick) {
  unsigned long *userhand;
  rq_block *block;

  if (!IsAccount(np)) {
    sendnoticetouser(rqnick, np, "Error: You must be authed to perform a service request.");

    return RQ_ERROR;
  }

  *cp = findchannel(channelname);

  if (*cp == NULL) {
    sendnoticetouser(rqnick, np, "Error: Channel '%s' does not exist on the network.",
          channelname);

    return RQ_ERROR;
  }

  *qnick = getnickbynick(RQ_QNICK);

  if (*qnick == NULL || findserver(RQ_QSERVER) < 0) {
    sendnoticetouser(rqnick, np, "Error: %s does not seem to be online. "
          "Please try again later.", RQ_QNICK);

    return RQ_ERROR;
  }

  userhand = getnumerichandlefromchanhash((*cp)->users, np->numeric);

  if (userhand == NULL) {
    sendnoticetouser(rqnick, np, "Error: You're not on that channel.");

    return RQ_ERROR;
  }

  if ((*userhand & CUMODE_OP) == 0) {
    sendnoticetouser(rqnick, np, "Error: You must be op'd on the channel to "
          "request a service.");

    return RQ_ERROR;
  }

  block = rq_findblock(channelname);

  if (block != NULL) {
     /* only say when block expires if <7 days */
     if ( block->expires < getnettime() + 3600 * 24 * 7) {
       sendnoticetouser(rqnick, np, "Error: You are not allowed to request a "
            "service to '%s'. Keep waiting for at least %s before you try again.",
            channelname, rq_longtoduration(block->expires - getnettime()));
       /* give them another 5 minutes to think about it */
       block->expires += 300;
       rq_saveblocks();
     } else {
       sendnoticetouser(rqnick, np, "Error: You are not allowed to request a "
          "service to '%s'.", channelname);
     }
    sendnoticetouser(rqnick, np, "Reason: %s", block->reason->content);

    rq_blocked++;

    return RQ_ERROR;
  }
  
  block = rq_findblock(np->authname);
  
  /* only tell the user if the block is going to expire in the next 48 hours
     so we can have our fun with longterm blocks.
     the request subsystems should deal with longterm blocks on their own */
  if (block != NULL && block->expires < getnettime() + 3600 * 24 * 2) {
    sendnoticetouser(rqnick, np, "Error: You are not allowed to request a "
          "service. Keep waiting for at least %s before you try again.", 
          rq_longtoduration(block->expires - getnettime()));

    sendnoticetouser(rqnick, np, "Reason: %s", block->reason->content);

    /* give them another 5 minutes to think about it */
    block->expires += 300;
    rq_saveblocks();

    rq_blocked++;
    
    return RQ_ERROR;
  }
  
  return RQ_OK;
}

int rqcmd_request(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  nick *qnick;
  unsigned long *qhand;
  channel *cp;
  int retval;
  time_t now_ts;
  char now[50];

  if (cargc < 1) {
    sendnoticetouser(rqnick, np, "Syntax: requestbot <#channel>");

    return RQ_ERROR;
  }

  rq_count++;

  if (rq_genericrequestcheck(np, cargv[0], &cp, &qnick) == RQ_ERROR) {
    rq_failed++;

    return RQ_ERROR;
  }

  qhand = getnumerichandlefromchanhash(cp->users, qnick->numeric);

  if (qhand != NULL) {
    sendnoticetouser(rqnick, np, "Error: %s is already on that channel.", RQ_QNICK);

    rq_failed++;

    return RQ_ERROR;
  }

  retval = RQ_ERROR;

  retval = lr_requestl(rqnick, np, cp, qnick);

  if (rq_logfd != NULL) {
    now[0] = '\0';
    now_ts = time(NULL);
    strftime(now, sizeof(now), "%c", localtime(&now_ts));

    fprintf(rq_logfd, "%s: request (%s) for %s from %s!%s@%s%s%s: Request was %s.\n",
      now, RQ_QNICK, cp->index->name->content,
      np->nick, np->ident, np->host->name->content, IsAccount(np)?"/":"", IsAccount(np)?np->authname:"",
      (retval == RQ_OK) ? "accepted" : "denied");
    fflush(rq_logfd);
  }

  if (retval == RQ_ERROR)
    rq_failed++;
  else if (retval == RQ_OK)
    rq_success++;

  return retval;
}

int rqcmd_requestspamscan(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  channel *cp;
  nick *qnick, *snick;
  unsigned long *qhand, *shand;
  int retval;

  if (cargc < 1) {
    sendnoticetouser(rqnick, np, "Syntax: requestspamscan <#channel>");

    return RQ_ERROR;
  }

  rq_count++;

  if (rq_genericrequestcheck(np, cargv[0], &cp, &qnick) == RQ_ERROR) {
    rq_failed++;

    return RQ_ERROR;
  }

  snick = getnickbynick(RQ_SNICK);

  if (snick == NULL || findserver(RQ_SSERVER) < 0) {
    sendnoticetouser(rqnick, np, "Error: %s does not seem to be online. "
            "Please try again later.", RQ_SNICK);

    rq_failed++;

    return RQ_ERROR;
  }

  /* does the user already have S on that channel? */  
  shand = getnumerichandlefromchanhash(cp->users, snick->numeric);

  if (shand != NULL) {
    sendnoticetouser(rqnick, np, "Error: %s is already on that channel.", RQ_SNICK);

    rq_failed++;

    return RQ_ERROR;
  }

  /* we need Q */
  qhand = getnumerichandlefromchanhash(cp->users, qnick->numeric);

  if (qhand) {
    /* great, now try to request */
    retval = qr_requests(rqnick, np, cp, qnick);
    
    if (retval == RQ_OK)
      rq_success++;
    else if (retval == RQ_ERROR)
      rq_failed++;
      
      return retval;
  } else {
    /* channel apparently doesn't have Q */
    
   sendnoticetouser(rqnick, np, "Error: You need %s in order to be "
        "able to request %s.", RQ_QNICK, RQ_SNICK);

    rq_failed++;

   return RQ_ERROR; 
  }
}

int rqcmd_requestop(void *source, int cargc, char **cargv) {
  nick *np2, *np = (nick *)source;
  nick *user = np;
  channel *cp;
  int ret, a, count;
  unsigned long *hand;
  modechanges changes;

  if (cargc < 1) {
    sendnoticetouser(rqnick, np, "Syntax: requestop <#channel> [nick]");

    return CMD_ERROR;
  }

  cp = findchannel(cargv[0]);

  if (cp == NULL) {
    sendnoticetouser(rqnick, np, "Error: Channel '%s' does not exist on the network.",
      cargv[0]);

    return CMD_ERROR;
  }

  if (cargc > 1) {
    user = getnickbynick(cargv[1]);

    if (!user) {
      sendnoticetouser(rqnick, np, "Error: No such user.");

      return CMD_ERROR;
    }
  }

  if (getnettime() - np->timestamp < 300) {
    sendnoticetouser(rqnick, np, "Error: You connected %s ago. To"
    			" request ops you must have been on the network for"
			" at least 5 minutes.",
			rq_longtoduration(getnettime() - np->timestamp));

    return CMD_ERROR;
  }

  if (getnettime() - user->timestamp < 300) {
    sendnoticetouser(rqnick, np, "Error: The nick you requested op for"
		    	" connected %s ago. To request op, it must have"
			"been on the network for at least 5 minutes.",
			rq_longtoduration(getnettime() - user->timestamp));

    return CMD_ERROR;
  }

  hand = getnumerichandlefromchanhash(cp->users, user->numeric);

  if (!hand) {
    sendnoticetouser(rqnick, np, "Error: User %s is not on channel '%s'.", user->nick, cargv[0]);

    return CMD_ERROR;
  }


  count = 0;

  localsetmodeinit(&changes, cp, rqnick);

  /* reop any services first */
  for(a=0;a<cp->users->hashsize;a++) {
    if(cp->users->content[a] != nouser) {
      np2 = getnickbynumeric(cp->users->content[a]);

      if (IsService(np2) && (np2->nick[1] == '\0') && !(cp->users->content[a] & CUMODE_OP)) {
        localdosetmode_nick(&changes, np2, MC_OP);
        count++;
      }
    }
  }

  localsetmodeflush(&changes, 1);

  if (count > 0) {
    if (count == 1)
      sendnoticetouser(rqnick, np, "1 service was reopped.");
    else
      sendnoticetouser(rqnick, np, "%d services were reopped.", count);

    return CMD_ERROR;
  }

  for (a=0;a<cp->users->hashsize;a++) {
    if ((cp->users->content[a] != nouser) && (cp->users->content[a] & CUMODE_OP)) {
      sendnoticetouser(rqnick, np, "There are ops on channel '%s'. This command can only be"
		      " used if there are no ops.", cargv[0]);

      return CMD_ERROR;
    }
  }

  if (sp_countsplitservers(SERVERTYPEFLAG_USER_STATE) > 0) {
    sendnoticetouser(rqnick, np, "One or more servers are currently split. Wait until the"
		    " netsplit is over and try again.");

    return CMD_ERROR;
  }

  if (cf_wouldreop(user, cp)) {
    localsetmodeinit(&changes, cp, rqnick);
    localdosetmode_nick(&changes, user, MC_OP);
    localsetmodeflush(&changes, 1);

    sendnoticetouser(rqnick, np, "Chanfix opped you on the specified channel.");
  } else {
    ret = cf_fixchannel(cp);

    if (ret == CFX_NOUSERSAVAILABLE)
      sendnoticetouser(rqnick, np, "Chanfix knows regular ops for that channel. They will"
                   " be opped when they return.");
    else
      sendnoticetouser(rqnick, np, "Chanfix has opped known ops for that channel.");
  }

  return CMD_OK;

}

int rqcmd_addblock(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  rq_block *block;
  time_t expires;
  char *account;
  int level = ru_getlevel(np);

  if (level < 20) {
    sendnoticetouser(rqnick, np, "You do not have access to this command.");

    return RQ_ERROR;
  }
  
  if (cargc < 3) {
    sendnoticetouser(rqnick, np, "Syntax: addblock <mask> <duration> <reason>");

    return RQ_ERROR;
  }

  block = rq_findblock(cargv[0]);

  if (block != NULL) {
    sendnoticetouser(rqnick, np, "That mask is already blocked by %s "
          "(reason: %s).", block->creator->content, block->reason->content);

    return RQ_ERROR;
  }

  if (IsAccount(np))
    account = np->authname;
  else
    account = "unknown";

  expires = getnettime() + durationtolong(cargv[1]);

  if (expires > getnettime() + RQU_HELPER_MAXEXPIRE && level < 30) {
    sendnoticetouser(rqnick, np, "Maximum expiry time is %s.", rq_longtoduration(RQU_HELPER_MAXEXPIRE));

    return RQ_ERROR;
  }
  
  rq_addblock(cargv[0], cargv[2], account, 0, expires);

  sendnoticetouser(rqnick, np, "Blocked channels/accounts matching '%s' from "
        "requesting a service.", cargv[0]);

  return RQ_OK;
}

int rqcmd_delblock(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  int result, level;
  rq_block *block;

  level = ru_getlevel(np);
  
  if (level < 20) {
    sendnoticetouser(rqnick, np, "You do not have access to this command.");

    return RQ_ERROR;
  }
 
  if (cargc < 1) {
    sendnoticetouser(rqnick, np, "Syntax: delblock <mask>");

    return RQ_ERROR;
  }

  block = rq_findblock(cargv[0]);

  if (block != NULL && level < 50) {
    if (ircd_strcmp(block->creator->content, np->authname) != 0) {
      sendnoticetouser(rqnick, np, "This block was created by someone else. You cannot remove it.");
      
      return RQ_ERROR;
    }
  }

  result = rq_removeblock(cargv[0]);

  if (result > 0) {
    sendnoticetouser(rqnick, np, "Block for '%s' was removed.", cargv[0]);

    return RQ_OK;
  } else {
    sendnoticetouser(rqnick, np, "There is no such block.");

    return RQ_OK;
  }
}

int rqcmd_listblocks(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  rq_block block;
  int i, level;

  level = ru_getlevel(np);
  
  if (level < 10) {
    sendnoticetouser(rqnick, np, "You do not have access to this command.");

    return RQ_ERROR;
  }
  
  sendnoticetouser(rqnick, np, "Mask        By        Expires"
        "                   Reason");

  for (i = 0; i < rqblocks.cursi; i++) {
    block = ((rq_block*)rqblocks.content)[i];
    
    if (block.expires != 0 && block.expires < getnettime())
      continue; /* ignore blocks which have already expired, 
                   rq_findblock will deal with them later on */

    if (cargc < 1 || match2strings(block.pattern->content, cargv[0]))
      sendnoticetouser(rqnick, np, "%-11s %-9s %-25s %s",
            block.pattern->content, block.creator->content,
            rq_longtoduration(block.expires - getnettime()),
            block.reason->content);
  }

  sendnoticetouser(rqnick, np, "--- End of blocklist");

  return RQ_OK;
}

int rqcmd_stats(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  int level = ru_getlevel(np);

  if (level < 10) {
    sendnoticetouser(rqnick, np, "You do not have access to this command.");

    return RQ_ERROR;
  }
  
  sendnoticetouser(rqnick, np, "Total requests:                   %d", rq_count);
  sendnoticetouser(rqnick, np, "Successful requests:              %d", rq_success);
  sendnoticetouser(rqnick, np, "Failed requests:                  %d", rq_failed);
  sendnoticetouser(rqnick, np, "- Blocked:                        %d", rq_blocked);

  lr_requeststats(rqnick, np);
  qr_requeststats(rqnick, np);

  return RQ_OK;
}

int rqcmd_adduser(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  int result, level;

  if (cargc < 2) {
    sendnoticetouser(rqnick, np, "Syntax: adduser <account> <level>");

    return RQ_ERROR;
  }

  level = atoi(cargv[1]);

  if (level <= 0) {
    sendnoticetouser(rqnick, np, "Level must be a positive integer.");

    return RQ_ERROR;
  }

  result = ru_create(cargv[0], level);

  if (result) {
    sendnoticetouser(rqnick, np, "User '%s' was added with level '%d'.", cargv[0], level);

    return RQ_OK;
  } else {
    sendnoticetouser(rqnick, np, "Something strange happened. Contact shroud.");

    return RQ_ERROR;
  }
}

int rqcmd_deluser(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  int level;

  if (cargc < 1) {
    sendnoticetouser(rqnick, np, "Syntax: deluser <account>");

    return RQ_ERROR;
  }

  level = ru_getlevel_str(cargv[0]);

  if (level <= 0) {
    sendnoticetouser(rqnick, np, "There is no such user.");

    return RQ_ERROR;
  }

  ru_destroy(cargv[0]);

  sendnoticetouser(rqnick, np, "Done.");

  return RQ_OK;
}

int rqcmd_changelev(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  int result, level;

  if (cargc < 2) {
    sendnoticetouser(rqnick, np, "Syntax: changelev <account> <level>");

    return RQ_ERROR;
  }

  level = atoi(cargv[1]);

  if (level <= 0) {
    sendnoticetouser(rqnick, np, "Level must be a positive integer.");

    return RQ_ERROR;
  }

  if (ru_getlevel_str(cargv[0]) <= 0) {
    sendnoticetouser(rqnick, np, "Unknown user.");

    return RQ_ERROR;
  }
  
  result = ru_setlevel(cargv[0], level);
  
  if (result != 0) {
    sendnoticetouser(rqnick, np, "Done.");

    return RQ_OK;
  } else {
    sendnoticetouser(rqnick, np, "Something strange happened. Contact shroud.");

    return RQ_ERROR;
  }
}

int rqcmd_userlist(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  r_user_t *userp = r_userlist;

  sendnoticetouser(rqnick, np, "User Level");

  while (userp) {
    sendnoticetouser(rqnick, np, "%s %d", userp->name, userp->level);
    userp = userp->next;
  }

  sendnoticetouser(rqnick, np, "--- End of USERS.");

  return RQ_OK;
}

