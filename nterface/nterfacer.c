/*
  nterfacer
  Copyright (C) 2004 Chris Porter.
  
  v1.05
    - added application level ping support
  v1.04
    - modified for new logging system
  v1.03
    - newserv seems to unload this module before the ones that depend on us,
      so deregister_service now checks to see if it's been freed already
  v1.02
    - moronic bug in linked lists fixed
  v1.01
    - logging
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <string.h>

#include "../lib/sstring.h"
#include "../lib/irc_string.h"
#include "../core/config.h"
#include "../core/events.h"

#include "nterfacer.h"
#include "logging.h"

struct service_node *tree = NULL;
struct esocket_events nterfacer_events;
struct esocket *nterfacer_sock;
struct rline *rlines = NULL;
unsigned short nterfacer_token = BLANK_TOKEN;
struct nterface_auto_log *nrl;

struct service_node *ping;

int ping_handler(struct rline *ri, int argc, char **argv);

void _init(void) {
  int loaded;
  int debug_mode = getcopyconfigitemintpositive("nterfacer", "debug", 0);
  
  nrl = nterface_open_log("nterfacer", "logs/nterfacer.log", debug_mode);

  loaded = load_permits();
  if(!loaded) {
    nterface_log(nrl, NL_ERROR, "No permits loaded successfully.");
    return;
  } else {
    nterface_log(nrl, NL_INFO, "Loaded %d permit%s successfully.", loaded, loaded==1?"":"s");
  }

  nterfacer_events.on_accept = nterfacer_accept_event;
  nterfacer_events.on_line = nterfacer_line_event;
  nterfacer_events.on_disconnect = NULL;

  nterfacer_token = esocket_token();

  ping = register_service("nterfacer");
  if(!ping) {
    MemError();
  } else {
    register_handler(ping, "ping", 0, ping_handler);
  }

  accept_fd = setup_listening_socket();
  if(accept_fd == -1) {
    nterface_log(nrl, NL_ERROR, "Unable to setup listening socket!");
  } else {
    nterfacer_sock = esocket_add(accept_fd, ESOCKET_UNIX_DOMAIN, &nterfacer_events, nterfacer_token);
  }

  /* the main unix domain socket must NOT have a disconnect event. */
  nterfacer_events.on_disconnect = nterfacer_disconnect_event;
}

void free_handler(struct handler *hp) {
  freesstring(hp->command);
  free(hp);
}

void free_handlers(struct service_node *tp) {
  struct handler *hp, *lp;

  for(hp=tp->handlers;hp;) {
    lp = hp;
    hp = hp->next;
    free_handler(lp);
  }

  tp->handlers = NULL;
}

void _fini(void) {
  struct service_node *tp, *lp;
  int i;

  if(ping)
    deregister_service(ping);

  for(tp=tree;tp;) {
    lp = tp;
    tp = tp->next;
    free_handlers(lp);
    free(lp);
  }
  tree = NULL;

  if((accept_fd != -1) && nterfacer_sock) {
    esocket_clean_by_token(nterfacer_token);
    nterfacer_sock = NULL;
    accept_fd = -1;
  }

  if(permits && permit_count) {
    for(i=0;i<permit_count;i++) {
      freesstring(permits[i].hostname);
      freesstring(permits[i].password);
    }
    free(permits);
    permit_count = 0;
    permits = NULL;
  }

  nrl = nterface_close_log(nrl);
}

