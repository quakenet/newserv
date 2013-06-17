#include <stdio.h>
#include <string.h>
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../core/config.h"
#include "../core/schedule.h"
#include "../lib/stringbuf.h"
#include "trusts.h"

static void registercommands(int, void *);
static void deregistercommands(int, void *);

typedef int (*trustmodificationfn)(void *, char *arg);

struct trustmodification {
  char name[50];
  trustmodificationfn fn;
};

static int trusts_cmdtrustadd(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *host;
  struct irc_in_addr ip;
  unsigned char bits;
  trusthost *th, *superset, *subset;

  if(cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  host = cargv[1];
  if(!ipmask_parse(host, &ip, &bits)) {
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
    if(ipmask_check(&ip, &th->ip, th->bits)) {
      controlreply(sender, "This host (or part of it) is already covered in the given group.");
      return CMD_ERROR;
    }
  }

  if(th_getbyhostandmask(&ip, bits)) {
    controlreply(sender, "This host already exists in another group with the same mask.");
    return CMD_ERROR;
  }

  /* this function will set both to NULL if it's equal, hence the check above */
  th_getsuperandsubsets(&ip, bits, &superset, &subset);
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
  trustlog(tg, sender->authname, "Added host '%s'.", host);

  return CMD_OK;
}

static int trusts_cmdtrustgroupadd(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *name, *contact, *comment, createdby[ACCOUNTLEN + 2];
  unsigned int howmany, maxperident, enforceident;
  time_t howlong, expires;
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
  if((howlong < 0) || (howlong > MAXDURATION)) {
    controlreply(sender, "Invalid duration supplied.");
    return CMD_ERROR;
  }

  if(howlong)
    expires = howlong + time(NULL);
  else
    expires = 0;

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
  itg.expires = expires;
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
  trustlog(tg, sender->authname, "Created trust group '%s' (ID #%d): howmany=%d, enforceident=%d, maxperident=%d, "
    "expires=%d, createdby=%s, contact=%s, comment=%s",
    tg->name->content, howmany, tg->id, enforceident, maxperident, expires, createdby, contact, comment);

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
  trustlog(tg, sender->authname, "Deleted group '%s'.", tg->name->content);

  triggerhook(HOOK_TRUSTS_DELGROUP, tg);
  tg_delete(tg);
  controlreply(sender, "Group deleted.");

  return CMD_OK;
}

static int trusts_cmdtrustdel(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  trusthost *th;
  struct irc_in_addr ip;
  unsigned char bits;
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
  if(!ipmask_parse(host, &ip, &bits)) {
    controlreply(sender, "Invalid IP/mask.");
    return CMD_ERROR;
  }

  for(th=tg->hosts;th;th=th->next)
    if(ipmask_check(&ip, &th->ip, th->bits) && th->bits == bits)
      break;

  if(!th) {
    controlreply(sender, "Couldn't find that host in that group.");
    return CMD_ERROR;
  }

  triggerhook(HOOK_TRUSTS_DELHOST, th);
  th_delete(th);
  controlreply(sender, "Host deleted.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTDEL'ed %s from group '%s'.", controlid(sender), host, tg->name->content);
  trustlog(tg, sender->authname, "Removed host '%s'.", host);

  return CMD_OK;
}

static int modifycomment(void *arg, char *comment) {
  trustgroup *tg = arg;
  sstring *n = getsstring(comment, COMMENTLEN);
  if(!n)
    return 0;

  freesstring(tg->comment);
  tg->comment = n;

  return 1;
}

static int modifycontact(void *arg, char *contact) {
  trustgroup *tg = arg;
  sstring *n = getsstring(contact, CONTACTLEN);
  if(!n)
    return 0;

  freesstring(tg->contact);
  tg->contact = n;

  return 1;
}

static int modifytrustedfor(void *arg, char *num) {
  trustgroup *tg = arg;
  unsigned int trustedfor = strtoul(num, NULL, 10);

  if(trustedfor > MAXTRUSTEDFOR)
    return 0;

  tg->trustedfor = trustedfor;

  return 1;
}

static int modifymaxperident(void *arg, char *num) {
  trustgroup *tg = arg;
  unsigned int maxperident = strtoul(num, NULL, 10);

  if(maxperident > MAXPERIDENT)
    return 0;

  tg->maxperident = maxperident;

  return 1;
}

static int modifyenforceident(void *arg, char *num) {
  trustgroup *tg = arg;

  if(num[0] == '1') {
    tg->mode = 1;
  } else if(num[0] == '0') {
    tg->mode = 0;
  } else {
    return 0;
  }

  return 1;
}

static int modifyexpires(void *arg, char *expires) {
  trustgroup *tg = arg;
  int howlong = durationtolong(expires);

  if((howlong < 0) || (howlong > MAXDURATION))
    return 0;

  if(howlong)
    tg->expires = time(NULL) + howlong;
  else
    tg->expires = 0; /* never */

  return 1;
}

static int modifymaxpernode(void *arg, char *num) {
  trusthost *th = arg;
  int maxpernode = strtol(num, NULL, 10);
  
  if((maxpernode < 0) || (maxpernode > MAXPERNODE))
    return 0;
  
  th->maxpernode = maxpernode;

  return 1;
}

static int modifynodebits(void *arg, char *num) {
  trusthost *th = arg;
  int nodebits = strtol(num, NULL, 10);

  if((nodebits < 0) || (nodebits > ((irc_in_addr_is_ipv4(&th->ip))?32:128)))
    return 0;

  if(irc_in_addr_is_ipv4(&th->ip))
    nodebits += 96;

  if(nodebits<th->bits)
    return 0;

  th->nodebits = nodebits;

  return 1;
}

static array trustgroupmods_a;
static struct trustmodification *trustgroupmods;
static array trusthostmods_a;
static struct trustmodification *trusthostmods;

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
  for(i=0;i<trustgroupmods_a.cursi;i++) {
    if(!strcmp(what, trustgroupmods[i].name)) {
      if(!(trustgroupmods[i].fn)(tg, to)) {
        controlreply(sender, "An error occured changing that property, check the syntax.");
        return CMD_ERROR;
      }
      break;
    }

    if(i > 0)
      sbaddstr(&b, ", ");
    sbaddstr(&b, trustgroupmods[i].name);
  }

  if(i == trustgroupmods_a.cursi) {
    sbterminate(&b);
    controlreply(sender, "No such field, valid fields are: %s", validfields);
    return CMD_ERROR;
  }

  triggerhook(HOOK_TRUSTS_MODIFYGROUP, tg);
  tg_update(tg);
  controlreply(sender, "Group modified.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTMODIFIED'ed group '%s' (field: %s, value: %s)", controlid(sender), tg->name->content, what, to);
  trustlog(tg, sender->authname, "Modified %s: %s", what, to);

  return CMD_OK;
}

