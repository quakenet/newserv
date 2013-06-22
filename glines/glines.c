#include <stdio.h>
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../trusts/trusts.h"
#include "../control/control.h"
#include "glines.h"

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

void glinesetmask(const char *mask, int duration, const char *reason) {
  controlwall(NO_OPER, NL_GLINES, "Would set gline on '%s' lasting %s with reason '%s'. CURRENTLY DISABLED FOR TESTING PURPOSES.", mask, longtoduration(duration, 0), reason);

#if 0
  irc_send("%s GL * +%s %d %jd :%s", mynumeric->content, mask, duration, (intmax_t)getnettime(), reason);
#endif
}

void glineremovemask(const char *mask, int duration, const char *reason) {
  controlwall(NO_OPER, NL_GLINES, "Would remove gline on '%s'. CURRENTLY DISABLED FOR TESTING PURPOSES.", mask);

#if 0
  irc_send("%s GL * -%s %d %jd :%s", mynumeric->content, mask, duration, (intmax_t)getnettime(), reason);
#endif
}

static void glinesetmask_cb(const char *mask, int hits, void *arg) {
  gline_params *gp = arg;

  controlwall(NO_OPER, NL_GLINES, "Would set gline on '%s' lasting %s with reason '%s' (hits: %d). CURRENTLY DISABLED FOR TESTING PURPOSES.", mask, longtoduration(gp->duration, 0), gp->reason, hits);

#if 0
  glinesetmask(mask, gp->duration, gp->reason);
#endif
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

int glinebyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int duration, const char *reason, int flags) {
  gline_params gp;
  gp.duration = duration;
  gp.reason = reason;
  return glinesuggestbyip(user, ip, bits, flags, glinesetmask_cb, &gp);
}

int glinesuggestbynick(nick *np, int flags, void (*callback)(const char *, int, void *), void *uarg) {
  if (flags & GLINE_ALWAYS_NICK) {
    char mask[512];
    snprintf(mask, sizeof(mask), "%s!*@*", np->nick);
    callback(mask, 1, uarg); /* TODO: figure out if user was online. */
    return 1; /* TODO: figure out if user was online. */
  } else {
    return glinesuggestbyip(np->ident, &np->p_ipaddr, 128, flags, callback, uarg);
  }
}

int glinebynick(nick *np, int duration, const char *reason, int flags) {
  gline_params gp;
  gp.duration = duration;
  gp.reason = reason;
  return glinesuggestbynick(np, flags, glinesetmask_cb, &gp);
}
