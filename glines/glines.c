#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../trusts/trusts.h"
#include "../control/control.h"
#include "glines.h"

MODULE_VERSION("");

static void glines_sched_save(void *arg);

void _init() {
  /* If we're connected to IRC, force a disconnect. */
  if (connected) {
    irc_send("%s SQ %s 0 :Resync [adding gline support]", mynumeric->content, myserver->content);
    irc_disconnected();
  }

  registerserverhandler("GL", handleglinemsg, 6);
  registerhook(HOOK_CORE_STATSREQUEST, handleglinestats);

  schedulerecurring(time(NULL), 0, GLSTORE_SAVE_INTERVAL, &glines_sched_save, NULL);

  glstore_load();
}

void _fini() {
  deregisterserverhandler("GL", handleglinemsg);
  deregisterhook(HOOK_CORE_STATSREQUEST, handleglinestats);

  deleteschedule(NULL, glines_sched_save, NULL);
}

static void glines_sched_save(void *arg) {
  glstore_save();
}

int gline_match_nick(gline *gl, nick *np) {
  if (gl->flags & GLINE_BADCHAN)
    return 0;

  if (gl->flags & GLINE_REALNAME) {
    if (gl->user && match(gl->user->content, np->realname->name->content) != 0)
      return 0;

    return 1;
  }

  if (gl->nick && match(gl->nick->content, np->nick) != 0)
    return 0;

  if (gl->user && match(gl->user->content, np->ident) != 0)
    return 0;

  if (gl->flags & GLINE_IPMASK) {
    if (!ipmask_check(&gl->ip, &np->p_ipaddr, gl->bits))
      return 0;
  } else {
    if (gl->host && match(gl->host->content, np->host->name->content) != 0)
      return 0;
  }

  return 1;
}

int gline_match_channel(gline *gl, channel *cp) {
  if (!(gl->flags & GLINE_BADCHAN))
    return 0;

  if (match(gl->user->content, cp->index->name->content) != 0)
    return 0;

  return 1;
}

int gline_count_hits(gline *gl) {
  int i, count;
  chanindex *cip;
  channel *cp;
  nick *np;

  count = 0;

  if (gl->flags & GLINE_BADCHAN) {
    for (i = 0; i<CHANNELHASHSIZE; i++) {
      for (cip = chantable[i]; cip; cip = cip->next) {
        cp = cip->channel;

        if (!cp)
          continue;

        if (gline_match_channel(gl, cp))
          count++;
      }
    }
  } else {
    for (i = 0; i < NICKHASHSIZE; i++) {
      for (np = nicktable[i]; np; np = np->next) {
        if (gline_match_nick(gl, np))
          count++;
      }
    }
  }

  return count;
}

int glinesetmask(const char *mask, int duration, const char *reason, const char *creator) {
  gline *gl;
  time_t now = getnettime();

  gl = findgline(mask);

  if (!gl) {
    /* new gline */
    gl = gline_add(creator, mask, reason, now + duration, now, now + duration);

    if (gl)
      gline_propagate(gl);
  } else {
    /* existing gline
     * in 1.4, can update expire, reason etc
     * in 1.3 can only activate an existing inactive gline */
    if (!(gl->flags & GLINE_ACTIVE))
      gline_activate(gl, 0, 1);
  }

  return 1;
}

int glineunsetmask(const char *mask) {
  gline *gl = findgline(mask);

  if (gl && (gl->flags & GLINE_ACTIVE)) {
    controlwall(NO_OPER, NL_GLINES, "Deactivating G-Lline on '%s'.", mask);
    gline_deactivate(gl, 0, 1);
    return 1;
  }

  return 0;
}

static void glinesetmask_cb(const char *mask, int hits, void *arg) {
  gline_params *gp = arg;
  glinesetmask(mask, gp->duration, gp->reason, gp->creator);
}

int glinesuggestbyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int flags, gline_callback callback, void *uarg) {
  trusthost *th, *oth;
  char mask[512];
  int hits;
  unsigned char nodebits;
  gline *gl;

  nodebits = getnodebits(ip);

  if (!(flags & GLINE_IGNORE_TRUST)) {
    th = th_getbyhost(ip);

    if (th && (th->group->flags & TRUST_RELIABLE_USERNAME)) { /* Trust with reliable usernames */
      hits = 0;

      for(oth = th->group->hosts; oth; oth = oth->next)
        hits += glinesuggestbyip(user, &oth->ip, oth->bits, flags | GLINE_ALWAYS_USER | GLINE_IGNORE_TRUST, callback, uarg);

      return hits;
    }
  }

  if (!(flags & GLINE_ALWAYS_USER))
    user = "*";

  /* Widen gline to match the node mask. */
  if (nodebits < bits)
    bits = nodebits;

  snprintf(mask, sizeof(mask), "%s@%s", user, trusts_cidr2str(ip, bits));

  gl = makegline(mask);

  hits = 0;

  if (gl) {
    hits = gline_count_hits(gl);

    if (!(flags & GLINE_SIMULATE))
      callback(mask, hits, uarg);

    freegline(gl);
  }

  return hits;
}

int glinebyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int duration, const char *reason, int flags, const char *creator) {
  gline_params gp;

  if (!(flags & GLINE_SIMULATE)) {
    int hits = glinesuggestbyip(user, ip, bits, flags | GLINE_SIMULATE, NULL, NULL);
    if (hits > MAXGLINEUSERS) {
      controlwall(NO_OPER, NL_GLINES, "Suggested G-Line(s) for '%s@%s' lasting %s with reason '%s' would hit %d users (limit: %d) - NOT SET.",
        user, trusts_cidr2str(ip, bits), longtoduration(duration, 0), reason, hits, MAXGLINEUSERS);
      return 0;
    }
  }

  gp.duration = duration;
  gp.reason = reason;
  gp.creator = creator;
  return glinesuggestbyip(user, ip, bits, flags, glinesetmask_cb, &gp);
}

int glinesuggestbynick(nick *np, int flags, void (*callback)(const char *, int, void *), void *uarg) {
  if (flags & GLINE_ALWAYS_NICK) {
    if (!(flags & GLINE_SIMULATE)) {
      char mask[512];
      snprintf(mask, sizeof(mask), "%s!*@*", np->nick);
      callback(mask, 1, uarg);
    }

    return 1;
  } else {
    return glinesuggestbyip(np->ident, &np->p_ipaddr, 128, flags, callback, uarg);
  }
}

int glinebynick(nick *np, int duration, const char *reason, int flags, const char *creator) {
  gline_params gp;

  if (!(flags & GLINE_SIMULATE)) {
    int hits = glinesuggestbyip(np->ident, &np->p_ipaddr, 128, flags | GLINE_SIMULATE, NULL, NULL);
    if (hits > MAXGLINEUSERS) {
      controlwall(NO_OPER, NL_GLINES, "Suggested G-Line(s) for nick '%s!%s@%s' lasting %s with reason '%s' would hit %d users (limit: %d) - NOT SET.",
        np->nick, np->ident, IPtostr(np->p_ipaddr), longtoduration(duration, 0), reason, hits, MAXGLINEUSERS);
      return 0;
    }
  }

  gp.duration = duration;
  gp.reason = reason;
  gp.creator = creator;
  return glinesuggestbynick(np, flags, glinesetmask_cb, &gp);
}

gline *gline_add(const char *creator, const char *mask, const char *reason, time_t expire, time_t lastmod, time_t lifetime) {
  gline *gl;

  gl = makegline(mask); /* sets up nick,user,host,node and flags */

  if (!gl) {
    /* couldn't process gline mask */
    Error("gline", ERR_WARNING, "Tried to add malformed G-Line %s!", mask);
    return 0;
  }

  gl->creator = getsstring(creator, 255);

  /* it's not unreasonable to assume gline is active, if we're adding a deactivated gline, we can remove this later */
  gl->flags |= GLINE_ACTIVE;

  gl->reason = getsstring(reason, 255);
  gl->expire = expire;
  gl->lastmod = lastmod;
  gl->lifetime = lifetime;

  gl->next = glinelist;
  glinelist = gl;

  return gl;
}

