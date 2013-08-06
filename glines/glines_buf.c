#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../lib/array.h"
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../control/control.h"
#include "../trusts/trusts.h"
#include "glines.h"

static int nextglinebufid = 1;
glinebuf *glinebuflog[MAXGLINELOG] = {};
int glinebuflogoffset = 0;

void glinebufinit(glinebuf *gbuf, int id) {
  gbuf->id = id;
  gbuf->comment = NULL;
  gbuf->commit = 0;
  gbuf->amend = 0;
  gbuf->glines = NULL;
  gbuf->hitsvalid = 1;
  gbuf->userhits = 0;
  gbuf->channelhits = 0;
  array_init(&gbuf->hits, sizeof(sstring *));
}

gline *glinebufadd(glinebuf *gbuf, const char *mask, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime) {
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

  gl->next = gbuf->glines;
  gbuf->glines = gl;

  gbuf->hitsvalid = 0;

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

  snprintf(mask, sizeof(mask), "%s@%s", user, CIDRtostr(*ip, bits));

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

void glinebufaddbywhowas(glinebuf *gbuf, whowas *ww, int flags, const char *creator, const char *reason, time_t expire, time_t lastmod, time_t lifetime) {
  nick *np = ww->nick;

  if (flags & GLINE_ALWAYS_NICK) {
    char mask[512];
    snprintf(mask, sizeof(mask), "%s!*@*", np->nick);
    glinebufadd(gbuf, mask, creator, reason, expire, lastmod, lifetime);
  } else {
    glinebufaddbyip(gbuf, np->ident, &np->p_ipaddr, 128, flags, creator, reason, expire, lastmod, lifetime);
  }
}

void glinebufcounthits(glinebuf *gbuf, int *users, int *channels) {
  gline *gl;
  int i, hit, slot;
  chanindex *cip;
  channel *cp;
  nick *np;
  char uhmask[512];

#if 0 /* Let's just do a new hit check anyway. */
  if (gbuf->hitsvalid)
    return;
#endif

  gbuf->userhits = 0;
  gbuf->channelhits = 0;

  for (i = 0; i < gbuf->hits.cursi; i++)
    freesstring(((sstring **)gbuf->hits.content)[i]);

  array_free(&gbuf->hits);
  array_init(&gbuf->hits, sizeof(sstring *));

  for (i = 0; i<CHANNELHASHSIZE; i++) {
    for (cip = chantable[i]; cip; cip = cip->next) {
      cp = cip->channel;

      if (!cp)
        continue;

      hit = 0;

      for (gl = gbuf->glines; gl; gl = gl->next) {
        if (gline_match_channel(gl, cp)) {
          hit = 1;
          break;
        }
      }

      if (hit) {
        snprintf(uhmask, sizeof(uhmask), "channel: %s", cip->name->content);

        gbuf->channelhits++;

        slot = array_getfreeslot(&gbuf->hits);
        ((sstring **)gbuf->hits.content)[slot] = getsstring(uhmask, 512);
      }
    }
  }

  for (i = 0; i < NICKHASHSIZE; i++) {
    for (np = nicktable[i]; np; np = np->next) {
      hit = 0;

      for (gl = gbuf->glines; gl; gl = gl->next) {
        if (gline_match_nick(gl, np)) {
          hit = 1;
          break;
        }
      }

      if (hit) {
        snprintf(uhmask, sizeof(uhmask), "user: %s!%s@%s%s%s r(%s)", np->nick, np->ident, np->host->name->content,
          (np->auth) ? "/" : "", (np->auth) ? np->authname : "", np->realname->name->content);

        gbuf->userhits++;

        slot = array_getfreeslot(&gbuf->hits);
        ((sstring **)gbuf->hits.content)[slot] = getsstring(uhmask, 512);
      }
    }
  }

  gbuf->hitsvalid = 1;  

  if (users)
    *users = gbuf->userhits;
  
  if (channels)
    *channels = gbuf->channelhits;
}

int glinebufchecksane(glinebuf *gbuf, nick *spewto, int overridesanity, int overridelimit) {
  gline *gl;
  int users, channels, good;
  const char *hint;

  glinebufcounthits(gbuf, &users, &channels);

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
    for (gl = gbuf->glines; gl; gl = gl->next) {
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
  int i;
  char timebuf[30], lastmod[30];

  if (!gbuf->hitsvalid)
    glinebufcounthits(gbuf, NULL, NULL);

  if (gbuf->id == 0)
    controlreply(spewto, "Uncommitted G-Line transaction.");
  else
    controlreply(spewto, "G-Line transaction ID %d", gbuf->id);

  controlreply(spewto, "Comment: %s", (gbuf->comment) ? gbuf->comment->content : "(no comment)");

  if (gbuf->commit) {
    strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M:%S", localtime(&gbuf->commit));
    controlreply(spewto, "Committed at: %s", timebuf);
  }

  if (gbuf->amend) {
    strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M:%S", localtime(&gbuf->amend));
    controlreply(spewto, "Amended at: %s", timebuf);
  }

  controlreply(spewto, "Mask                                     Expiry               Last modified        Creator                   Reason");
  
  for (gl = gbuf->glines; gl; gl = gl->next) {
    strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M:%S", localtime(&gl->expire));
 
    if (gl->lastmod == 0)
      strncpy(lastmod, "<ulined>", sizeof(lastmod));
    else
      strftime(lastmod, sizeof(lastmod), "%d/%m/%y %H:%M:%S", localtime(&gl->lastmod));

    controlreply(spewto, "%-40s %-20s %-20s %-25s %s", glinetostring(gl), timebuf, lastmod, gl->creator->content, gl->reason ? gl->reason->content : "");
  }

  controlreply(spewto, "Hits");

  for (i = 0; i < gbuf->hits.cursi; i++) {
    controlreply(spewto, "%s", ((sstring **)gbuf->hits.content)[i]->content);

    if (i >= 500) {
      controlreply(spewto, "More than 500 hits, list truncated.");
      break;
    }
  }

  if (i == 0)
    controlreply(spewto, "(no hits)");
}

void glinebufmerge(glinebuf *gbuf) {
  /* TODO: reimplement */

/*
  if (gbuf->merge) {
    /-* Check if an existing gline supercedes this mask *-/
    for (sgl = gbuf->glines; sgl; sgl = sgl->next) {
      if (gline_match_mask(sgl, gl)) {
        freegline(gl);
        return sgl;
      }
    }

    /-* Remove older glines this gline matches *-/
    for (pnext = &gbuf->glines; *pnext; pnext = &((*pnext)->next)) {
      sgl = *pnext;

      if (gline_match_mask(gl, sgl)) {
        *pnext = sgl->next;
        freegline(sgl);

        if (!*pnext)
          break;
      }
    }
  }
*/
}

int glinebufcommit(glinebuf *gbuf, int propagate) {
  gline *gl, *next, *sgl;
  int users, channels, id;

  /* Sanity check */
  glinebufcounthits(gbuf, &users, &channels);

  if (propagate && (users > MAXGLINEUSERHITS || channels > MAXGLINECHANNELHITS)) {
    controlwall(NO_OPER, NL_GLINES_AUTO, "G-Line buffer would hit %d users/%d channels. Not setting G-Lines.");
    glinebufabort(gbuf);
    return 0;
  }

  /* Record the commit time */
  time(&gbuf->commit);

  id = glinebufwritelog(gbuf, propagate);

  /* Move glines to the global gline list */
  for (gl = gbuf->glines; gl; gl = next) {
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

#if SNIRCD_VERSION >= 140
      sgl->expire = gl->expire;

      if (gl->lifetime > sgl->lifetime) 
        sgl->lifetime = gl->lifetime;

      freesstring(sgl->reason);
      sgl->reason = getsstring(gl->reason, 512);
#endif

      freegline(gl);
      gl = sgl;
    } else {
      gl->next = glinelist;
      glinelist = gl;
    }

    gl->glinebufid = id;

    if (propagate)
      gline_propagate(gl);
  }

  /* We've moved all glines to the global gline list. Clear glines link in the glinebuf. */  
  gbuf->glines = NULL;

  glinebufabort(gbuf);

  return id;
}

void glinebufabort(glinebuf *gbuf) {
  gline *gl, *next;
  int i;

  for (gl = gbuf->glines; gl; gl = next) {
    next = gl->next;

    freegline(gl);
  }
  
  freesstring(gbuf->comment);

  for (i = 0; i < gbuf->hits.cursi; i++)
    freesstring(((sstring **)gbuf->hits.content)[i]);

  array_free(&gbuf->hits);
}

int glinebufundo(int id) {
  glinebuf *gbl;
  gline *gl, *sgl;
  int i;

  for (i = 0; i < MAXGLINELOG; i++) {
    gbl = glinebuflog[i];
    
    if (!gbl || gbl->id != id)
      continue;

    for (gl = gbl->glines; gl; gl = gl->next) {
      sgl = findgline(glinetostring(gl));
      
      if (!sgl)
        continue;

      sgl->glinebufid = 0;

      gline_deactivate(sgl, 0, 1);
    }
    
    glinebufabort(gbl);
    glinebuflog[i] = NULL;

    return 1;
  }
  
  return 0;
}

void glinebufcommentf(glinebuf *gbuf, const char *format, ...) {
  char comment[512];
  va_list va;

  va_start(va, format);
  vsnprintf(comment, 511, format, va);
  comment[511] = '\0';
  va_end(va);
  
  gbuf->comment = getsstring(comment, 512);
}

void glinebufcommentv(glinebuf *gbuf, const char *prefix, int cargc, char **cargv) {
  char comment[512];
  int i;

  if (prefix)
    strncpy(comment, prefix, sizeof(comment));
  else
    comment[0] = '\0';
  
  for (i = 0; i < cargc; i++) {
    if (comment[0])
      strncat(comment, " ", sizeof(comment));

    strncat(comment, cargv[i], sizeof(comment));
  }
  
  comment[511] = '\0';
  
  gbuf->comment = getsstring(comment, 512);
}

int glinebufwritelog(glinebuf *gbuf, int propagating) {
  int i, slot;
  gline *gl, *sgl;
  glinebuf *gbl;

  if (!gbuf->glines)
    return 0; /* Don't waste log IDs on empty gline buffers */

  gbl = NULL;

  /* Find an existing log buffer with the same id */
  if (gbuf->id != 0) {
    for (i = 0; i < MAXGLINELOG; i++) {
      if (!glinebuflog[i])
        continue;

      if (glinebuflog[i]->id == gbuf->id) {
        gbl = glinebuflog[i];
        gbl->amend = gbuf->commit;

        /* We're going to re-insert this glinebuf, so remove it for now */
        glinebuflog[i] = NULL;

        break;
      }
    }
  }

  /* Find a recent glinebuf that's a close match */
  if (!gbl && !propagating) {
    for (i = 0; i < MAXGLINELOG; i++) {
      if (!glinebuflog[i])
        continue;

      if (glinebuflog[i]->commit < getnettime() - 5 && glinebuflog[i]->amend < getnettime() - 5)
        continue;

      assert(glinebuflog[i]->glines);

      if (strcmp(glinebuflog[i]->glines->creator->content, gbuf->glines->creator->content) != 0)
        continue;

      gbl = glinebuflog[i];
      gbl->amend = gbuf->commit;

      /* We're going to re-insert this glinebuf, so remove it for now */
      glinebuflog[i] = NULL;

      break;
    }
  }

  /* Make a new glinebuf for the log */
  if (!gbl && gbuf->id == 0) {
    gbl = malloc(sizeof(glinebuf));
    glinebufinit(gbl, (gbuf->id == 0) ? nextglinebufid++ : gbuf->id);

    assert(gbl->hitsvalid);

    if (gbuf->comment)
      glinebufcommentf(gbl, "%s", gbuf->comment->content);
    else if (!propagating)
      glinebufcommentf(gbl, "G-Lines set by %s", gbuf->glines->creator->content);

    gbl->commit = gbuf->commit;
  }

  /* Save a duplicate of the glines in the log buffer */
  for (gl = gbuf->glines; gl; gl = gl->next) {
    sgl = glinedup(gl);
    sgl->next = gbl->glines;
    gbl->glines = sgl;
  }

  gbl->userhits += gbuf->userhits;
  gbl->channelhits += gbuf->channelhits;

  assert(gbuf->userhits + gbuf->channelhits == gbuf->hits.cursi);

  for (i = 0; i < gbuf->hits.cursi; i++) {
    slot = array_getfreeslot(&gbl->hits);
    ((sstring **)gbl->hits.content)[slot] = getsstring(((sstring **)gbuf->hits.content)[i]->content, 512);
  }

  /* Log the transaction */
  glinebuflogoffset++;

  if (glinebuflogoffset >= MAXGLINELOG)
    glinebuflogoffset = 0;

  if (glinebuflog[glinebuflogoffset])
    glinebufabort(glinebuflog[glinebuflogoffset]);

  glinebuflog[glinebuflogoffset] = gbl;

  return gbl->id;
}
