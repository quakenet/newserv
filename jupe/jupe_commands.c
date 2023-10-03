#include "../control/control.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "jupe.h"

int ju_addjupe(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int result, duration;

  if (cargc < 3) {
    return CMD_USAGE;
  }

  if (jupe_find(cargv[0]) != NULL) {
    controlreply(np, "There is already a jupe for that server.");
    return CMD_OK;
  }

  duration = durationtolong(cargv[1]);

  if (duration > JUPE_MAX_EXPIRE) {
    controlreply(np, "A jupe's maximum duration is %s. Could not create jupe.", longtoduration(JUPE_MAX_EXPIRE, 0));
    return CMD_OK;
  }

  result = jupe_add(cargv[0], cargv[2], duration, JUPE_ACTIVE);

  if (result) {
    controlwall(NO_OPER, NL_MISC, "%s added JUPE for '%s' expiring in %s with reason %s", controlid(np), cargv[0], longtoduration(duration, 0), cargv[2]);
    controlreply(np, "Done.");
  } else
    controlreply(np, "Jupe could not be created.");

  return CMD_OK;
}

int ju_activatejupe(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  jupe_t *jupe;

  if (cargc < 1) {
    return CMD_USAGE;
  }

  jupe = jupe_find(cargv[0]);

  if (jupe == NULL) {
    controlreply(np, "There is no such jupe.");
    return CMD_OK;
  }

  if (jupe->ju_flags & JUPE_ACTIVE) {
    controlreply(np, "This jupe is already activated.");

    return CMD_OK;
  }

  jupe_activate(jupe);

  controlwall(NO_OPER, NL_MISC, "%s reactivated JUPE for '%s'", controlid(np), cargv[0]);
  controlreply(np, "Done.");
  return CMD_OK;
}

int ju_deactivatejupe(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  jupe_t *jupe;

  if (cargc < 1) {
    return CMD_USAGE;
  }

  jupe = jupe_find(cargv[0]);

  if (jupe == NULL) {
    controlreply(np, "There is no such jupe.");
    return CMD_OK;
  }

  if ((jupe->ju_flags & JUPE_ACTIVE) == 0) {
    controlreply(np, "This jupe is already deactivated.");
    return CMD_OK;
  }

  jupe_deactivate(jupe);

  controlwall(NO_OPER, NL_MISC, "%s deactivated JUPE for '%s'", controlid(np), cargv[0]);
  controlreply(np, "Done.");
  return CMD_OK;
}

int ju_jupelist(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;

  controlreply(np, "Server Reason Expires Status");

  for (jupe_t *jupe = jupe_next(NULL); jupe; jupe = jupe_next(jupe)) {
    controlreply(np, "%s %s %s %s", JupeServer(jupe), JupeReason(jupe), longtoduration(jupe->ju_expire - getnettime(), 0), (jupe->ju_flags & JUPE_ACTIVE) ? "activated" : "deactivated");
  }

  controlreply(np, "--- End of JUPE list.");
  return CMD_OK;
}

void _init(void) {
  registercontrolhelpcmd("addjupe", NO_OPER, 3, ju_addjupe, "Usage: addjupe <servername> <duration> <reason>");
  registercontrolhelpcmd("activatejupe", NO_OPER, 1, ju_activatejupe, "Usage: activatejupe <servername>");
  registercontrolhelpcmd("deactivatejupe", NO_OPER, 1, ju_deactivatejupe, "Usage: deactivatejupe <servername>");
  registercontrolhelpcmd("jupelist", NO_OPER, 0, ju_jupelist, "Usage: jupelist");
}

void _fini(void) {
  deregistercontrolcmd("addjupe", ju_addjupe);
  deregistercontrolcmd("activatejupe", ju_activatejupe);
  deregistercontrolcmd("deactivatejupe", ju_deactivatejupe);
  deregistercontrolcmd("jupelist", ju_jupelist);
}
