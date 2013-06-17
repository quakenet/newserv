#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../core/hooks.h"
#include "../core/config.h"
#include "../core/error.h"
#include "../lib/sha1.h"
#include "../lib/hmac.h"
#include "../xsb/xsb.h"
#include "../control/control.h"
#include "trusts.h"

static void broadcast(SHA1_CTX *c, unsigned int replicationid, unsigned int lineno, char *command, char *format, ...) {
  char buf[512], buf2[600];
  va_list va;
  int len;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  len = snprintf(buf2, sizeof(buf2), "%u %u %s", replicationid, lineno, buf);
  xsb_broadcast(command, NULL, "%s", buf2);

  if(len > (sizeof(buf2) - 1))
    len = sizeof(buf2) - 1;
  if(len < 0)
    len = 0;

  buf2[len] = '\n';

  SHA1Update(c, (unsigned char *)buf2, len + 1);
}

static void replicate(int forced) {
  SHA1_CTX s;
  unsigned int lineno, lines;
  unsigned char digest[SHA1_DIGESTSIZE];
  char digestbuf[SHA1_DIGESTSIZE*2+1];
  static unsigned int replicationid = 0;
  trustgroup *tg;
  trusthost *th;

  if(++replicationid > 10000)
    replicationid = 1;

  for(lines=2,tg=tglist;tg;tg=tg->next) {
    lines++;
    for(th=tg->hosts;th;th=th->next)
      lines++;
  }

  SHA1Init(&s);
  lineno = 1;
  broadcast(&s, replicationid, lineno++, "trinit", "%d %u", forced, lines);

  for(tg=tglist;tg;tg=tg->next) {
    broadcast(&s, replicationid, lineno++, "trdata", "G %s", dumptg(tg, 0));

    for(th=tg->hosts;th;th=th->next)
      broadcast(&s, replicationid, lineno++, "trdata", "H %s", dumpth(th, 0));
  }

  SHA1Final(digest, &s);
  xsb_broadcast("trfini", NULL, "%u %u %s", replicationid, lineno, hmac_printhex(digest, digestbuf, SHA1_DIGESTSIZE));
}

static int xsb_tr_requeststart(void *source, int argc, char **argv) {
  static time_t last = 0;
  time_t t = time(NULL);

  if(t - last > 5) {
    last = t;

    replicate(0);
  }

  return CMD_OK;
}

static void groupadded(int hooknum, void *arg) {
  trustgroup *tg = arg;

  xsb_broadcast("traddgroup", NULL, "%s", dumptg(tg, 0));
}

static void groupremoved(int hooknum, void *arg) {
  trustgroup *tg = arg;

  xsb_broadcast("trdelgroup", NULL, "%u", tg->id);
}

static void hostadded(int hooknum, void *arg) {
  trusthost *th = arg;

  xsb_broadcast("traddhost", NULL, "%s", dumpth(th, 0));
}

static void hostremoved(int hooknum, void *arg) {
  trusthost *th = arg;

  xsb_broadcast("trdelhost", NULL, "%u", th->id);
}

static void groupmodified(int hooknum, void *arg) {
  trustgroup *tg = arg;

  xsb_broadcast("trmodifygroup", NULL, "%s", dumptg(tg, 0));
}

static void hostmodified(int hooknum, void *arg) {
  trusthost *th = arg;

  xsb_broadcast("trmodifyhost", NULL, "%s", dumpth(th, 0));
}

static int trusts_cmdtrustforceresync(void *source, int argc, char **argv) {
  nick *np = source;

  controlreply(np, "Resync in progress. . .");
  replicate(1);
  controlreply(np, "Resync complete.");

  return CMD_OK;
}

static int commandsregistered, loaded;
static void __dbloaded(int hooknum, void *arg) {
  if(commandsregistered)
    return;

  commandsregistered = 1;

  xsb_addcommand("trrequeststart", 0, xsb_tr_requeststart);

  registerhook(HOOK_TRUSTS_ADDGROUP, groupadded);
  registerhook(HOOK_TRUSTS_DELGROUP, groupremoved);
  registerhook(HOOK_TRUSTS_ADDHOST, hostadded);
  registerhook(HOOK_TRUSTS_DELHOST, hostremoved);
  registerhook(HOOK_TRUSTS_MODIFYGROUP, groupmodified);
  registerhook(HOOK_TRUSTS_MODIFYHOST, hostmodified);

  /* we've just reloaded */
  /* if we're not online, no problem, other nodes will ask us individually */
  if(trusts_fullyonline())
    replicate(1);

  registercontrolhelpcmd("trustforceresync", NO_DEVELOPER, 0, trusts_cmdtrustforceresync, "Usage: trustforceresync");
}

static void __dbclosed(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  xsb_delcommand("trrequeststart", xsb_tr_requeststart);

  registerhook(HOOK_TRUSTS_ADDGROUP, groupadded);
  registerhook(HOOK_TRUSTS_DELGROUP, groupremoved);
  registerhook(HOOK_TRUSTS_ADDHOST, hostadded);
  registerhook(HOOK_TRUSTS_DELGROUP, hostremoved);
  registerhook(HOOK_TRUSTS_MODIFYGROUP, groupmodified);
  registerhook(HOOK_TRUSTS_MODIFYHOST, hostmodified);

  deregistercontrolcmd("trustforceresync", trusts_cmdtrustforceresync);
}

void _init(void) {
  sstring *m;

  m = getconfigitem("trusts", "master");
  if(!m || (atoi(m->content) != 1)) {
    Error("trusts_master", ERR_ERROR, "Not a master server, not loaded.");
    return;
  }

  loaded = 1;

  registerhook(HOOK_TRUSTS_DB_LOADED, __dbloaded);
  registerhook(HOOK_TRUSTS_DB_CLOSED, __dbclosed);

  if(trustsdbloaded) {
    /* this will force a sync if we're online */
    /* if we're not we'll be asked to sync when we come online */
    __dbloaded(0, NULL);
  } else {
    if(!trusts_loaddb())
      Error("trusts_master", ERR_ERROR, "Unable to load database.");

    /* do nothing else, other servers will ask us when we come online */
    /* also the __dbloaded hook will be triggered */
  }
}

void _fini(void) {
  if(!loaded)
    return;

  deregisterhook(HOOK_TRUSTS_DB_LOADED, __dbloaded);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, __dbclosed);

  trusts_closedb(0);

  __dbclosed(0, NULL);
}
