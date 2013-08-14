#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../core/schedule.h"
#include "../control/control.h"
#include "../newsearch/newsearch.h"
#include "../newsearch/parser.h"

typedef struct nickwatch {
  int id;

  char createdby[64];
  int hits;
  char term[512];
  parsertree *tree;

  struct nickwatch *next;
} nickwatch;

typedef struct nickwatchevent {
  char description[128];
  struct nickwatchevent *next;
} nickwatchevent;

static nickwatch *nickwatches;
static int nextnickwatch = 1;
static int nickwatchext;

static void nw_dummyreply(nick *np, char *format, ...) { }
static void nw_dummywall(int level, char *format, ...) { }

static nickwatch *nw_currentwatch;
static array nw_pendingnicks;

static void nw_printnick(searchCtx *ctx, nick *sender, nick *np) {
  char hostbuf[HOSTLEN+NICKLEN+USERLEN+4];
  char events[512];
  nickwatchevent *nwe = np->exts[nickwatchext];
  int len;

  nw_currentwatch->hits++;

  events[0] = '\0';
  len = 0;

  for (nwe = np->exts[nickwatchext]; nwe; nwe = nwe->next) {
    if (len > 0)
      len += snprintf(events + len, sizeof(events) - len, ", ");

    len += snprintf(events + len, sizeof(events) - len, "%s", nwe->description);
  }

  controlwall(NO_OPER, NL_HITS, "nickwatch(#%d, %s): %s [%s] (%s) (%s)", nw_currentwatch->id, events, visiblehostmask(np,hostbuf),
               IPtostr(np->ipaddress), printflags(np->umodes, umodeflags), np->realname->name->content);
}

static void nwe_enqueue(nick *np, const char *format, ...) {
  nickwatchevent *nwe;
  va_list va;
  int slot;

  nwe = malloc(sizeof(nickwatchevent));

  va_start(va, format);
  vsnprintf(nwe->description, sizeof(nwe->description), format, va);
  va_end(va);

  nwe->next = np->exts[nickwatchext];
  np->exts[nickwatchext] = nwe;

  slot = array_getfreeslot(&nw_pendingnicks);
  ((nick **)nw_pendingnicks.content)[slot] = np;
}

static void nwe_clear(nick *np) {
  nickwatchevent *nwe, *next;

  for (nwe = np->exts[nickwatchext]; nwe; nwe = next) {
    next = nwe->next;
    free(nwe);
  }

  np->exts[nickwatchext] = NULL;
}

static void nw_sched_processevents(void *arg) {
  nickwatch *nw;
  int i;
  nick *np;

  for (nw = nickwatches; nw; nw = nw->next) {
    nw_currentwatch = nw;
    ast_nicksearch(nw->tree->root, &nw_dummyreply, mynick, &nw_dummywall, &nw_printnick, NULL, NULL, 10, &nw_pendingnicks);
  }

  for (i = 0; i < nw_pendingnicks.cursi; i++) {
    np = ((nick **)nw_pendingnicks.content)[i];
    nwe_clear(np);
  }

  array_free(&nw_pendingnicks);
  array_init(&nw_pendingnicks, sizeof(nick *));
}

static void nw_hook_newnick(int hooknum, void *arg) {
  nick *np = arg;
  nwe_enqueue(np, "new user");
}

static void nw_hook_lostnick(int hooknum, void *arg) {
  nick *np = arg;
  int i;

  nwe_clear(np);

  for (i = 0; i < nw_pendingnicks.cursi;)
    if (((nick **)nw_pendingnicks.content)[i] == np)
      array_delslot(&nw_pendingnicks, i);
    else
      i++;
}

static void nw_hook_rename(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *oldnick = args[1];
  nwe_enqueue(np, "renamed from %s", oldnick);
}

static void nw_hook_umodechange(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  flag_t oldmodes = (uintptr_t)args[1];
  char buf[64];
  strncpy(buf, printflags(np->umodes, umodeflags), sizeof(buf));
  nwe_enqueue(np, "umodes %s -> %s", printflags(oldmodes, umodeflags), buf);
}

static void nw_hook_message(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  int isnotice = (uintptr_t)args[2];
  nwe_enqueue(np, isnotice ? "notice" : "message");
}

static void nw_hook_joinchannel(int hooknum, void *arg) {
  void **args = arg;
  channel *cp = args[0];
  nick *np = args[1];
  nwe_enqueue(np, "join channel %s", cp->index->name->content);
}

