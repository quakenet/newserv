#include <stdio.h>
#include <string.h>
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../core/nsmalloc.h"
#include "trusts.h"

static void registercommands(int, void *);
static void deregistercommands(int, void *);

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

static void insertth(array *parents, trusthost *th) {
  int i;
  trusthost **p2 = (trusthost **)(parents->content);

  /* this eliminates common subtrees */
  for(i=0;i<parents->cursi;i++)
    if(p2[i] == th)
      break;

  if(i == parents->cursi) {
    int pos = array_getfreeslot(parents);
    ((trusthost **)(parents->content))[pos] = th;
  }
}

static void marktree(array *parents, unsigned int marker, trusthost *th) {
  trusthost *pth;
  int parentcount = 0;

  for(pth=th->parent;pth;pth=pth->next) {
    insertth(parents, pth);

    pth->marker = marker;
  }

  if(parentcount == 0)
    insertth(parents, th);

  /* sadly we need to recurse down */
  traverseandmark(marker, th);
}

static void outputtree(nick *np, unsigned int marker, trustgroup *originalgroup, trusthost *th, int depth) {
  char *cidrstr, *prespacebuf, *postspacebuf, parentbuf[512];

  if(th->marker != marker)
    return;

  cidrstr = trusts_cidr2str(&th->ip, th->bits);
  calculatespaces(depth + 1, 20 + 1, cidrstr, &prespacebuf, &postspacebuf);

  if(th->group == originalgroup) {
    prespacebuf[0] = '>';

    parentbuf[0] = '\0';
  } else {
    /* show the ids of other groups */

    snprintf(parentbuf, sizeof(parentbuf), "%-10d %s", th->group->id, th->group->name->content);
  }

  controlreply(np, "%s%s%s %-10d %-10d %-21s %-15d /%-14d%s", prespacebuf, cidrstr, postspacebuf, th->count, th->maxusage, (th->count>0)?"(now)":((th->lastseen>0)?trusts_timetostr(th->lastseen):"(never)"), th->maxpernode, (irc_in_addr_is_ipv4(&th->ip))?(th->nodebits - 96):th->nodebits, parentbuf);  

  for(th=th->children;th;th=th->nextbychild)
    outputtree(np, marker, originalgroup, th, depth + 1);
}

static void displaygroup(nick *sender, trustgroup *tg) {
  trusthost *th, **p2;
  unsigned int marker;
  array parents;
  int i;
  time_t t = time(NULL);

  /* abusing the ternary operator a bit :( */
  controlreply(sender, "Name:            : %s", tg->name->content);
  controlreply(sender, "Trusted for      : %d", tg->trustedfor);
  controlreply(sender, "Currently using  : %d", tg->count);
  controlreply(sender, "Clients per user : %d (%senforcing ident)", tg->maxperident, tg->mode?"":"not ");
  controlreply(sender, "Contact:         : %s", tg->contact->content);
  controlreply(sender, "Expires in       : %s", (tg->expires)?((tg->expires>t)?longtoduration(tg->expires - t, 2):"(the past -- BUG)"):"never");
  controlreply(sender, "Last changed by  : %s", tg->createdby->content);
  controlreply(sender, "Comment:         : %s", tg->comment->content);
  controlreply(sender, "ID:              : %u", tg->id);
  controlreply(sender, "Last used        : %s", (tg->count>0)?"(now)":((tg->lastseen>0)?trusts_timetostr(tg->lastseen):"(never)"));
  controlreply(sender, "Max usage        : %d", tg->maxusage);
  controlreply(sender, "Last max reset   : %s", tg->lastmaxusereset?trusts_timetostr(tg->lastmaxusereset):"(never)");

  controlreply(sender, "Host                 Current    Max        Last seen             Max per Node    Node Mask      Group ID   Group name");

  marker = nextthmarker();
  array_init(&parents, sizeof(trusthost *));

  for(th=tg->hosts;th;th=th->next)
    marktree(&parents, marker, th);

  p2 = (trusthost **)(parents.content);
  for(i=0;i<parents.cursi;i++)
    outputtree(sender, marker, tg, p2[i], 0);

  array_free(&parents);

  controlreply(sender, "End of list.");
}

static int trusts_cmdtrustlist(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg = NULL;
  int found = 0, remaining = 50;
  char *name;
  trusthost *th;
  struct irc_in_addr ip;
  unsigned char bits;

  if(cargc < 1)
    return CMD_USAGE;

  name = cargv[0];

  tg = tg_strtotg(name);

  if(tg) {
    displaygroup(sender, tg);
    return CMD_OK;
  }

  if(ipmask_parse(name, &ip, &bits)) {
    th = th_getbyhost(&ip);

    if(!th) {
      controlreply(sender, "Specified IP address is not trusted.");
      return CMD_OK;
    }

    displaygroup(sender, th->group);
    return CMD_OK;
  }

  for(tg=tglist;tg;tg=tg->next) {
    if(match(name, tg->name->content))
      continue;

    displaygroup(sender, tg);
    if(--remaining == 0) {
      controlreply(sender, "Maximum number of matches reached.");
      return CMD_OK;
    }
    found = 1;
  }

  if(!found)
    controlreply(sender, "No matches found.");

  return CMD_OK;
}