static int trusts_cmdtrusthostmodify(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  trusthost *th;
  nick *sender = source;
  char *what, *to, validfields[512];
  int i;
  StringBuf b;
  struct irc_in_addr ip;
  unsigned char bits;

  if(cargc < 4)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  if(!ipmask_parse(cargv[1], &ip, &bits)) {
    controlreply(sender, "Invalid host.");
    return CMD_ERROR;
  }

  th = th_getbyhostandmask(&ip, bits);

  if(th->group != tg) {
    controlreply(sender, "Host does not belong to the specified group.");
    return CMD_ERROR;
  }

  what = cargv[2];
  to = cargv[3];

  sbinit(&b, validfields, sizeof(validfields));
  for(i=0;i<trusthostmods_a.cursi;i++) {
    if(!strcmp(what, trusthostmods[i].name)) {
      if(!(trusthostmods[i].fn)(th, to)) {
        controlreply(sender, "An error occured changing that property, check the syntax.");
        return CMD_ERROR;
      }
      break;
    }

    if(i > 0)
      sbaddstr(&b, ", ");
    sbaddstr(&b, trusthostmods[i].name);
  }

  if(i == trusthostmods_a.cursi) {
    sbterminate(&b);
    controlreply(sender, "No such field, valid fields are: %s", validfields);
    return CMD_ERROR;
  }

  triggerhook(HOOK_TRUSTS_MODIFYHOST, th);
  th_update(th);
  controlreply(sender, "Host modified.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTMODIFIED'ed host '%s' in group '%s' (field: %s, value: %s)", controlid(sender), trusts_cidr2str(&ip, bits), tg->name->content, what, to);
  trustlog(tg, sender->authname, "Modified %s for host '%s': %s", what, tg->name->content, to);

  return CMD_OK;
}

