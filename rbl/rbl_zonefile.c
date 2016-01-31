#include <string.h>
#include <stdio.h>
#include "../core/hooks.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../core/config.h"
#include "rbl_zonefile.h"

MODULE_VERSION("");

static int rbl_zf_cmpip(const struct irc_in_addr *a, const struct irc_in_addr *b) {
  for (int i = 0; i < 8; i++) {
    if (ntohs(a->in6_16[i]) < ntohs(b->in6_16[i]))
      return -1;
    else if (ntohs(a->in6_16[i]) > ntohs(b->in6_16[i]))
      return 1;
  }

  return 0;
}

static int rbl_zf_bsearch(array *entries, struct irc_in_addr *ip, char *message, size_t msglen) {
  int first, last, mid;

  first = 0;
  last = entries->cursi - 1;
  mid = (first + last) / 2;

  while (first <= last) {
    rbl_zf_entry *ze = &((rbl_zf_entry *)entries->content)[mid];

    if (rbl_zf_cmpip(&ze->ipaddress, ip) < 0 && !ipmask_check(&ze->ipaddress, ip, ze->bits))
      first = mid + 1;
    else if (ipmask_check(&ze->ipaddress, ip, ze->bits)) {
      if (message) {
        if (ze->message)
          strncpy(message, ze->message->content, msglen);
        else
          message[0] = '\0';
      }

      return 1;
    } else
      last = mid - 1;

    mid = (first + last) / 2;
  }

  return -1;
}

static int rbl_zf_lookup(rbl_instance *rbl, struct irc_in_addr *ip, char *message, size_t msglen) {
  rbl_zf_udata *udata = rbl->udata;

  if (rbl_zf_bsearch(&udata->whitelist, ip, NULL, 0) > 0)
    return -1;

  return rbl_zf_bsearch(&udata->blacklist, ip, message, msglen);
}

static void rbl_zf_freeentries(array *entries) {
  rbl_zf_entry *ze;
  int i;

  for (i = 0; i < entries->cursi; i++) {
    ze = &((rbl_zf_entry *)entries->content)[i];
    freesstring(ze->message);
  }
  array_free(entries);
}

static int rbl_zf_parseentry(rbl_instance *rbl, const char *line) {
  rbl_zf_udata *udata = rbl->udata;
  int slot, exempt = 0;
  char mask[255], message[255];
  struct irc_in_addr ip;
  unsigned char bits;
  rbl_zf_entry *ze;

  if (line[0] == '$' || line[0] == ':')
    return 0; /* Ignore option lines */

  if (line[0] == '!') {
    exempt = 1;
    line++;
  }

  message[0] = '\0';

  if (sscanf(line, "%s %[^\n]", mask, message) < 1)
    return -1;

  if (!ipmask_parse(mask, &ip, &bits))
    return -1;

  if (exempt) {
    slot = array_getfreeslot(&udata->whitelist);
    ze = &((rbl_zf_entry *)udata->whitelist.content)[slot];
  } else {
    slot = array_getfreeslot(&udata->blacklist);
    ze = &((rbl_zf_entry *)udata->blacklist.content)[slot];
  }

  memcpy(&ze->ipaddress, &ip, sizeof(ip));
  ze->bits = bits;
  ze->exempt = exempt;
  ze->message = getsstring(message, 255);

  return 0;
}

static int rbl_zf_cmpentry(const void *a, const void *b) {
  const rbl_zf_entry *za = a;
  const rbl_zf_entry *zb = b;

  return rbl_zf_cmpip(&za->ipaddress, &zb->ipaddress);
}

static int rbl_zf_refresh(rbl_instance *rbl) {
  char line[512];
  rbl_zf_udata *udata = rbl->udata;
  FILE *fp = fopen(udata->file->content, "r");

  if (!fp)
    return -1;

  rbl_zf_freeentries(&udata->whitelist);
  array_init(&udata->whitelist, sizeof(rbl_zf_entry));

  rbl_zf_freeentries(&udata->blacklist);
  array_init(&udata->blacklist, sizeof(rbl_zf_entry));

  while (!feof(fp)) {
    if (fgets(line, sizeof(line), fp) == NULL)
      break;

    if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

    if (line[strlen(line) - 1] == '\r')
      line[strlen(line) - 1] = '\0';

    if (line[0] != '\0')
      rbl_zf_parseentry(rbl, line);
  }

  fclose(fp);

  qsort(udata->whitelist.content, udata->whitelist.cursi, sizeof(rbl_zf_entry), rbl_zf_cmpentry);
  qsort(udata->blacklist.content, udata->blacklist.cursi, sizeof(rbl_zf_entry), rbl_zf_cmpentry);

  return 0;
}

static void rbl_zf_dtor(rbl_instance *rbl) {
  rbl_zf_udata *udata = rbl->udata;
  freesstring(udata->file);
  rbl_zf_freeentries(&udata->whitelist);
  rbl_zf_freeentries(&udata->blacklist);
}

static rbl_ops rbl_zonefile_ops = {
  .lookup = rbl_zf_lookup,
  .refresh = rbl_zf_refresh,
  .dtor = rbl_zf_dtor
};

int rbl_zf_load(const char *name, const char *file) {
  rbl_zf_udata *udata = malloc(sizeof(*udata));
  udata->file = getsstring(file, 512);
  array_init(&udata->whitelist, sizeof(rbl_zf_entry));
  array_init(&udata->blacklist, sizeof(rbl_zf_entry));
  return registerrbl(name, &rbl_zonefile_ops, udata);
}

void _init(void) {
  array *zfiles;

  zfiles = getconfigitems("rbl_zonefile", "zone");
  if (!zfiles) {
    Error("rbl_zonefile", ERR_INFO, "No zonefiles added.");
  } else {
    sstring **files = (sstring **)(zfiles->content);
    int i;
    for(i=0;i<zfiles->cursi;i++) {
      char line[512];
      char *pos;

      strncpy(line, files[i]->content, sizeof(line));

      pos = strchr(line, ',');

      if(!pos) {
        Error("rbl_zonefile", ERR_INFO, "RBL zonefile line is missing zone path: %s", line);
        continue;
      }

      *pos = '\0';

      if (rbl_zf_load(line, pos + 1) < 0)
        Error("rbl_zonefile", ERR_WARNING, "Failed to load zonefile: %s", pos + 1);
    }
  }
}

void _fini(void) {
  deregisterrblbyops(&rbl_zonefile_ops);
}

