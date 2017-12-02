#include <string.h>
#include <stdio.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../core/config.h"
#include "whowas.h"

#define XStringify(x) Stringify(x)
#define Stringify(x) #x

MODULE_VERSION("");

whowas *whowasrecs;
int whowasoffset = 0;
int whowasmax;

whowas *whowas_fromnick(nick *np, int standalone) {
  whowas *ww;
  nick *wnp;
  struct irc_in_addr ipaddress_canonical;
  void *args[2];

  /* Create a new record. */
  if (standalone)
    ww = malloc(sizeof(whowas));
  else {
    ww = &whowasrecs[whowasoffset];
    whowas_clean(ww);
    whowasoffset = (whowasoffset + 1) % whowasmax;
  }

  memset(ww, 0, sizeof(whowas));

  wnp = &ww->nick;
  memset(wnp, 0, sizeof(nick));
  strncpy(wnp->nick, np->nick, NICKLEN + 1);
  wnp->numeric = np->numeric;
  strncpy(wnp->ident, np->ident, USERLEN + 1);

  wnp->host = newhost();
  memset(wnp->host, 0, sizeof(host));
  wnp->host->name = getsstring(np->host->name->content, HOSTLEN);

  wnp->realname = newrealname();
  memset(wnp->realname, 0, sizeof(realname));
  wnp->realname->name = getsstring(np->realname->name->content, REALLEN);
  wnp->shident = np->shident ? getsstring(np->shident->content, 512) : NULL;
  wnp->sethost = np->sethost ? getsstring(np->sethost->content, 512) : NULL;
  wnp->opername = np->opername ? getsstring(np->opername->content, 512) : NULL;
  wnp->umodes = np->umodes;
  if (np->auth) {
    wnp->auth = newauthname();
    memset(wnp->auth, 0, sizeof(authname));
    wnp->auth->userid = np->auth->userid;
    strncpy(wnp->auth->name, np->auth->name, ACCOUNTLEN + 1);
    wnp->authname = wnp->auth->name;
  }
  wnp->timestamp = np->timestamp;
  wnp->accountts = np->accountts;
  wnp->away = np->away ? getsstring(np->away->content, 512) : NULL;

  memcpy(&wnp->ipaddress, &np->ipaddress, sizeof(struct irc_in_addr));

  ip_canonicalize_tunnel(&ipaddress_canonical, &np->ipaddress);
  wnp->ipnode = refnode(iptree, &ipaddress_canonical, PATRICIA_MAXBITS);

  wnp->next = (nick *)ww; /* Yuck. */

  ww->timestamp = getnettime();
  ww->type = WHOWAS_USED;

  args[0] = ww;
  args[1] = np;
  triggerhook(HOOK_WHOWAS_NEWRECORD, args);

  return ww;
}

void whowas_clean(whowas *ww) {
  nick *np;

  if (!ww || ww->type == WHOWAS_UNUSED)
    return;

  triggerhook(HOOK_WHOWAS_LOSTRECORD, ww);

  np = &ww->nick;
  freesstring(np->host->name);
  freehost(np->host);
  freesstring(np->realname->name);
  freerealname(np->realname);
  freesstring(np->shident);
  freesstring(np->sethost);
  freesstring(np->opername);
  freeauthname(np->auth);
  freesstring(np->away);
  derefnode(iptree, np->ipnode);
  freesstring(ww->reason);
  freesstring(ww->newnick);
  ww->type = WHOWAS_UNUSED;
}

void whowas_free(whowas *ww) {
  whowas_clean(ww);
  free(ww);
}

static void whowas_handlequitorkill(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *reason = args[1];
  char *rreason;
  char resbuf[512];
  whowas *ww;

  /* Create a new record. */
  ww = whowas_fromnick(np, 0);

  if (hooknum == HOOK_NICK_KILL) {
    if ((rreason = strchr(reason, ' '))) {
      sprintf(resbuf, "Killed%s", rreason);
      reason = resbuf;
    }

    ww->type = WHOWAS_KILL;
  } else {
    if (strncmp(reason, "G-lined", 7) == 0)
      ww->type = WHOWAS_KILL;
    else
      ww->type = WHOWAS_QUIT;
  }

  ww->reason = getsstring(reason, WW_REASONLEN);
}

