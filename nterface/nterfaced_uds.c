/*
  nterfaced UNIX domain socket module
  Copyright (C) 2003-2004 Chris Porter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include "../core/config.h"
#include "../core/events.h"

#include "nterfaced_uds.h"
#include "nterfaced.h"
#include "logging.h"
#include "library.h"
#include "transports.h"
#include "requests.h"

struct transport *otu;
struct esocket_events uds_events;
unsigned short uds_token;

void _init(void) {
  otu = register_transport("uds");
  MemCheck(otu);

  otu->on_line = uds_transport_line_event;
  otu->on_disconnect = uds_transport_disconnect_event;
  otu->type = TT_INPUT;

  socket_path = getcopyconfigitem("nterfaced", "socketpath", "/tmp/nterfaced", 100);
  MemCheck(socket_path);

  uds = create_uds();
  if(uds < 0) {
    nterface_log(ndl, NL_ERROR, "UDS: Unable to create domain socket!");

    freesstring(socket_path);
    socket_path = NULL;

    uds = -1;
    return;
  }

  uds_events.on_line = uds_buffer_line_event;
  uds_events.on_accept = uds_buffer_accept_event;
  uds_events.on_disconnect = NULL;

  uds_token = esocket_token();
  esocket_add(uds, ESOCKET_UNIX_DOMAIN, &uds_events, uds_token);

  /* UDS must not have a disconnect event */
  uds_events.on_disconnect = uds_buffer_disconnect_event;
}

void _fini(void) {
  if(uds != -1) {
    esocket_clean_by_token(uds_token);

    if(socket_path && (unlink(socket_path->content) == -1))
      nterface_log(ndl, NL_WARNING, "UDS: Unable to delete socket %s.", socket_path->content);
  }

  if(socket_path)
    freesstring(socket_path);

  if(otu)
    deregister_transport(otu);

}

int create_uds(void) {
  int sock;
  struct sockaddr_un sa;

  sock = socket(PF_UNIX, SOCK_STREAM, 0);

  if(sock < 0) {
    nterface_log(ndl, NL_ERROR, "UDS: Unable to create UNIX domain socket!");
    return -1;
  }

  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, socket_path->content, sizeof(sa.sun_path) - 1);
  sa.sun_path[sizeof(sa.sun_path) - 1] = '\0';

  if((unlink(socket_path->content) == -1) && (errno != ENOENT)) {
    nterface_log(ndl, NL_ERROR, "UDS: Unable to delete already existing socket (%s)!", socket_path->content);
    close(sock);
    return -1;
  }

  if(bind(sock, (struct sockaddr *)&sa, sizeof(sa))) {
    nterface_log(ndl, NL_ERROR, "UDS: Unable to bind socket to %s!", socket_path->content);
    close(sock);
    return -1;
  }

  if(chmod(socket_path->content, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH) == -1) {
    nterface_log(ndl, NL_ERROR, "UDS: Unable to change permissions of socket %s!", socket_path->content);
    close(sock);
    if(unlink(socket_path->content) == -1)
      nterface_log(ndl, NL_WARNING, "UDS: Unable to delete socket %s.", socket_path->content);
    return -1;
  }

  if(listen(sock, 5)) {
    nterface_log(ndl, NL_ERROR, "UDS: Unable to listen on socket!");
    close(sock);
    if(unlink(socket_path->content) == -1)
      nterface_log(ndl, NL_WARNING, "UDS: Unable to delete socket %s.", socket_path->content);
    return -1;
  }

  return sock;
}

void uds_buffer_accept_event(struct esocket *active) {
  int newfd = accept(active->fd, NULL, 0);
  unsigned short opt = 1;
  if(!newfd == -1) {
    nterface_log(ndl, NL_WARNING, "UDS: Unable to accept new UNIX domain socket!");
    return;
  }

  nterface_log(ndl, NL_INFO|NL_LOG_ONLY, "UDS: New connection");
  if(ioctl(newfd, FIONBIO, &opt)) {
    close(newfd);
    return;
  }

  if(!esocket_add(newfd, ESOCKET_UNIX_DOMAIN_CONNECTED, &uds_events, uds_token)) {
    close(newfd);
    return;
  }
}

int uds_buffer_line_event(struct esocket *socket, char *newline) {
  int number, reason;
  
  nterface_log(ndl, NL_INFO|NL_LOG_ONLY, "UDS: L: %s", newline);
  reason = new_request(otu, socket->fd, newline, &number);
  if(reason) {
    if(reason == RE_BAD_LINE) {
      return 1;
    } else {
      int ret = esocket_write_line(socket, "%s,%d,E%d,%s", newline, number, reason, request_error(reason));
      if(ret)
        return ret;
    }
  }
  return 0;
}

int uds_transport_line_event(struct request *request, char *buf) {
  struct esocket *es;
  es = find_esocket_from_fd(request->input.tag);

  if(!es)
    return 1;

  return esocket_write_line(es, "%s,%d,%s", request->service->service->content, request->input.token, buf);
}

void uds_transport_disconnect_event(struct request *req) {
  struct esocket *es = find_esocket_from_fd(req->input.tag);
  if(!es)
    return;

  esocket_write_line(es, "%s,%d,E%d,%s", req->service->service->content, req->input.token, RE_CONNECTION_CLOSED, request_error(RE_CONNECTION_CLOSED));
}

void uds_buffer_disconnect_event(struct esocket *socket) {
  transport_disconnect(otu, socket->fd);
}
