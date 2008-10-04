#include <stdio.h>
#include <string.h>
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "trusts.h"

int trusts_migration_start(TrustDBMigrationCallback, void *);
void trusts_migration_stop(void);
static void registercommands(int, void *);
static void deregistercommands(int, void *);

static void migrate_status(int errcode, void *tag) {
  long sender = (long)tag;
  nick *np = getnickbynumeric(sender);

  if(!np)
    return;

  if(!errcode || errcode == MIGRATION_LASTERROR) {
    if(!errcode) {
       controlreply(np, "Migration complete.");
       controlreply(np, "Attempting to reload database. . .");
    } else {
      controlreply(np, "An error occured after the database was unloaded, attempting reload. . .");
    }
    if(trusts_loaddb()) {
      controlreply(np, "Database reloaded successfully.");
    } else {
      controlreply(np, "An error occured, please reload the module manually.");
    }
  } else {
    controlreply(np, "Error %d occured during migration, commands reregistered.", errcode);
    registercommands(0, NULL);
  }
}

static int trusts_cmdmigrate(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int ret;

  /* iffy but temporary */
  ret = trusts_migration_start(migrate_status, (void *)(sender->numeric));
  if(!ret) {
    controlreply(sender, "Migration started, commands deregistered.");
    deregistercommands(0, NULL);
  } else {
    controlreply(sender, "Error %d starting migration.", ret);
  }

  return CMD_OK;
}

void calculatespaces(int spaces, int width, char *str, char **_prebuf, char **_postbuf) {
  static char prebuf[512], postbuf[512];
  int spacelen;

  if(spaces + 5 >= sizeof(prebuf)) {
    prebuf[0] = prebuf[1] = '\0';
  } else {
    memset(prebuf, ' ', spaces);
    prebuf[spaces] = '\0';
  }

  spacelen = width - (strlen(str) + spaces);
  if(spacelen <= 0 || spacelen + 5 >= sizeof(postbuf)) {
    postbuf[0] = postbuf[1] = '\0';
  } else {
    memset(postbuf, ' ', spacelen);
    postbuf[spacelen] = '\0';
  }

  *_prebuf = prebuf;
  *_postbuf = postbuf;
}

static void traverseandmark(unsigned int marker, trusthost *th) {
  th->marker = marker;

  for(th=th->children;th;th=th->nextbychild) {
    th->marker = marker;
    traverseandmark(marker, th);
  }
}

static void marktree(array *parents, unsigned int marker, trusthost *th) {
  trusthost *pth;

  for(pth=th->parent;pth;pth=pth->next) {
    trusthost **p2 = (trusthost **)(parents->content);
    int i;

    /* this eliminates common subtrees */
    for(i=0;i<parents->cursi;i++)
      if(p2[i] == pth)
        break;

    if(i == parents->cursi) {
      int pos = array_getfreeslot(parents);
      ((trusthost **)(parents->content))[pos] = pth;
    }

    pth->marker = marker;
  }

  /* sadly we need to recurse down */
  traverseandmark(marker, th);
}

static void outputtree(nick *np, unsigned int marker, trustgroup *originalgroup, trusthost *th, int depth) {
  char *cidrstr, *prespacebuf, *postspacebuf, parentbuf[512];

  if(th->marker != marker)
    return;

  cidrstr = trusts_cidr2str(th->ip, th->mask);
  calculatespaces(depth + 1, 20 + 1, cidrstr, &prespacebuf, &postspacebuf);

  if(th->group == originalgroup) {
    prespacebuf[0] = '>';

    parentbuf[0] = '\0';
  } else {
    /* show the ids of other groups */

    snprintf(parentbuf, sizeof(parentbuf), "%-10d %s", th->group->id, th->group->name->content);
  }

  controlreply(np, "%s%s%s %-10d %-10d %-20s%s", prespacebuf, cidrstr, postspacebuf, th->count, th->maxusage, (th->count>0)?"(now)":((th->lastseen>0)?trusts_timetostr(th->lastseen):"(never)"), parentbuf);  

  for(th=th->children;th;th=th->nextbychild)
    outputtree(np, marker, originalgroup, th, depth + 1);
}

