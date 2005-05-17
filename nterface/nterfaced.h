/*
  nterfaced
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterfaced_H
#define __nterfaced_H

#include "../lib/sstring.h"
#include "transports.h"

typedef struct service {
  sstring *service;
  sstring *realname;
  sstring *transport_name;
  struct transport *transport;
  int tag;
} service;

extern int service_count;
extern struct service *services;
extern struct nterface_auto_log *ndl;

int load_config_file(void);

#endif
