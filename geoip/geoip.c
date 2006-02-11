/*
  Geoip module
  Copyright (C) 2004-2006 Chris Porter.
*/

#include "../nick/nick.h"
#include "../core/error.h"
#include "../core/config.h"
#include "../core/hooks.h"
#include "../control/control.h"

#include "geoip.h"

int geoip_totals[COUNTRY_MAX + 1];
int geoip_nickext = -1;
GeoIP *gi = NULL;

void geoip_setupuser(nick *np);

void geoip_new_nick(int hook, void *args);
void geoip_quit(int hook, void *args);
void geoip_whois_handler(int hooknum, void *arg);

void _init(void) {
  int i;
  nick *np;
  sstring *filename;

  filename = getcopyconfigitem("geoip", "db", "GeoIP.dat", 256);
  gi = GeoIP_new(GEOIP_MEMORY_CACHE, filename->content);
  freesstring(filename);

  if(!gi)
    return;

  geoip_nickext = registernickext("geoip");
  if(geoip_nickext == -1)
    return; /* PPA: registerchanext produces an error, however the module should stop loading */

  memset(geoip_totals, 0, sizeof(geoip_totals));

  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      geoip_setupuser(np);

  registerhook(HOOK_NICK_LOSTNICK, &geoip_quit);
  registerhook(HOOK_NICK_NEWNICK, &geoip_new_nick);
  registerhook(HOOK_CONTROL_WHOISREQUEST, &geoip_whois_handler);
}

void _fini(void) {
  if(gi)
    GeoIP_delete(gi);

  if(geoip_nickext == -1)
    return;

  releasenickext(geoip_nickext);

  deregisterhook(HOOK_NICK_NEWNICK, &geoip_new_nick);
  deregisterhook(HOOK_NICK_LOSTNICK, &geoip_quit);
  deregisterhook(HOOK_CONTROL_WHOISREQUEST, &geoip_whois_handler);
}

void geoip_setupuser(nick *np) {
  int country = GeoIP_id_by_ipnum(gi, np->ipaddress);
  if((country < COUNTRY_MIN) || (country > COUNTRY_MAX))
    return;

  geoip_totals[country]++;
  np->exts[geoip_nickext] = (void *)(long)country;
}

void geoip_new_nick(int hook, void *args) {
  geoip_setupuser((nick *)args);
}

void geoip_quit(int hook, void *args) {
  int item;
  nick *np = (nick *)args;

  item = (int)((long)np->exts[geoip_nickext]);
  
  if((item < COUNTRY_MIN) || (item > COUNTRY_MAX))
    return;

  geoip_totals[item]--;
}

void geoip_whois_handler(int hooknum, void *arg) {
  int item;
  char message[512], *longcountry, *shortcountry, *shortcountry3;
  nick *np = (nick *)arg;

  if(!np)
    return;

  item = (int)((long)np->exts[geoip_nickext]);
  if((item < COUNTRY_MIN) || (item > COUNTRY_MAX))
    return;

  if(GeoIP_country_info_by_id(item, &shortcountry, &shortcountry3, &longcountry)) {
    snprintf(message, sizeof(message), "Country   : %s (%s)", shortcountry, longcountry);
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);
  }
}