gline *makegline(const char *mask) {
  /* populate gl-> user,host,node,nick and set appropriate flags */
  gline *gl;
  char nick[512], user[512], host[512];
  const char *pnick = NULL, *puser = NULL, *phost = NULL;

  gl = newgline();

  if (!gl) {
    Error("gline", ERR_ERROR, "Failed to allocate new gline");
    return NULL;
  }

  if (*mask == '#' || *mask == '&') {
    gl->flags |= GLINE_BADCHAN;
    gl->user = getsstring(mask, CHANNELLEN);
    return gl;
  }

  if (*mask == '$') {
    if (mask[1] != 'R') {
      freegline(gl);
      return NULL;
    }

    gl->flags |= GLINE_REALNAME;
    gl->user = getsstring(mask + 2, REALLEN);
    return gl;
  }

  if (sscanf(mask, "%[^!]!%[^@]@%s", nick, user, host) == 3) {
    pnick = nick;
    puser = user;
    phost = host;
  } else if (sscanf(mask, "%[^@]@%s", user, host) == 2) {
    puser = user;
    phost = host;
  } else {
    phost = mask;
  }

  /* validate length of the mask components */
  if ((pnick && (pnick[0] == '\0' || strlen(pnick) > NICKLEN)) ||
      (puser && (puser[0] == '\0' || strlen(puser) > USERLEN)) ||
      (phost && (phost[0] == '\0' || strlen(phost) > HOSTLEN))) {
    freegline(gl);
    return NULL;
  }

  /* ! and @ are not allowed in the mask components */
  if ((pnick && (strchr(pnick, '!') || strchr(pnick, '@'))) ||
      (puser && (strchr(puser, '!') || strchr(puser, '@'))) ||
      (phost && (strchr(phost, '!') || strchr(phost, '@')))) {
    freegline(gl);
    return NULL;
  }

  if (phost && ipmask_parse(phost, &gl->ip, &gl->bits))
    gl->flags |= GLINE_IPMASK;
  else
    gl->flags |= GLINE_HOSTMASK;

  if (pnick && strcmp(pnick, "*") != 0)
    gl->nick = getsstring(pnick, NICKLEN);

  if (puser && strcmp(puser, "*") != 0)
    gl->user = getsstring(puser, USERLEN);

  if (phost && strcmp(phost, "*") != 0)
    gl->host = getsstring(phost, HOSTLEN);

  return gl;
}

gline *findgline(const char *mask) {
  gline *gl, *next;
  gline *globalgline;
  time_t curtime = time(0);

  globalgline = makegline(mask);

  if (!globalgline)
    return NULL; /* gline mask couldn't be processed */

 for (gl = glinelist; gl; gl = next) {
    next = gl->next;

    if (gl->lifetime <= curtime) {
      removegline(gl);
      continue;
    } else if (gl->expire <= curtime) {
      gl->flags &= ~GLINE_ACTIVE;
    }

    if (glineequal(globalgline, gl)) {
      freegline(globalgline);
      return gl;
    }
  }

  freegline(globalgline);
  return 0;
}

char *glinetostring(gline *gl) {
  static char mask[512]; /* check */

  if (gl->flags & GLINE_REALNAME) {
   if (gl->user)
     snprintf(mask, sizeof(mask), "$R%s", gl->user->content);
   else
     strncpy(mask, "$R*", sizeof(mask));

   return mask;
  }

  if (gl->flags & GLINE_BADCHAN) {
    assert(gl->user);

    strncpy(mask, gl->user->content, sizeof(mask));
    return mask;
  }

  snprintf(mask, sizeof(mask), "%s!%s@%s",
    gl->nick ? gl->nick->content : "*",
    gl->user ? gl->user->content : "*",
    gl->host ? gl->host->content : "*");

  return mask;
}

