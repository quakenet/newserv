#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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

time_t qab_startime;
int qabot_nickext;
int qabot_spam_nickext;
int qabot_chanext;
nick* qabot_nick = 0;
CommandTree* qabot_commands;
CommandTree* qabot_chancommands;
qab_bot* qab_bots = 0;
unsigned long qab_lastq_crc = 0;
int qab_lastq_count = 0;
qab_bot* qabot_currentbot = 0;
channel* qabot_currentchan = 0;

void _init() {
  qab_startime = time(0);
  
  registerhook(HOOK_NICK_LOSTNICK, qabot_lostnick);
  registerhook(HOOK_CHANNEL_PART, qabot_channel_part);
  
  qabot_commands = newcommandtree();
  addcommandtotree(qabot_commands, "showcommands", 0, 0, qabot_doshowcommands);
  addcommandtotree(qabot_commands, "help", QAUFLAG_STAFF, 1, qabot_dohelp);
  addcommandtotree(qabot_commands, "hello", 0, 0, qabot_dohello);
  addcommandtotree(qabot_commands, "save", QAUFLAG_ADMIN, 0, qabot_dosave);
  addcommandtotree(qabot_commands, "listbots", QAUFLAG_STAFF, 0, qabot_dolistbots);
  addcommandtotree(qabot_commands, "listusers", QAUFLAG_STAFF, 0, qabot_dolistusers);
  addcommandtotree(qabot_commands, "showbot", QAUFLAG_STAFF, 1, qabot_doshowbot);
  addcommandtotree(qabot_commands, "addbot", QAUFLAG_STAFF, 6, qabot_doaddbot);
  addcommandtotree(qabot_commands, "delbot", QAUFLAG_STAFF, 1, qabot_dodelbot);
  addcommandtotree(qabot_commands, "adduser", QAUFLAG_ADMIN, 2, qabot_doadduser);
  addcommandtotree(qabot_commands, "changelev", QAUFLAG_ADMIN, 2, qabot_dochangelev);
  addcommandtotree(qabot_commands, "deluser", QAUFLAG_ADMIN, 1, qabot_dodeluser);
  addcommandtotree(qabot_commands, "whois", QAUFLAG_STAFF, 1, qabot_dowhois);
  addcommandtotree(qabot_commands, "status", QAUFLAG_DEVELOPER, 0, qabot_dostatus);
  
  qabot_chancommands = newcommandtree();
  addcommandtotree(qabot_chancommands, "answer", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 2, qabot_dochananswer);
  addcommandtotree(qabot_chancommands, "block", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 2, qabot_dochanblock);
  addcommandtotree(qabot_chancommands, "clear", QAC_STAFFCHAN, 0, qabot_dochanclear);
  addcommandtotree(qabot_chancommands, "closechan", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 0, qabot_dochanclosechan);
  addcommandtotree(qabot_chancommands, "config", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 2, qabot_dochanconfig);
  addcommandtotree(qabot_chancommands, "help", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 1, qabot_dochanhelp);
  addcommandtotree(qabot_chancommands, "listblocks", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 0, qabot_dochanlistblocks);
  addcommandtotree(qabot_chancommands, "mic", QAC_STAFFCHAN, 0, qabot_dochanmic);
  addcommandtotree(qabot_chancommands, "moo", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 0, qabot_dochanmoo);
  addcommandtotree(qabot_chancommands, "offtopic", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 30, qabot_dochanofftopic);
  addcommandtotree(qabot_chancommands, "openchan", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 0, qabot_dochanopenchan);
  addcommandtotree(qabot_chancommands, "ping", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 0, qabot_dochanping);
  addcommandtotree(qabot_chancommands, "reset", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 1, qabot_dochanreset);
  addcommandtotree(qabot_chancommands, "spam", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 30, qabot_dochanspam);
  addcommandtotree(qabot_chancommands, "status", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 0, qabot_dochanstatus);
  addcommandtotree(qabot_chancommands, "unblock", QAC_QUESTIONCHAN|QAC_STAFFCHAN, 2, qabot_dochanunblock);

  if ((qabot_nickext = registernickext("QABOT")) == -1) {
    return;
  }
  
  if ((qabot_spam_nickext = registernickext("QABOT_SPAM")) == -1) {
    return;
  }
  
  qabot_loaddb();

  scheduleoneshot(time(NULL) + 1, &qabot_createfakeuser, NULL);
  scheduleoneshot(time(NULL) + QABOT_SAVEWAIT, &qabot_savetimer, NULL);
}

