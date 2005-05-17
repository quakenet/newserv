/*
  nterfaced service TCP link module
  Copyright (C) 2003-2004 Chris Porter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <netdb.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

#include "../core/config.h"
#include "../core/schedule.h"
#include "../core/events.h"
#include "../lib/sstring.h"

#include "nterfaced_service_link.h"
#include "logging.h"

struct transport *otsl;
struct slink *slinks;
int link_count = 0;

struct esocket_events sl_events;

void _init(void) {
  otsl = register_transport("service_link");
  MemCheck(otsl);
  
  otsl->on_line = s_l_transport_line_event;
  otsl->on_disconnect = s_l_transport_disconnect_event;
  otsl->type = TT_OUTPUT;

  sl_events.on_connect = s_l_buffer_connect_event;
  sl_events.on_disconnect = s_l_buffer_disconnect_event;
  sl_events.on_line = s_l_buffer_line_event;

  scheduleoneshot(time(NULL) + 1, service_link_startup, NULL);
}

void _fini(void) {
  int i;

  if(otsl) {
    deregister_transport(otsl);
    otsl = NULL;
  }

  if(slinks) {
    for(i=0;i<link_count;i++) {
      if(slinks[i].status != SL_IDLE)
        esocket_disconnect(slinks[i].socket);

      if(slinks[i].reconnect)
        deleteschedule(slinks[i].reconnect, &connect_link, &slinks[i]);

      freesstring(slinks[i].hostname);
      freesstring(slinks[i].password);
    }

    free(slinks);
    slinks = NULL;
    link_count = 0;
  }
}

void service_link_startup(void *args) {
  int loaded = load_links(), i;
  if(!loaded) {
    nterface_log(ndl, NL_ERROR, "SL: No config lines loaded successfully.");
    return;
  } else {
    nterface_log(ndl, NL_INFO, "SL: Loaded %d link%s successfully.", loaded, loaded==1?"":"s");
  }

  for(i=0;i<loaded;i++)
    connect_link(&slinks[i]);
}

int load_links(void) {
  int lines, i, loaded_lines = 0;
  struct slink *new_links, *resized, *item, *ci;
  char buf[50];

  lines = getcopyconfigitemintpositive("nterfaced", "links", 0);
  if(lines < 1) {
    nterface_log(ndl, NL_ERROR, "SL: No links found in config file.");
    return 0;
  }
  nterface_log(ndl, NL_INFO, "SL: Loading %d link%s from config file", lines, lines==1?"":"s");

  new_links = calloc(lines, sizeof(struct slink));
  item = new_links;

  for(i=1;i<=lines;i++) {
    snprintf(buf, sizeof(buf), "port%d", i);
    item->port = getcopyconfigitemintpositive("nterfaced", buf, 0);
    if(item->port < 1) {
      nterface_log(ndl, NL_WARNING, "SL: Error loading port for item %d.", i);
      continue;
    }

    snprintf(buf, sizeof(buf), "hostname%d", i);
    item->hostname = getcopyconfigitem("nterfaced", buf, "", 100);
    if(!item->hostname) {
      MemError();
      continue;
    }

    for(ci=new_links;ci<item;ci++) {
      if((ci->port == item->port) && !strcmp(ci->hostname->content, item->hostname->content)) {
        nterface_log(ndl, NL_WARNING, "SL: Duplicate hostname/port %s:%d (%d:%d), ignoring item at position %d.", item->hostname->content, item->port, ci - new_links + 1, i, i);
        item->port = 0;
        break;
      }
    }

    if(!item->port) {
      freesstring(item->hostname);
      continue;
    }

    snprintf(buf, sizeof(buf), "password%d", i);
    item->password = getcopyconfigitem("nterfaced", buf, "", 100);
    if(!item->password) {
      MemError();
      freesstring(item->hostname);
      continue;
    }

    nterface_log(ndl, NL_DEBUG, "SL: Loaded link, hostname: %s port: %d.", item->hostname->content, item->port);

    item->status = SL_IDLE;
    item->reconnect = NULL;
    item->tagid = i;

    item++;
    loaded_lines++;
  }

  if(!loaded_lines) {
    free(new_links);
    return 0;
  }

  resized = realloc(new_links, sizeof(struct slink) * loaded_lines);
  if(!resized) {
    MemError();
    free(new_links);
    return 0;
  }
  slinks = resized;
  link_count = loaded_lines;

  return link_count;
}

void connect_link(void *vlink) {
  struct slink *nlink = (struct slink *)vlink;
  struct sockaddr_in sin;
  struct hostent *host;
  unsigned int opt = 1;
  int res = 1;
  int fd;

  if(nlink->reconnect)
    nlink->reconnect = NULL;

  if(nlink->status != SL_IDLE)
    return;

  nterface_log(ndl, NL_INFO, "SL: Connecting to %s:%d. . .", nlink->hostname->content, nlink->port);

  /* shamelessly ripped from irc/irc.c and proxyscan/proxyscanconnect.c */

  host = gethostbyname(nlink->hostname->content);

  if (!host) {
    nterface_log(ndl, NL_WARNING, "SL: Couldn't resolve hostname: %s (port %d) retrying in %d seconds.", nlink->hostname->content, nlink->port, DNSERROR_DURATION);
    nlink->reconnect = scheduleoneshot(time(NULL) + DNSERROR_DURATION, &connect_link, nlink);
    return;
  }

  fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    nterface_log(ndl, NL_WARNING, "SL: Couldn't create socket for %s:%d, retrying in %d seconds.", nlink->hostname->content, nlink->port, RECONNECT_DURATION);
    link_schedule_reconnect(nlink);
    return;
  }

  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;
  sin.sin_port = htons(nlink->port);

  memcpy(&sin.sin_addr.s_addr, host->h_addr, host->h_length);
  
  if (ioctl(fd, FIONBIO, &res)) {
    nterface_log(ndl, NL_ERROR, "SL: Unable to make socket nonblocking (%s:%d), retrying in %d seconds.", nlink->hostname->content, nlink->port, RECONNECT_DURATION);
    close(fd);
    link_schedule_reconnect(nlink);
    return;
  }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt))) {
    nterface_log(ndl, NL_ERROR, "SL: Unable to set SO_REUSEADDR on socket (%s:%d), retrying in %d seconds.", nlink->hostname->content, nlink->port, RECONNECT_DURATION);
    close(fd);
    link_schedule_reconnect(nlink);
    return;
  }