static int trusts_cmdtrustlogspew(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *name;
  int groupid;
  int limit = 0;

  if(cargc < 1)
    return CMD_USAGE;

  if(cargc>1)
    limit = strtoul(cargv[1], NULL, 10);

  if(limit==0)
    limit = 100;

  name = cargv[0];

  if (name[0] == '#') {
    groupid = strtoul(name + 1, NULL, 10);
    trustlogspewid(sender, groupid, limit);
  } else {
    trustlogspewname(sender, name, limit);
  }

  return CMD_OK;
}

static int trusts_cmdtrustloggrep(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *pattern;
  int limit = 0;

  if(cargc < 1)
    return CMD_USAGE;

  pattern = cargv[0];

  if(cargc>1)
    limit = strtoul(cargv[1], NULL, 10);

  if(limit==0)
    limit = 100;

  trustloggrep(sender, pattern, limit);

  return CMD_OK;
}

static int trusts_cmdtrustcomment(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg = NULL;
  char *name, *comment;

  if(cargc < 2)
    return CMD_USAGE;

  name = cargv[0];
  comment = cargv[1];

  tg = tg_strtotg(name);

  if(!tg) {
    controlreply(sender, "Invalid trust group name or ID.");
    return CMD_OK;
  }

    controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTCOMMENT'ed group '%s': %s", controlid(sender), tg->name->content, comment);
  trustlog(tg, sender->authname, "Comment: %s", comment);

  return CMD_OK;
}

static void cleanuptrusts(void *arg);

static int trusts_cmdtrustcleanup(void *source, int cargc, char **cargv) {
  cleanuptrusts(source);

  controlreply(source, "Done.");

  return CMD_OK;
}


static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustgroupadd", NO_OPER, 7, trusts_cmdtrustgroupadd, "Usage: trustgroupadd <name> <howmany> <howlong> <maxperident> <enforceident> <contact> ?comment?");
  registercontrolhelpcmd("trustadd", NO_OPER, 2, trusts_cmdtrustadd, "Usage: trustadd <#id|name|id> <host>");
  registercontrolhelpcmd("trustgroupdel", NO_OPER, 1, trusts_cmdtrustgroupdel, "Usage: trustgroupdel <#id|name|id>");
  registercontrolhelpcmd("trustdel", NO_OPER, 2, trusts_cmdtrustdel, "Usage: trustdel <#id|name|id> <ip/mask>");
  registercontrolhelpcmd("trustgroupmodify", NO_OPER, 3, trusts_cmdtrustgroupmodify, "Usage: trustgroupmodify <#id|name|id> <field> <new value>");
  registercontrolhelpcmd("trusthostmodify", NO_OPER, 4, trusts_cmdtrusthostmodify, "Usage: trusthostmodify <#id|name|id> <host> <field> <new value>");
  registercontrolhelpcmd("trustlogspew", NO_OPER, 2, trusts_cmdtrustlogspew, "Usage: trustlogspew <#id|name> ?limit?\nShows log for the specified trust group.");
  registercontrolhelpcmd("trustloggrep", NO_OPER, 2, trusts_cmdtrustloggrep, "Usage trustloggrep <pattern> ?limit?\nShows maching log entries.");
  registercontrolhelpcmd("trustcomment", NO_OPER, 2, trusts_cmdtrustcomment, "Usage: trustcomment <#id|name> <comment>\nLogs a comment for a trust.");
  registercontrolhelpcmd("trustcleanup", NO_DEVELOPER, 0, trusts_cmdtrustcleanup, "Usage: trustcleanup\nCleans up unused trusts.");
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
  deregistercontrolcmd("trusthostmodify", trusts_cmdtrusthostmodify);
  deregistercontrolcmd("trustlogspew", trusts_cmdtrustlogspew);
  deregistercontrolcmd("trustloggrep", trusts_cmdtrustloggrep);
  deregistercontrolcmd("trustcomment", trusts_cmdtrustcomment);
  deregistercontrolcmd("trustcleanup", trusts_cmdtrustcleanup);
}

static int loaded;

#define _ms_(x) (struct trustmodification){ .name = # x, .fn = modify ## x }
#define MSGROUP(x) { int slot = array_getfreeslot(&trustgroupmods_a); trustgroupmods = (struct trustmodification *)trustgroupmods_a.content; memcpy(&trustgroupmods[slot], &_ms_(x), sizeof(struct trustmodification)); }
#define MSHOST(x) { int slot = array_getfreeslot(&trusthostmods_a); trusthostmods = (struct trustmodification *)trusthostmods_a.content; memcpy(&trusthostmods[slot], &_ms_(x), sizeof(struct trustmodification)); }

