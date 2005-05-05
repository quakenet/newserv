#include <stdio.h>
#include <string.h>
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

qab_user* qabot_users = 0;

void qabot_loaddb() {
  FILE* f;
  char buf[2049];
  char* args[100];
  time_t created;
  flag_t flags;
  int spam_interval;
  int ask_wait;
  int queued_question_interval;
  qab_bot* bot;
  qab_block* b;
  char blocktype;
  
  if (!(f = fopen("qab_users", "r")))
    return;
  
  while (!feof(f)) {
    if (fgets(buf, 2048, f)) {
      buf[2048] = '\0';
      if (splitline(buf, args, 50, 0) != 3) {
        fclose(f);
        return;
      }
      
      flags = (flag_t)strtoul(args[1], NULL, 10);
      created = strtoul(args[2], NULL, 10);
      
      qabot_adduser(args[0], flags, created);
    }
  }
  
  fclose(f);
  
  if (!(f = fopen("qab_bots", "r")))
    return;
  
  while (!feof(f)) {
    if (fgets(buf, 2048, f)) {
      buf[2048] = '\0';
      if (splitline(buf, args, 50, 0) != 10) {
        fclose(f);
        return;
      }
      
      qabot_addbot(args[0], args[1], args[2], args[3], args[4], args[5], (flag_t)strtol(args[6], NULL, 10), 
        (int)strtol(args[7], NULL, 10), (int)strtol(args[8], NULL, 10), (int)strtol(args[9], NULL, 10), 0);
    }
  }
  
  fclose(f);
  
  if (!(f = fopen("qab_blocks", "r")))
    return;
  
  while (!feof(f)) {
    if (fgets(buf, 2048, f)) {
      buf[2048] = '\0';
      if (splitline(buf, args, 50, 0) != 5) {
        fclose(f);
        return;
      }
      
      /* args[0] = botname
         args[1] = type
         args[2] = creator
         args[3] = created
         args[4] = blockstr */
      if (!strcmp(args[0], "q")) {
        /* account block */
        if (strchr(args[4], '*') || args[4](target, '?'))
          continue; /* wildcard account blocks are not supported */
        
        blocktype = QABBLOCK_ACCOUNT;
      }
      else if (!strcmp(args[0], "t")) {
        /* text block */
        blocktype = QABBLOCK_TEXT;
      }
      else if (!strcmp(args[0], "h")) {
        /* hostmask block */
        blocktype = QABBLOCK_HOST;
      }
      else
        continue; /* unknown block, lets skip and hope everything works out */
      
      if (!(bot = qabot_findbot(args[0])))
        continue; /* no such bot added */
      
      b = (qab_block*)malloc(sizeof(qab_block));
      b->type = blocktype;
      b->created = (long)strol(args[3]);
      strncpy(b->creator, args[2], ACCOUNTLEN);
      b->creator[ACCOUNTLEN] = '\0';
      b->blockstr = strdup(args[4]);
      b->prev = 0;
      b->next = bot->blocks;
      if (bot->blocks)
        bot->blocks->prev = b;
      bot->blocks = b;
      bot->block_count++;
    }
  }
  
  fclose(f);
}

void qabot_savedb() {
  FILE* f;
  qab_user* u;
  qab_bot* b;
  qab_block* block;
  
  if (!(f = fopen("qab_users", "w")))
    return;
  
  for (u = qabot_users; u; u = u->next)
    fprintf(f, "%s %d %ld\n", u->authname, (int)u->flags, u->created);
  
  fclose(f);
  
  if (!(f = fopen("qab_bots", "w")))
    return;
  
  for (b = qab_bots; b; b = b->next)
    fprintf(f, "%s %s %s %s %s %s %d %d %d %d\n", b->nick, b->user, b->host, b->public_chan->name->content, 
      b->question_chan->name->content, b->staff_chan->name->content, (int)b->flags, b->spam_interval, 
      b->ask_wait, b->queued_question_interval);
  
  fclose(f);
  
  if (!(f = fopen("qab_blocks", "w")))
    return;
  
  for (b = qab_bots; b; b = b->next)
    for (block = b->blocks; block; block = block->next)
      fprintf(f, "%s %c %s %ld %s\n", b->nick, block->type == QABBLOCK_ACCOUNT ? 'q' : block->type == QABBLOCK_HOST ? 'h' : 't', 
        block->creator, block->created, block->blockstr);
  
  fclose(f);
}

void qabot_savetimer(void* arg) {
  qabot_savedb();
}

void qabot_adduser(const char* authname, flag_t flags, time_t created) {
  qab_user* u;

  if ((u = qabot_getuser(authname)))
    return;

  u = (qab_user*)malloc(sizeof(qab_user));
  strncpy(u->authname, authname, ACCOUNTLEN);
  u->authname[ACCOUNTLEN] = '\0';
  u->flags = flags;
  u->created = created;

  u->prev = 0;
  u->next = qabot_users;
  if (qabot_users)
    qabot_users->prev = u;
  qabot_users = u;
}

void qabot_deluser(const char* authname) {
  qab_user* u = qabot_getuser(authname);

  if (!u)
    return;

  qabot_squelchuser(u);
}

void qabot_squelchuser(qab_user* user) {
  if (user->next)
    user->next->prev = user->prev;
  if (user->prev)
    user->prev->next = user->next;
  else
    qabot_users = user->next;

  free(user);
}

qab_user* qabot_getuser(const char* authname) {
  qab_user* u;

  for (u = qabot_users; u; u = u->next)
    if (!ircd_strcmp(u->authname, authname))
      return u;

  return 0;
}

qab_bot* qabot_findbot(const char* nickname) {
  qab_bot* bot;
  
  for (bot = qab_bots; bot; bot = bot->next)
    if (!ircd_strcmp(bot->nick, nickname))
      return bot;
  
  return 0;
}
