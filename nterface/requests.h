/*
  nterfaced request management
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterfaced_requests_H
#define __nterfaced_requests_H

#include "nterfaced.h"
#include "transports.h"

typedef struct request_transport {
  struct transport *transport;
  int tag;
  int token;
} request_transport;

typedef struct request {
  struct request_transport input;
  struct request_transport output;
  struct service *service;
  struct request *next;
} request;

extern struct request *requests;

int new_request(struct transport *input, int tag, char *line, int *number);
void finish_request(struct transport *output, char *data);
void free_request(struct request *rp);
struct transport *transport_from_service(char *service);
int get_output_token(void);

#endif
