#include <string.h>
#include <stdio.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "whowas.h"

MODULE_VERSION("");

whowas *whowas_head = NULL, *whowas_tail = NULL;
int whowas_count = 0;

whowas *whowas_fromnick(nick *np) {
  whowas *ww;
  nick *wnp;
  struct irc_in_addr ipaddress_canonical;

  /* Create a new record. */
  ww = malloc(sizeof(whowas));
  memset(ww, 0, sizeof(whowas));

  wnp = newnick();
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
  wnp->shident = np->away ? getsstring(np->shident->content, 512) : NULL;
  wnp->sethost = np->away ? getsstring(np->sethost->content, 512) : NULL;
  wnp->opername = np->away ? getsstring(np->opername->content, 512) : NULL;
  wnp->umodes = np->umodes;
  if (np->auth) {
    wnp->auth = newauthname();
    memset(wnp->auth, 0, sizeof(authname));
    wnp->auth->userid = np->auth->userid;
    strncpy(wnp->auth->name, np->auth->name, ACCOUNTLEN + 1);
    wnp->auth = np->auth;
  }
  wnp->authname = wnp->auth->name;
  wnp->timestamp = np->timestamp;
  wnp->accountts = np->accountts;
  wnp->away = np->away ? getsstring(np->away->content, 512) : NULL;

  memcpy(&wnp->ipaddress, &np->ipaddress, sizeof(struct irc_in_addr));

  ip_canonicalize_tunnel(&ipaddress_canonical, &np->ipaddress);
  wnp->ipnode = refnode(iptree, &ipaddress_canonical, PATRICIA_MAXBITS);

  wnp->next = (nick *)ww; /* Yuck. */

  ww->nick = wnp;
  ww->timestamp = getnettime();

  return ww;
}

void whowas_linkrecord(whowas *ww) {
  if (whowas_head)
    whowas_head->prev = ww;

  ww->next = whowas_head;
  whowas_head = ww;

  ww->prev = NULL;

  if (!whowas_tail)
    whowas_tail = ww;

  whowas_count++;
}

void whowas_unlinkrecord(whowas *ww) {
  if (!ww->next)
    whowas_tail = ww->prev;

  if (ww->prev)
    ww->prev->next = NULL;
  else
    whowas_head = ww->prev;

  whowas_count--;
}

void whowas_free(whowas *ww) {
  nick *np;

  if (!ww)
    return;

  np = ww->nick;

  freesstring(np->host->name);
  freehost(np->host);
  freesstring(np->realname->name);
  freerealname(np->realname);
  freesstring(np->shident);
  freesstring(np->sethost);
  freesstring(np->opername);
  freeauthname(np->auth);
  freesstring(np->away);
  freenick(np);
  freesstring(ww->reason);
  free(ww);
}

static void whowas_cleanup(void) {
  time_t now;
  whowas *ww;

  time(&now);

  /* Clean up old records. */
  while (whowas_tail && (whowas_tail->timestamp < now - WW_MAXAGE || whowas_count >= WW_MAXENTRIES)) {
    ww = whowas_tail;
    whowas_unlinkrecord(ww);
    whowas_free(ww);
  }
}

static void whowas_handlequitorkill(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *reason = args[1];
  char *rreason;
  char resbuf[512];
  whowas *ww;

  whowas_cleanup();

  /* Create a new record. */
  ww = whowas_fromnick(np);

  if (hooknum == HOOK_NICK_KILL) {
    if ((rreason = strchr(reason, ' '))) {
      sprintf(resbuf, "Killed%s", rreason);
      reason = resbuf;
    }

    ww->type = WHOWAS_KILL;
  } else
    ww->type = WHOWAS_QUIT;

  ww->reason = getsstring(reason, WW_REASONLEN);

  whowas_linkrecord(ww);
}

static void whowas_handlerename(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  char *oldnick = args[1];
  whowas *ww;
  nick *wnp;

  whowas_cleanup();

  ww = whowas_fromnick(np);
  ww->type = WHOWAS_RENAME;
  wnp = ww->nick;
  ww->newnick = getsstring(wnp->nick, NICKLEN);
  strncpy(wnp->nick, oldnick, NICKLEN + 1);

  whowas_linkrecord(ww);
}

whowas *whowas_chase(const char *nick, int maxage) {
  whowas *ww;
  time_t now;

  now = getnettime();

  for (ww = whowas_head; ww; ww = ww->next) {
    if (ww->timestamp < now - maxage)
      break; /* records are in timestamp order, we're done */

    if (ircd_strcmp(ww->nick->nick, nick) == 0)
      return ww;
  }

  return NULL;
}

const char *whowas_format(whowas *ww) {
  nick *np = ww->nick;
  static char buf[512];
  char timebuf[30];
  char hostmask[512];

  snprintf(hostmask, sizeof(hostmask), "%s!%s@%s%s%s [%s]",
           np->nick, np->ident, np->host->name->content,
           np->auth ? "/" : "", np->auth ? np->authname : "",
           IPtostr(np->p_ipaddr));
  strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M:%S", localtime(&(ww->timestamp)));

  if (ww->type == WHOWAS_RENAME)
    snprintf(buf, sizeof(buf), "[%s] NICK %s r(%s) -> %s", timebuf, hostmask, np->realname->name->content, ww->newnick->content);
  else
    snprintf(buf, sizeof(buf), "[%s] %s %s r(%s): %s", timebuf, (ww->type == WHOWAS_QUIT) ? "QUIT" : "KILL", hostmask, np->realname->name->content, ww->reason->content);

  return buf;
}

unsigned int nextwhowasmarker() {
  whowas *ww;
  static unsigned int whowasmarker=0;

  whowasmarker++;

  if (!whowasmarker) {
    /* If we wrapped to zero, zap the marker on all records */
    for (ww = whowas_head; ww; ww = ww->next)
        ww->marker=0;
    whowasmarker++;
  }

  return whowasmarker;
}

void _init(void) {
  registerhook(HOOK_NICK_QUIT, whowas_handlequitorkill);
  registerhook(HOOK_NICK_KILL, whowas_handlequitorkill);
  registerhook(HOOK_NICK_RENAME, whowas_handlerename);
}

void _fini(void) {
  deregisterhook(HOOK_NICK_QUIT, whowas_handlequitorkill);
  deregisterhook(HOOK_NICK_KILL, whowas_handlequitorkill);
  deregisterhook(HOOK_NICK_RENAME, whowas_handlerename);

  while (whowas_head)
    whowas_unlinkrecord(whowas_head);
}
