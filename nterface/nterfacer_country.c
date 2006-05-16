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

MODULE_VERSION("$Id")

int country_nickext = -1;

struct handler *hl = NULL, *hl2 = NULL;

int handle_countrytotals(struct rline *li, int argc, char **argv);
int handle_countrywhois(struct rline *li, int argc, char **argv);
int GeoIP_country_info_by_id(int id, char **two, char **three, char **full);

void _init(void) {
  if(!n_node) {
    Error("nterfacer_country", ERR_ERROR, "Unable to register country as nterfacer_control isn't loaded!");
    return;
  }

  country_nickext = findnickext("geoip");
  if(country_nickext == -1)
    return; /* PPA: registerchanext produces an error, however the module should stop loading */

  hl = register_handler(n_node, "countrytotals", 0, handle_countrytotals);
  hl2 = register_handler(n_node, "countrywhois", 1, handle_countrywhois);
}

void _fini(void) {
  if(country_nickext == -1)
    return;

  if(hl2)
    deregister_handler(hl2);
  if(hl)
    deregister_handler(hl);
}

int handle_countrytotals(struct rline *li, int argc, char **argv) {
  int i;
  for(i=COUNTRY_MIN;i<=COUNTRY_MAX;i++)
    if(ri_append(li, "%d", geoip_totals[i]) == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow.");

  return ri_final(li);
}

int handle_countrywhois(struct rline *li, int argc, char **argv) {
  int result;
  char *longcountry, *shortcountry, *shortcountry3;
  nick *np = getnickbynick(argv[0]);

  if(!np)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "User not online");

  result = (int)((long)np->exts[country_nickext]);
  if((result < COUNTRY_MIN) || (result > COUNTRY_MAX))
    result = -1;

 if(!GeoIP_country_info_by_id(result, &shortcountry, &shortcountry3, &longcountry))
    result = -1;

  ri_append(li, "%d", result);

  if(result == -1) {
    ri_append(li, "%s", "!!");
    ri_append(li, "%s", "!!");
    ri_append(li, "%s", "Error");
  } else {
    ri_append(li, "%s", shortcountry);
    ri_append(li, "%s", shortcountry3);
    ri_append(li, "%s", longcountry);
  }

  return ri_final(li);
}

