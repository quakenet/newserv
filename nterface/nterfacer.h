/*
  nterfacer
  Copyright (C) 2004 Chris Porter.
*/

#ifndef __nterfacer_H
#define __nterfacer_H

#include <netinet/in.h>

#include "esockets.h"
#include "library.h"

#define BF_OK      0x00
#define BF_OVER    0xFF

#define SS_IDLE           0x00
#define SS_VERSIONED      0x01
#define SS_AUTHENTICATED  0x02

#define NTERFACER_PORT    2438

#define MAX_ARGS 100

#define PROTOCOL_VERSION "1"
#define ANTI_FULL_VERSION "service_link " PROTOCOL_VERSION

typedef struct handler {
  sstring *command;
  int args; /* MINIMUM ARGUMENTS */
  void *function;
  struct handler *next;
} handler;

typedef struct service_node {
  sstring *name;
  struct handler *handlers;
  struct service_node *next;
} service_node;

typedef struct rline {
  struct handler *handler;
  int id;
  struct service_node *service;
  char buf[MAX_BUFSIZE];
  char *curpos;
  struct rline *next;
  void *tag;
  struct esocket *socket;
} rline;

typedef struct permitted {
  sstring *hostname;
  sstring *password;
  in_addr_t ihost;
} permitted;

typedef struct sconnect {
  int status;
  char response[20 * 2 + 1]; /* could store this in binary form but I really can't be assed */
  struct permitted *permit;
  unsigned char ournonce[NONCE_LEN];
} sconnect;

extern struct nterface_auto_log *nrl;
  
int accept_fd = -1;
struct permitted *permits;
int permit_count = 0;

typedef int (*handler_function)(struct rline *ri, int argc, char **argv);

struct service_node *register_service(char *name);
struct handler *register_handler(struct service_node *service, char *command, int args, handler_function fp);
void deregister_service(struct service_node *service);
int respond(struct rline *li, int argc, ...);
int error_respond(struct rline *li, int error_code, char *format, ...);

int ri_append(struct rline *li, char *format, ...);
int ri_error(struct rline *li, int error_code, char *format, ...);
int ri_final(struct rline *li);

int load_permits(void);
int setup_listening_socket(void);

void nterfacer_accept_event(struct esocket *socket);
void nterfacer_disconnect_event(struct esocket *socket);
int nterfacer_line_event(struct esocket *socket, char *newline);
int nterfacer_new_rline(char *line, struct esocket *socket, int *number);
struct sconnect *find_sconnect_from_fd(int fd);

#endif

