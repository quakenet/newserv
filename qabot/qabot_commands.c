#include <stdio.h>
#include <time.h>

#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"

#include "qabot.h"

int qabot_doshowcommands(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  Command* cmdlist[150];
  int i, j;

  sendnoticetouser(qabot_nick, np, "The following commands are registered at present:");
  for (i = 0, j = getcommandlist(qabot_commands, cmdlist, 150); i < j; i++) {
    if (cmdlist[i]->level)
      sendnoticetouser(qabot_nick, np, "%s (%s)", cmdlist[i]->command->content, qabot_uflagtostr(cmdlist[i]->level));
    else
      sendnoticetouser(qabot_nick, np, "%s", cmdlist[i]->command->content);
  }
  sendnoticetouser(qabot_nick, np, "End of list.");

  return CMD_OK;
}

int qabot_dohelp(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  
  if (cargc < 1) {
    sendnoticetouser(qabot_nick, np, "Syntax: help <command>");
    return CMD_ERROR;
  }
  
  return qabot_showhelp(np, cargv[0]);
}

int qabot_dohello(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;

  if (!IsOper(np)) {
    sendnoticetouser(qabot_nick, np, "You must be opered to use this command.");
    return CMD_ERROR;
  }

  if (qabot_users) {
    sendnoticetouser(qabot_nick, np, "This command cannot be used once the initial account has been created.");
    return CMD_ERROR;
  }
  
  qabot_adduser(np->authname, QAUFLAG_STAFF|QAUFLAG_ADMIN|QAUFLAG_DEVELOPER, time(0));
  qabot_savedb();
  sendnoticetouser(qabot_nick, np, "An account has been created with admin privileges.");

  return CMD_OK;
}

int qabot_dosave(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  
  sendnoticetouser(qabot_nick, np, "Saving...");
  qabot_savedb();
  sendnoticetouser(qabot_nick, np, "Done.");
  
  return CMD_OK;
}

int qabot_dolistbots(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  qab_bot* b;
  
  if (!qab_bots) {
    sendnoticetouser(qabot_nick, np, "There are no bots currently added.");
    return CMD_OK;
  }
  
  sendnoticetouser(qabot_nick, np, "The following bots are added:");
  for (b = qab_bots; b; b = b->next)
    sendnoticetouser(qabot_nick, np, "%s (%s@%s)", b->nick, b->user, b->host);
  sendnoticetouser(qabot_nick, np, "End of list.");

  return CMD_OK;
}

int qabot_dolistusers(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  qab_user* u;
  
  if (!qabot_users) {
    sendnoticetouser(qabot_nick, np, "There are no users currently added.");
    return CMD_OK;
  }
  
  sendnoticetouser(qabot_nick, np, "The following users are added:");
  for (u = qabot_users; u; u = u->next)
    sendnoticetouser(qabot_nick, np, "%-15s (%s)", u->authname, qabot_uflagtostr(u->flags));
  sendnoticetouser(qabot_nick, np, "End of list.");

  return CMD_OK;
}

