#include <stdio.h>
#include <string.h>
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../core/config.h"
#include "../lib/stringbuf.h"
#include "trusts.h"

static void registercommands(int, void *);
static void deregistercommands(int, void *);

typedef int (*trustmodificationfn)(trustgroup *, char *arg);

struct trustmodification {
  char name[50];
  trustmodificationfn fn;
};

static int trusts_cmdtrustadd(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *host;
  uint32_t ip, mask;
  trusthost *th, *superset, *subset;

  if(cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  host = cargv[1];
  if(!trusts_str2cidr(host, &ip, &mask)) {
    controlreply(sender, "Invalid host.");
    return CMD_ERROR;
  }

  /* OKAY! Lots of checking here!
   *
   * Need to check:
   *   - host isn't already covered by given group (reject if it is)
   *   - host doesn't already exist exactly already (reject if it does)
   *   - host is more specific than an existing one (warn if it is, fix up later)
   *   - host is less specific than an existing one (warn if it is, don't need to do anything special)
   */

  for(th=tg->hosts;th;th=th->next) {
    if(th->ip == (ip & th->mask)) {
      controlreply(sender, "This host (or part of it) is already covered in the given group.");
      return CMD_ERROR;
    }
  }

  if(th_getbyhostandmask(ip, mask)) {
    controlreply(sender, "This host already exists in another group with the same mask.");
    return CMD_ERROR;
  }

  /* this function will set both to NULL if it's equal, hence the check above */
  th_getsuperandsubsets(ip, mask, &superset, &subset);
  if(superset) {
    /* a superset exists for us, we will be more specific than one existing host */

    controlreply(sender, "Warning: this host already exists in another group, but this new host will override it as it has a smaller prefix.");
  }
  if(subset) {
    /* a subset of us exists, we will be less specific than some existing hosts */

    controlreply(sender, "Warning: this host already exists in at least one other group, the new host has a larger prefix and therefore will not override those hosts.");
  }
  if(superset || subset)
    controlreply(sender, "Adding anyway...");

  th = th_new(tg, host);
  if(!th) {
    controlreply(sender, "An error occured adding the host to the group.");
    return CMD_ERROR;
  }

  controlreply(sender, "Host added.");
  triggerhook(HOOK_TRUSTS_ADDHOST, th);

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTADD'ed host %s to group '%s'", controlid(sender), host, th->group->name->content);

  return CMD_OK;
}

static int trusts_cmdtrustgroupadd(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *name, *contact, *comment, createdby[ACCOUNTLEN + 2];
  unsigned int howmany, maxperident, enforceident;
  time_t howlong;
  trustgroup *tg, itg;

  if(cargc < 6)
    return CMD_USAGE;

  name = cargv[0];
  howmany = strtoul(cargv[1], NULL, 10);
  if(!howmany || (howmany > MAXTRUSTEDFOR)) {
    controlreply(sender, "Bad value maximum number of clients.");
    return CMD_ERROR;
  }

  howlong = durationtolong(cargv[2]);
  if((howlong <= 0) || (howlong > MAXDURATION)) {
    controlreply(sender, "Invalid duration supplied.");
    return CMD_ERROR;
  }

  maxperident = strtoul(cargv[3], NULL, 10);
  if(!howmany || (maxperident > MAXPERIDENT)) {
    controlreply(sender, "Bad value for max per ident.");
    return CMD_ERROR;
  }

  if(cargv[4][0] != '1' && cargv[4][0] != '0') {
    controlreply(sender, "Bad value for enforce ident (use 0 or 1).");
    return CMD_ERROR;
  }
  enforceident = cargv[4][0] == '1';

  contact = cargv[5];

  if(cargc < 7) {
    comment = "(no comment)";
  } else {
    comment = cargv[6];
  }

  /* don't allow #id or id forms */
  if((name[0] == '#') || strtoul(name, NULL, 10)) {
    controlreply(sender, "Invalid trustgroup name.");
    return CMD_ERROR;
  }

  tg = tg_strtotg(name);
  if(tg) {
    controlreply(sender, "A group with that name already exists");
    return CMD_ERROR;
  }

  snprintf(createdby, sizeof(createdby), "#%s", sender->authname);

  itg.trustedfor = howmany;
  itg.mode = enforceident;
  itg.maxperident = maxperident;
  itg.expires = howlong + time(NULL);
  itg.createdby = getsstring(createdby, CREATEDBYLEN);
  itg.contact = getsstring(contact, CONTACTLEN);
  itg.comment = getsstring(comment, COMMENTLEN);
  itg.name = getsstring(name, TRUSTNAMELEN);

  if(itg.createdby && itg.contact && itg.comment && itg.name) {
    tg = tg_new(&itg);
  } else {
    tg = NULL;
  }

  freesstring(itg.createdby);
  freesstring(itg.comment);
  freesstring(itg.name);
  freesstring(itg.contact);

  if(!tg) {
    controlreply(sender, "An error occured adding the trustgroup.");
    return CMD_ERROR;
  }

  controlreply(sender, "Group added.");
  triggerhook(HOOK_TRUSTS_ADDGROUP, tg);

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTGROUPADD'ed '%s'", controlid(sender), tg->name->content);

  return CMD_OK;
}

static int trusts_cmdtrustgroupdel(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;

  if(cargc < 1)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  if(tg->hosts) {
    controlreply(sender, "Delete all hosts before deleting the group.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTGROUPDEL'ed '%s'.", controlid(sender), tg->name->content);

  triggerhook(HOOK_TRUSTS_DELGROUP, tg);
  tg_delete(tg);
  controlreply(sender, "Group deleted.");

  return CMD_OK;
}

static int trusts_cmdtrustdel(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  trusthost *th;
  uint32_t ip, mask;
  nick *sender = source;
  char *host;

  if(cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  host = cargv[1];
  if(!trusts_str2cidr(host, &ip, &mask)) {
    controlreply(sender, "Invalid IP/mask.");
    return CMD_ERROR;
  }

  for(th=tg->hosts;th;th=th->next)
    if((th->ip == ip) && (th->mask == mask))
      break;

  if(!th) {
    controlreply(sender, "Couldn't find that host in that group.");
    return CMD_ERROR;
  }

  triggerhook(HOOK_TRUSTS_DELHOST, th);
  th_delete(th);
  controlreply(sender, "Host deleted.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTDEL'ed %s from group '%s'.", controlid(sender), host, tg->name->content);

  return CMD_OK;
}

static int modifycomment(trustgroup *tg, char *comment) {
  sstring *n = getsstring(comment, COMMENTLEN);
  if(!n)
    return 0;

  freesstring(tg->comment);
  tg->comment = n;

  return 1;
}

static int modifycontact(trustgroup *tg, char *contact) {
  sstring *n = getsstring(contact, CONTACTLEN);
  if(!n)
    return 0;

  freesstring(tg->contact);
  tg->contact = n;

  return 1;
}

static int modifytrustedfor(trustgroup *tg, char *num) {
  unsigned int trustedfor = strtoul(num, NULL, 10);

  if(trustedfor > MAXTRUSTEDFOR)
    return 0;

  tg->trustedfor = trustedfor;

  return 1;
}

static int modifymaxperident(trustgroup *tg, char *num) {
  unsigned int maxperident = strtoul(num, NULL, 10);

  if(maxperident > MAXPERIDENT)
    return 0;

  tg->maxperident = maxperident;

  return 1;
}

static int modifyenforceident(trustgroup *tg, char *num) {
  if(num[0] == '1') {
    tg->mode = 1;
  } else if(num[0] == '0') {
    tg->mode = 0;
  } else {
    return 0;
  }

  return 1;
}

static int modifyexpires(trustgroup *tg, char *expires) {
  int howlong = durationtolong(expires);

  if((howlong <= 0) || (howlong > MAXDURATION))
    return 0;

  tg->expires = time(NULL) + howlong;

  return 1;
}

static array trustmods_a;
static struct trustmodification *trustmods;

static int trusts_cmdtrustgroupmodify(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *what, *to, validfields[512];
  int i;
  StringBuf b;

  if(cargc < 3)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  what = cargv[1];
  to = cargv[2];

  sbinit(&b, validfields, sizeof(validfields));
  for(i=0;i<trustmods_a.cursi;i++) {
    if(!strcmp(what, trustmods[i].name)) {
      if(!(trustmods[i].fn)(tg, to)) {
        controlreply(sender, "An error occured changing that property, check the syntax.");
        return CMD_ERROR;
      }
      break;
    }

    if(i > 0)
      sbaddstr(&b, ", ");
    sbaddstr(&b, trustmods[i].name);
  }

  if(i == trustmods_a.cursi) {
    sbterminate(&b);
    controlreply(sender, "No such field, valid fields are: %s", validfields);
    return CMD_ERROR;
  }

  triggerhook(HOOK_TRUSTS_MODIFYGROUP, tg);
  tg_update(tg);
  controlreply(sender, "Group modified.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTMODIFIED'ed group '%s' (field: %s, value: %s)", controlid(sender), tg->name->content, what, to);

  return CMD_OK;
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustgroupadd", NO_OPER, 6, trusts_cmdtrustgroupadd, "Usage: trustgroupadd <name> <howmany> <howlong> <maxperident> <enforceident> <contact> ?comment?");
  registercontrolhelpcmd("trustadd", NO_OPER, 2, trusts_cmdtrustadd, "Usage: trustadd <#id|name|id> <host>");
  registercontrolhelpcmd("trustgroupdel", NO_OPER, 1, trusts_cmdtrustgroupdel, "Usage: trustgroupdel <#id|name|id>");
  registercontrolhelpcmd("trustdel", NO_OPER, 2, trusts_cmdtrustdel, "Usage: trustdel <#id|name|id> <ip/mask>");
  registercontrolhelpcmd("trustgroupmodify", NO_OPER, 3, trusts_cmdtrustgroupmodify, "Usage: trustgroupmodify <#id|name|id> <field> <new value>");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustgroupadd", trusts_cmdtrustgroupadd);
  deregistercontrolcmd("trustadd", trusts_cmdtrustadd);
  deregistercontrolcmd("trustgroupdel", trusts_cmdtrustgroupdel);
  deregistercontrolcmd("trustdel", trusts_cmdtrustdel);
  deregistercontrolcmd("trustgroupmodify", trusts_cmdtrustgroupmodify);
}

static int loaded;

#define _ms_(x) (struct trustmodification){ .name = # x, .fn = modify ## x }
#define MS(x) { int slot = array_getfreeslot(&trustmods_a); trustmods = (struct trustmodification *)trustmods_a.content; memcpy(&trustmods[slot], &_ms_(x), sizeof(struct trustmodification)); }

static void setupmods(void) {
  MS(expires);
  MS(enforceident);
  MS(maxperident);
  MS(contact);
  MS(comment);
  MS(trustedfor);
}

void _init(void) {
  sstring *m;

  array_init(&trustmods_a, sizeof(struct trustmodification));
  setupmods();

  m = getconfigitem("trusts", "master");
  if(!m || (atoi(m->content) != 1)) {
    Error("trusts_management", ERR_ERROR, "Not a master server, not loaded.");
    return;
  }

  loaded = 1;

  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if(trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  array_free(&trustmods_a);

  if(!loaded)
    return;

  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  deregistercommands(0, NULL);
}