void _fini() {
  deregisterhook(HOOK_NICK_LOSTNICK, qabot_lostnick);
  deregisterhook(HOOK_CHANNEL_PART, qabot_channel_part);
  
  deleteschedule(0, qabot_savetimer, NULL);
  
  qabot_savedb();
  
  while (qab_bots)
    qabot_delbot(qab_bots);

  if (qabot_nickext != -1)
    releasenickext(qabot_nickext);
  
  if (qabot_spam_nickext != -1)
    releasenickext(qabot_spam_nickext);

  if (qabot_nick) {
      deregisterlocaluser(qabot_nick, "Module unloaded.");
      qabot_nick = NULL;
  }

  while (qabot_users)
    qabot_squelchuser(qabot_users);
  
  deletecommandfromtree(qabot_commands, "showcommands", qabot_doshowcommands);
  deletecommandfromtree(qabot_commands, "help", qabot_dohelp);
  deletecommandfromtree(qabot_commands, "hello", qabot_dohello);
  deletecommandfromtree(qabot_commands, "save", qabot_dosave);
  deletecommandfromtree(qabot_commands, "listbots", qabot_dolistbots);
  deletecommandfromtree(qabot_commands, "listusers", qabot_dolistusers);
  deletecommandfromtree(qabot_commands, "showbot", qabot_doshowbot);
  deletecommandfromtree(qabot_commands, "addbot", qabot_doaddbot);
  deletecommandfromtree(qabot_commands, "delbot", qabot_dodelbot);
  deletecommandfromtree(qabot_commands, "adduser", qabot_doadduser);
  deletecommandfromtree(qabot_commands, "changelev", qabot_dochangelev);
  deletecommandfromtree(qabot_commands, "deluser", qabot_dodeluser);
  deletecommandfromtree(qabot_commands, "whois", qabot_dowhois);
  deletecommandfromtree(qabot_commands, "status", qabot_dostatus);
  destroycommandtree(qabot_commands);
  
  deletecommandfromtree(qabot_chancommands, "answer", qabot_dochananswer);
  deletecommandfromtree(qabot_chancommands, "block", qabot_dochanblock);
  deletecommandfromtree(qabot_chancommands, "clear", qabot_dochanclear);
  deletecommandfromtree(qabot_chancommands, "closechan", qabot_dochanclosechan);
  deletecommandfromtree(qabot_chancommands, "config", qabot_dochanconfig);
  deletecommandfromtree(qabot_chancommands, "help", qabot_dochanhelp);
  deletecommandfromtree(qabot_chancommands, "listblocks", qabot_dochanlistblocks);
  deletecommandfromtree(qabot_chancommands, "mic", qabot_dochanmic);
  deletecommandfromtree(qabot_chancommands, "moo", qabot_dochanmoo);
  deletecommandfromtree(qabot_chancommands, "offtopic", qabot_dochanofftopic);
  deletecommandfromtree(qabot_chancommands, "openchan", qabot_dochanopenchan);
  deletecommandfromtree(qabot_chancommands, "ping", qabot_dochanping);
  deletecommandfromtree(qabot_chancommands, "reset", qabot_dochanreset);
  deletecommandfromtree(qabot_chancommands, "spam", qabot_dochanspam);
  deletecommandfromtree(qabot_chancommands, "status", qabot_dochanstatus);
  deletecommandfromtree(qabot_chancommands, "unblock", qabot_dochanunblock);
  destroycommandtree(qabot_chancommands);
}

