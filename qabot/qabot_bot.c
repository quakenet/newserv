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

qab_bot* qabot_getbot() {
  qab_bot* bot;
  int i;
  
  bot = (qab_bot*)malloc(sizeof(qab_bot));
  bot->nick[0] = '\0';
  bot->user[0] = '\0';
  bot->host = 0;
  bot->np = 0;
  bot->public_chan = 0;
  bot->question_chan = 0;
  bot->staff_chan = 0;
  bot->blocks = 0;
  bot->block_count = 0;
  
  bot->spammed = 0;
  
  bot->flags = DEFAULTBOTFLAGS;
  
  /*bot->question_interval = QUESTIONINTERVAL;*/
  bot->spam_interval = SPAMINTERVAL;
  bot->ask_wait = ASKWAIT;
  bot->queued_question_interval = QUEUEDQUESTIONINTERVAL;
  
  bot->lastquestionID = 0;
  bot->answered = 0;
  
  for (i = 0; i < QUESTIONHASHSIZE; i++)
    bot->questions[i] = 0;
  
  bot->nextspam = 0;
  bot->lastspam = 0;
  bot->spamtime = 0;
  bot->answers = 0;
  
  bot->micnumeric = 0;
  bot->recnumeric = 0;
  bot->recfile = NULL;
  bot->playfile = NULL;
  
  bot->next = 0;
  bot->prev = 0;

  return bot;
}

int qabot_addbot(char* nickname, char* user, char* host, char* pub_chan, char* qu_chan, char* stff_chan, flag_t flags, int spam_interval, 
  int ask_wait, int queued_question_interval, nick* sender) {
  qab_bot* bot;
  channel* cp;
  
  if (*pub_chan != '#') {
    if (sender)
      sendnoticetouser(qabot_nick, sender, "Invalid public channel.");
    return 0;
  }
  
  if (*qu_chan != '#') {
    if (sender)
      sendnoticetouser(qabot_nick, sender, "Invalid question channel.");
    return 0;
  }
  
  if (*stff_chan != '#') {
    if (sender)
      sendnoticetouser(qabot_nick, sender, "Invalid staff channel.");
    return 0;
  }
  
  for (bot = qab_bots; bot; bot = bot->next) {
    if (!ircd_strcmp(bot->nick, nickname)) {
      if (sender)
        sendnoticetouser(qabot_nick, sender, "That QABot already exists.");
      return 0;
    }
    
    if (!ircd_strcmp(bot->public_chan->name->content, pub_chan)) {
      if (sender)
        sendnoticetouser(qabot_nick, sender, "That public channel is already in use by %s.", bot->nick);
      return 0;
    }
    
    if (!ircd_strcmp(bot->question_chan->name->content, qu_chan)) {
      if (sender)
        sendnoticetouser(qabot_nick, sender, "That question channel is already in use by %s.", bot->nick);
      return 0;
    }
    
    if (!ircd_strcmp(bot->staff_chan->name->content, stff_chan)) {
      if (sender)
        sendnoticetouser(qabot_nick, sender, "That staff channel is already in use by %s.", bot->nick);
      return 0;
    }
  }
  
  bot = qabot_getbot();
  strncpy(bot->nick, nickname, NICKLEN);
  bot->nick[NICKLEN] = '\0';
  strncpy(bot->user, user, USERLEN);
  bot->user[USERLEN] = '\0';
  bot->host = strdup(host);
  if (!ircd_strcmp(nickname, "Tutor"))
    bot->np = registerlocaluser(nickname, user, host, TUTOR_NAME, TUTOR_ACCOUNT, QABOT_CHILD_UMODE|UMODE_ACCOUNT, &qabot_child_handler);
  else
    bot->np = registerlocaluser(nickname, user, host, QABOT_NAME, NULL, QABOT_CHILD_UMODE, &qabot_child_handler);
  
  bot->flags = flags;
  bot->spam_interval = spam_interval;
  bot->ask_wait = ask_wait;
  bot->queued_question_interval = queued_question_interval;
  
  bot->mic_timeout = QABOT_MICTIMEOUT;
  
  bot->lastmic = 0;
  
  if ((cp = findchannel(pub_chan))) {
    localjoinchannel(bot->np, cp);
    localgetops(bot->np, cp);
    bot->public_chan = cp->index;
  }
  else {
    localcreatechannel(bot->np, pub_chan);
    cp = findchannel(pub_chan);
    bot->public_chan = cp->index;
  }
  
  if ((cp = findchannel(qu_chan))) {
    localjoinchannel(bot->np, cp);
    bot->question_chan = cp->index;
  }
  else {
    localcreatechannel(bot->np, qu_chan);
    cp = findchannel(qu_chan);
    bot->question_chan = cp->index;
  }
  
  if (!IsModerated(cp)) {
    irc_send("%s M %s +m\r\n", mynumeric->content, cp->index->name->content);
    SetModerated(cp);
  }
  
  if ((cp = findchannel(stff_chan))) {
    localjoinchannel(bot->np, cp);
    localgetops(bot->np, cp);
    bot->staff_chan = cp->index;
  }
  else {
    localcreatechannel(bot->np, stff_chan);
    cp = findchannel(stff_chan);
    bot->staff_chan = cp->index;
  }
  
  bot->next = qab_bots;
  if (qab_bots)
    qab_bots->prev = bot;
  qab_bots = bot;
  
  bot->np->exts[qabot_nickext] = (void*)bot;
  
  scheduleoneshot(time(NULL) + 10, &qabot_timer, (void*)bot);
  
  return 1;
}

