/* TODO:
 * - fix STOP error when we can't find control's nick
 * - deal with Q/fishbot/O/T clones
 * - deal with our own localusers
 */

#include <stdio.h>
#include "../lib/version.h"
#include "../lib/ccassert.h"
#include "../lib/strlfunc.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../nick/nick.h"
#include "../control/control.h"
#include "signontracker.h"

#define MAXWHOISLINES 50 /* from the ircd */

MODULE_VERSION("");

static void hook_newnick(int hook, void *arg);
static void hook_rename(int hook, void *arg);
static void queue_scan_nick(nick *np);
static void flush_queue_sched(void *arg);
static int whois_reply_handler(void *source, int cargc, char **cargv);

static int nickext = -1;

#define QUEUEBUFLEN (MAXWHOISLINES*(NICKLEN+1)+1)

static struct {
  char buf[QUEUEBUFLEN];
  int len;
  void *sched;
} queue[MAXSERVERS];

void _init(void) {
  nick *np;
  int i;

  CCASSERT(sizeof(time_t) <= sizeof(long));

  nickext = registernickext("signontracker");
  if(nickext < 0)
    return;

  registerhook(HOOK_NICK_NEWNICK, &hook_newnick);
  registerhook(HOOK_NICK_RENAME, &hook_rename);
  registernumerichandler(317, whois_reply_handler, 7);

  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      queue_scan_nick(np);
}

void _fini(void) {
  int i;

  if(nickext < 0)
    return;

  deregisterhook(HOOK_NICK_NEWNICK, &hook_newnick);
  deregisterhook(HOOK_NICK_RENAME, &hook_rename);
  deregisternumerichandler(317, whois_reply_handler);

  for(i=0;i<MAXSERVERS;i++)
    deleteschedule(queue[i].sched, flush_queue_sched, (void *)(long)i);

  releasenickext(nickext);
}

static int whois_reply_handler(void *source, int cargc, char **cargv) {
  time_t connected;
  nick *np;

  if(cargc < 5)
    return CMD_OK;

  np = getnickbynick(cargv[3]);
  if(!np)
    return CMD_OK;

  connected = strtoul(cargv[5], NULL, 10);
  np->exts[nickext] = (void *)connected;

  triggerhook(HOOK_SIGNONTRACKER_HAVETIME, np);

  return CMD_OK;
}

static void flush_queue(int server) {
  char control_numeric[6];

  if(!queue[server].len)
    return;

  if(!mynick) {
    /* TODO: fix */
    Error("signontracker", ERR_STOP, "Unable to get control nick.");
    return;
  }
  longtonumeric2(mynick->numeric, 5, control_numeric);

  irc_send("%s W %s %s", control_numeric, longtonumeric(server, 2), queue[server].buf);
  queue[server].len = 0;
  queue[server].buf[0] = '\0';
}

static void queue_scan_nick(nick *np) {
  int server;

  if(NickOnServiceServer(np)) /* TODO: fix */
    return;

  server = homeserver(np->numeric);

  if(queue[server].len)
    strlcat(queue[server].buf, ",", QUEUEBUFLEN);

  queue[server].len++;

  strlcat(queue[server].buf, np->nick, QUEUEBUFLEN);

  if(queue[server].len == MAXWHOISLINES) {
    flush_queue(server);
  } else if(queue[server].sched == NULL) {
    queue[server].sched = scheduleoneshot(time(NULL), flush_queue_sched, (void *)(long)server);
  }
}

static void flush_queue_sched(void *arg) {
  int server = (long)arg;

  flush_queue(server);
  queue[server].sched = NULL;
}

static void hook_newnick(int hook, void *arg) {
  nick *np = (nick *)arg;

  queue_scan_nick(np);
}

static void hook_rename(int hook, void *arg) {
  nick *np = (nick *)arg;

  if(getnicksignon(np))
    return;

  queue_scan_nick(np);
}

time_t getnicksignon(nick *np) {
  if(nickext < 0)
    return 0;

  return (time_t)np->exts[nickext];
}
