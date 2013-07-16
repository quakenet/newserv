#include <stdio.h>
#include <string.h>
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../control/control.h"
#include "../trusts/trusts.h"
#include "glines.h"

void glinebufinit(glinebuf *gbuf, int merge) {
  gbuf->head = NULL;
  gbuf->merge = merge;
}

gline *glinebufadd(glinebuf *gbuf, const char *mask, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime) {
  gline *gl, *sgl, **pnext;

  gl = makegline(mask); /* sets up nick,user,host,node and flags */

  if (!gl) {
    /* couldn't process gline mask */
    Error("gline", ERR_WARNING, "Tried to add malformed G-Line %s!", mask);
    return 0;
  }

  if (gbuf->merge) {
    /* Check if an existing gline supercedes this mask */
    for (sgl = gbuf->head; sgl; sgl = sgl->next) {
      if (gline_match_mask(sgl, gl)) {
        freegline(gl);
        return sgl;
      }
    }

    /* Remove older glines this gline matches */
    for (pnext = &gbuf->head; *pnext; pnext = &((*pnext)->next)) {
      sgl = *pnext;

      if (gline_match_mask(gl, sgl)) {
        *pnext = sgl->next;
        freegline(sgl);
        
        if (!*pnext)
          break;
      }
    }
  }

  gl->creator = getsstring(creator, 255);

  /* it's not unreasonable to assume gline is active, if we're adding a deactivated gline, we can remove this later */
  gl->flags |= GLINE_ACTIVE;

  gl->reason = getsstring(reason, 255);
  gl->expire = expire;
  gl->lastmod = lastmod;
  gl->lifetime = lifetime;

  gl->next = gbuf->head;
  gbuf->head = gl;

  return gl;
}

void glinebufaddbyip(glinebuf *gbuf, const char *user, struct irc_in_addr *ip, unsigned char bits, int flags, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime) {
  trusthost *th, *oth;
  char mask[512];
  unsigned char nodebits;

  nodebits = getnodebits(ip);

  if (!(flags & GLINE_IGNORE_TRUST)) {
    th = th_getbyhost(ip);

    if (th && (th->group->flags & TRUST_RELIABLE_USERNAME)) { /* Trust with reliable usernames */
      for (oth = th->group->hosts; oth; oth = oth->next)
        glinebufaddbyip(gbuf, user, &oth->ip, oth->bits, flags | GLINE_ALWAYS_USER | GLINE_IGNORE_TRUST, creator, reason, expire, lastmod, lifetime);

      return;
    }
  }

  if (!(flags & GLINE_ALWAYS_USER))
    user = "*";

  /* Widen gline to match the node mask. */
  if (nodebits < bits)
    bits = nodebits;

  snprintf(mask, sizeof(mask), "%s@%s", user, trusts_cidr2str(ip, bits));

  glinebufadd(gbuf, mask, creator, reason, expire, lastmod, lifetime);
}

void glinebufaddbynick(glinebuf *gbuf, nick *np, int flags, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime) {
  if (flags & GLINE_ALWAYS_NICK) {
    char mask[512];
    snprintf(mask, sizeof(mask), "%s!*@*", np->nick);
    glinebufadd(gbuf, mask, creator, reason, expire, lastmod, lifetime);
  } else {
    glinebufaddbyip(gbuf, np->ident, &np->p_ipaddr, 128, flags, creator, reason, expire, lastmod, lifetime);
  }
}

void glinebufcounthits(glinebuf *gbuf, int *users, int *channels, nick *spewto) {
  gline *gl;
  int i, hit;
  chanindex *cip;
  channel *cp;
  nick *np;

  if (users)
    *users = 0;

  if (channels)
    *channels = 0;

  if (channels) {
    for (i = 0; i<CHANNELHASHSIZE; i++) {
      for (cip = chantable[i]; cip; cip = cip->next) {
        cp = cip->channel;

        if (!cp)
          continue;

        hit = 0;

        for (gl = gbuf->head; gl; gl = gl->next) {
          if (gline_match_channel(gl, cp)) {
            hit = 1;
            break;
          }
        }

        if (hit) {
          if (spewto)
            controlreply(spewto, "channel: %s", cip->name->content);

          (*channels)++;
        }
      }
    }
  }

  if (users) {
    for (i = 0; i < NICKHASHSIZE; i++) {
      for (np = nicktable[i]; np; np = np->next) {
        hit = 0;

        for (gl = gbuf->head; gl; gl = gl->next) {
          if (gline_match_nick(gl, np)) {
            hit = 1;
            break;
          }
        }

        if (hit) {
          if (spewto)
            controlreply(spewto, "user: %s!%s@%s r(%s)", np->nick, np->ident, np->host->name->content, np->realname->name->content);

          (*users)++;
        }
      }
    }
  }
}

int glinebufchecksane(glinebuf *gbuf, nick *spewto, int overridesanity, int overridelimit, int spewhits) {
  gline *gl;
  int users, channels, good;
  const char *hint;

  glinebufcounthits(gbuf, &users, &channels, spewhits ? spewto : NULL);

  if (!overridelimit) {
    if (channels > MAXUSERGLINECHANNELHITS) {
      controlreply(spewto, "G-Lines would hit %d channels. Limit is %d. Not setting G-Lines.", channels, MAXUSERGLINECHANNELHITS);
      return 0;
    } else if (users > MAXUSERGLINEUSERHITS) {
      controlreply(spewto, "G-Lines would hit %d users. Limit is %d. Not setting G-Lines.", users, MAXUSERGLINEUSERHITS);
      return 0;
    }
  }

  good = 1;

  if (!overridesanity) {
    /* Remove glines that fail the sanity check */
    for (gl = gbuf->head; gl; gl = gl->next) {
      if (!isglinesane(gl, &hint)) {
        controlreply(spewto, "Sanity check failed for G-Line on '%s' - Skipping: %s",
          glinetostring(gl), hint);
        good = 0;
      }
    }
  }

  return good;
}

void glinebufspew(glinebuf *gbuf, nick *spewto) {
  gline *gl;

  for (gl = gbuf->head; gl; gl = gl->next)
    controlreply(spewto, "mask: %s", glinetostring(gl));
}

void glinebufflush(glinebuf *gbuf, int propagate) {
  gline *gl, *next, *sgl;
  int users, channels;

  /* Sanity check */
  glinebufcounthits(gbuf, &users, &channels, NULL);

  if (propagate && (users > MAXGLINEUSERHITS || channels > MAXGLINECHANNELHITS)) {
    controlwall(NO_OPER, NL_GLINES, "G-Line buffer would hit %d users/%d channels. Not setting G-Lines.");
    glinebufabandon(gbuf);
    return;
  }

  for (gl = gbuf->head; gl; gl = next) {
    next = gl->next;

    sgl = findgline(glinetostring(gl));

    if (sgl) {
      /* existing gline
       * in 1.4, can update expire, reason etc
       * in 1.3 can only activate/deactivate an existing gline */

      if (gl->flags & GLINE_ACTIVE && !(sgl->flags & GLINE_ACTIVE))
        gline_activate(sgl, 0, 0);
      else if (!(gl->flags & GLINE_ACTIVE) && sgl->flags & GLINE_ACTIVE)
        gline_deactivate(sgl, 0, 0);

      freegline(gl);
      gl = sgl;
    } else {
      gl->next = glinelist;
      glinelist = gl;
    }

    if (propagate)
      gline_propagate(gl);
  }
}

void glinebufabandon(glinebuf *gbuf) {
  gline *gl, *next;

  for (gl = gbuf->head; gl; gl = next) {
    next = gl->next;

    freegline(gl);
  }
}