void qabot_delbot(qab_bot* bot) {
  qab_block* b;
  qab_question* q;
  qab_spam* s;
  qab_spam* ns;
  qab_answer* a;
  qab_answer* na;
  int i;
  
  deleteschedule(0, qabot_spam, (void*)bot);
  deleteschedule(0, qabot_spamstored, (void*)bot);
  deleteschedule(0, qabot_createbotfakeuser, (void*)bot);
  deleteschedule(0, qabot_timer, (void*)bot);
  
  if (bot->recfile) {
    fclose(bot->recfile);
    bot->recfile = NULL;
  }
  
  if (bot->playfile) {
    fclose(bot->playfile);
    bot->playfile = NULL;
  }
  
  while (bot->blocks) {
    b = bot->blocks;
    bot->blocks = bot->blocks->next;
    if (b->blockstr)
      free(b->blockstr);
    free(b);
  }
  
  for (i = 0; i < QUESTIONHASHSIZE; i++) {
    while (bot->questions[i]) {
      q = bot->questions[i];
      bot->questions[i] = bot->questions[i]->next;
      if (q->question)
        free(q->question);
      if (q->answer)
        free(q->answer);
      free(q);
    }
  }
  
  for (s = bot->nextspam; s; s = ns) {
    ns = s->next;
    free(s->message);
    free(s);
  }
  
  for (a = bot->answers; a; a = na) {
    na = a->next;
    free(a);
  }
  
  if (bot->host)
    free(bot->host);
  deregisterlocaluser(bot->np, "Bot deleted.");
  
  if (bot->next)
    bot->next->prev = bot->prev;
  if (bot->prev)
    bot->prev->next = bot->next;
  else
    qab_bots = bot->next;
  
  free(bot);
}

void qabot_playback(qab_bot *bot) {
  char buf[1024];
  unsigned long lines;
  
  if (!(bot->playfile)) {
    return;
  }
  
  lines = 0;
  
  while (!feof(bot->playfile)) {
    fscanf(bot->playfile, "%[^\n]\n", buf);
    
    if (!ircd_strcmp("--PAUSE--", buf)) {
      if (bot->staff_chan && bot->staff_chan->channel) { 
        sendmessagetochannel(bot->np, bot->staff_chan->channel, "Pause found in playback, use !continue when ready to continue playback.");
        sendmessagetochannel(bot->np, bot->staff_chan->channel, "%lu lines ready to send to channel. This will take approx. %sto send.", lines, longtoduration((lines * SPAMINTERVAL), 1));
      }
      
      return;
    }
    
    lines++;
    
    /* Copied from microphone code */
    if (bot->lastspam) {
      qab_spam* s;
      
      s = (qab_spam*)malloc(sizeof(qab_spam));
      s->message = strdup(buf);
      s->next = 0;
      bot->lastspam->next = s;
      bot->lastspam = s;
    }
    else {
      if ((bot->spamtime + bot->spam_interval) < time(0)) {
        sendmessagetochannel(bot->np, bot->public_chan->channel, "%s", buf);
        bot->spammed++;
        bot->spamtime = time(0);
      }
      else {
        qab_spam* s;
        
        s = (qab_spam*)malloc(sizeof(qab_spam));
        s->message = strdup(buf);
        s->next = 0;
        bot->nextspam = bot->lastspam = s;
        scheduleoneshot(bot->spamtime + bot->spam_interval, qabot_spam, (void*)bot);
      }
    }
  }
  
  fclose(bot->playfile);
  bot->playfile = NULL;
  
  if (bot->staff_chan && bot->staff_chan->channel) { 
    sendmessagetochannel(bot->np, bot->staff_chan->channel, "End of playback.");
    sendmessagetochannel(bot->np, bot->staff_chan->channel, "%lu lines ready to send to channel. This will take approx. %sto send.", lines, longtoduration((lines * SPAMINTERVAL), 1));
  }
}

