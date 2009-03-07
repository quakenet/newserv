#include "../irc/irc.h"

#include "../trusts/trusts.h"
#include "glines.h"

void glinebynick(nick *np, int duration, char *reason) {
  irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, istrusted(np)?np->ident:"*", IPtostr(np->p_ipaddr), duration, (intmax_t)getnettime(), reason);
}

void glinebyhost(char *ident, char *hostname, int duration, char *reason) {
  /* TODO: resolve trustgroup and trustgline */

  irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
}

void unglinebyhost(char *ident, char *hostname, int duration, char *reason) {
  /* TODO: trustungline */

  irc_send("%s GL * -%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
}