int load_permits(void) {
  int lines, loaded_lines = 0, i, j;
  struct permitted *new_permits, *resized, *item;
  struct hostent *host;
  char buf[50];

  lines = getcopyconfigitemintpositive("nterfacer", "lines", 0);
  if(lines < 1) {
    nterface_log(nrl, NL_ERROR, "No permits found in config file.");
    return 0;
  }
  nterface_log(nrl, NL_INFO, "Loading %d permit%s from config file", lines, lines==1?"":"s");

  new_permits = calloc(lines, sizeof(struct permitted));
  item = new_permits;

  for(i=1;i<=lines;i++) {
    snprintf(buf, sizeof(buf), "hostname%d", i);
    item->hostname = getcopyconfigitem("nterfacer", buf, "", 100);
    if(!item->hostname) {
      MemError();
      continue;
    }

    host = gethostbyname(item->hostname->content);
    if (!host) {
      nterface_log(nrl, NL_WARNING, "Couldn't resolve hostname: %s (item %d).", item->hostname->content, i);
      freesstring(item->hostname);
      continue;
    }

    item->ihost = (*(struct in_addr *)host->h_addr).s_addr;
    for(j=0;j<loaded_lines;j++) {
      if(new_permits[j].ihost == item->ihost) {
        nterface_log(nrl, NL_WARNING, "Host with items %d and %d is identical, dropping item %d.", j + 1, i, i);
        host = NULL;
      }
    }

    if(!host) {
      freesstring(item->hostname);
      continue;
    }

    snprintf(buf, sizeof(buf), "password%d", i);
    item->password = getcopyconfigitem("nterfacer", buf, "", 100);
    if(!item->password) {
      MemError();
      freesstring(item->hostname);
      continue;
    }

    nterface_log(nrl, NL_DEBUG, "Loaded permit, hostname: %s.", item->hostname->content);

    item++;
    loaded_lines++;
  }

  if(!loaded_lines) {
    free(new_permits);
    return 0;
  }

  resized = realloc(new_permits, sizeof(struct permitted) * loaded_lines);
  if(!resized) {
    MemError();
    free(new_permits);
    return 0;
  }
  permits = resized;
  permit_count = loaded_lines;

  return permit_count;
}

int setup_listening_socket(void) {
  struct sockaddr_in sin;
  int fd;
  unsigned int opt = 1;
  
  fd = socket(AF_INET, SOCK_STREAM, 0);

  /* also shamelessly ripped from proxyscan */
  if(fd == -1) {
    nterface_log(nrl, NL_ERROR, "Unable to open listening socket (%d).", errno);
    return -1;
  }
  
  if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt)) != 0) {
    nterface_log(nrl, NL_ERROR, "Unable to set SO_REUSEADDR on listen socket.");
    return -1;
  }
  
  /* Initialiase the addresses */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port = htons(NTERFACER_PORT);
  
  if(bind(fd, (struct sockaddr *) &sin, sizeof(sin))) {
    nterface_log(nrl, NL_ERROR, "Unable to bind listen socket (%d).", errno);
    return -1;
  }
  
  listen(fd, 5);
  
  if(ioctl(fd, FIONBIO, &opt) !=0) {
    nterface_log(nrl, NL_ERROR, "Unable to set listen socket non-blocking.");
    return -1;
  }
  
  return fd;
}

struct service_node *register_service(char *name) {
  struct service_node *np = malloc(sizeof(service_node));
  MemCheckR(np, NULL);

  np->name = getsstring(name, strlen(name));
  if(!np->name) {
    MemError();
    free(np);
    return NULL;
  }

  np->handlers = NULL;
  np->next = tree;
  tree = np;

  return np;
}

struct handler *register_handler(struct service_node *service, char *command, int args, handler_function fp) {
  struct handler *hp = malloc(sizeof(handler));
  MemCheckR(hp, NULL);

  hp->command = getsstring(command, strlen(command));
  if(!hp->command) {
    MemError();
    free(hp);
    return NULL;
  }

  hp->function = (handler_function *)fp;
  hp->args = args;

  hp->next = service->handlers;
  hp->service = service;
  service->handlers = hp;

  return hp;
}

void deregister_handler(struct handler *hl) {
  struct service_node *service = (struct service_node *)hl->service;  
  struct handler *np, *lp = NULL;
  for(np=service->handlers;np;lp=np,np=np->next) {
    if(hl == np) {
      if(lp) {
        lp->next = np->next;
      } else {
        service->handlers = np->next;
      }
      free_handler(np);
      return;
    }
  }
}