gline *gline_activate(gline *agline, time_t lastmod, int propagate) {
  time_t now = getnettime();
  agline->flags |= GLINE_ACTIVE;

  if (lastmod) {
    agline->lastmod = lastmod;
  } else {
    if (now <= agline->lastmod)
      agline->lastmod++;
    else
      agline->lastmod = now;
  }

  if (propagate)
    gline_propagate(agline);

  return agline;
}

gline *gline_deactivate(gline *agline, time_t lastmod, int propagate) {
  time_t now = getnettime();
  agline->flags &= ~GLINE_ACTIVE;

  if (lastmod) {
    agline->lastmod = lastmod;
  } else {
    if (now <= agline->lastmod)
      agline->lastmod++;
    else
      agline->lastmod = now;
  }

  if (propagate)
    gline_propagate(agline);

  return agline;
}

void gline_propagate(gline *agline) {
  if (agline->flags & GLINE_ACTIVE) {
    controlwall(NO_OPER, NL_GLINES, "Activating G-Line on '%s' lasting %s with reason '%s', created by: %s",
      glinetostring(agline), longtoduration(agline->expire-getnettime(), 0),
      agline->reason->content, agline->creator->content);

#if 0
    irc_send("%s GL * +%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#endif
    controlwall(NO_OPER, NL_GLINES, "%s GL * +%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
  } else {
    controlwall(NO_OPER, NL_GLINES, "Deactivating G-Line on '%s'",
      glinetostring(agline));

#if 0
    irc_send("%s GL * -%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#endif
    controlwall(NO_OPER, NL_GLINES, "%s GL * -%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
  }
}

/* returns non-zero if the glines are exactly the same */
int glineequal(gline *gla, gline *glb) {
  if ((gla->flags & GLINE_BADCHAN) != (glb->flags & GLINE_BADCHAN))
    return 0;

  if ((gla->flags & GLINE_REALNAME) != (glb->flags & GLINE_REALNAME))
    return 0;

  if ((gla->flags & GLINE_IPMASK) != (glb->flags & GLINE_IPMASK))
    return 0;

  if ((!gla->nick && glb->nick) || (gla->nick && !glb->nick))
    return 0;

  if (gla->nick && ircd_strcmp(gla->nick->content, glb->nick->content) != 0)
    return 0;

  if ((!gla->user && glb->user) || (gla->user && !glb->user))
    return 0;

  if (gla->user && ircd_strcmp(gla->user->content, glb->user->content) != 0)
    return 0;

  if (gla->flags & GLINE_IPMASK) {
    if (gla->bits != glb->bits)
      return 0;

    if (!ipmask_check(&gla->ip, &glb->ip, gla->bits))
      return 0;
  } else {
    if ((!gla->host && glb->host) || (gla->host && !glb->host))
      return 0;

    if (gla->host && ircd_strcmp(gla->host->content, glb->host->content) != 0)
      return 0;
  }

  return 1;
}

/* returns non-zero on match */
int gline_match_mask(gline *gla, gline *glb) {
  if ((gla->flags & GLINE_BADCHAN) != (glb->flags & GLINE_BADCHAN))
    return 0;

  if ((gla->flags & GLINE_REALNAME) != (glb->flags & GLINE_REALNAME))
    return 0;

  if ((gla->flags & GLINE_IPMASK) != (glb->flags & GLINE_IPMASK))
    return 0;

  if (gla->nick && !glb->nick)
    return 0;

  if (gla->nick && glb->nick && match(gla->nick->content, glb->nick->content) != 0)
    return 0;

  if (gla->user && !glb->user)
    return 0;

  if (gla->user && glb->user && match(gla->user->content, glb->user->content) != 0)
    return 0;

  if (gla->flags & GLINE_IPMASK) {
    if (gla->bits > glb->bits)
      return 0;

    if (!ipmask_check(&gla->ip, &glb->ip, gla->bits))
      return 0;
  } else {
    if (gla->host && !glb->host)
      return 0;

    if (gla->host && glb->host && match(gla->host->content, glb->host->content) != 0)
      return 0;
  }

  return 1;
}

