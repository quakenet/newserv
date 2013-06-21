#include <stdio.h>
#include <../nick/nick.h>
#include "../irc/irc.h"
#include "trusts.h"

int istrusted(nick *np) {
  return gettrusthost(np) != NULL;
}

unsigned char getnodebits(struct irc_in_addr *ip) {
  trusthost *th;

  th = th_getbyhost(ip);

  if(th)
    return th->nodebits;

  if(irc_in_addr_is_ipv4(ip))
    return 128;
  else
    return 64;
}