void deregister_service(struct service_node *service) {
  struct service_node *sp, *lp = NULL;
  struct rline *li, *pi = NULL;

  for(sp=tree;sp;lp=sp,sp=sp->next) {
    if(sp == service) {
      if(lp) {
        lp->next = sp->next;
      } else {
        tree = sp->next;
      }
      break;
    }
  }

  if(!sp) /* already freed */
    return;

  free_handlers(service);

  for(li=rlines;li;) {
    if(li->service == service) {
      if(pi) {
        pi->next = li->next;
        free(li);
        li = pi->next;
      } else {
        rlines = li->next;
        free(li);
        li = rlines;
      }
    } else {
      pi=li,li=li->next;
    }
  }
  freesstring(service->name);

  free(service);
}

void nterfacer_accept_event(struct esocket *socket) {
  struct sockaddr_in sin;
  unsigned int addrsize = sizeof(sin);
  int newfd = accept(socket->fd, (struct sockaddr *)&sin, &addrsize), i;
  struct sconnect *temp;
  struct permitted *item = NULL;
  struct esocket *newsocket;

  if(newfd == -1) {
    nterface_log(nrl, NL_WARNING, "Unable to accept nterfacer fd!");
    return;
  }

  for(i=0;i<permit_count;i++) {
    if(permits[i].ihost == sin.sin_addr.s_addr) {
      item = &permits[i];
      break;
    }
  }

  if(!item) {
    nterface_log(nrl, NL_INFO, "Unauthorised connection from: %s", IPtostr(htonl(sin.sin_addr.s_addr)));
    close(newfd);
    return;
  }

  temp = (struct sconnect *)malloc(sizeof(struct sconnect));
  if(!temp) {
    MemError();
    close(newfd);
    return;
  }
    
  /* do checks on hostname first */

  newsocket = esocket_add(newfd, ESOCKET_UNIX_DOMAIN_CONNECTED, &nterfacer_events, nterfacer_token);
  if(!newsocket) {
    free(temp);
    close(newfd);
    return;
  }
  newsocket->tag = temp;

  nterface_log(nrl, NL_INFO, "New connection from %s.", item->hostname->content);

  temp->status = SS_IDLE;
  temp->permit = item;

  esocket_write_line(newsocket, "nterfacer " PROTOCOL_VERSION);
}

int nterfacer_line_event(struct esocket *sock, char *newline) {
  struct sconnect *socket = sock->tag;
  char *response, *theirnonceh = NULL;
  unsigned char theirnonce[NONCE_LEN];
  int number, reason;

  switch(socket->status) {
    case SS_IDLE:
      if(strcasecmp(newline, ANTI_FULL_VERSION)) {
        nterface_log(nrl, NL_INFO, "Protocol mismatch from %s: %s", socket->permit->hostname->content, newline);
        return 1;
      } else {
        char *hex, hexbuf[NONCE_LEN * 2 + 1]; /* Vekoma mAD HoUSe */

        hex = get_random_hex();
        if(!hex) {
          nterface_log(nrl, NL_ERROR, "Unable to open challenge entropy bin!");
          return 1;
        }

        memcpy(socket->response, challenge_response(hex, socket->permit->password->content), sizeof(socket->response));
        socket->response[sizeof(socket->response) - 1] = '\0'; /* just in case */

        socket->status = SS_VERSIONED;
        if(!generate_nonce(socket->ournonce, 1)) {
          nterface_log(nrl, NL_ERROR, "Unable to generate nonce!");
          return 1;
        }

        return esocket_write_line(sock, "%s %s", hex, int_to_hex(socket->ournonce, hexbuf, NONCE_LEN));
      }
      break;
    case SS_VERSIONED:
      for(response=newline;*response;response++) {
        if((*response == ' ') && (*(response + 1))) {
          *response = '\0';
          theirnonceh = response + 1;
          break;
        }
      }

      if(!theirnonceh || (strlen(theirnonceh) != 32) || !hex_to_int(theirnonceh, theirnonce, sizeof(theirnonce))) {
        nterface_log(nrl, NL_INFO, "Protocol error drop: %s", socket->permit->hostname->content);
        return 1;
      }

      if(!memcmp(socket->ournonce, theirnonce, sizeof(theirnonce))) {
        nterface_log(nrl, NL_INFO, "Bad nonce drop: %s", socket->permit->hostname->content);
        return 1;
      }

      if(!strncasecmp(newline, socket->response, sizeof(socket->response))) {
        int ret;

        nterface_log(nrl, NL_INFO, "Authed: %s", socket->permit->hostname->content);
        socket->status = SS_AUTHENTICATED;
        ret = esocket_write_line(sock, "Oauth");
        if(!ret) {
          switch_buffer_mode(sock, socket->permit->password->content, socket->ournonce, theirnonce);
        } else {
          return ret;
        }
      } else {
        nterface_log(nrl, NL_INFO, "Bad CR drop: %s", socket->permit->hostname->content);
        
        return 1;
      }
      break;
    case SS_AUTHENTICATED:
      nterface_log(nrl, NL_INFO|NL_LOG_ONLY, "L(%s): %s", socket->permit->hostname->content, newline);
      reason = nterfacer_new_rline(newline, sock, &number);
      if(reason) {
        if(reason == RE_SOCKET_ERROR)
          return BUF_ERROR;
        if(reason != RE_BAD_LINE) {
          return esocket_write_line(sock, "%d,E%d,%s", number, reason, request_error(reason));
        } else {
          return 1;
        }
      }
      break;
  }

  return 0;
}