static int nw_cmd_nickwatch(void *source, int cargc, char **cargv) {
  nick *sender = source;
  nickwatch *nw;
  parsertree *tree;

  if (cargc < 1)
    return CMD_USAGE;

  tree = parse_string(reg_nicksearch, cargv[0]);
  if (!tree) {
    displaystrerror(controlreply, sender, cargv[0]);
    return CMD_ERROR;
  }

  nw = malloc(sizeof(nickwatch));
  nw->id = nextnickwatch++;
  snprintf(nw->createdby, sizeof(nw->createdby), "#%s", sender->authname);
  nw->hits = 0;
  strncpy(nw->term, cargv[0], sizeof(nw->term));
  nw->tree = parse_string(reg_nicksearch, cargv[0]);
  nw->next = nickwatches;
  nickwatches = nw;

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int nw_cmd_nickunwatch(void *source, int cargc, char **cargv) {
  nick *sender = source;
  nickwatch **pnext, *nw;
  int id;

  if (cargc < 1)
    return CMD_USAGE;

  id = atoi(cargv[0]);

  for (pnext = &nickwatches; *pnext; pnext = &((*pnext)->next)) {
    nw = *pnext;

    if (nw->id == id) {
      parse_free(nw->tree);
      *pnext = nw->next;
      free(nw);

      controlreply(sender, "Done.");
      return CMD_OK;
    }
  }

  controlreply(sender, "Nickwatch #%d not found.", id);

  return CMD_ERROR;
}

static int nw_cmd_nickwatches(void *source, int cargc, char **cargv) {
  nick *sender = source;
  nickwatch *nw;

  controlreply(sender, "ID    Created By      Hits    Term");

  for (nw = nickwatches; nw; nw = nw->next)
    controlreply(sender, "%-5d %-15s %-7d %s", nw->id, nw->createdby, nw->hits, nw->term);

  controlreply(sender, "--- End of nickwatches.");

  return CMD_OK;
}

void _init(void) {
  nickwatchext = registernickext("nickwatch");

  array_init(&nw_pendingnicks, sizeof(nick *));

  registercontrolhelpcmd("nickwatch", NO_OPER, 1, &nw_cmd_nickwatch, "Usage: nickwatch <nicksearch term>\nAdds a nickwatch entry.");
  registercontrolhelpcmd("nickunwatch", NO_OPER, 1, &nw_cmd_nickunwatch, "Usage: nickunwatch <#id>\nRemoves a nickwatch entry.");
  registercontrolhelpcmd("nickwatches", NO_OPER, 0, &nw_cmd_nickwatches, "Usage: nickwatches\nLists nickwatches.");

  registerhook(HOOK_NICK_NEWNICK, &nw_hook_newnick);
  registerhook(HOOK_NICK_LOSTNICK, &nw_hook_lostnick);
  registerhook(HOOK_NICK_RENAME, &nw_hook_rename);
  registerhook(HOOK_NICK_MODECHANGE, &nw_hook_umodechange);
  registerhook(HOOK_NICK_MESSAGE, &nw_hook_message);
  registerhook(HOOK_CHANNEL_CREATE, &nw_hook_joinchannel);
  registerhook(HOOK_CHANNEL_JOIN, &nw_hook_joinchannel);

  schedulerecurring(time(NULL) + 5, 0, 1, &nw_sched_processevents, NULL);
}

void _fini(void) {
  nickwatch *nw, *next;

  deregistercontrolcmd("nickwatch", &nw_cmd_nickwatch);
  deregistercontrolcmd("nickunwatch", &nw_cmd_nickunwatch);
  deregistercontrolcmd("nickwatches", &nw_cmd_nickwatches);

  deregisterhook(HOOK_NICK_NEWNICK, &nw_hook_newnick);
  deregisterhook(HOOK_NICK_LOSTNICK, &nw_hook_lostnick);
  deregisterhook(HOOK_NICK_RENAME, &nw_hook_rename);
  deregisterhook(HOOK_NICK_MODECHANGE, &nw_hook_umodechange);
  deregisterhook(HOOK_NICK_MESSAGE, &nw_hook_message);
  deregisterhook(HOOK_CHANNEL_CREATE, &nw_hook_joinchannel);
  deregisterhook(HOOK_CHANNEL_JOIN, &nw_hook_joinchannel);

  deleteallschedules(&nw_sched_processevents);

  /* Process all pending events */
  nw_sched_processevents(NULL);

  array_free(&nw_pendingnicks);

  releasenickext(nickwatchext);

  for (nw = nickwatches; nw; nw = next) {
    next = nw->next;

    parse_free(nw->tree);
    free(nw);
  }
}
