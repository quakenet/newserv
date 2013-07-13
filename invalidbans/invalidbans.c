#include <string.h>
#include "../core/schedule.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"
#include "../lib/version.h"

MODULE_VERSION("");

static int ib_nickext;

typedef struct ibnick {
  time_t timeout;
  void *sched;
} ibnick;

static void ib_clear_ext(void *arg) {
  nick *np = arg;

  if (!np->exts[ib_nickext])
    return;

  free(np->exts[ib_nickext]);
  np->exts[ib_nickext] = NULL;
}

static void ib_hook_newnick(int hooknum, void *arg) {
  nick *np = arg;
  np->exts[ib_nickext] = NULL;
}

static void ib_hook_lostnick(int hooknum, void *arg) {
  nick *np = arg;
  ibnick *inp = np->exts[ib_nickext];

  if (!inp)
    return;

  deleteschedule(inp->sched, ib_clear_ext, np);
  free(np->exts[ib_nickext]);
}

static void ib_hook_modechange(int hooknum, void *arg) {
  void **arglist=(void **)arg;
  channel *cp=(channel *)arglist[0];
  nick *np=(nick *)arglist[1];
  long changeflags=(long)arglist[2];
  chanban *cbp;
  const char *p;
  int colons, colonsnext;
  modechanges changes;
  ibnick *inp;
  char *mask, *pos;
  int slot, i;
  array bans;

  if (!(changeflags & MODECHANGE_BANS))
    return;

  inp = np->exts[ib_nickext];

  /* Ignore the mode change if the same user has recently
   * set/unset a channel ban. */
  if (inp && inp->timeout > time(NULL))
    return;

  if (inp) {
    deleteschedule(inp->sched, ib_clear_ext, np);
    free(inp);
    np->exts[ib_nickext] = NULL;
  }

  array_init(&bans, 512);

  for (cbp = cp->bans; cbp; cbp = cbp->next) {
    if (!cbp->host)
      continue;

    colons = 0;
    colonsnext = 0;

    for (p = cbp->host->content; *p; p++) {
      if (*p == ':') {
        colons++;

        if (*(p + 1) == ':')
          colonsnext = 1;
      }
    }

    if (colons - colonsnext >= 8) {
      slot = array_getfreeslot(&bans);
      mask = ((char *)bans.content) + slot * 512;
      strncpy(mask, bantostring(cbp), 512);
    }
  }

  if (bans.cursi > 0) {
    localsetmodeinit(&changes, cp, NULL);

    for (i = 0; i < bans.cursi; i++) {
      mask = ((char *)bans.content) + i * 512;

      pos = strchr(mask, '@');

      if (!pos)
        continue; /* This should never happen. */

      pos++; /* Skip over the @ sign. */

      for (; *pos; pos++) {
        if (*pos != ':') {
          *pos = '?';
          break;
        }
      }

      localdosetmode_ban(&changes, mask, MCB_ADD);
    }

    localsetmodeflush(&changes, 1);

    /* Ignore the user for a short amount of time. If it's a bot
     * it'll probably try re-setting the ban immediately. */
    inp = malloc(sizeof(ibnick));
    inp->timeout = time(NULL) + 15;
    inp->sched = scheduleoneshot(inp->timeout + 1, ib_clear_ext, np);
    np->exts[ib_nickext] = inp;
  }

  array_free(&bans);
}

void _init() {
  registerhook(HOOK_NICK_NEWNICK, &ib_hook_newnick);
  registerhook(HOOK_NICK_LOSTNICK, &ib_hook_lostnick);
  registerhook(HOOK_CHANNEL_MODECHANGE, &ib_hook_modechange);

  ib_nickext = registernickext("invalidbans");
}

void _fini () {
  deregisterhook(HOOK_NICK_NEWNICK, &ib_hook_newnick);
  deregisterhook(HOOK_NICK_LOSTNICK, &ib_hook_lostnick);
  deregisterhook(HOOK_CHANNEL_MODECHANGE, &ib_hook_modechange);

  releasenickext(ib_nickext);
}