int nterfacer_new_rline(char *line, struct esocket *socket, int *number) {
  char *sp, *p, *parsebuf, *pp, commandbuf[MAX_BUFSIZE], *args[MAX_ARGS], *newp;
  int argcount;
  struct service_node *service;
  struct rline *prequest;
  struct handler *hl;
  int re;

  if(!line || !line[0] || (line[0] == ','))
    return 0;

  for(sp=line;*sp;sp++)
    if(*sp == ',')
      break;

  if(!*sp || !*(sp + 1))
    return RE_BAD_LINE;

  *sp = '\0';

  for(service=tree;service;service=service->next)
    if(!strcmp(service->name->content, line))
      break;

  for(p=sp+1;*p;p++)
    if(*p == ',')
      break;

  if(!*p || !(p + 1))
    return RE_BAD_LINE;
  
  *p = '\0';
  *number = positive_atoi(sp + 1);
  
  if((*number < 1) || (*number > 0xffff))
    return RE_BAD_LINE;

  if (!service) {
    nterface_log(nrl, NL_DEBUG, "Unable to find service: %s", line);
    return RE_SERVICER_NOT_FOUND;
  }

  newp = commandbuf;

  for(pp=p+1;*pp;pp++) {
    if((*pp == '\\') && *(pp + 1)) {
      if(*(pp + 1) == ',') {
        *newp++ = ',';
      } else if(*(pp + 1) == '\\') {
        *newp++ = '\\';
      }
      pp++;
    } else if(*pp == ',') {
      break;
    } else {
      *newp++ = *pp;
    }
  }

  if(*pp == '\0') { /* if we're at the end already, we have no arguments */
    argcount = 0;
  } else {
    argcount = 1; /* we have a comma, so we have one already */
  }
  
  *newp = '\0';

  for(hl=service->handlers;hl;hl=hl->next)
    if(!strncmp(hl->command->content, commandbuf, sizeof(commandbuf)))
      break;

  if(!hl)
    return RE_COMMAND_NOT_FOUND;

  if(argcount) {
    parsebuf = (char *)malloc(strlen(pp) + 1);
    MemCheckR(parsebuf, RE_MEM_ERROR);
    newp = parsebuf;
  
    for(newp=args[0]=parsebuf,pp++;*pp;pp++) {
      if((*pp == '\\') && *(pp + 1)) {
        if(*(pp + 1) == ',') {
          *newp++ = ',';
        } else if(*(pp + 1) == '\\') {
          *newp++ = '\\';
        }
        pp++;
      } else if(*pp == ',') {
        *newp++ = '\0';
        args[argcount++] = newp;
        if(argcount > MAX_ARGS) {
          free(parsebuf);
          return RE_TOO_MANY_ARGS;
        }
      } else {
        *newp++ = *pp;
      }
    }
    *newp = '\0';
  }
  if(argcount < hl->args) {
    if(argcount)
      free(parsebuf);
    return RE_WRONG_ARG_COUNT;
  }

  prequest = (struct rline *)malloc(sizeof(struct rline));
  if(!prequest) {
    MemError();
    if(argcount)
      free(parsebuf);
    return RE_MEM_ERROR;
  }

  prequest->service = service;
  prequest->handler = hl;
  prequest->curpos = prequest->buf;
  prequest->tag = NULL;
  prequest->id = *number;
  prequest->next = rlines;
  prequest->socket = socket;

  rlines = prequest;
  re = ((handler_function)hl->function)(prequest, argcount, args);
  
  if(argcount)
    free(parsebuf);

  return re;
}

