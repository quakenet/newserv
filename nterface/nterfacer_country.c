/*
  nterfacer newserv geoip country module
  Copyright (C) 2004 Chris Porter.
*/

#include "../nick/nick.h"
#include "../core/error.h"
#include "../core/config.h"
#include "../core/hooks.h"

#include "library.h"
#include "nterfacer_control.h"

#include "libGeoIP/GeoIP.h"

struct handler *hl = NULL, *hl2 = NULL;

#define COUNTRY_MIN 0
#define COUNTRY_MAX 246

unsigned int totals[COUNTRY_MAX + 1];
int country_nickext = -1;
GeoIP *gi = NULL;

void country_setupuser(nick *np);
int handle_countrytotals(struct rline *li, int argc, char **argv);
int handle_countrywhois(struct rline *li, int argc, char **argv);

void country_new_nick(int hook, void *args);
void country_quit(int hook, void *args);

void _init(void) {
  int i;
  nick *np;
  sstring *filename;

  if(!n_node) {
    Error("nterfacer_country", ERR_ERROR, "Unable to register country as nterfacer_control isn't loaded!");
    return;
  }

  filename = getcopyconfigitem("nterfacer", "geoipdb", "GeoIP.dat", 256);
  gi = GeoIP_new(GEOIP_MEMORY_CACHE, filename->content);
  freesstring(filename);

  if(!gi)
    return;

  country_nickext = registernickext("nterfacer_country");
  if(country_nickext == -1)
    return; /* PPA: registerchanext produces an error, however the module should stop loading */

  hl = register_handler(n_node, "countrytotals", 0, handle_countrytotals);
  hl2 = register_handler(n_node, "countrywhois", 1, handle_countrywhois);
  memset(totals, 0, sizeof(totals));

  for(i=0;i<NICKHASHSIZE;i++) {
    for(np=nicktable[i];np;np=np->next) {
      country_setupuser(np);
    }
  }

  registerhook(HOOK_NICK_LOSTNICK, &country_quit);
  registerhook(HOOK_NICK_NEWNICK, &country_new_nick);
}

void _fini(void) {
  if(gi)
    GeoIP_delete(gi);

  if(country_nickext == -1)
    return;

  releasenickext(country_nickext);

  deregisterhook(HOOK_NICK_NEWNICK, &country_new_nick);
  deregisterhook(HOOK_NICK_LOSTNICK, &country_quit);

  if(hl2)
    deregister_handler(hl2);
  if(hl)
    deregister_handler(hl);
}

int handle_countrytotals(struct rline *li, int argc, char **argv) {
  int i;
  for(i=COUNTRY_MIN;i<=COUNTRY_MAX;i++)
    if(ri_append(li, "%d", totals[i]) == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow.");

  return ri_final(li);
}

void country_setupuser(nick *np) {
  int country = GeoIP_id_by_ipnum(gi, np->ipaddress);
  if((country < COUNTRY_MIN) || (country > COUNTRY_MAX))
    return;

  totals[country]++;
  np->exts[country_nickext] = (void *)&totals[country];
}

void country_new_nick(int hook, void *args) {
  country_setupuser((nick *)args);
}

void country_quit(int hook, void *args) {
  nick *np = (nick *)args;

  unsigned int *item;
  if((item = (unsigned int *)np->exts[country_nickext]))
    (*item)--;
}

int handle_countrywhois(struct rline *li, int argc, char **argv) {
  int result;
  char *longcountry, *shortcountry, *shortcountry3;
  nick *np = getnickbynick(argv[0]);

  if(!np)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "User not online");

  result = GeoIP_id_by_ipnum(gi, np->ipaddress);
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

