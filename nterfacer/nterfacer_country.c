/*
  nterfacer newserv geoip country module
  Copyright (C) 2004 Chris Porter.
*/

#include "../nick/nick.h"
#include "../core/error.h"
#include "../geoip/geoip.h"
#include "../lib/version.h"

#include "library.h"
#include "nterfacer_control.h"

MODULE_VERSION("");

static struct handler *hl, *hl2;

static int handle_countrytotals(struct rline *li, int argc, char **argv) {
  for(struct country *c = geoip_next(NULL); c; c = geoip_next(c))
    if(ri_append(li, "%s:%d", geoip_code(c), geoip_total(c)) == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow.");

  return ri_final(li);
}

static int handle_countrywhois(struct rline *li, int argc, char **argv) {
  nick *np = getnickbynick(argv[0]);

  if(!np)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "User not online");

  struct country *c = geoip_lookup_nick(np);
  if(!c)
    return ri_error(li, ERR_UNKNOWN_ERROR, "Country lookup for user failed");

  ri_append(li, "%s", geoip_code(c));
  ri_append(li, "%s", geoip_name(c));

  return ri_final(li);
}

void _init(void) {
  if(!n_node) {
    Error("nterfacer_country", ERR_ERROR, "Unable to register country as nterfacer_control isn't loaded!");
    return;
  }

  hl = register_handler(n_node, "countrytotals", 0, handle_countrytotals);
  hl2 = register_handler(n_node, "countrywhois", 1, handle_countrywhois);
}

void _fini(void) {
  if(hl2)
    deregister_handler(hl2);
  if(hl)
    deregister_handler(hl);
}