#ifdef __FreeBSD__
  opt = IP_PORTRANGE_HIGH;
  if (setsockopt(fd, IPPROTO_IP, IP_PORTRANGE, (char *)&opt, sizeof(opt)))
    nterface_log(ndl, NL_WARNING, "SL: Error selecting high port range (%s:%d).", nlink->hostname->content, nlink->port);  
#endif

  if(!connect(fd, (const struct sockaddr *)&sin, sizeof(sin))) {
    nterface_log(ndl, NL_INFO, "SL: Connected to %s:%d", nlink->hostname->content, nlink->port);
    nlink->socket = esocket_add(fd, ESOCKET_OUTGOING_TCP_CONNECTED, &sl_events, BLANK_TOKEN);

    if(!nlink->socket) {
      close(fd);
      link_schedule_reconnect(nlink);
      return;
    }

    nlink->status = SL_CONNECTED;

    authenticate_link(nlink);
  } else {
    if (errno != EINPROGRESS) {
      nterface_log(ndl, NL_INFO, "SL: Couldn't connect to %s:%d", nlink->hostname->content, nlink->port);
      link_schedule_reconnect(nlink);
      return;
    }

    nlink->socket = esocket_add(fd, ESOCKET_OUTGOING_TCP, &sl_events, BLANK_TOKEN);
    if(!nlink->socket) {
      close(fd);
      link_schedule_reconnect(nlink);
      return;
    }

    nlink->status = SL_CONNECTING;
  }
 
}

void authenticate_link(struct slink *nlink) {
}

void link_schedule_reconnect(void *vlink) {
  struct slink *nlink = (struct slink *)vlink;

  if(nlink->reconnect)
    deleteschedule(nlink->reconnect, &connect_link, nlink);
  nlink->reconnect = scheduleoneshot(time(NULL) + RECONNECT_DURATION, &connect_link, nlink);
}

void disconnect_link(struct slink *nlink) {
  if(nlink->status == SL_IDLE)
    return;

  nlink->status = SL_IDLE;

  transport_disconnect(otsl, nlink->socket->fd);

  nlink->socket = NULL;

  link_schedule_reconnect(nlink);
}

struct slink *find_s_l_from_fd(int fd) {
  int i = 0;
  for(;i<link_count;i++)
    if((slinks[i].status != SL_IDLE) && (fd == slinks[i].socket->fd))
      return &slinks[i];

  return NULL;
}

void s_l_buffer_connect_event(struct esocket *socket) {
  struct slink *nlink = find_s_l_from_fd(socket->fd);
  if(!nlink) {
    nterface_log(ndl, NL_ERROR, "SL: Unable to find service link from fd!");
    return;
  }

  nterface_log(ndl, NL_INFO, "SL: Connected to %s:%d", nlink->hostname->content, nlink->port);
  nlink->status = SL_CONNECTED;
  authenticate_link(nlink);
}

