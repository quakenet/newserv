#include <stdio.h>
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../irc/irc.h"
#include "../trusts/trusts.h"
#include "../control/control.h"
#include "glines.h"

MODULE_VERSION("");

static int countmatchingusers(const char *identmask, struct irc_in_addr *ip, unsigned char bits) {
  nick *np;
  int i;
  int count = 0;

  for(i=0;i<NICKHASHSIZE;i++)
    for (np=nicktable[i];np;np=np->next)
      if (ipmask_check(&np->p_nodeaddr, ip, bits) && match2strings(identmask, np->ident))
        count++;

  return count;
}

void glinesetmask(const char *mask, int duration, const char *reason, const char *creator) {
  controlwall(NO_OPER, NL_GLINES, "(NOT) Setting gline on '%s' lasting %s with reason '%s', created by: %s", mask, longtoduration(duration, 0), reason, creator);

#if 0
  irc_send("%s GL * +%s %d %jd :%s", mynumeric->content, mask, duration, (intmax_t)getnettime(), reason);
#endif
}

void glineremovemask(const char *mask) {
  controlwall(NO_OPER, NL_GLINES, "(NOT) Removing gline on '%s'.", mask);

#if 0
  irc_send("%s GL * -%s", mynumeric->content, mask);
#endif
}

static void glinesetmask_cb(const char *mask, int hits, void *arg) {
  gline_params *gp = arg;
  glinesetmask(mask, gp->duration, gp->reason, gp->creator);
}

int glinesuggestbyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int flags, gline_callback callback, void *uarg) {
  trusthost *th, *oth;
  int count;
  unsigned char nodebits;

  nodebits = getnodebits(ip);

  if (!(flags & GLINE_IGNORE_TRUST)) {
    th = th_getbyhost(ip);

    if(th && (th->group->flags & TRUST_ENFORCE_IDENT)) { /* Trust with enforceident enabled */
      count = 0;

      for(oth=th->group->hosts;oth;oth=oth->next)
        count += glinesuggestbyip(user, &oth->ip, oth->bits, flags | GLINE_ALWAYS_USER | GLINE_IGNORE_TRUST, callback, uarg);

      return count;
    }
  }

  if (!(flags & GLINE_ALWAYS_USER))
    user = "*";

  /* Widen gline to match the node mask. */
  if(nodebits<bits)
    bits = nodebits;

  count = countmatchingusers(user, ip, bits);

  if (!(flags & GLINE_SIMULATE)) {
    char mask[512];
    snprintf(mask, sizeof(mask), "%s@%s", user, trusts_cidr2str(ip, bits));
    callback(mask, count, uarg);
  }

  return count;
}

int glinebyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int duration, const char *reason, int flags, const char *creator) {
  gline_params gp;

  if(!(flags & GLINE_SIMULATE)) {
    int hits = glinesuggestbyip(user, ip, bits, flags | GLINE_SIMULATE, NULL, NULL);
    if(hits>MAXGLINEUSERS) {
      controlwall(NO_OPER, NL_GLINES, "Suggested gline(s) for '%s@%s' lasting %s with reason '%s' would hit %d users (limit: %d) - NOT SET.",
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
    if(!(flags & GLINE_SIMULATE)) {
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

  if(!(flags & GLINE_SIMULATE)) {
    int hits = glinesuggestbyip(np->ident, &np->p_ipaddr, 128, flags | GLINE_SIMULATE, NULL, NULL);
    if(hits>MAXGLINEUSERS) {
      controlwall(NO_OPER, NL_GLINES, "Suggested gline(s) for nick '%s!%s@%s' lasting %s with reason '%s' would hit %d users (limit: %d) - NOT SET.",
        np->nick, np->ident, IPtostr(np->p_ipaddr), longtoduration(duration, 0), reason, hits, MAXGLINEUSERS);
      return 0;
    }
  }

  gp.duration = duration;
  gp.reason = reason;
  gp.creator = creator;
  return glinesuggestbynick(np, flags, glinesetmask_cb, &gp);
}
