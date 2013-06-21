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

int glinebynick(nick *np, int duration, const char *reason, int flags) {
  return glinebyhost(np->ident, IPtostr(np->p_ipaddr), duration, reason, flags);
}

int glinebyhost(const char *ident, const char *hostname, int duration, const char *reason, int flags) {
  struct irc_in_addr ip;
  unsigned char bits;
  trusthost *th;
  int count;

  if(flags & GLINE_SIMULATE)
    return -1; /* TODO */

  if(!ipmask_parse(hostname, &ip, NULL)) {
    irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
    return 0;
  }

  bits = getnodebits(&ip);

  if (!(flags & GLINE_IGNORE_TRUST)) {
    th = th_getbyhost(&ip);

    if(th && (th->group->flags & TRUST_ENFORCE_IDENT)) { /* Trust with enforceident enabled */
      trustgline(th->group, ident, duration, reason);
      return 0;
    }
  }

  if (!(flags & GLINE_ALWAYS_USER))
    ident = "*";

  count = countmatchingusers(ident, &ip, bits);

  if (!(flags & MAXGLINEUSERS) && count>MAXGLINEUSERS) {
    controlwall(NO_OPER, NL_GLINES, "Attempted to set gline on %s@%s: Would match %d users (limit: %d)- not set.", ident, trusts_cidr2str(&ip, bits), count, MAXGLINEUSERS);
    return -1;
  }

  irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, ident, trusts_cidr2str(&ip, bits), duration, (intmax_t)getnettime(), reason);
  return count;
}

void unglinebyhost(const char *ident, const char *hostname, int duration, const char *reason, int flags) {
  struct irc_in_addr ip;
  unsigned char bits;
  trusthost *th;

  if(!ipmask_parse(hostname, &ip, &bits)) {
    irc_send("%s GL * -%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
    return;
  }

  bits = getnodebits(&ip);

  if (!(flags & GLINE_IGNORE_TRUST)) {
    th = th_getbyhost(&ip);

    if(th && th->group->mode) { /* Trust with enforceident enabled */
      trustgline(th->group, ident, duration, reason);
      return;
    }
  }

  if (!(flags & GLINE_ALWAYS_USER))
    ident = "*";

  irc_send("%s GL * -%s@%s %d %jd :%s", mynumeric->content, ident, trusts_cidr2str(&ip, bits), duration, (intmax_t)getnettime(), reason);
}
