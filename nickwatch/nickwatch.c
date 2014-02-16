#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../core/schedule.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../control/control.h"
#include "../newsearch/newsearch.h"
#include "../newsearch/parser.h"

#define NW_FORMAT_TIME "%d/%m/%y %H:%M GMT"
#define NW_DURATION_MAX (60*60*24*7) // 7 days

typedef struct nickwatch {
  int id;

  char createdby[64];
  int hits;
  time_t lastactive;
  time_t expiry;
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

static int nw_nickunwatch(int id) {
  nickwatch **pnext, *nw;

  for (pnext = &nickwatches; *pnext; pnext = &((*pnext)->next)) {
    nw = *pnext;

    if (nw->id == id) {
      parse_free(nw->tree);
      *pnext = nw->next;
      free(nw);
      return 0;
    }
  }

  return 1;
}

static void nw_printnick(searchCtx *ctx, nick *sender, nick *np) {
  char hostbuf[HOSTLEN+NICKLEN+USERLEN+4], modebuf[34];
  char events[512];
  nickwatchevent *nwe = np->exts[nickwatchext];
  int len;

  nw_currentwatch->hits++;
  nw_currentwatch->lastactive = time(NULL);

  events[0] = '\0';
  len = 0;

  for (nwe = np->exts[nickwatchext]; nwe; nwe = nwe->next) {
    if (len > 0)
      len += snprintf(events + len, sizeof(events) - len, ", ");

    len += snprintf(events + len, sizeof(events) - len, "%s", nwe->description);
  }

  strncpy(modebuf, printflags(np->umodes, umodeflags), sizeof(modebuf));

  controlwall(NO_OPER, NL_HITS, "nickwatch(#%d, %s): %s [%s] (%s) (%s)", nw_currentwatch->id, events, visiblehostmask(np,hostbuf),
               IPtostr(np->ipaddress), modebuf, np->realname->name->content);
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
  nickwatch *nw, *next;
  int i, slot;
  unsigned int marker;
  nick *np;
  array nicks;
  time_t now = time(NULL);

  array_init(&nicks, sizeof(nick *));
  marker = nextnickmarker();

  for (i = 0; i < nw_pendingnicks.cursi; i++) {
    np = ((nick **)nw_pendingnicks.content)[i];

    if (!np)
      continue;

    if (np->marker != marker) {
      np->marker = marker;
      slot = array_getfreeslot(&nicks);
      ((nick **)nicks.content)[slot] = np;
    }
  }

  array_free(&nw_pendingnicks);
  array_init(&nw_pendingnicks, sizeof(nick *));

  for (nw = nickwatches; nw; nw = next) {
    nw_currentwatch = nw;
    next = nw->next;
    ast_nicksearch(nw->tree->root, &nw_dummyreply, mynick, &nw_dummywall, &nw_printnick, NULL, NULL, 10, &nicks);
    if (nw->expiry && nw->expiry <= now) {
      controlwall(NO_OPER, NL_HITS, "nickwatch(#%d) by %s expired (%d hits): %s", nw->id, nw->createdby, nw->hits, nw->term);
      nw_nickunwatch(nw->id);
    }
  }

  for (i = 0; i < nicks.cursi; i++) {
    np = ((nick **)nicks.content)[i];
    nwe_clear(np);
  }

  array_free(&nicks);
}

static void nw_hook_newnick(int hooknum, void *arg) {
  nick *np = arg;
  nwe_enqueue(np, "new user");
}

static void nw_hook_account(int hooknum, void *arg) {
  nick *np = arg;
  nwe_enqueue(np, "logged in with account %s", np->authname);
}

static void nw_hook_lostnick(int hooknum, void *arg) {
  nick *np = arg;
  int i;

  nwe_clear(np);

  for (i = 0; i < nw_pendingnicks.cursi; i++)
    if (((nick **)nw_pendingnicks.content)[i] == np)
      ((nick **)nw_pendingnicks.content)[i] = NULL;
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
  time_t duration = NW_DURATION_MAX;
  size_t i;

  for (i = 0; i < cargc && cargv[i][0] == '-'; i++) {
    switch (cargv[i][1]) {
      case 'd':
        if (++i == cargc)
          return CMD_USAGE;
        duration = durationtolong(cargv[i]);
        if (!duration || duration > NW_DURATION_MAX) {
          controlreply(sender, "Invalid duration. Maximum: %s.", longtoduration(NW_DURATION_MAX, 1));
          return CMD_ERROR;
        }
        break;
      default:
        return CMD_USAGE;
    }
  }

  if (i == cargc)
    return CMD_USAGE;

  if (i < (cargc - 1))
    rejoinline(cargv[i],cargc-i);

  tree = parse_string(reg_nicksearch, cargv[i]);
  if (!tree) {
    displaystrerror(controlreply, sender, cargv[i]);
    return CMD_ERROR;
  }

  nw = malloc(sizeof(nickwatch));
  nw->id = nextnickwatch++;
  snprintf(nw->createdby, sizeof(nw->createdby), "#%s", sender->authname);
  nw->hits = 0;
  nw->lastactive = 0;
  nw->expiry = duration + time(NULL);
  strncpy(nw->term, cargv[i], sizeof(nw->term));
  nw->tree = tree;
  nw->next = nickwatches;
  nickwatches = nw;

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int nw_cmd_nickunwatch(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int id;

  if (cargc < 1)
    return CMD_USAGE;

  id = atoi(cargv[0]);

  if (nw_nickunwatch(id)) {
    controlreply(sender, "Nickwatch #%d not found.", id);
    return CMD_ERROR;
  }

  controlreply(sender, "Done.");
  return CMD_OK;
}

static void nw_formattime(time_t time, char *buf, size_t bufsize) {
  if (time == 0)
    strncpy(buf, "(never)", bufsize);
  else
    strftime(buf, bufsize, NW_FORMAT_TIME, gmtime(&time));
}


static int nw_cmd_nickwatches(void *source, int cargc, char **cargv) {
  nick *sender = source;
  nickwatch *nw;
  char timebuf1[20], timebuf2[20];

  controlreply(sender, "ID    Created By      Hits    Expires            Last active        Term");

  for (nw = nickwatches; nw; nw = nw->next) {
    nw_formattime(nw->expiry, timebuf1, sizeof(timebuf1));
    nw_formattime(nw->lastactive, timebuf2, sizeof(timebuf2));
    controlreply(sender, "%-5d %-15s %-7d %-18s %-18s %s", nw->id, nw->createdby, nw->hits, timebuf1, timebuf2, nw->term);
  }

  controlreply(sender, "--- End of nickwatches.");

  return CMD_OK;
}

void _init(void) {
  nickwatchext = registernickext("nickwatch");

  array_init(&nw_pendingnicks, sizeof(nick *));

  registercontrolhelpcmd("nickwatch", NO_OPER, 3, &nw_cmd_nickwatch, "Usage: nickwatch ?-d <duration (e.g. 12h5m)>? <nicksearch term>\nAdds a nickwatch entry.");
  registercontrolhelpcmd("nickunwatch", NO_OPER, 1, &nw_cmd_nickunwatch, "Usage: nickunwatch <#id>\nRemoves a nickwatch entry.");
  registercontrolhelpcmd("nickwatches", NO_OPER, 0, &nw_cmd_nickwatches, "Usage: nickwatches\nLists nickwatches.");

  registerhook(HOOK_NICK_NEWNICK, &nw_hook_newnick);
  registerhook(HOOK_NICK_ACCOUNT, &nw_hook_account);
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
  deregisterhook(HOOK_NICK_ACCOUNT, &nw_hook_account);
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
