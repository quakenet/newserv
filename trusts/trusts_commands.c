#include <stdio.h>
#include <string.h>
#include "../lib/version.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../core/nsmalloc.h"
#include "../irc/irc.h"
#include "../newsearch/newsearch.h"
#include "../glines/glines.h"
#include "trusts.h"
#include "newsearch/trusts_newsearch.h"

MODULE_VERSION("");

static void registercommands(int, void *);
static void deregistercommands(int, void *);

extern void printnick_channels(searchCtx *, nick *, nick *);

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

static void traverseandmark(unsigned int marker, trusthost *th, int markchildren) {
  th->marker = marker;

  if(markchildren) {
    for(th=th->children;th;th=th->nextbychild) {
      th->marker = marker;
      traverseandmark(marker, th, markchildren);
    }
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

static void marktree(array *parents, unsigned int marker, trusthost *th, int showchildren) {
  trusthost *pth;
  int parentcount = 0;

  for(pth=th->parent;pth;pth=pth->parent) {
    insertth(parents, pth);

    pth->marker = marker;
  }

  if(parentcount == 0)
    insertth(parents, th);

  /* sadly we need to recurse down */
  traverseandmark(marker, th, showchildren);
}

static void outputtree(nick *np, unsigned int marker, trustgroup *originalgroup, trusthost *th, int depth, int showchildren) {
  char *cidrstr, *prespacebuf, *postspacebuf, parentbuf[512];

  if(th->marker != marker)
    return;

  cidrstr = trusts_cidr2str(&th->ip, th->bits);
  calculatespaces(depth + 2, 30 + 1, cidrstr, &prespacebuf, &postspacebuf);

  if(th->group == originalgroup) {
    if(!showchildren && th->group == originalgroup && th->children)
      prespacebuf[0] = '*';
    else
      prespacebuf[0] = ' ';

    prespacebuf[1] = '>';

    parentbuf[0] = '\0';
  } else {
    /* show the ids of other groups */

    snprintf(parentbuf, sizeof(parentbuf), "%-10d %s", th->group->id, th->group->name->content);
  }

  controlreply(np, "%s%s%s %-10d %-10d %-21s %-15d /%-14d%s", prespacebuf, cidrstr, postspacebuf, th->count, th->maxusage, (th->count>0)?"(now)":((th->lastseen>0)?trusts_timetostr(th->lastseen):"(never)"), th->maxpernode, (irc_in_addr_is_ipv4(&th->ip))?(th->nodebits - 96):th->nodebits, parentbuf);  

  /* Make sure we're not seeing this subtree again. */
  th->marker = -1;

  for(th=th->children;th;th=th->nextbychild)
    outputtree(np, marker, originalgroup, th, depth + 1, showchildren);
}

static char *formatflags(int flags) {
  static char buf[512];

  buf[0] = '\0';

  if(flags & TRUST_ENFORCE_IDENT)
    strncat(buf, "enforcing ident", 512);

  if(flags & TRUST_NO_CLEANUP) {
    if(buf[0])
      strncat(buf, ", ", 512);

    strncat(buf, "exempt from cleanup", 512);
  }

  if(flags & TRUST_PROTECTED) {
    if(buf[0])
      strncat(buf, ", ", 512);

    strncat(buf, "protected", 512);
  }

  if(flags & TRUST_RELIABLE_USERNAME) {
    if(buf[0])
      strncat(buf, ", ", 512);

    strncat(buf, "reliable username", 512);
  }

  buf[512-1] = '\0';

  return buf;
}

static char *formatlimit(unsigned int limit) {
  static char buf[64];

  if(limit)
    snprintf(buf, sizeof(buf), "%u", limit);
  else
    strncpy(buf, "unlimited", sizeof(buf));

  return buf;
}

static void displaygroup(nick *sender, trustgroup *tg, int showchildren) {
  trusthost *th, **p2;
  unsigned int marker;
  array parents;
  int i;
  time_t t = getnettime();

  /* abusing the ternary operator a bit :( */
  controlreply(sender, "Name:            : %s", tg->name->content);
  controlreply(sender, "Trusted for      : %s", formatlimit(tg->trustedfor));
  controlreply(sender, "Currently using  : %d", tg->count);
  controlreply(sender, "Clients per user : %s", formatlimit(tg->maxperident));
  controlreply(sender, "Flags            : %s", formatflags(tg->flags));
  controlreply(sender, "Contact:         : %s", tg->contact->content);
  controlreply(sender, "Expires in       : %s", (tg->expires)?((tg->expires>t)?longtoduration(tg->expires - t, 2):"the past (will be removed during next cleanup)"):"never");
  controlreply(sender, "Created by       : %s", tg->createdby->content);
  controlreply(sender, "Comment:         : %s", tg->comment->content);
  controlreply(sender, "ID:              : %u", tg->id);
  controlreply(sender, "Last used        : %s", (tg->count>0)?"(now)":((tg->lastseen>0)?trusts_timetostr(tg->lastseen):"(never)"));
  controlreply(sender, "Max usage        : %d", tg->maxusage);
  controlreply(sender, "Last max reset   : %s", tg->lastmaxusereset?trusts_timetostr(tg->lastmaxusereset):"(never)");

  controlreply(sender, "---");
  controlreply(sender, "Attributes: * (has hidden children, show with -v), > (belongs to this trust group)");
  controlreply(sender, "Host                            Current    Max        Last seen             Max per Node    Node Mask      Group ID   Group name");

  marker = nextthmarker();
  array_init(&parents, sizeof(trusthost *));

  for(th=tg->hosts;th;th=th->next)
    marktree(&parents, marker, th, showchildren);

  p2 = (trusthost **)(parents.content);
  for(i=0;i<parents.cursi;i++)
    outputtree(sender, marker, tg, p2[i], 0, showchildren);

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
  int showchildren;

  if(cargc < 1)
    return CMD_USAGE;

  if(strcmp(cargv[0], "-v") == 0) {
    if(cargc < 2)
      return CMD_USAGE;

    showchildren = 1;
    name = cargv[1];
  } else {
    showchildren = 0;
    name = cargv[0];
  }

  tg = tg_strtotg(name);

  if(tg) {
    displaygroup(sender, tg, showchildren);
    return CMD_OK;
  }

  if(ipmask_parse(name, &ip, &bits)) {
    th = th_getbyhost(&ip);

    if(!th) {
      controlreply(sender, "Specified IP address is not trusted.");
      return CMD_OK;
    }

    displaygroup(sender, th->group, showchildren);
    return CMD_OK;
  }

  for(tg=tglist;tg;tg=tg->next) {
    if(match(name, tg->name->content))
      continue;

    displaygroup(sender, tg, showchildren);
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

static void trusts_suggestgline_cb(const char *mask, int hits, void *uarg) {
  nick *sender = uarg;

  controlreply(sender, "mask: %s, hits: %d", mask, hits);
}

static int trusts_cmdtrustglinesuggest(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char mask[512];
  char *p, *user, *host;
  struct irc_in_addr ip;
  unsigned char bits;
  int count;

  if(cargc < 1)
    return CMD_USAGE;

  strncpy(mask, cargv[0], sizeof(mask));

  p = strchr(mask, '@');

  if(!p)
    return CMD_USAGE;

  user = mask;
  host = p + 1;
  *p = '\0';

  if(!ipmask_parse(host, &ip, &bits)) {
    controlreply(sender, "Invalid CIDR.");
    return CMD_ERROR;
  }

  count = glinesuggestbyip(user, &ip, 128, 0, trusts_suggestgline_cb, sender);

  controlreply(sender, "Total hits: %d", count);

  return CMD_OK;
}

static int trusts_cmdtrustspew(void *source, int cargc, char **cargv) {
  nick *sender = source;
  searchASTExpr tree;

  if(cargc < 1)
    return CMD_USAGE;

  tree = NSASTNode(tgroup_parse, NSASTLiteral(cargv[0]));
  return ast_nicksearch(&tree, controlreply, sender, NULL, printnick_channels, NULL, NULL, 2000);
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustlist", NO_OPER, 2, trusts_cmdtrustlist, "Usage: trustlist [-v] <#id|name|IP>\nShows trust data for the specified trust group.");
  registercontrolhelpcmd("trustdump", NO_OPER, 2, trusts_cmdtrustdump, "Usage: trustdump <#id> <number>");
  registercontrolhelpcmd("trustglinesuggest", NO_OPER, 1, trusts_cmdtrustglinesuggest, "Usage: trustglinesuggest <user@host>\nSuggests glines for the specified hostmask.");
  registercontrolhelpcmd("trustspew", NO_OPER, 1, trusts_cmdtrustspew, "Usage: trustspew <#id|name>\nShows currently connected users for the specified trust group.");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustlist", trusts_cmdtrustlist);
  deregistercontrolcmd("trustdump", trusts_cmdtrustdump);
  deregistercontrolcmd("trustglinesuggest", trusts_cmdtrustglinesuggest);
  deregistercontrolcmd("trustspew", trusts_cmdtrustspew);
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