int qabot_doshowbot(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  qab_bot* bot;
  nick* botnick;
  
  if (cargc < 1) {
    sendnoticetouser(qabot_nick, np, "Syntax: showbot <nick>");
    return CMD_ERROR;
  }
  
  if (!(botnick = getnickbynick(cargv[0]))) {
    sendnoticetouser(qabot_nick, np, "No such nickname.");
    return CMD_ERROR;
  }
  
  bot = botnick->exts[qabot_nickext];
  if (!bot) {
    sendnoticetouser(qabot_nick, np, "%s is not a QABot.", botnick->nick);
    return CMD_ERROR;
  }
  
  sendnoticetouser(qabot_nick, np, "- Information for %s ---", bot->nick);
  sendnoticetouser(qabot_nick, np, "User:                     %s", bot->user);
  sendnoticetouser(qabot_nick, np, "Host:                     %s", bot->host);
  sendnoticetouser(qabot_nick, np, "Public chan:              %s", bot->public_chan->name->content);
  sendnoticetouser(qabot_nick, np, "Question chan:            %s", bot->question_chan->name->content);
  sendnoticetouser(qabot_nick, np, "Staff chan:               %s", bot->staff_chan->name->content);
  sendnoticetouser(qabot_nick, np, "Spam interval:            %d", bot->spam_interval);
  sendnoticetouser(qabot_nick, np, "Nick block interval:      %d", bot->ask_wait);
  sendnoticetouser(qabot_nick, np, "Queued question interval: %d", bot->queued_question_interval);  
  sendnoticetouser(qabot_nick, np, "Lines spammed:            %d", bot->spammed);
  sendnoticetouser(qabot_nick, np, "Questions asked:          %d", bot->lastquestionID);
  sendnoticetouser(qabot_nick, np, "Questions answered:       %d", bot->answered);
  sendnoticetouser(qabot_nick, np, "Blocks:                   %d", bot->block_count);
  sendnoticetouser(qabot_nick, np, "Block control chars:      %s", (bot->flags & QAB_CONTROLCHAR) ? "Yes" : "No");
  sendnoticetouser(qabot_nick, np, "Block colour:             %s", (bot->flags & QAB_COLOUR) ? "Yes" : "No");
  sendnoticetouser(qabot_nick, np, "Authed users only:        %s", (bot->flags & QAB_AUTHEDONLY) ? "Yes" : "No");
  sendnoticetouser(qabot_nick, np, "Line break:               %s", (bot->flags & QAB_LINEBREAK) ? "Yes" : "No");
  sendnoticetouser(qabot_nick, np, "Question flood detection: %s", (bot->flags & QAB_FLOODDETECT) ? "Yes" : "No");
  sendnoticetouser(qabot_nick, np, "Question flood blocking:  %s", (bot->flags & QAB_FLOODSTOP) ? "Yes" : "No");
  sendnoticetouser(qabot_nick, np, "Blocks mark as spam:      %s", (bot->flags & QAB_BLOCKMARK) ? "Yes" : "No");
  if (bot->micnumeric) {
    nick* mnick = getnickbynumeric(bot->micnumeric);
    
    sendnoticetouser(qabot_nick, np, "Mic:                      Enabled (%s)", mnick ? mnick->nick : "UNKNOWN");
  }
  else
    sendnoticetouser(qabot_nick, np, "Mic:                      Disabled");
  sendnoticetouser(qabot_nick, sender, "Mic timeout:              %d", bot->mic_timeout);
  sendnoticetouser(qabot_nick, np, "End of list.");
  
  return CMD_OK;
}

int qabot_doaddbot(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  
  if (cargc < 3) {
    sendnoticetouser(qabot_nick, np, "Syntax: addbot <nick> <user> <host> <public channel> <question channel> <staff channel>");
    return CMD_ERROR;
  }
  
  if (!IsOper(np)) {
    sendnoticetouser(qabot_nick, np, "You must be opered to use this command.");
    return CMD_ERROR;
  }
  
  if (!qabot_addbot(cargv[0], cargv[1], cargv[2], cargv[3], cargv[4], cargv[5], DEFAULTBOTFLAGS, SPAMINTERVAL, ASKWAIT, QUEUEDQUESTIONINTERVAL, np))
    return CMD_ERROR;
  
  sendnoticetouser(qabot_nick, np, "Created %s!%s@%s (%s, %s, %s).", cargv[0], cargv[1], cargv[2], cargv[3], cargv[4], cargv[5]);
  
  return CMD_OK;
}

int qabot_dodelbot(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  nick* botnick;
  qab_bot* bot;
  
  if (cargc < 1) {
    sendnoticetouser(qabot_nick, np, "Syntax: delbot <nick>");
    return CMD_ERROR;
  }
  
  if (!IsOper(np)) {
    sendnoticetouser(qabot_nick, np, "You must be opered to use this command.");
    return CMD_ERROR;
  }
  
  if (!(botnick = getnickbynick(cargv[0]))) {
    sendnoticetouser(qabot_nick, np, "No such nickname.");
    return CMD_ERROR;
  }
  
  bot = botnick->exts[qabot_nickext];
  if (!bot) {
    sendnoticetouser(qabot_nick, np, "%s is not a QABot.", botnick->nick);
    return CMD_ERROR;
  }
  
  qabot_delbot(bot);
  sendnoticetouser(qabot_nick, np, "Bot deleted.");
  
  return CMD_OK;
}

