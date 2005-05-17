/*
  Easy async socket library with HELIX encryption and authentication
  Copyright (C) 2004 Chris Porter.
*/

#ifndef __esockets_H
#define __esockets_H

#include "../lib/helix.h"

#define ESOCKET_UNIX_DOMAIN            ESOCKET_LISTENING
#define ESOCKET_UNIX_DOMAIN_CONNECTED  ESOCKET_INCOMING
#define ESOCKET_TCP_LISTENING          ESOCKET_LISTENING
#define ESOCKET_INCOMING_TCP           ESOCKET_INCOMING
#define ESOCKET_OUTGOING_TCP           ESOCKET_OUTGOING
#define ESOCKET_OUTGOING_TCP_CONNECTED ESOCKET_OUTGOING_CONNECTED

#define ESOCKET_LISTENING          1
#define ESOCKET_OUTGOING           2
#define ESOCKET_INCOMING           3
#define ESOCKET_OUTGOING_CONNECTED 4

#define ST_CONNECTING 1
#define ST_CONNECTED  2
#define ST_LISTENING  3
#define ST_DISCONNECT 4
#define ST_BLANK      5

#define BLANK_TOKEN  0

#define BUF_CONT -1
#define BUF_OVERFLOW -2

#define MAX_BUFSIZE 50000

#define USED_MAC_LEN 3

typedef unsigned short packet_t;

#define MAX_BINARY_LINE_SIZE MAX_BUFSIZE
#define MAX_ASCII_LINE_SIZE  MAX_BINARY_LINE_SIZE - sizeof(packet_t) - USED_MAC_LEN

#define MAX_OUT_QUEUE_SIZE   1500

struct buffer;
struct esocket;

typedef int (*parse_event)(struct esocket *sock);

typedef int (*line_event)(struct esocket *sock, char *newline);

typedef struct esocket_in_buffer {
  char data[MAX_BUFSIZE];
  char *writepos;
  char *curpos;
  char *startpos;
  unsigned short buffer_size;
  parse_event on_parse;
  packet_t packet_length;
} in_buffer;

typedef struct esocket_packet {
  char *line;
  packet_t size;
  struct esocket_packet *next;
} esocket_packet;

typedef struct esocket_out_buffer {
  struct esocket_packet *head;
  struct esocket_packet *end;
  unsigned short count;
} out_buffer;

typedef void (*esocket_event)(struct esocket *socket);

typedef struct esocket_events {
  esocket_event on_connect;
  esocket_event on_accept;
  esocket_event on_disconnect;
  line_event    on_line;
} esocket_events;

typedef struct esocket {
  int fd;
  struct esocket_in_buffer in;
  struct esocket_out_buffer out;
  char socket_type;
  char socket_status;
  struct esocket_events events;
  struct esocket *next;
  unsigned short token;
  void *tag;
  helix_ctx keysend;
  helix_ctx keyreceive;
} esocket;

struct esocket *esocket_add(int fd, char socket_type, struct esocket_events *events, unsigned short token);
struct esocket *find_esocket_from_fd(int fd);
void esocket_poll_event(int fd, short events);
void esocket_disconnect(struct esocket *active);
int buffer_parse_ascii(struct esocket *sock);
int esocket_read(struct esocket *sock);
int esocket_write(struct esocket *sock, char *buffer, int bytes);
int esocket_write_line(struct esocket *sock, char *format, ...);
unsigned short esocket_token(void);
struct esocket *find_esocket_from_fd(int fd);
void esocket_clean_by_token(unsigned short token);
void switch_buffer_mode(struct esocket *sock, char *key, unsigned char *ournonce, unsigned char *theirnonce);
void esocket_disconnect_when_complete(struct esocket *active);
int esocket_raw_write(struct esocket *sock, char *buffer, int bytes);

#endif