void qabot_spam(void* arg) {
  qab_bot* bot = (qab_bot*)arg;
  qab_spam* s;
  
  if (!bot->nextspam)
    return;
  
  if (bot->np) {
    sendmessagetochannel(bot->np, bot->public_chan->channel, "%s", bot->nextspam->message);
    bot->spammed++;
  }
  
  s = bot->nextspam;
  bot->nextspam = s->next;
  
  free(s->message);
  free(s);
  
  bot->spamtime = time(0);
  
  if (bot->nextspam)
    scheduleoneshot(bot->spamtime + bot->spam_interval, qabot_spam, (void*)bot);
  else {
    bot->lastspam = 0;
    if (!bot->micnumeric)
      qabot_spamstored((void*)bot);
  }
}

void qabot_spamstored(void* arg) {
  qab_bot* bot = (qab_bot*)arg;
  qab_answer* a;
  
  if (!bot->answers)
    return;
  
  if (bot->np) {
    sendmessagetochannel(bot->np, bot->public_chan->channel, "%s asked: %s", bot->answers->question->nick, 
      bot->answers->question->question);
    sendmessagetochannel(bot->np, bot->public_chan->channel, "%s answered: %s", bot->answers->nick, 
    bot->answers->question->answer);
  }
  
  a = bot->answers;
  bot->answers = a->next;
  
  free(a);
  
  if (bot->answers)
    scheduleoneshot(time(0) + bot->queued_question_interval, qabot_spamstored, (void*)bot);
}

void qabot_createbotfakeuser(void* arg) {
  qab_bot* bot = (qab_bot*)arg;
  
  if (bot->np)
    return;
  
  bot->np = registerlocaluser(bot->nick, bot->user, bot->host, QABOT_NAME, NULL, QABOT_CHILD_UMODE, &qabot_child_handler);
  
  bot->np->exts[qabot_nickext] = (void*)bot;
  
  if (bot->public_chan->channel) {
    localjoinchannel(bot->np, bot->public_chan->channel);
    localgetops(bot->np, bot->public_chan->channel);
  }
  else
    localcreatechannel(bot->np, bot->public_chan->name->content);
  
  if (bot->question_chan->channel) {
    localjoinchannel(bot->np, bot->question_chan->channel);
    localgetops(bot->np, bot->question_chan->channel);
  }
  else
    localcreatechannel(bot->np, bot->question_chan->name->content);
  
  if (bot->staff_chan->channel) {
    localjoinchannel(bot->np, bot->staff_chan->channel);
    localgetops(bot->np, bot->staff_chan->channel);
  }
  else
    localcreatechannel(bot->np, bot->staff_chan->name->content);
}

void qabot_timer(void* arg) {
  qab_bot* bot = (qab_bot*)arg;
  
  if (!bot)
    return;
  
  if (bot->micnumeric && (bot->mic_timeout > 0) && (time(NULL) >= (bot->lastmic + bot->mic_timeout))) {
    bot->micnumeric = 0;
    
    sendmessagetochannel(bot->np, bot->staff_chan->channel, "Mic deactivated (idle timeout).");
    if (!bot->lastspam)
      qabot_spamstored((void*)bot);
  }
  
  scheduleoneshot(time(NULL) + 10, &qabot_timer, (void*)bot);
}
