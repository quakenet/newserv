 /* shroud's service request */

#include <string.h>
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../control/control.h"
#include "request.h"
#include "request_block.h"
#include "lrequest.h"
#include "sqrequest.h"

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
int rqcmd_legacyrequest(void *user, int cargc, char **cargv);

#define min(a,b) ((a > b) ? b : a)

/* stats counters */
int rq_count = 0;
int rq_failed = 0;
int rq_success = 0;
int rq_blocked = 0;

void _init(void) {
  rqcommands = newcommandtree();

  addcommandtotree(rqcommands, "showcommands", RQU_ANY, 1, &rqcmd_showcommands);
  addcommandtotree(rqcommands, "requestbot", RQU_ANY, 1, &rqcmd_request);
  addcommandtotree(rqcommands, "requestspamscan", RQU_ANY, 1, &rqcmd_requestspamscan);
  addcommandtotree(rqcommands, "addblock", RQU_OPER, 3, &rqcmd_addblock);
  addcommandtotree(rqcommands, "delblock", RQU_OPER, 1, &rqcmd_delblock);
  addcommandtotree(rqcommands, "listblocks", RQU_OPER, 1, &rqcmd_listblocks);
  addcommandtotree(rqcommands, "stats", RQU_OPER, 1, &rqcmd_stats);

  /* old legacy stuff */
  addcommandtotree(rqcommands, "requestq", RQU_ANY, 1, &rqcmd_legacyrequest);

  rq_initblocks();
  qr_initrequest();

  scheduleoneshot(time(NULL) + 1, (ScheduleCallback)&rq_registeruser, NULL);
}

void _fini(void) {
  deregisterlocaluser(rqnick, NULL);

  deletecommandfromtree(rqcommands, "showcommands", &rqcmd_showcommands);
  deletecommandfromtree(rqcommands, "requestbot", &rqcmd_request);
  deletecommandfromtree(rqcommands, "requestspamscan", &rqcmd_requestspamscan);
  deletecommandfromtree(rqcommands, "addblock", &rqcmd_addblock);
  deletecommandfromtree(rqcommands, "delblock", &rqcmd_delblock);
  deletecommandfromtree(rqcommands, "listblocks", &rqcmd_listblocks);
  deletecommandfromtree(rqcommands, "stats", &rqcmd_stats);

  /* old legacy stuff */
  deletecommandfromtree(rqcommands, "requestq", &rqcmd_legacyrequest);

  destroycommandtree(rqcommands);

  rq_finiblocks();
  qr_finirequest();

  deleteallschedules((ScheduleCallback)&rq_registeruser);
}

