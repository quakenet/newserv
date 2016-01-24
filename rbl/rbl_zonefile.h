#ifndef __RBL_ZONEFILE_H
#define __RBL_ZONEFILE_H

#include "rbl.h"

typedef struct rbl_zf_entry {
  struct irc_in_addr ipaddress;
  unsigned char bits;
  int exempt;
  sstring *message;
} rbl_zf_entry;

typedef struct rbl_zf_udata {
  sstring *file;
  array whitelist;
  array blacklist;
} rbl_zf_udata;

int rbl_zf_load(const char *name, const char *file);

#endif /* __RBL_ZONEFILE_H */
