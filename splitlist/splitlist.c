/* shroud's splitlist */

#include <string.h>
#include "../irc/irc.h"
#include "splitlist.h"
#include "../core/hooks.h"

MODULE_VERSION("");

array splitlist;

void sphook_newserver(int hook, void *arg);
void sphook_lostserver(int hook, void *arg);

void _init(void) {
  registerhook(HOOK_SERVER_NEWSERVER, &sphook_newserver);
  registerhook(HOOK_SERVER_LOSTSERVER, &sphook_lostserver);

  array_init(&splitlist, sizeof(splitserver));
  array_setlim1(&splitlist, 5);
  array_setlim2(&splitlist, 5);

  sp_addsplit("default.split.quakenet.org", getnettime());
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

void sphook_newserver(int hook, void *arg) {
  sp_deletesplit(serverlist[(int)arg].name->content);
}

void sphook_lostserver(int hook, void *arg) {
  sp_addsplit(serverlist[(int)arg].name->content, getnettime());
}

int sp_countsplitservers(void) {
  return splitlist.cursi;
}

int sp_issplitserver(const char *name) {
  int i;

  for (i = 0; i < splitlist.cursi; i++) {
    if (strcmp(name, ((splitserver*)splitlist.content)[i].name->content) == 0)
      return 1;
  }

  return 0;
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

void sp_addsplit(const char *name, time_t ts) {
  int slot;
  splitserver *srv;

  slot = array_getfreeslot(&splitlist);

  srv = &(((splitserver*)splitlist.content)[slot]);

  srv->name = getsstring(name, HOSTLEN);
  srv->ts = ts;
}
