#include <stdio.h>
#include <../nick/nick.h>
#include "../irc/irc.h"
#include "trusts.h"

int istrusted(nick *np) {
  return gettrusthost(np) != NULL;
}

int trustgline(trustgroup *tg, const char *ident, int duration, const char *reason) {
  trusthost *th;
  int count = 0;

  for(th=tg->hosts;th;th=th->next) {
    char *cidrstr = trusts_cidr2str(&th->ip, th->bits);
    irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, ident, cidrstr, duration, (intmax_t)getnettime(), reason);
    count++;
  }

  return count;
}

int trustungline(trustgroup *tg, const char *ident, int duration, const char *reason) {
  trusthost *th;
  int count = 0;

  for(th=tg->hosts;th;th=th->next) {
    char *cidrstr = trusts_cidr2str(&th->ip, th->bits);
    irc_send("%s GL * +%s@%s %d %jd :%s", mynumeric->content, ident, cidrstr, duration, (intmax_t)getnettime(), reason);
    count++;
  }

  return count;
}