int qabot_doadduser(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  char* ch;
  flag_t flags = 0;
  char* victim;
  
  if (cargc < 2) {
    sendnoticetouser(qabot_nick, np, "Syntax: adduser <nick> <flags>");
    return CMD_ERROR;
  }
  
  if (!(victim = qabot_getvictim(np, cargv[0])))
    return CMD_ERROR;
  
  if (qabot_getuser(victim)) {
    sendnoticetouser(qabot_nick, np, "Account %s already exists.", victim);
    return CMD_ERROR;
  }
  
  for (ch = cargv[1]; *ch; ch++) {
    switch (*ch) {
    case '+':
      break;
      
    case 'a':
      flags |= (QAUFLAG_ADMIN|QAUFLAG_STAFF);
      break;
      
    case 'd':
      flags |= (QAUFLAG_DEVELOPER|QAUFLAG_ADMIN|QAUFLAG_STAFF);
      
    case 's':
      flags |= QAUFLAG_STAFF;
      break;
      
    default:
      sendnoticetouser(qabot_nick, np, "Unknown flag '%c'.", *ch);
      return CMD_ERROR;
    }
  }
  
  qabot_adduser(victim, flags, time(0));
  sendnoticetouser(qabot_nick, np, "Account created.");
  
  return CMD_OK;
}

int qabot_dodeluser(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  qab_user* u;
  char* victim;
  
  if (cargc < 1) {
    sendnoticetouser(qabot_nick, np, "Syntax: <account>");
    return CMD_ERROR;
  }
  
  if (!(victim = qabot_getvictim(np, cargv[0])))
    return CMD_ERROR;
  
  if (!(u = qabot_getuser(victim))) {
    sendnoticetouser(qabot_nick, np, "Account does not exist.");
    return CMD_ERROR;
  }
  
  if (!ircd_strcmp(u->authname, np->authname)) {
    sendnoticetouser(qabot_nick, np, "You cannot delete your own account.");
    return CMD_ERROR;
  }
  
  qabot_squelchuser(u);
  sendnoticetouser(qabot_nick, np, "Account deleted.");
  
  return CMD_OK;
}

int qabot_dochangelev(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  char* victim;
  qab_user* u;
  
  if (cargc < 1) {
    sendnoticetouser(qabot_nick, np, "Syntax: changelev <nick|#authname> <flags>");
    return CMD_ERROR;
  }
  
  if (!(victim = qabot_getvictim(np, cargv[0])))
    return CMD_ERROR;
  
  if (!(u = qabot_getuser(victim))) {
    sendnoticetouser(qabot_nick, np, "Account does not exist.");
    return CMD_ERROR;
  }
  
  sendnoticetouser(qabot_nick, np, "TODO: Finish this! :)");
  
  return CMD_OK;
}

int qabot_dowhois(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  char* victim;
  qab_user* u;
  
  if (cargc < 1) {
    sendnoticetouser(qabot_nick, np, "Syntax: whois <nick|#authname>");
    return CMD_ERROR;
  }
  
  if (!(victim = qabot_getvictim(np, cargv[0])))
    return CMD_ERROR;
  
  if (!(u = qabot_getuser(victim))) {
    sendnoticetouser(qabot_nick, np, "Account does not exist.");
    return CMD_ERROR;
  }
  
  sendnoticetouser(qabot_nick, np, "Account: %s", u->authname);
  sendnoticetouser(qabot_nick, np, "Flags:   %s", qabot_uflagtostr(u->flags));
  sendnoticetouser(qabot_nick, np, "Created: %s", qabot_formattime(u->created));
  
  return CMD_OK;
}

int qabot_dostatus(void* sender, int cargc, char** cargv) {
  nick* np = (nick*)sender;
  int botc = 0, blockc = 0, userc = 0, spamc = 0, qc = 0, qac = 0;
  qab_bot* b;
  qab_user* u;
  
  for (b = qab_bots; b; b = b->next) {
    botc++;
    blockc += b->block_count;
    spamc += b->spammed;
    qc += b->lastquestionID;
    qac += b->answered;
  }
  
  for (u = qabot_users; u; u = u->next)
    userc++;
  
  sendnoticetouser(qabot_nick, np, "Up since:                 %s", qabot_formattime(qab_startime));
  sendnoticetouser(qabot_nick, np, "Bots:                     %d", botc);
  sendnoticetouser(qabot_nick, np, "Blocks:                   %d", blockc);
  sendnoticetouser(qabot_nick, np, "Users:                    %d", userc);
  sendnoticetouser(qabot_nick, np, "Total lines spammed:      %d", spamc);
  sendnoticetouser(qabot_nick, np, "Total questions asked:    %d", qc);
  sendnoticetouser(qabot_nick, np, "Total questions answered: %d", qac);
  
  return CMD_OK;
}