static void setupmods(void) {
  MSGROUP(expires);
  MSGROUP(enforceident);
  MSGROUP(maxperident);
  MSGROUP(contact);
  MSGROUP(comment);
  MSGROUP(trustedfor);

  MSHOST(maxpernode);
  MSHOST(nodebits);
}

static int cleanuptrusts_active;

static void cleanuptrusts(void *arg) {
  unsigned int now, to_age;
  nick *np = (nick *)arg;
  trustgroup *tg;
  trusthost *th;
  int thcount = 0, tgcount = 0;
  int i;
  array expiredths, expiredtgs;

  now = time(NULL);
  to_age = now - (CLEANUP_TH_INACTIVE * 3600 * 24);

  if(np) {
    controlwall(NO_OPER, NL_TRUSTS, "CLEANUPTRUSTS: Manually started by %s.", np->nick);
  } else {
    controlwall(NO_OPER, NL_TRUSTS, "CLEANUPTRUSTS: Automatically started.");
  }

  if (cleanuptrusts_active) {
    controlwall(NO_OPER, NL_TRUSTS, "CLEANUPTRUSTS: ABORTED! Cleanup already in progress! BUG BUG BUG!");
    return;
  }

  cleanuptrusts_active=1;

  array_init(&expiredtgs, sizeof(trustgroup *));

  for(tg=tglist;tg;tg=tg->next) {
    array_init(&expiredths, sizeof(trusthost *));

    for(th=tg->hosts;th;th=th->next) {
      if((th->count == 0 && th->created < to_age && th->lastseen < to_age) || (tg->expires && tg->expires < now)) {
        int pos = array_getfreeslot(&expiredths);
        ((trusthost **)(expiredths.content))[pos] = th;
      }       
    }

    for(i=0;i<expiredths.cursi;i++) {
      char *cidrstr;

      th = ((trusthost **)(expiredths.content))[i];
      triggerhook(HOOK_TRUSTS_DELHOST, th);
      th_delete(th);

      cidrstr = trusts_cidr2str(&th->ip, th->bits);
      trustlog(tg, "cleanuptrusts", "Removed host '%s' because it was unused for %d days.", cidrstr, CLEANUP_TH_INACTIVE);

      thcount++;
    }

    if(!tg->hosts) {
      int pos = array_getfreeslot(&expiredtgs);
      ((trustgroup **)(expiredtgs.content))[pos] = tg;
    }
  }

  for(i=0;i<expiredtgs.cursi;i++) {
    tg = ((trustgroup **)(expiredtgs.content))[i];
    triggerhook(HOOK_TRUSTS_DELGROUP, tg);
    trustlog(tg, "cleanuptrusts", "Deleted group '%s' because it had no hosts left.", tg->name->content);
    tg_delete(tg);
    tgcount++;
  }

  controlwall(NO_OPER, NL_TRUSTS, "CLEANUPTRUSTS: Removed %d trust hosts (inactive for %d days) and %d trust groups.", thcount, CLEANUP_TH_INACTIVE, tgcount);

  cleanuptrusts_active=0;
}

static void schedulecleanup(int hooknum, void *arg) {
  /* run at 1am but only if we're more than 15m away from it, otherwise run tomorrow */

  time_t t = time(NULL);
  time_t next_run = ((t / 86400) * 86400 + 86400) + 3600;
  if(next_run - t < 900)
    next_run+=86400;

  schedulerecurring(next_run,0,86400,cleanuptrusts,NULL);
}

void _init(void) {
  sstring *m;

  array_init(&trustgroupmods_a, sizeof(struct trustmodification));
  array_init(&trusthostmods_a, sizeof(struct trustmodification));
  setupmods();

  m = getconfigitem("trusts", "master");
  if(!m || (atoi(m->content) != 1)) {
    Error("trusts_management", ERR_ERROR, "Not a master server, not loaded.");
    return;
  }

  loaded = 1;

  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_LOADED, schedulecleanup);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if(trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  array_free(&trustgroupmods_a);
  array_free(&trusthostmods_a);

  if(!loaded)
    return;

  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  deregistercommands(0, NULL);

  deleteallschedules(cleanuptrusts);
}