static int trusts_cmdtrustlist(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg;
  trusthost *th, **p2;
  time_t t;
  unsigned int marker;
  array parents;
  int i;

  if(cargc < 1)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't find a trustgroup with that id.");
    return CMD_ERROR;
  }

  t = time(NULL);

  /* abusing the ternary operator a bit :( */
  controlreply(sender, "Name:            : %s", tg->name->content);
  controlreply(sender, "Trusted for      : %d", tg->trustedfor);
  controlreply(sender, "Currently using  : %d", tg->count);
  controlreply(sender, "Clients per user : %d (%senforcing ident)", tg->maxperident, tg->mode?"":"not ");
  controlreply(sender, "Contact:         : %s", tg->contact->content);
  controlreply(sender, "Expires in       : %s", (tg->expires>t)?longtoduration(tg->expires - t, 2):"(the past -- BUG)");
  controlreply(sender, "Last changed by  : %s", tg->createdby->content);
  controlreply(sender, "Comment:         : %s", tg->comment->content);
  controlreply(sender, "ID:              : %u", tg->id);
  controlreply(sender, "Last used        : %s", (tg->count>0)?"(now)":((tg->lastseen>0)?trusts_timetostr(tg->lastseen):"(never)"));
  controlreply(sender, "Max usage        : %d", tg->maxusage);
  controlreply(sender, "Last max reset   : %s", tg->lastmaxuserreset?trusts_timetostr(tg->lastmaxuserreset):"(never)");

  controlreply(sender, "Host                 Current    Max        Last seen           Group ID   Group name");

  marker = nextthmarker();
  array_init(&parents, sizeof(trusthost *));

  for(th=tg->hosts;th;th=th->next)
    marktree(&parents, marker, th);

  p2 = (trusthost **)(parents.content);
  for(i=0;i<parents.cursi;i++)
    outputtree(sender, marker, tg, p2[i], 0);

  array_free(&parents);

  controlreply(sender, "End of list.");

  return CMD_OK;
}

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
  /* TODO: controlwall */

  return CMD_OK;
}

static int trusts_cmdtrustgroupadd(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *name, *contact, *comment, createdby[ACCOUNTLEN + 2];
  unsigned int howmany, maxperident, enforceident;
  time_t howlong;
  trustgroup *tg;

  if(cargc < 6)
    return CMD_USAGE;

  name = cargv[0];
  howmany = strtoul(cargv[1], NULL, 10);
  if(!howmany || (howmany > 50000)) {
    controlreply(sender, "Bad value maximum number of clients.");
    return CMD_ERROR;
  }

  howlong = durationtolong(cargv[2]);
  if((howlong <= 0) || (howlong > 365 * 86400 * 20)) {
    controlreply(sender, "Invalid duration supplied.");
    return CMD_ERROR;
  }

  maxperident = strtoul(cargv[3], NULL, 10);
  if(!howmany || (maxperident > 1000)) {
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

  tg = tg_new(name, howmany, enforceident, maxperident, howlong + time(NULL), createdby, contact, comment);
  if(!tg) {
    controlreply(sender, "An error occured adding the trustgroup.");
    return CMD_ERROR;
  }

  controlreply(sender, "Group added.");
  /* TODO: controlwall */

  return CMD_OK;
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustmigrate", NO_DEVELOPER, 0, trusts_cmdmigrate, "Usage: trustmigrate\nCopies trust data from O and reloads the database.");
  registercontrolhelpcmd("trustlist", NO_OPER, 1, trusts_cmdtrustlist, "Usage: trustlist <#id|name|id>\nShows trust data for the specified trust group.");
  registercontrolhelpcmd("trustgroupadd", NO_OPER, 6, trusts_cmdtrustgroupadd, "Usage: trustgroupadd <name> <howmany> <howlong> <maxperident> <enforceident> <contact> ?comment?");
  registercontrolhelpcmd("trustadd", NO_OPER, 2, trusts_cmdtrustadd, "Usage: trustadd <#id|name|id> <host>");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustmigrate", trusts_cmdmigrate);
  deregistercontrolcmd("trustlist", trusts_cmdtrustlist);
  deregistercontrolcmd("trustgroupadd", trusts_cmdtrustgroupadd);
  deregistercontrolcmd("trustadd", trusts_cmdtrustadd);
}

void _init(void) {
  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if(trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  trusts_migration_stop();

  deregistercommands(0, NULL);
}
