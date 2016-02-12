/* shroud's splitlist */

#include <string.h>
#include "../irc/irc.h"
#include "splitlist.h"
#include "../core/hooks.h"
#include "../lib/version.h"

MODULE_VERSION("");

array splitlist;

static void sphook_newserver(int hook, void *arg);
static void sphook_lostserver(int hook, void *arg);

void _init(void) {
  registerhook(HOOK_SERVER_NEWSERVER, &sphook_newserver);
  registerhook(HOOK_SERVER_LOSTSERVER, &sphook_lostserver);

  array_init(&splitlist, sizeof(splitserver));
  array_setlim1(&splitlist, 5);
  array_setlim2(&splitlist, 5);

  sp_addsplit("default.split.quakenet.org", getnettime(), SERVERTYPEFLAG_CRITICAL_SERVICE | SERVERTYPEFLAG_SERVICE);
}

void _fini(void) {
  int i;

  deregisterhook(HOOK_SERVER_NEWSERVER, &sphook_newserver);
  deregisterhook(HOOK_SERVER_LOSTSERVER, &sphook_lostserver);

  for (i = 0; i < splitlist.cursi; i++) {
    freesstring(((splitserver*)splitlist.content)[i].name);
  }

  array_free(&splitlist);
}

static void sphook_newserver(int hook, void *arg) {
  sp_deletesplit(serverlist[(long)arg].name->content);
}

static void sphook_lostserver(int hook, void *arg) {
  server *server = &serverlist[(long)arg];
  sp_addsplit(server->name->content, getnettime(), getservertype(server));
}

int sp_countsplitservers(flag_t orflags) {
  int result = 0;
  int i;

  for (i = 0; i < splitlist.cursi; i++)
    if((((splitserver*)splitlist.content)[i].type | orflags) != 0)
      result++;

  return result;
}

void sp_deletesplit(const char *name) {
  int i;

  if (splitlist.cursi == 0)
    return;

  for (i = splitlist.cursi - 1; i >= 0; i--) {
    if (strcmp(name, ((splitserver*)splitlist.content)[i].name->content) == 0) {
      freesstring(((splitserver*)splitlist.content)[i].name);
      array_delslot(&splitlist, i);
    }
  }
}

void sp_addsplit(const char *name, time_t ts, flag_t type) {
  int slot;
  splitserver *srv;

  slot = array_getfreeslot(&splitlist);

  srv = &(((splitserver*)splitlist.content)[slot]);

  srv->name = getsstring(name, HOSTLEN);
  srv->ts = ts;
  srv->type = type;
}
