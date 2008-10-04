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

qab_user* qabot_users = 0;

void qabot_loaddb() {
  FILE* f;
  char buf[2049];
  char* args[100];
  time_t created;
  flag_t flags;
  
  if (!(f = fopen("data/qab_users", "r")))
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
  
  if (!(f = fopen("data/qab_bots", "r")))
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
}

void qabot_savedb() {
  FILE* f;
  qab_user* u;
  qab_bot* b;
  
  if (!(f = fopen("data/qab_users", "w")))
    return;
  
  for (u = qabot_users; u; u = u->next)
    fprintf(f, "%s %d %ld\n", u->authname, (int)u->flags, u->created);
  
  fclose(f);
  
  if (!(f = fopen("data/qab_bots", "w")))
    return;
  
  for (b = qab_bots; b; b = b->next)
    fprintf(f, "%s %s %s %s %s %s %d %d %d %d\n", b->nick, b->user, b->host, b->public_chan->name->content, 
      b->question_chan->name->content, b->staff_chan->name->content, (int)b->flags, b->spam_interval, 
      b->ask_wait, b->queued_question_interval);
  
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