static int comparetgs(const void *_a, const void *_b) {
  const trustgroup *a = _a;
  const trustgroup *b = _b;

  if(a->id > b->id)
    return 1;
  if(a->id < b-> id)
    return -1;
  return 0;
}

static int trusts_cmdtrustdump(void *source, int argc, char **argv) {
  trusthost *th;
  trustgroup *tg, **atg;
  unsigned int wanted, max, maxid, totalcount, i, groupcount, linecount;
  nick *np = source;

  if((argc < 2) || (argv[0][0] != '#'))
    return CMD_USAGE;

  wanted = atoi(&argv[0][1]);
  max = atoi(argv[1]);

  for(maxid=totalcount=0,tg=tglist;tg;tg=tg->next) {
    if(totalcount == 0 || tg->id > maxid)
      maxid = tg->id;

    totalcount++;
  }

  if(maxid > totalcount) {
    controlreply(np, "Start ID cannot exceed current maximum group ID (#%u)", maxid);
    return CMD_OK;
  }

  atg = nsmalloc(POOL_TRUSTS, sizeof(trusthost *) * totalcount);
  if(!atg) {
    controlreply(np, "Memory error.");
    return CMD_ERROR;
  }

  for(i=0,tg=tglist;i<totalcount&&tg;tg=tg->next,i++)
    atg[i] = tg;

  qsort(atg, totalcount, sizeof(trustgroup *), comparetgs);

  for(i=0;i<totalcount;i++)
    if(atg[i]->id >= wanted)
      break;

  for(groupcount=linecount=0;i<totalcount;i++) {
    linecount++;
    groupcount++;

    controlreply(np, "G,%s", dumptg(atg[i], 1));

    for(th=atg[i]->hosts;th;th=th->next) {
      linecount++;
      controlreply(np, "H,%s", dumpth(th, 1));
    }

    if(--max == 0)
      break;
  }
  nsfree(POOL_TRUSTS, atg);

  controlreply(np, "End of list, %u groups and %u lines returned.", groupcount, linecount);
  return CMD_OK;
}

static int trusts_cmdtrustgline(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *user, *reason;
  int duration, count;

  if(cargc < 4)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  user = cargv[1];

  duration = durationtolong(cargv[2]);
  if((duration <= 0) || (duration > MAXDURATION)) {
    controlreply(sender, "Invalid duration supplied.");
    return CMD_ERROR;
  }

  reason = cargv[3];

  count = trustgline(tg, user, duration, reason);

  controlwall(NO_OPER, NL_GLINES|NL_TRUSTS, "%s TRUSTGLINE'd user '%s' on group '%s', %d gline(s) set.", controlid(sender), user, tg->name->content, count);
  controlreply(sender, "Done. %d gline(s) set.", count);

  return CMD_OK;
}

static int trusts_cmdtrustungline(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *user;
  int count;

  if(cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  user = cargv[1];

  count = trustungline(tg, user, 0, "Deactivated.");

  controlwall(NO_OPER, NL_GLINES|NL_TRUSTS, "%s TRUSTUNGLINE'd user '%s' on group '%s', %d gline(s) removed.", controlid(sender), user, tg->name->content, count);
  controlreply(sender, "Done. %d gline(s) removed.", count);

  return CMD_OK;
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustlist", NO_OPER, 1, trusts_cmdtrustlist, "Usage: trustlist <#id|name|IP>\nShows trust data for the specified trust group.");
  registercontrolhelpcmd("trustdump", NO_OPER, 2, trusts_cmdtrustdump, "Usage: trustdump <#id> <number>");
  registercontrolhelpcmd("trustgline", NO_OPER, 4, trusts_cmdtrustgline, "Usage: trustgline <#id|name> <user> <duration> <reason>\nGlines a user on all hosts of a trust group.");
  registercontrolhelpcmd("trustungline", NO_OPER, 4, trusts_cmdtrustungline, "Usage: trustungline <#id|name> <user>\nUnglines a user on all hosts of a trust group.");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustlist", trusts_cmdtrustlist);
  deregistercontrolcmd("trustdump", trusts_cmdtrustdump);
  deregistercontrolcmd("trustgline", trusts_cmdtrustgline);
  deregistercontrolcmd("trustungline", trusts_cmdtrustungline);
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

  deregistercommands(0, NULL);
}
