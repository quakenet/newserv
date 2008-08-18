#include <string.h>

#include "../nick/nick.h"
#include "../core/hooks.h"
#include "../lib/version.h"
#include "usercount.h"

MODULE_VERSION("")

int servercount[MAXSERVERS];

static void uc_newserver(int hook, void *arg);
static void uc_newnick(int hook, void *arg);
static void uc_lostnick(int hook, void *arg);

void _init(void) {
  nick *np;
  int i;

  memset(servercount, 0, sizeof(servercount));

  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      servercount[homeserver(np->numeric)]++;

  registerhook(HOOK_SERVER_NEWSERVER, uc_newserver);
  registerhook(HOOK_NICK_NEWNICK, uc_newnick);
  registerhook(HOOK_NICK_LOSTNICK, uc_lostnick);
}

void _fini(void) {
  deregisterhook(HOOK_SERVER_NEWSERVER, uc_newserver);
  deregisterhook(HOOK_NICK_NEWNICK, uc_newnick);
  deregisterhook(HOOK_NICK_LOSTNICK, uc_lostnick);
}

static void uc_newserver(int hook, void *arg) {
  long num = (long)arg;

  servercount[num] = 0;
}

static void uc_newnick(int hook, void *arg) {
  nick *np = (nick *)arg;

  if(np)
    servercount[homeserver(np->numeric)]++;
}

static void uc_lostnick(int hook, void *arg) {
  nick *np = (nick *)arg;

  if(np)
    servercount[homeserver(np->numeric)]--;
}
