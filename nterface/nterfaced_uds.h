/*
  nterfaced UNIX domain socket module
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterfaced_uds_H
#define __nterfaced_uds_H

#include "../lib/sstring.h"
#include "esockets.h"
#include "requests.h"

int create_uds(void);
int uds_buffer_line_event(struct esocket *active, char *newline);
int uds_transport_line(struct request *request, char *buf);
void uds_transport_disconnect_event(struct request *req);
void uds_buffer_disconnect_event(struct esocket *active);
void uds_accept(struct esocket *active);
void uds_buffer_accept_event(struct esocket *active);
void uds_transport_disconnect(struct request *req);
void uds_startup(void);
void uds_shutdown(void);

#endif
