#include "../irc/irc.h"

#include "../trusts/trusts.h"
#include "glines.h"

void glinebynick(nick *np, int duration, const char *reason) {
  glinebyhost(np->ident, IPtostr(np->p_ipaddr), duration, reason);
}

void glinebyhost(const char *ident, const char *hostname, int duration, const char *reason) {
  struct irc_in_addr ip;
  unsigned char bits;
  trusthost *th;

  if(ipmask_parse(hostname, &ip, &bits)) {
    th = th_getbyhost(&ip);

    if(th && th->group->mode) {
      trustgline(th->group, ident, duration, reason);
      return;
    }
  }

  irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
}

void unglinebyhost(const char *ident, const char *hostname, int duration, const char *reason) {
  struct irc_in_addr ip;
  unsigned char bits;
  trusthost *th;

  if(ipmask_parse(hostname, &ip, &bits)) {
    th = th_getbyhost(&ip);

    if(th && th->group->mode) {
      trustungline(th->group, ident, duration, reason);
      return;
    }
  }

  irc_send("%s GL * -%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
}
