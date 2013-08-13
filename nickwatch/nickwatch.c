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
  long numeric;
} nickwatchevent;

static nickwatch *nickwatches;
static int nextnickwatch = 1;

static void nw_dummyreply(nick *np, char *format, ...) { }

static void nw_dummywall(int level, char *format, ...) { }

static nickwatch *nw_currentwatch;
static nickwatchevent *nw_currentevent;

static void nw_printnick(searchCtx *ctx, nick *sender, nick *np) {
  char hostbuf[HOSTLEN+NICKLEN+USERLEN+4];

  nw_currentwatch->hits++;

  controlwall(NO_OPER, NL_HITS, "nickwatch(#%d, %s): %s [%s] (%s) (%s)", nw_currentwatch->id, nw_currentevent->description, visiblehostmask(np,hostbuf),
               IPtostr(np->ipaddress), printflags(np->umodes, umodeflags), np->realname->name->content);
}

static nickwatchevent *nwe_new(nick *np, const char *format, ...) {
  nickwatchevent *nwe;
  va_list va;

  nwe = malloc(sizeof(nickwatchevent));
  nwe->numeric = np->numeric;

  va_start(va, format);
  vsnprintf(nwe->description, sizeof(nwe->description), format, va);
  va_end(va);

  return nwe;
}

static void nwe_free(nickwatchevent *nwe) {
  free(nwe);
}

static void nw_sched_processevent(void *arg) {
  nickwatchevent *nwe = arg;
  nick *np;
  nickwatch *nw;

  np = getnickbynumeric(nwe->numeric);

  if (!np) {
    nwe_free(nwe);
    return;
  }
  nw_currentevent = nwe;

  for (nw = nickwatches; nw; nw = nw->next) {
    nw_currentwatch = nw;
    ast_nicksearch(nw->tree->root, &nw_dummyreply, mynick, &nw_dummywall, &nw_printnick, NULL, NULL, 10, np);
  }

  nwe_free(nwe);
}

static void nw_hook_newnick(int hooknum, void *arg) {
  nick *np = arg;
  nickwatchevent *nwe = nwe_new(np, "new user");
  scheduleoneshot(0, nw_sched_processevent, nwe);
}

static void nw_hook_rename(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *oldnick = args[1];
  nickwatchevent *nwe = nwe_new(np, "renamed from %s", oldnick);
  scheduleoneshot(0, nw_sched_processevent, nwe);
}

static void nw_hook_umodechange(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  flag_t oldmodes = (uintptr_t)args[1];
  char buf[64];
  strncpy(buf, printflags(np->umodes, umodeflags), sizeof(buf));
  nickwatchevent *nwe = nwe_new(np, "umodes %s -> %s", printflags(oldmodes, umodeflags), buf);
  scheduleoneshot(0, nw_sched_processevent, nwe);
}

static void nw_hook_message(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  int isnotice = (uintptr_t)args[2];
  nickwatchevent *nwe = nwe_new(np, isnotice ? "notice" : "message");
  scheduleoneshot(0, nw_sched_processevent, nwe);
}

static void nw_hook_joinchannel(int hooknum, void *arg) {
  void **args = arg;
  channel *cp = args[0];
  nick *np = args[1];
  nickwatchevent *nwe = nwe_new(np, "join channel %s", cp->index->name->content);
  scheduleoneshot(0, nw_sched_processevent, nwe);
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
  registercontrolhelpcmd("nickwatch", NO_OPER, 1, &nw_cmd_nickwatch, "Usage: nickwatch <nicksearch term>\nAdds a nickwatch entry.");
  registercontrolhelpcmd("nickunwatch", NO_OPER, 1, &nw_cmd_nickunwatch, "Usage: nickunwatch <#id>\nRemoves a nickwatch entry.");
  registercontrolhelpcmd("nickwatches", NO_OPER, 0, &nw_cmd_nickwatches, "Usage: nickwatches\nLists nickwatches.");

  registerhook(HOOK_NICK_NEWNICK, &nw_hook_newnick);
  registerhook(HOOK_NICK_RENAME, &nw_hook_rename);
  registerhook(HOOK_NICK_MODECHANGE, &nw_hook_umodechange);
  registerhook(HOOK_NICK_MESSAGE, &nw_hook_message);
  registerhook(HOOK_CHANNEL_CREATE, &nw_hook_joinchannel);
  registerhook(HOOK_CHANNEL_JOIN, &nw_hook_joinchannel);
}

void _fini(void) {
  nickwatch *nw, *next;

  deregistercontrolcmd("nickwatch", &nw_cmd_nickwatch);
  deregistercontrolcmd("nickunwatch", &nw_cmd_nickunwatch);
  deregistercontrolcmd("nickwatches", &nw_cmd_nickwatches);

  deregisterhook(HOOK_NICK_NEWNICK, &nw_hook_newnick);
  deregisterhook(HOOK_NICK_RENAME, &nw_hook_rename);
  deregisterhook(HOOK_NICK_MODECHANGE, &nw_hook_umodechange);
  deregisterhook(HOOK_NICK_MESSAGE, &nw_hook_message);
  deregisterhook(HOOK_CHANNEL_CREATE, &nw_hook_joinchannel);
  deregisterhook(HOOK_CHANNEL_JOIN, &nw_hook_joinchannel);

  for (nw = nickwatches; nw; nw = next) {
    next = nw->next;

    parse_free(nw->tree);
    free(nw);
  }
}
