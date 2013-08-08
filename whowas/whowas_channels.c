#include <string.h>
#include <assert.h>
#include "../lib/version.h"
#include "../nick/nick.h"
#include "../chanindex/chanindex.h"
#include "../channel/channel.h"
#include "../core/hooks.h"
#include "whowas.h"

MODULE_VERSION("");

static int wwcnext, wwccext;

static void wwc_refchannel(chanindex *cip) {
  uintptr_t *refcount = (uintptr_t *)&cip->exts[wwccext];
  (*refcount)++;
}

static void wwc_derefchannel(chanindex *cip) {
  uintptr_t *refcount = (uintptr_t *)&cip->exts[wwccext];
  (*refcount)--;

  assert(*refcount >= 0);

  if (*refcount == 0)
    releasechanindex(cip);
}

static void wwc_hook_joincreate(int hooknum, void *arg) {
  void **args = arg;
  channel *cp = args[0];
  nick *np = args[1];
  chanindex **wchans = np->exts[wwcnext];

  if (!wchans) {
    wchans = calloc(sizeof(chanindex *), WW_MAXCHANNELS);
    np->exts[wwcnext] = wchans;
  }

  wwc_refchannel(cp->index);

  memmove(&wchans[1], &wchans[0], sizeof(chanindex *) * (WW_MAXCHANNELS - 1));
  wchans[0] = cp->index;
}

static void wwc_hook_lostnick(int hooknum, void *arg) {
  nick *np = arg;
  chanindex **wchans = np->exts[wwcnext];
  int i;

  if (!wchans)
    return;

  for (i = 0; i < WW_MAXCHANNELS; i++) {
    if (!wchans[i])
      break;

    wwc_derefchannel(wchans[i]);
  }

  free(wchans);
}

static void wwc_hook_newrecord(int hooknum, void *arg) {
  void **args = arg;
  whowas *ww = args[0];
  nick *np = args[1];
  chanindex **wchans = np->exts[wwcnext];
  int i;

  memset(ww->channels, 0, sizeof(ww->channels));

  if (!wchans)
    return;

  for (i = 0; i < WW_MAXCHANNELS; i++) {
    if (!wchans[i])
      break;

    wwc_refchannel(wchans[i]);
    ww->channels[i] = wchans[i];
  }
}

static void wwc_hook_lostrecord(int hooknum, void *arg) {
  whowas *ww = arg;
  int i;

  for (i = 0; i < WW_MAXCHANNELS; i++) {
    if (!ww->channels[i])
      break;

    wwc_derefchannel(ww->channels[i]);
  }
}

void _init(void) {
  wwcnext = registernickext("whowas_channels");
  wwccext = registerchanext("whowas_channels");

  registerhook(HOOK_CHANNEL_JOIN, &wwc_hook_joincreate);
  registerhook(HOOK_CHANNEL_CREATE, &wwc_hook_joincreate);
  registerhook(HOOK_NICK_LOSTNICK, &wwc_hook_lostnick);
  registerhook(HOOK_WHOWAS_NEWRECORD, &wwc_hook_newrecord);
  registerhook(HOOK_WHOWAS_LOSTRECORD, &wwc_hook_lostrecord);
}

void _fini(void) {
  releasenickext(wwcnext);
  releasechanext(wwccext);

  deregisterhook(HOOK_CHANNEL_JOIN, &wwc_hook_joincreate);
  deregisterhook(HOOK_CHANNEL_CREATE, &wwc_hook_joincreate);
  deregisterhook(HOOK_NICK_LOSTNICK, &wwc_hook_lostnick);
  deregisterhook(HOOK_WHOWAS_NEWRECORD, &wwc_hook_newrecord);
  deregisterhook(HOOK_WHOWAS_LOSTRECORD, &wwc_hook_lostrecord);
}
