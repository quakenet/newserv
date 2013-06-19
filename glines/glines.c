#include "../irc/irc.h"
#include "../trusts/trusts.h"
#include "glines.h"

void glinesetbynick(nick *np, int duration, char *reason, char *creator, int flags) {
  irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, istrusted(np)?np->ident:"*", IPtostr(np->p_ipaddr), duration, (intmax_t)getnettime(), reason);
}

void glinesetbynode( patricia_node_t *node, int duration, char *reason, char *creator) {
  irc_send("%s GL * +*@%s %d %jd :%s", mynumeric->content, IPtostr(node->prefix->sin), duration, (intmax_t)getnettime(), reason);
}


/**
 * This should be avoided (where possible) and gline on Nick/IPs instead
 */
void glinesetbyhost(char *ident, char *hostname, int duration, char *reason, char *creator) {
  irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
}

void unglinebyhost(char *ident, char *hostname, int duration, char *reason) {
  irc_send("%s GL * -%s@%s %d %jd :%s", mynumeric->content, ident, hostname, duration, (intmax_t)getnettime(), reason);
}
