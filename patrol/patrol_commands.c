#include "../core/schedule.h"
#include "../control/control.h"
#include "../localuser/localuserchannel.h"
#include "../lib/version.h"
#include "../lib/irc_string.h"
#include "patrol.h"

MODULE_VERSION("");

typedef struct patrolchannel {
  sstring *channel;
  nick *nick;

  struct patrolchannel *next;
} patrolchannel;

static patrolchannel *patrolchannels;

static void patroluserhandler(nick *np, int event, void **args) {
  /* Nothing to do here. */
}

static void pc_check(void) {
  patrolchannel *pc;
  channel *cp;

  for (pc = patrolchannels; pc; pc = pc->next) {
    if (pc->nick && pc->nick->timestamp > getnettime() - 900)
      continue;

    if (pc->nick)
      deregisterlocaluser(pc->nick, NULL);

    cp = findchannel(pc->channel->content);

    if (!cp) {
      pc->nick = NULL;
      continue;
    }

    pc->nick = patrol_generateclone(0, patroluserhandler);
    localjoinchannel(pc->nick, cp);
  }
}

static void pc_sched_check(void *arg) {
  pc_check();
}

static int pc_join(char *name) {
  patrolchannel *pc = NULL;
  channel *cp;

  for (pc = patrolchannels; pc; pc = pc->next)
    if (ircd_strcmp(pc->channel->content, name) == 0)
      return 0;

  cp = findchannel(name);

  if (!cp)
    return -1;

  pc = malloc(sizeof(patrolchannel));

  pc->channel = getsstring(name, 512);
  pc->nick = NULL;

  pc->next = patrolchannels;
  patrolchannels = pc;

  pc_check();

  return 0;
}

static int pc_part(char *name) {
  patrolchannel **pnext, *pc;

  for (pnext = &patrolchannels; *pnext; pnext = &((*pnext)->next)) {
    pc = *pnext;

    if (ircd_strcmp(pc->channel->content, name) == 0) {
      freesstring(pc->channel);
      deregisterlocaluser(pc->nick, NULL);

      *pnext = pc->next;
      free(pc);

      return 0;
    }
  }

  return -1;
}

static int pc_cmd_patroljoin(void *source, int cargc, char **cargv) {
  nick *sender = source;

  if (cargc < 1)
    return CMD_USAGE;

  if (pc_join(cargv[0]) < 0) {
    controlreply(sender, "Could not join channel.");

    return CMD_ERROR;
  }

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int pc_cmd_patrolpart(void *source, int cargc, char **cargv) {
  nick *sender = source;

  if (cargc < 1)
    return CMD_USAGE;

  if (pc_part(cargv[0]) < 0) {
    controlreply(sender, "Could not join channel.");

    return CMD_ERROR;
  }

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int pc_cmd_patrollist(void *source, int cargc, char **cargv) {
  nick *sender = source;
  patrolchannel *pc;

  for (pc = patrolchannels; pc; pc = pc->next)
    controlreply(sender, "%s - %s", pc->channel->content, pc->nick ? controlid(pc->nick) : "Not currently joined.");

  controlreply(sender, "End of list.");

  return CMD_OK;
}

void _init(void) {
  registercontrolhelpcmd("patroljoin", NO_OPER, 1, &pc_cmd_patroljoin, "Usage: patroljoin <channel>\nJoins a patrol client to a channel.");
  registercontrolhelpcmd("patrolpart", NO_OPER, 1, &pc_cmd_patrolpart, "Usage: patrolpart <#id>\nRemoves a patrol client from a channel.");
  registercontrolhelpcmd("patrollist", NO_OPER, 0, &pc_cmd_patrollist, "Usage: patrollist\nLists patrol channels.");

  schedulerecurring(time(NULL) + 5, 0, 10, &pc_sched_check, NULL);
}

void _fini(void) {
  patrolchannel *pc, *next;

  deregistercontrolcmd("patroljoin", &pc_cmd_patroljoin);
  deregistercontrolcmd("patrolpart", &pc_cmd_patrolpart);
  deregistercontrolcmd("patrollist", &pc_cmd_patrollist);

  deleteallschedules(&pc_sched_check);

  for (pc = patrolchannels; pc; pc = next) {
    next = pc->next;

    freesstring(pc->channel);

    if (pc->nick)
      deregisterlocaluser(pc->nick, NULL);

    free(pc);
  }
}