static void whowas_handlerename(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *oldnick = args[1];
  whowas *ww;
  nick *wnp;

  ww = whowas_fromnick(np, 0);
  ww->type = WHOWAS_RENAME;
  wnp = &ww->nick;
  ww->newnick = getsstring(wnp->nick, NICKLEN);
  strncpy(wnp->nick, oldnick, NICKLEN + 1);
}

whowas *whowas_chase(const char *target, int maxage) {
  whowas *ww;
  nick *wnp;
  time_t now;
  int i;

  now = getnettime();

  for (i = whowasoffset + whowasmax - 1; i >= whowasoffset; i--) {
    ww = &whowasrecs[i % whowasmax];

    if (ww->type == WHOWAS_UNUSED)
      continue;

    wnp = &ww->nick;

    if (ww->timestamp < now - maxage)
      break; /* records are in timestamp order, we're done */

    if (ircd_strcmp(wnp->nick, target) == 0)
      return ww;
  }

  return NULL;
}

const char *whowas_format(whowas *ww) {
  nick *np = &ww->nick;
  static char buf[512];
  char timebuf[30];
  char hostmask[512];

  snprintf(hostmask, sizeof(hostmask), "%s!%s@%s%s%s [%s] (%s)",
           np->nick, np->ident, np->host->name->content,
           np->auth ? "/" : "", np->auth ? np->authname : "",
           IPtostr(np->ipaddress),
           printflags(np->umodes, umodeflags));
  strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M:%S", localtime(&(ww->timestamp)));

  if (ww->type == WHOWAS_RENAME)
    snprintf(buf, sizeof(buf), "[%s] NICK %s r(%s) -> %s", timebuf, hostmask, np->realname->name->content, ww->newnick->content);
  else
    snprintf(buf, sizeof(buf), "[%s] %s %s r(%s): %s", timebuf, (ww->type == WHOWAS_QUIT) ? "QUIT" : "KILL", hostmask, np->realname->name->content, ww->reason->content);

  return buf;
}

const char *whowas_formatchannels(whowas *ww) {
  static char buf[512];
  int i, first = 1;

  strcpy(buf, "Channels: ");

  for (i = 0; i < WW_MAXCHANNELS; i++) {
    if (!ww->channels[i])
      break;

    if (!first)
      strncat(buf, ", ", sizeof(buf));
    else
      first = 0;

    strncat(buf, ww->channels[i]->name->content, sizeof(buf));
  }

  if (!ww->channels[0])
    strncat(buf, "(No channels.)", sizeof(buf));

  buf[sizeof(buf) - 1] = '\0';

  return buf;
}

unsigned int nextwhowasmarker() {
  whowas *ww;
  int i;
  static unsigned int whowasmarker=0;

  whowasmarker++;

  if (!whowasmarker) {
    /* If we wrapped to zero, zap the marker on all records */
    for (i = 0; i < whowasmax; i++) {
      ww = &whowasrecs[i % whowasmax];
      ww->marker=0;
    }

    whowasmarker++;
  }

  return whowasmarker;
}

void _init(void) {
  {
    sstring *temp = getcopyconfigitem("whowas", "maxentries", XStringify(WW_DEFAULT_MAXENTRIES), 10);
    whowasmax = atoi(temp->content);
    freesstring(temp);
  }
  whowasrecs = calloc(whowasmax, sizeof(whowas));

  registerhook(HOOK_NICK_QUIT, whowas_handlequitorkill);
  registerhook(HOOK_NICK_KILL, whowas_handlequitorkill);
  registerhook(HOOK_NICK_RENAME, whowas_handlerename);
}

void _fini(void) {
  int i;
  whowas *ww;

  deregisterhook(HOOK_NICK_QUIT, whowas_handlequitorkill);
  deregisterhook(HOOK_NICK_KILL, whowas_handlequitorkill);
  deregisterhook(HOOK_NICK_RENAME, whowas_handlerename);

  for (i = 0; i < whowasmax; i++) {
    ww = &whowasrecs[i];
    whowas_clean(ww);
  }

  free(whowasrecs);
}