void qabot_lostnick(int hooknum, void* arg) {
  nick* np = (nick*)arg;
  qab_bot* b;
  
  if (!qab_bots)
    return;
  
  if (!IsAccount(np))
    return;
  
  for (b = qab_bots; b; b = b->next) {
    if (b->micnumeric == np->numeric) {
      b->micnumeric = 0;
      sendmessagetochannel(b->np, b->staff_chan->channel, "Mic deactivated.");
      if (!b->lastspam)
        qabot_spamstored((void*)b);
    }
  }
}

void qabot_channel_part(int hooknum, void* arg) {
  channel* cp = (channel*)((void**)arg)[0];
  nick* np = (nick*)((void**)arg)[1];
  qab_bot* b;
  
  if (!qab_bots)
    return;
  
  if (!IsAccount(np))
    return;
  
  for (b = qab_bots; b; b = b->next) {
    if ((b->micnumeric == np->numeric) && (b->staff_chan->channel == cp)) {
      b->micnumeric = 0;
      sendmessagetochannel(b->np, b->staff_chan->channel, "Mic deactivated.");
      if (!b->lastspam)
        qabot_spamstored((void*)b);
    }
  }
}

void qabot_createfakeuser(void* arg) {
  channel* cp;
  
  if ((qabot_nick = registerlocaluser(QABOT_NICK, QABOT_USER, QABOT_HOST,
    QABOT_NAME, QABOT_ACCT, QABOT_UMDE, &qabot_handler))) {
    if ((cp = findchannel(QABOT_HOMECHAN))) {
      localjoinchannel(qabot_nick, cp);
      localgetops(qabot_nick, cp);
    }
    else
    localcreatechannel(qabot_nick, QABOT_HOMECHAN);
  }
}

void qabot_handler(nick* me, int type, void** args) {
  nick* sender;
  Command* cmd;
  channel* cp;
  char* cargv[50];
  int cargc;
  qab_user* u;

  switch (type) {
  case LU_PRIVMSG:
  case LU_SECUREMSG:
    sender = (nick*)args[0];

    if (!strncmp("\001VERSION", args[1], 8)) {
      sendnoticetouser(qabot_nick, sender, "\001VERSION %s\001", QABOT_NAME);
      return;
    }

	cargc = splitline((char*)args[1], cargv, 50, 0);
	cmd = findcommandintree(qabot_commands, cargv[0], 1);
	if (!cmd) {
	  sendnoticetouser(qabot_nick, sender, "Unknown command.");
	  return;
    }

    if (!IsAccount(sender)) {
      sendnoticetouser(qabot_nick, sender, "You must be authed to use this command.");
	  return;
	}

	u = qabot_getuser(sender->authname);
	if (cmd->level) {
      if (!u) {
        sendnoticetouser(qabot_nick, sender, "You need an account to use this command.");
        return;
	  }

	  if ((cmd->level & QAUFLAG_STAFF) && !QAIsStaff(u)) {
        sendnoticetouser(qabot_nick, sender, "You do not have access to this command.");
        return;
      }

      if ((cmd->level & QAUFLAG_ADMIN) && !QAIsAdmin(u)) {
        sendnoticetouser(qabot_nick, sender, "You do not have access to this command.");
        return;
      }
      
      if ((cmd->level & QAUFLAG_DEVELOPER) && !QAIsDeveloper(u)) {
        sendnoticetouser(qabot_nick, sender, "You do not have access to this command.");
        return;
      }
    }
    
    if (cmd->maxparams < (cargc-1)) {
      rejoinline(cargv[cmd->maxparams], cargc - (cmd->maxparams));
      cargc=(cmd->maxparams) + 1;
    }
    
    (cmd->handler)((void*)sender, cargc - 1, &(cargv[1]));

    break;

  case LU_KILLED:
    qabot_nick = NULL;
    scheduleoneshot(time(NULL) + 1, &qabot_createfakeuser, NULL);
    break;

  case LU_KICKED:
    cp = (channel*)args[1];
    if (cp) {
      localjoinchannel(qabot_nick, cp);
      localgetops(qabot_nick, cp);
    }
    break;
  }
}