void rq_registeruser(void) {
  channel *cp;

  rqnick = registerlocaluser(RQ_REQUEST_NICK, RQ_REQUEST_USER, RQ_REQUEST_HOST,
                             RQ_REQUEST_REAL, RQ_REQUEST_AUTH,
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

      if (cmd->level & RQU_OPER && !IsOper(user)) {
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
      qr_handlenotice(params[0], params[1]);

      break;
  }
}

int rqcmd_showcommands(void *user, int cargc, char **cargv) {
  int n, i;
  Command* cmdlist[50];

  n = getcommandlist(rqcommands, cmdlist, 50);

  sendnoticetouser(rqnick, (nick*)user, "Available commands:");
  sendnoticetouser(rqnick, (nick*)user, "-------------------");

  for (i = 0; i < n; i++) {
    if ((cmdlist[i]->level & RQU_OPER) == 0 || IsOper((nick*)user))
      sendnoticetouser(rqnick, (nick*)user, "%s", cmdlist[i]->command->content);
  }

  sendnoticetouser(rqnick, (nick*)user, "End of SHOWCOMMANDS");

  return 0;
}

int rq_genericrequestcheck(nick *np, char *channelname, channel **cp, nick **lnick, nick **qnick) {
  unsigned long *userhand;
  rq_block *block;

  if (!IsAccount(np)) {
    sendnoticetouser(rqnick, np, "Error: You must be authed.");

    return RQ_ERROR;
  }

  *cp = findchannel(channelname);

  if (*cp == NULL) {
    sendnoticetouser(rqnick, np, "Error: Channel %s does not exist.",
          channelname);

    return RQ_ERROR;
  }

  *lnick = getnickbynick(RQ_LNICK);

  if (*lnick == NULL || findserver(RQ_LSERVER) < 0) {
    sendnoticetouser(rqnick, np, "Error: %s does not seem to be online. "
          "Try again later.", RQ_LNICK);

    return RQ_ERROR;
  }

  *qnick = getnickbynick(RQ_QNICK);

  if (*qnick == NULL || findserver(RQ_QSERVER) < 0) {
    sendnoticetouser(rqnick, np, "Error: %s does not seem to be online. "
          "Try again later.", RQ_QNICK);

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
    sendnoticetouser(rqnick, np, "Error: You are not allowed to request a "
          "service to this channel.");
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
  nick *lnick, *qnick;
  unsigned long *lhand, *qhand;
  channel *cp;
  int retval;

  if (cargc < 1) {
    sendnoticetouser(rqnick, np, "Syntax: requestbot <#channel>");

    return RQ_ERROR;
  }

  rq_count++;

  if (rq_genericrequestcheck(np, cargv[0], &cp, &lnick, &qnick) == RQ_ERROR) {
    rq_failed++;

    return RQ_ERROR;
  }

  lhand = getnumerichandlefromchanhash(cp->users, lnick->numeric);

  qhand = getnumerichandlefromchanhash(cp->users, qnick->numeric);

  if (qhand != NULL) {
    sendnoticetouser(rqnick, np, "Error: %s is already on that channel.", RQ_QNICK);

    rq_failed++;

    return RQ_ERROR;
  }

  retval = RQ_ERROR;

  if (lhand == NULL && qhand == NULL) {
    /* try 'instant' Q request */
    retval = qr_instantrequestq(np, cp);
  }

  if (retval == RQ_ERROR) {
    if (lhand == NULL) {
      /* user 'wants' L */

      retval = lr_requestl(rqnick, np, cp, lnick);
    } else {
      /* user 'wants' Q */

      retval = qr_requestq(rqnick, np, cp, lnick, qnick);
    }
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
  nick *lnick, *qnick, *snick;
  unsigned long *lhand, *qhand, *shand;
  int retval;

  if (cargc < 1) {
    sendnoticetouser(rqnick, np, "Syntax: requestspamscan <#channel>");

    return RQ_ERROR;
  }

  rq_count++;

  if (rq_genericrequestcheck(np, cargv[0], &cp, &lnick, &qnick) == RQ_ERROR) {
    rq_failed++;

    return RQ_ERROR;
  }

  snick = getnickbynick(RQ_SNICK);

  if (snick == NULL || findserver(RQ_SSERVER) < 0) {
    sendnoticetouser(rqnick, np, "Error: %s does not seem to be online. "
            "Try again later.", RQ_SNICK);

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

  /* we need either L or Q */
  lhand = getnumerichandlefromchanhash(cp->users, lnick->numeric);
  qhand = getnumerichandlefromchanhash(cp->users, qnick->numeric);

  if (lhand || qhand) {
    /* great, now try to request */
    retval = qr_requests(rqnick, np, cp, lnick, qnick);
    
    if (retval == RQ_OK)
      rq_success++;
    else if (retval == RQ_ERROR)
      rq_failed++;
      
      return retval;
  } else {
    /* channel apparently doesn't have L or Q */
    
   sendnoticetouser(rqnick, np, "Error: You need %s or %s in order to be "
        "able to request %s.", RQ_LNICK, RQ_QNICK, RQ_SNICK);

    rq_failed++;

   return RQ_ERROR; 
  }
}

int rqcmd_addblock(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  rq_block *block;
  time_t expires;
  char *account;

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

  rq_addblock(cargv[0], cargv[2], account, 0, expires);

  sendnoticetouser(rqnick, np, "Blocked channels/accounts matching '%s' from "
        "requesting a service.", cargv[0]);

  return RQ_OK;
}

int rqcmd_delblock(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;
  int result;

  if (cargc < 1) {
    controlreply(np, "Syntax: delblock <mask>");

    return RQ_ERROR;
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
  int i;

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

  sendnoticetouser(rqnick, np, "Total requests:                   %d", rq_count);
  sendnoticetouser(rqnick, np, "Successful requests:              %d", rq_success);
  sendnoticetouser(rqnick, np, "Failed requests:                  %d", rq_failed);
  sendnoticetouser(rqnick, np, "- Blocked:                        %d", rq_blocked);

  lr_requeststats(rqnick, np);
  qr_requeststats(rqnick, np);

  return RQ_OK;
}

int rqcmd_legacyrequest(void *user, int cargc, char **cargv) {
  nick *np = (nick*)user;

  sendnoticetouser(rqnick, np, "This command is no longer valid. Please use "
  "/msg %s REQUESTBOT #channel instead.", RQ_REQUEST_NICK);
  
  return RQ_OK;
}
