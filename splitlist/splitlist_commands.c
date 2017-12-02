/* oper commands for the splitlist */

#include <errno.h>
#include <string.h>

#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../irc/irc_config.h"
#include "../splitlist/splitlist.h"
#include "../serverlist/serverlist.h"
#include "../control/control.h"
#include "../lib/version.h"
#include "../lib/flags.h"

MODULE_VERSION("");

int spcmd_splitlist(void *source, int cargc, char **cargv);
int spcmd_splitdel(void *source, int cargc, char **cargv);
int spcmd_splitadd(void *source, int cargc, char **cargv);

void _init(void) {
  registercontrolhelpcmd("splitlist", NO_STAFF, 0, &spcmd_splitlist, "Usage: splitlist\nLists servers currently split from the network.");
  registercontrolcmd("splitdel", 10, 1, &spcmd_splitdel);
  registercontrolhelpcmd("splitadd", 10, 3, &spcmd_splitadd,
      "Usage: splitadd <servername> [+flags] [split time as unix timestamp]\n"
      " Adds a server as split from the network.\n"
      " Flags:\n"
      "  +c: Client server\n"
      "  +h: Hub server\n"
      "  +s: Service\n"
      "  +Q: Q/CServe\n"
      "  +S: S/spamscan\n"
      "  +X: Other critical service\n"
      " If no flags are given, an attempt to figure them out based on name\n"
      " will be made, but it's likely not a good one.");
}

void _fini(void) {
  deregistercontrolcmd("splitlist", &spcmd_splitlist);
  deregistercontrolcmd("splitdel", &spcmd_splitdel);
  deregistercontrolcmd("splitadd", &spcmd_splitadd);
}

/* todo: add RELINK status */
int spcmd_splitlist(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int i;
  splitserver srv;

  if (splitlist.cursi == 0) {
    controlreply(np, "There currently aren't any registered splits.");

    return CMD_OK;
  }

  controlreply(np, "Server Status Split for");

  for (i = 0; i < splitlist.cursi; i++) {
    srv = ((splitserver*)splitlist.content)[i];

    controlreply(np, "%s M.I.A. %s (%s)", srv.name->content, longtoduration(getnettime() - srv.ts, 1), printflags(srv.type, servertypeflags));
  }

  controlreply(np, "--- End of splitlist");
  
  return CMD_OK;
}

int spcmd_splitdel(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int i, count;
  splitserver srv;

  if (cargc < 1) {
    controlreply(np, "Syntax: splitdel <pattern>");

    return CMD_ERROR;
  }

  count = 0;

  for (i = splitlist.cursi - 1; i >= 0; i--) {
    srv = ((splitserver*)splitlist.content)[i];

    if (match2strings(cargv[0], srv.name->content)) {
      sp_deletesplit(srv.name->content); /* inefficient .. but it doesn't matter */
      count++;
    }
  }

  if (count > 0)
    controlwall(NO_OPERED, NL_MISC, "%s/%s used SPLITDEL on %s", np->nick, np->authname, cargv[0]);
  controlreply(np, "%d %s deleted.", count, count != 1 ? "splits" : "split");

  return CMD_OK;
}

static int doessplitalreadyexist(const char *servername) {
  splitserver srv;
  unsigned int i;

  for (i = 0; i < splitlist.cursi; i++) {
    srv = ((splitserver*)splitlist.content)[i];

    if (!strcasecmp(srv.name->content, servername))
      return 1;
  }

  return 0;
}

int spcmd_splitadd(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  unsigned long long num;
  char *end;
  flag_t servertype = 0;
  char *servername;
  size_t servernamelen;
  time_t splittime;
  server fake;

  if (cargc < 1) {
      controlreply(np, "Usage: splitadd <servername> [+flags] [split time as unix timestamp]");
      return CMD_ERROR;
  }

  servername = cargv[0];
  servernamelen = strlen(servername);

  if (findserver(servername) != -1) {
    controlreply(np, "Server %s is linked right now, refusing to add split.",
        servername);
    return CMD_ERROR;
  }

  if (doessplitalreadyexist(servername)) {
    controlreply(np, "There is a split for %s already.", servername);
    return CMD_ERROR;
  }

  if (servernamelen > SERVERLEN) {
    controlreply(np, "Server name %s is too long (max: %d characters)",
        servername, SERVERLEN);
    return CMD_ERROR;
  }

  /* Handle flags */
  if (cargc > 1) {
    if (setflags(&servertype, (flag_t)-1, cargv[1], servertypeflags,
          REJECT_UNKNOWN) != REJECT_NONE) {
      controlreply(np, "Flag string %s contained invalid flags.", cargv[1]);
      return CMD_ERROR;
    }
  } else {
    /* Set up a fake server for getservertype. */
    memset(&fake, 0, sizeof(fake));

    fake.name = getsstring(servername, servernamelen);
    servertype = getservertype(&fake);
    freesstring(fake.name);
  }

  /* Handle timestamp */
  if (cargc < 3) {
    splittime = getnettime();
  } else {
    errno = 0;
    num = strtoull(cargv[2], &end, 10);
    if (errno == ERANGE) {
      controlreply(np, "%s is out of range for a timestamp.", cargv[2]);
      return CMD_ERROR;
    }

    /* Truncation may happen here. 
     * However, there's no way to get the max time_t value, so we'll just try to
     * find out after the fact.
     */
    splittime = (time_t)num;

    if ((unsigned long long)splittime < num) {
      controlreply(np, "Tried to use %llu as split time value, but it's too "
          "large for the system to handle", num);
      return CMD_ERROR;
    }
  }

  sp_addsplit(servername, splittime, servertype);
  controlreply(np, "Added split for %s (%s ago) with flags %s.",
      servername, longtoduration(getnettime() - splittime, 1),
      printflags(servertype, servertypeflags));

  return CMD_OK;
}