void qabot_child_handler(nick* me, int type, void** args) {
  nick* sender;
  channel* cp;
  qab_bot* bot = me->exts[qabot_nickext];
  char* text;
  Command* cmd;
  char* cargv[50];
  int cargc;
  
  if (!bot) {
    return;
  }
  
  switch (type) {
  case LU_CHANMSG:
    sender = (nick*)args[0];
    cp = (channel*)args[1];
    text = (char*)args[2];
    
    if (*text == '!') {
      if (*(++text) == '\0') {
        sendnoticetouser(me, sender, "No command specified.");
        return;
      }
      cargc = splitline((char*)text, cargv, 50, 0);
      cmd = findcommandintree(qabot_chancommands, cargv[0], 1);
      if (!cmd) {
        sendnoticetouser(me, sender, "Unknown command.");
        return;
      }

      if ((cp->index == bot->staff_chan) && !(cmd->level & QAC_STAFFCHAN)) {
        sendnoticetouser(me, sender, "This command cannot be used in the staff channel.");
        return;
      }
      
      if ((cp->index == bot->question_chan) && !(cmd->level & QAC_QUESTIONCHAN)) {
        sendnoticetouser(me, sender, "This command cannot be used in the question channel.");
        return;
      }
      
      if (cmd->maxparams < (cargc-1)) {
        rejoinline(cargv[cmd->maxparams], cargc - (cmd->maxparams));
        cargc=(cmd->maxparams) + 1;
      }
      
      qabot_currentbot = bot;
      qabot_currentchan = cp;
      
      (cmd->handler)((void*)sender, cargc - 1, &(cargv[1]));
    }
    else if ((*text != '!') && (bot->micnumeric == sender->numeric) && (cp->index == bot->staff_chan)) {
      bot->lastmic = time(NULL);
      if (bot->lastspam) {
        qab_spam* s;
        
        s = (qab_spam*)malloc(sizeof(qab_spam));
        s->message = strdup(text);
        s->next = 0;
        bot->lastspam->next = s;
        bot->lastspam = s;
      }
      else {
        if ((bot->spamtime + bot->spam_interval) < time(0)) {
          sendmessagetochannel(me, bot->public_chan->channel, "%s", text);
          bot->spammed++;
          bot->spamtime = time(0);
        }
        else {
          qab_spam* s;
          
          s = (qab_spam*)malloc(sizeof(qab_spam));
          s->message = strdup(text);
          s->next = 0;
          bot->nextspam = bot->lastspam = s;
          scheduleoneshot(bot->spamtime + bot->spam_interval, qabot_spam, (void*)bot);
        }
      }
    }
    break;
  
  case LU_PRIVMSG:
  case LU_SECUREMSG:
    sender = (nick*)args[0];
    text = (char*)args[1];
    time_t lastmsg;
    
    lastmsg = (time_t)sender->exts[qabot_spam_nickext];
    
    if ((lastmsg + bot->ask_wait) > time(0)) {
      sendnoticetouser(me, sender, "You have already sent a question recently, please wait at least %d seconds between asking questions.", bot->ask_wait);
      return;
    }
    else {
      char hostbuf[NICKLEN + USERLEN + HOSTLEN + 3];
      qab_block* b;
      qab_question* q;
      unsigned long crc = 0;
      int len;
      
      sendnoticetouser(me, sender, "Your question has been relayed to the %s staff.", bot->public_chan->name->content);
      
      if (!getnumerichandlefromchanhash(bot->public_chan->channel->users, sender->numeric))
        return;
      
      if (*text == '<')
        text++;
      
      len = strlen(text);
      if ((len > 0) && (text[len - 1] == '>')) {
        text[len - 1] = '\0';
        len--;
      }
      
      if ((len < 5) || !strchr(text, ' ') || (*text == 1))
        return;
      
      if ((bot->flags & QAB_AUTHEDONLY) && !IsAccount(sender))
        return;
      
      if (bot->flags & QAB_CONTROLCHAR) {
        char* ch;
        
        for (ch = text; *ch; ch++)
          if ((*ch == 2) || (*ch == 3) || (*ch == 22) || (*ch == 27) || (*ch == 31))
            return;
      }
      else if (bot->flags & QAB_COLOUR) {
        char* ch;
        
        for (ch = text; *ch; ch++)
          if (*ch == 3)
            return;
      }
      
      if (bot->flags & QAB_FLOODDETECT) {
        crc = crc32i(text);
        if (crc == qab_lastq_crc) {
          qab_lastq_count++;
          if (qab_lastq_count >= 3) {
            if ((qab_lastq_count == 3) || ((qab_lastq_count % 10) == 0))
              sendmessagetochannel(me, bot->question_chan->channel, "WARNING: Possible question flood detected%s", (bot->flags & QAB_FLOODSTOP) ? " - AUTO IGNORED." : ".");
            if (bot->flags & QAB_FLOODSTOP)
              return;
          }
        }
        else {
          qab_lastq_crc = crc;
          qab_lastq_count = 1;
        }
      }
      
      sprintf(hostbuf,"%s!%s@%s", sender->nick, sender->ident, sender->host->name->content);
      
      for (b = bot->blocks; b; b = b->next) {
        switch (b->type) {
        case QABBLOCK_ACCOUNT:
          if (IsAccount(sender) && !ircd_strncmp(sender->authname, b->blockstr, ACCOUNTLEN))
            return;
          break;
          
        case QABBLOCK_HOST:
          if (!match(b->blockstr, hostbuf))
            return;
          break;
          
        case QABBLOCK_TEXT:
          if (!match(b->blockstr, text))
            return;
          break;
        }
      }
      
      sender->exts[qabot_spam_nickext] = (void*)time(0);
      
      q = (qab_question*)malloc(sizeof(qab_question));
      q->id = ++bot->lastquestionID;
      q->question = strdup(text);
      q->flags = QAQ_NEW;
      strncpy(q->nick, sender->nick, NICKLEN);
      q->nick[NICKLEN] = '\0';
      q->numeric = sender->numeric;
      q->crc = (bot->flags & QAB_FLOODDETECT) ? crc : 0;
      q->answer = 0;
      q->next = bot->questions[q->id % QUESTIONHASHSIZE];
      
      bot->questions[q->id % QUESTIONHASHSIZE] = q;
      
      sendmessagetochannel(me, bot->question_chan->channel, "ID: %3d <%s> %s", q->id, visiblehostmask(sender, hostbuf), text);
      
      if ((bot->flags & QAB_LINEBREAK) && ((bot->lastquestionID % 10) == 0))
        sendmessagetochannel(me, bot->question_chan->channel, "-----------------------------------");
    }
    break;
    
  case LU_KILLED:
    bot->np = NULL;
    scheduleoneshot(time(NULL) + 1, &qabot_createbotfakeuser, (void*)bot);
    break;

  case LU_KICKED:
    cp = (channel*)args[1];
    if (cp) {
      localjoinchannel(bot->np, cp);
      localgetops(bot->np, cp);
    }
    break;
  
  default:
    break;
  }
}

