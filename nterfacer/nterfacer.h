/*
  nterfacer
  Copyright (C) 2004 Chris Porter.
*/

#ifndef __nterfacer_H
#define __nterfacer_H

#include <sys/types.h>
#include <netinet/in.h>

#include "../lib/sstring.h"

#include "esockets.h"
#include "library.h"

#define BF_OK      0x00
#define BF_OVER    0xFF
#define BF_UNLOADED 0xFE

#define SS_IDLE           0x00
#define SS_VERSIONED      0x01
#define SS_AUTHENTICATED  0x02

#define NTERFACER_PORT    2438

#define MAX_ARGS 100

#define PROTOCOL_VERSION "4"
#define ANTI_FULL_VERSION "service_link " PROTOCOL_VERSION

struct rline;

typedef int (*handler_function)(struct rline *ri, int argc, char **argv);
typedef void (*rline_callback)(int failed, int linec, char **linev, void *tag);

typedef struct handler {
  sstring *command;
  int args; /* MINIMUM ARGUMENTS */
  handler_function function;
  struct handler *next;
  void *service;
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
  rline_callback callback;
  struct esocket *socket;
} rline;

typedef struct permitted {
  sstring *hostname;
  sstring *password;
  in_addr_t ihost;
} permitted;

typedef struct sconnect {
  int status;
  char response[32 * 2 + 1], challenge[32 * 2 + 1];
  unsigned char iv[16];
  struct permitted *permit;
  unsigned char ournonce[16];
} sconnect;

extern struct nterface_auto_log *nrl;
  
struct service_node *register_service(char *name);
struct handler *register_handler(struct service_node *service, char *command, int args, handler_function fp);
void deregister_service(struct service_node *service);
void deregister_handler(struct handler *hp);
int respond(struct rline *li, int argc, ...);
int error_respond(struct rline *li, int error_code, char *format, ...);

int ri_append(struct rline *li, char *format, ...) __attribute__ ((format (printf, 2, 3)));
int ri_error(struct rline *li, int error_code, char *format, ...) __attribute__ ((format (printf, 3, 4)));
int ri_final(struct rline *li);

int load_permits(void);
int setup_listening_socket(void);

void nterfacer_accept_event(struct esocket *socket);
void nterfacer_disconnect_event(struct esocket *socket);
int nterfacer_line_event(struct esocket *socket, char *newline);
int nterfacer_new_rline(char *line, struct esocket *socket, int *number);
struct sconnect *find_sconnect_from_fd(int fd);

void *nterfacer_sendline(char *service, char *command, int argc, char **argv, rline_callback callback, void *tag);
void nterfacer_freeline(void *arg);

#endif
