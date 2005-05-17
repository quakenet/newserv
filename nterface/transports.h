/*
  nterfaced end to end transport code
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterfaced_transports_H
#define __nterfaced_transports_H

#include "requests.h"

typedef int (*transport_event_line)(struct request *req, char *data);
typedef void (*transport_event_disconnect)(struct request *req);

#define TT_INPUT   0x00
#define TT_OUTPUT  0x01

typedef struct transport {
  sstring *name;
  int type;
  transport_event_line on_line;
  transport_event_disconnect on_disconnect;
  struct transport *next;
} transport;

extern struct transport *transports;

struct transport *register_transport(char *name);
void deregister_transport(struct transport *tp);
void transport_disconnect(struct transport *tp, int tag);

#endif
