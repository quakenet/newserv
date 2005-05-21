/*
  nterfaced service TCP link module
  Copyright (C) 2003-2004 Chris Porter.
*/

#ifndef __nterfaced_service_link_H
#define __nterfaced_service_link_H

#include "esockets.h"
#include "library.h"
#include "requests.h"

#define DNSERROR_DURATION   120
#define RECONNECT_DURATION  60

#define SL_EMPTY            0x00
#define SL_IDLE             0x01
#define SL_CONNECTING       0x02
#define SL_CONNECTED        0x04
#define SL_VERSIONED        0x08
#define SL_AUTHENTICATING   0x10
#define SL_AUTHENTICATED    0x20
#define SL_BLANK            0x40

#define PROTOCOL_VERSION    "1"
#define ANTI_FULL_VERSION   "nterfacer " PROTOCOL_VERSION

#define PING_DURATION       5 * 60

typedef struct slink {
  sstring *hostname;
  sstring *password;
  int port;
  int tagid;
  void *reconnect;
  int status;
  unsigned char nonce[NONCE_LEN];
  unsigned char theirnonce[NONCE_LEN];
  struct esocket *socket;
  void *pingschedule;
} slink;

extern struct slink *slinks;
extern int link_count;

int load_links(void);
void service_link_startup(void *args);

int s_l_transport_line_event(struct request *request, char *buf);
void s_l_transport_disconnect_event(struct request *req);
void authenticate_link(struct slink *nlink);
void link_schedule_reconnect(void *vlink);
void s_l_buffer_poll_event(int fd, short events);
void connect_link(void *vlink);
void disconnect_link(struct slink *nlink);
struct slink *find_s_l_from_fd(int fd);
int s_l_buffer_line_event(struct esocket *socket, char *newline);
void s_l_buffer_disconnect_event(struct esocket *socket);
void s_l_buffer_connect_event(struct esocket *socket);
void sl_startup(void);
void sl_shutdown(void);
int sl_transport_online(struct request *request, char *buf);

#endif