void nterfacer_disconnect_event(struct esocket *sock) {
  struct sconnect *socket = sock->tag;
  struct rline *li;
  /* not tested */

  nterface_log(nrl, NL_INFO, "Disconnected from %s.", socket->permit->hostname->content);

  /* not tested */
  for(li=rlines;li;li=li->next)
    if(li->socket->tag == socket)
      li->socket = NULL;

  free(socket);
}

int ri_append(struct rline *li, char *format, ...) {
  char buf[MAX_BUFSIZE], escapedbuf[MAX_BUFSIZE * 2 + 1], *p, *tp;
  int sizeleft = sizeof(li->buf) - (li->curpos - li->buf);  
  va_list ap;

  va_start(ap, format);

  if(vsnprintf(buf, sizeof(buf), format, ap) >= sizeof(buf)) {
    va_end(ap);
    return BF_OVER;
  }

  va_end(ap);

  for(tp=escapedbuf,p=buf;*p||(*tp='\0');*tp++=*p++)
    if((*p == ',') || (*p == '\\'))
      *tp++ = '\\';

  if(sizeleft > 0) {
    if(li->curpos == li->buf) { 
      li->curpos+=snprintf(li->curpos, sizeleft, "%s", escapedbuf);
    } else {
      li->curpos+=snprintf(li->curpos, sizeleft, ",%s", escapedbuf);
    }
  }

  if(sizeof(li->buf) - (li->curpos - li->buf) > 0) {
    return BF_OK;
  } else {
    return BF_OVER;
  }
}

int ri_error(struct rline *li, int error_code, char *format, ...) {
  char buf[MAX_BUFSIZE], escapedbuf[MAX_BUFSIZE * 2 + 1], *p, *tp;
  struct rline *pp, *lp = NULL;
  va_list ap;

  if(li->socket) {
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    for(tp=escapedbuf,p=buf;*p||(*tp='\0');*tp++=*p++)
      if((*p == ',') || (*p == '\\'))
        *tp++ = '\\';

    esocket_write_line(li->socket, "%d,OE%d,%s", li->id, error_code, escapedbuf);
  }

  for(pp=rlines;pp;lp=pp,pp=pp->next) {
    if(pp == li) {
      if(lp) {
        lp->next = li->next;
      } else {
        rlines = li->next;
      }
      free(li);
      break;
    }
  }
  
  return RE_OK;
}

int ri_final(struct rline *li) {
  struct rline *pp, *lp = NULL;
  int retval = RE_OK;

  if(li->socket)
    if(esocket_write_line(li->socket, "%d,OO%s", li->id, li->buf))
      retval = RE_SOCKET_ERROR;

  for(pp=rlines;pp;lp=pp,pp=pp->next) {
    if(pp == li) {
      if(lp) {
        lp->next = li->next;
      } else {
        rlines = li->next;
      }
      free(li);
      break;
    }
  }

  return retval;
}

int ping_handler(struct rline *ri, int argc, char **argv) {
  ri_append(ri, "OK");
  return ri_final(ri);
}
