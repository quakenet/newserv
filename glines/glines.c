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

void gline_activate(gline *agline, time_t lastmod, int propagate) {
  time_t now = getnettime();

  agline->flags |= GLINE_ACTIVE;

  if (lastmod)
    agline->lastmod = lastmod;
  else if (now <= agline->lastmod)
    agline->lastmod++;
  else
    agline->lastmod = now;

  if (propagate)
    gline_propagate(agline);
}

void gline_deactivate(gline *agline, time_t lastmod, int propagate) {
  time_t now = getnettime();

  if (agline->lastmod == 0) {
    Error("gline", ERR_WARNING, "Tried to deactivate gline with lastmod == 0: %s", glinetostring(agline));
    return;
  }

  agline->flags &= ~GLINE_ACTIVE;

  if (lastmod)
    agline->lastmod = lastmod;
  else if (now <= agline->lastmod)
    agline->lastmod++;
  else
    agline->lastmod = now;

  if (propagate)
    gline_propagate(agline);
}

void gline_destroy(gline *agline, time_t lastmod, int propagate) {
  time_t now = getnettime();

  agline->flags &= ~GLINE_ACTIVE;
  agline->flags |= GLINE_DESTROYED;

  if (agline->lastmod == 0) {
    Error("gline", ERR_WARNING, "Tried to destroy gline with lastmod == 0: %s", glinetostring(agline));
    return;
  }

  if (lastmod)
    agline->lastmod = lastmod;
  else if (now <= agline->lastmod)
    agline->lastmod++;
  else
    agline->lastmod = now;

  if (propagate)
    gline_propagate(agline);

  removegline(agline);
}

void gline_propagate(gline *agline) {
  if (agline->flags & GLINE_DESTROYED) {
    controlwall(NO_OPER, NL_GLINES, "Destroying G-Line on '%s' lasting %s with reason '%s', created by: %s",
      glinetostring(agline), longtoduration(agline->expire-getnettime(), 0),
      agline->reason->content, agline->creator->content);

#if 1
    irc_send("%s GL * %%-%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#else
    controlwall(NO_OPER, NL_GLINES, "%s GL * %%-%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#endif
  } else if (agline->flags & GLINE_ACTIVE) {
    controlwall(NO_OPER, NL_GLINES, "Activating G-Line on '%s' lasting %s with reason '%s', created by: %s",
      glinetostring(agline), longtoduration(agline->expire-getnettime(), 0),
      agline->reason->content, agline->creator->content);

#if 1
    irc_send("%s GL * +%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#else
    controlwall(NO_OPER, NL_GLINES, "%s GL * +%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#endif
  } else {
    controlwall(NO_OPER, NL_GLINES, "Deactivating G-Line on '%s'",
      glinetostring(agline));

#if 1
    irc_send("%s GL * -%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#else
    controlwall(NO_OPER, NL_GLINES, "%s GL * -%s %lu %lu :%s\r\n", mynumeric->content,
      glinetostring(agline), agline->expire - getnettime(),
      agline->lastmod, agline->reason->content);
#endif
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

int isglinesane(gline *gl, const char **hint) {
  int wildcard, nowildcardcount;
  char *pos;
  trusthost *th;

  /* Hits all realnames. */
  if ((gl->flags & GLINE_REALNAME) && !gl->user) {
    *hint = "Matches all realnames.";
    return 0;
  }

  /* Hits all local/non-local channels. */
  if ((gl->flags & GLINE_BADCHAN) && (strcmp(gl->user->content, "&*") == 0 || strcmp(gl->user->content, "#*") == 0)) {
    *hint = "Matches all local/non-local channels.";
    return 0;
  }

  /* Skip the other checks for nickname glines. */
  if (gl->nick)
    return 1;

  /* Mask wider than /16 for IPv4 or /32 for IPv6. */
  if ((gl->flags & GLINE_IPMASK) && gl->bits < (irc_in_addr_is_ipv4(&gl->ip) ? (96 + 16) : 32)) {
    *hint = "CIDR mask too wide.";
    return 0;
  }

  /* Doesn't have at least two non-wildcarded host components. */
  if (gl->flags & GLINE_HOSTMASK) {
    nowildcardcount = 0;

    wildcard = 0;
    pos = gl->host->content;

    for (;;) {
      switch (*pos) {
        case '*':
          /* fall through */
        case '?':
          wildcard = 1;
          break;

        case '.':
        case '\0':
          if (!wildcard)
            nowildcardcount++;
          else
            wildcard = 0;

          if (*pos == '\0')
            pos = NULL; /* Leave the loop. */

          break;
      }

      if (pos)
        pos++;
      else
        break;
    }
    
    if (nowildcardcount < 2) {
      *hint = "Needs at least 2 non-wildcarded host components.";
      return 0;
    }
  }

  /* Wildcard username match for trusted host with reliable usernames. */
  if ((gl->flags & GLINE_IPMASK) && (!gl->user || strchr(gl->user->content, '*') || strchr(gl->user->content, '?'))) {
    th = th_getbyhost(&gl->ip);

    if (th && (th->group->flags & TRUST_RELIABLE_USERNAME)) {
      *hint = "Wildcard username match for a trusted host that has reliable usernames.";
      return 0;
    }
  }

  return 1;
}