char* qabot_getvictim(nick* np, char* target) {
  if (*target != '#') {
    nick* victim;
    
    if (!(victim = getnickbynick(target))) {
      sendnoticetouser(qabot_nick, np, "No such nickname.");
      return 0;
    }
    
    if (!IsAccount(victim)) {
      sendnoticetouser(qabot_nick, np, "%s is not authed.", victim->nick);
      return 0;
    }
    
    return victim->authname;
  }
  
  return target + 1;
}

const char* qabot_uflagtostr(flag_t flags) {
  static char buf[20];
  int i = 0;
  
  buf[i++] = '+';
  
  if (flags & QAUFLAG_ADMIN)     { buf[i++] = 'a'; }
  if (flags & QAUFLAG_DEVELOPER) { buf[i++] = 'd'; }
  if (flags & QAUFLAG_STAFF)     { buf[i++] = 's'; }
  
  buf[i] = '\0';
  
  return buf;
}

const char* qabot_formattime(time_t tme) {
  static char buf[50];
  struct tm* tmp;
  
  tmp = gmtime(&tme);
  strftime(buf, 15, "%d/%m/%y %H:%M", tmp);
  
  return buf;
}

qab_bot* qabot_getcurrentbot() {
  return qabot_currentbot;
}

channel* qabot_getcurrentchannel() {
  return qabot_currentchan;
}
