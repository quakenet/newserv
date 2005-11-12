/*
  nterfacer relay4
  Copyright (C) 2004 Chris Porter.
*/

#ifndef __nterfacer_relay_H
#define __nterfacer_relay_H

#include <pcre.h>

#include "../lib/sstring.h"
#include "../localuser/localuserchannel.h"
#include "nterfacer.h"

#define MESSAGE_TIMEOUT        10

#define RELAY_PARSE_ERROR      0x01
#define RELAY_NICK_NOT_FOUND   0x02
#define RELAY_MEMORY_ERROR     0x03
#define RELAY_UNLOADED         0x04
#define RELAY_KILLED           0x05
#define RELAY_OVER_BUFFER      0x06 /* deprecated, use BF_OVER instead */
#define RELAY_NICK_ERROR       0x07
#define RELAY_TIMEOUT          0x08
#define RELAY_TARGET_LEFT      0x09
#define RELAY_O_AUTH_ERROR     0x0A
#define RELAY_REGEX_ERROR      0x0B
#define RELAY_REGEX_HINT_ERROR 0x0C
#define RELAY_NOT_ON_IRC       0x0D
#define RELAY_DISCONNECTED     0x0E
#define RELAY_SERVER_NOT_FOUND 0x0F
#define RELAY_INVALID_COMMAND  0x10
#define RELAY_INVALID_CHARS    0x11
#define RELAY_LOCAL_USER       0x12

#define MODE_TAG     0x01
#define MODE_LINES   0x02
#define MODE_IS_O    0x04
#define MODE_O_AUTH1 0x08
#define MODE_O_AUTH2 0x10
#define MODE_STATS   0x20

#define PCRE_FLAGS PCRE_CASELESS | PCRE_ANCHORED

#define DEFAULT_NICK_PREFIX "nterfacer"

#define dispose_rld(a) dispose_rld_dontquit(a, 0)

typedef struct regex {
  pcre *phrase;
  pcre_extra *hint;
} regex;

typedef struct rld {
  short mode;
  union {
    int remaining_lines;
    regex pcre;
  } termination; 
  struct rline *rline;
  nick *nick;
  nick *dest;
  struct rld *next;
  void *schedule;
} rld;

struct rld *list = NULL;

int relay_handler(struct rline *ri, int argc, char **argv);
void relay_messages(nick *target, int messagetype, void **args);
void dispose_rld_prev(struct rld *item, struct rld *prev);
void dispose_rld_dontquit(struct rld *item, int dontquit);
void relay_timeout(void *arg);
void relay_quits(int hook, void *args);
void relay_disconnect(int hook, void *args);
void relay_rehash(int hook, void *args);
int load_config(void);

#endif
