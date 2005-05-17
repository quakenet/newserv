/*
  nterfaced UNIX domain socket module
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterfaced_uds_H
#define __nterfaced_uds_H

#include "../lib/sstring.h"
#include "transports.h"
#include "esockets.h"

int uds = -1;
sstring *socket_path = NULL;

int create_uds(void);
int uds_buffer_line_event(struct esocket *active, char *newline);
int uds_transport_line_event(struct request *request, char *buf);
void uds_transport_disconnect_event(struct request *req);
void uds_buffer_disconnect_event(struct esocket *active);
void uds_accept(struct esocket *active);
void uds_buffer_accept_event(struct esocket *active);

#endif