int s_l_transport_line_event(struct request *request, char *buf) {
  int i;
  
  for(i=0;i<service_count;i++)
    if(&services[i] == request->service) {
      int j;
      for(j=0;j<link_count;j++) {
        if((slinks[j].status == SL_AUTHENTICATED) && (slinks[j].tagid == services[i].tag)) {
          request->output.tag = slinks[j].socket->fd;
          if(request->service->realname) {
            return esocket_write_line(slinks[j].socket, "%s,%d,%s", request->service->realname->content, request->output.token, buf);
          } else {
            return esocket_write_line(slinks[j].socket, "%s,%d,%s", request->service->service->content, request->output.token, buf);
          }
        }
      }
      return 1;
    }

  return 1;
}

void s_l_transport_disconnect_event(struct request *req) {
  /* we can't do much about this, request has already gone into oblivion */
}

int s_l_buffer_line_event(struct esocket *sock, char *newline) {
  struct slink *vlink = find_s_l_from_fd(sock->fd);
  char *response, *theirnonceh = NULL, hexbuf[NONCE_LEN * 2 + 1];

  if(!vlink) {
    nterface_log(ndl, NL_ERROR, "SL: Unable to find service link from FD!");
    return 1;
  }
 
  switch(vlink->status) {
    case SL_CONNECTED:
      if(strcasecmp(newline, ANTI_FULL_VERSION)) {
        int ret;
        nterface_log(ndl, NL_WARNING, "SL: Dropped connection with %s:%d due to protocol version mismatch.", vlink->hostname->content, vlink->port);
        
        ret = esocket_write_line(vlink->socket, "service_link " PROTOCOL_VERSION);
        if(!ret)
          esocket_disconnect_when_complete(vlink->socket);
        return ret;
      } else {
        vlink->status = SL_VERSIONED;
        return esocket_write_line(vlink->socket, "service_link " PROTOCOL_VERSION);
      }
      break;
    case SL_AUTHENTICATED:
      finish_request(otsl, newline);
      break;
    case SL_VERSIONED:
      for(response=newline;*response;response++) {
        if((*response == ' ') && (*(response + 1))) {
          *response = '\0';
          theirnonceh = response + 1;
          break;
        }
      }
      if(!theirnonceh || (strlen(theirnonceh) != 32) || !hex_to_int(theirnonceh, vlink->theirnonce, sizeof(vlink->theirnonce))) {
        nterface_log(ndl, NL_WARNING, "SL: Dropped connection with %s:%d due to protocol error.", vlink->hostname->content, vlink->port);
        return 1;
      }

      if(!generate_nonce(vlink->nonce, 0)) {
        nterface_log(ndl, NL_ERROR, "SL: Unable to generate nonce!");
        return 1;
      }

      if(!memcmp(vlink->nonce, vlink->theirnonce, sizeof(vlink->theirnonce))) {
        nterface_log(ndl, NL_WARNING, "SL: Dropped connection with %s:%d due to identical nonces (this SHOULD be impossible).", vlink->hostname->content, vlink->port);
        return 1;
      }

      vlink->status = SL_AUTHENTICATING;
      return esocket_write_line(vlink->socket, "%s %s", challenge_response(newline, vlink->password->content), int_to_hex(vlink->nonce, hexbuf, 16));
      break;
    case SL_AUTHENTICATING:
      vlink->status = SL_AUTHENTICATED;
      nterface_log(ndl, NL_INFO, "SL: Authenticated with %s:%d, switching to crypted mode", vlink->hostname->content, vlink->port);
      switch_buffer_mode(sock, vlink->password->content, vlink->nonce, vlink->theirnonce);
      break;
  }
  return 0;
}

void s_l_buffer_disconnect_event(struct esocket *socket) {
  struct slink *vlink = find_s_l_from_fd(socket->fd);
  if(!vlink) {
    nterface_log(ndl, NL_ERROR, "SL: Unable to find socket in disconnect event!");
    return;
  }

  if(vlink->status == SL_CONNECTING) {
    nterface_log(ndl, NL_WARNING, "SL: Connection failed (%s:%d), retrying in %d seconds.", vlink->hostname->content, vlink->port, RECONNECT_DURATION);
  } else {
    nterface_log(ndl, NL_INFO, "SL: Connection closed (%s:%d), reconnecting in %d seconds.", vlink->hostname->content, vlink->port, RECONNECT_DURATION);
  }

  disconnect_link(vlink);
}
