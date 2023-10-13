/*
  Geoip module
  Copyright (C) 2004-2023 Chris Porter.
*/

#include "../nick/nick.h"
#include "../core/error.h"
#include "../core/config.h"
#include "../core/hooks.h"
#include "../control/control.h"
#include "../lib/version.h"

#include <string.h>

#include <maxminddb.h>
#include "geoip.h"

MODULE_VERSION("");

#define COUNTRY_MAX (26 * 26 + 1)
#define UNKNOWN_COUNTRY (COUNTRY_MAX - 1)
#define COUNTRY_NAME_LEN 40

struct country {
  int total;
  char code[3];
  char name[COUNTRY_NAME_LEN + 1];
};

static struct country countries[COUNTRY_MAX] = { [UNKNOWN_COUNTRY] = { 0, "??", "Unknown" } };
static struct country *unknown_country = &countries[UNKNOWN_COUNTRY];

static int nickext = -1;
static MMDB_s db;

static struct country *lookup_country(struct sockaddr *addr) {
  int mmdb_error;
  MMDB_lookup_result_s result = MMDB_lookup_sockaddr(&db, addr, &mmdb_error);
  if (mmdb_error != MMDB_SUCCESS || !result.found_entry) {
    return NULL;
  }

  MMDB_entry_data_s entry_data;
  int status = MMDB_get_value(&result.entry, &entry_data, "country", "iso_code", NULL);
  if (status != MMDB_SUCCESS || !entry_data.has_data || entry_data.type != MMDB_DATA_TYPE_UTF8_STRING || entry_data.data_size != 2) {
    return NULL;
  }

  /* not null terminated, sad */
  const char *code_u = entry_data.utf8_string;
  const char code[3] = { code_u[0], code_u[1], '\0' };

  struct country *c = geoip_lookup_code(code);
  if (!c) {
    return NULL;
  }

  if (c->code[0] == '\0') {
    status = MMDB_get_value(&result.entry, &entry_data, "country", "names", "en", NULL);
    if (status != MMDB_SUCCESS || !entry_data.has_data || entry_data.type != MMDB_DATA_TYPE_UTF8_STRING) {
      return NULL;
    }

    /* not null terminated, sad */
    const char *name = entry_data.utf8_string;
    size_t name_len = entry_data.data_size;
    if (name_len > COUNTRY_NAME_LEN) {
      name_len = COUNTRY_NAME_LEN;
    }

    memcpy(c->code, code, 3);
    memcpy(c->name, name, name_len);
    c->name[name_len] = '\0';
  }

  return c;
}

static void nick_setup(nick *np) {
  union {
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } u = {};

  struct irc_in_addr *ip = &np->ipaddress;
  if (irc_in_addr_is_ipv4(ip)) {
    u.sin.sin_family = AF_INET;
    u.sin.sin_addr.s_addr = htonl(irc_in_addr_v4_to_int(ip));
  } else {
    u.sin6.sin6_family = AF_INET6;
    memcpy(&u.sin6.sin6_addr.s6_addr, ip->in6_16, sizeof(ip->in6_16));
  }

  struct country *c = lookup_country((struct sockaddr *)&u);
  if (!c) {
    c = unknown_country;
  }

  c->total++;
  np->exts[nickext] = c;
}

static void nick_new(int hook, void *args) {
  nick_setup(args);
}

static void nick_lost(int hook, void *args) {
  nick *np = args;

  struct country *c = geoip_lookup_nick(np);
  if (!c) {
    return;
  }

  c->total--;
}

static void whois_handler(int hooknum, void *arg) {
  nick *np = arg;
  if (!np) {
    return;
  }

  struct country *c = geoip_lookup_nick(np);
  if (!c) {
    return;
  }

  char buf[512];
  snprintf(buf, sizeof(buf), "Country   : %s (%s)", geoip_code(c), geoip_name(c));
  triggerhook(HOOK_CONTROL_WHOISREPLY, buf);
}

struct country *geoip_lookup_code(const char *code) {
  if (code[0] < 'A' || code[0] > 'Z' || code[1] < 'A' || code[1] > 'Z' || code[2] != '\0') {
    if (!strcmp("??", code)) {
      return unknown_country;
    }

    return NULL;
  }

  return &countries[(code[0] - 'A') * 26 + (code[1] - 'A')];
}

const char *geoip_code(struct country *c) {
  return c->code;
}

const char *geoip_name(struct country *c) {
  return c->name;
}

int geoip_total(struct country *c) {
  return c->total;
}

struct country *geoip_next(struct country *c) {
  int pos;
  if (c == NULL) {
    pos = 0;
  } else {
    pos = c - countries + 1;
  }

  for (; pos < COUNTRY_MAX; pos++) {
    c = &countries[pos];
    if (!c->total) {
      continue;
    }

    return c;
  }

  return NULL;
}

struct country *geoip_lookup_nick(nick *np) {
  if (nickext == -1) {
    return NULL;
  }

  return np->exts[nickext];
}

void _init(void) {
  nickext = registernickext("geoip");
  if (nickext == -1) {
    return;
  }

  sstring *filename = getcopyconfigitem("geoip", "db", "GeoLite2-Country.mmdb", 256);
  int result = MMDB_open(filename->content, MMDB_MODE_MMAP, &db);
  freesstring(filename);
  if (result != MMDB_SUCCESS) {
    Error("geoip", ERR_WARNING, "Unable to load geoip database [filename: %s] [code: %d]", filename->content, result);
    return;
  }

  for (int i = 0; i < NICKHASHSIZE; i++) {
    for (nick *np = nicktable[i]; np; np=np->next) {
      nick_setup(np);
    }
  }

  registerhook(HOOK_NICK_NEWNICK, nick_new);
  registerhook(HOOK_NICK_LOSTNICK, nick_lost);
  registerhook(HOOK_CONTROL_WHOISREQUEST, whois_handler);
}

void _fini(void) {
  if (nickext != -1) {
    releasenickext(nickext);
  }

  MMDB_close(&db);

  deregisterhook(HOOK_NICK_NEWNICK, nick_new);
  deregisterhook(HOOK_NICK_LOSTNICK, nick_lost);
  deregisterhook(HOOK_CONTROL_WHOISREQUEST, whois_handler);
}
