/*
  nterfacer
  Copyright (C) 2004-2007 Chris Porter.
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
#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../lib/sstring.h"
#include "../lib/irc_string.h"
#include "../core/config.h"
#include "../core/events.h"
#include "../lib/version.h"
#include "../core/schedule.h"

#include "nterfacer.h"
#include "logging.h"

MODULE_VERSION("1.1p" PROTOCOL_VERSION);

struct service_node *tree = NULL;
struct esocket_events nterfacer_events;
struct esocket *nterfacer_sock;
struct rline *rlines = NULL;
unsigned short nterfacer_token = BLANK_TOKEN;
struct nterface_auto_log *nrl;

struct service_node *ping = NULL;
int accept_fd = -1;
struct permitted *permits;
int permit_count = 0;

int ping_handler(struct rline *ri, int argc, char **argv);
static void nterfacer_sendcallback(struct rline *ri, int error, char *buf);

void _init(void) {
  int loaded;
  int debug_mode = getcopyconfigitemintpositive("nterfacer", "debug", 0);
  
  nrl = nterface_open_log("nterfacer", "logs/nterfacer.log", debug_mode);

  loaded = load_permits();
  nterface_log(nrl, NL_INFO, "Loaded %d permit%s successfully.", loaded, loaded==1?"":"s");

  if(!loaded)
    return;

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
  struct rline *li, *pi = NULL;

  for(li=rlines;li;) {
    if(li->handler == hp) {
      if(li->socket) {
        esocket_write_line(li->socket, "%d,OE%d,%s", li->id, BF_UNLOADED, "Service was unloaded.");
      } else if(li->callback) {
        nterfacer_sendcallback(li, BF_UNLOADED, "Service was unloaded.");
      }
      if(pi) {
        pi->next = li->next;
        ntfree(li);
        li = pi->next;
      } else {
        rlines = li->next;
        ntfree(li);
        li = rlines;
      }
    } else {
      pi=li,li=li->next;
    }
  }

  freesstring(hp->command);
  ntfree(hp);
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
    ntfree(lp);
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
    ntfree(permits);
    permit_count = 0;
    permits = NULL;
  }

  nrl = nterface_close_log(nrl);
  nscheckfreeall(POOL_NTERFACER);
}

int load_permits(void) {
  int loaded_lines = 0, i, j;
  struct permitted *new_permits, *resized, *item;
  struct hostent *host;
  array *hostnamesa, *passwordsa;
  sstring **hostnames, **passwords;

  hostnamesa = getconfigitems("nterfacer", "hostname");
  passwordsa = getconfigitems("nterfacer", "password");
  if(!hostnamesa || !passwordsa)
    return 0;
  if(hostnamesa->cursi != passwordsa->cursi) {
    nterface_log(nrl, NL_ERROR, "Different number of hostnames/passwords in config file.");
    return 0;
  }

  hostnames = (sstring **)hostnamesa->content;
  passwords = (sstring **)passwordsa->content;

  new_permits = ntmalloc(hostnamesa->cursi * sizeof(struct permitted));
  memset(new_permits, 0, hostnamesa->cursi * sizeof(struct permitted));
  item = new_permits;

  for(i=0;i<hostnamesa->cursi;i++) {
    item->hostname = getsstring(hostnames[i]->content, hostnames[i]->length);

    host = gethostbyname(item->hostname->content);
    if (!host) {
      nterface_log(nrl, NL_WARNING, "Couldn't resolve hostname: %s (item %d).", item->hostname->content, i + 1);
      freesstring(item->hostname);
      continue;
    }

    item->ihost = (*(struct in_addr *)host->h_addr_list[0]).s_addr;
    for(j=0;j<loaded_lines;j++) {
      if(new_permits[j].ihost == item->ihost) {
        nterface_log(nrl, NL_WARNING, "Host with items %d and %d is identical, dropping item %d.", j + 1, i + 1, i + 1);
        host = NULL;
      }
    }

    if(!host) {
      freesstring(item->hostname);
      continue;
    }

    item->password = getsstring(passwords[i]->content, passwords[i]->length);
    nterface_log(nrl, NL_DEBUG, "Loaded permit, hostname: %s.", item->hostname->content);

    item++;
    loaded_lines++;
  }

  if(!loaded_lines) {
    ntfree(new_permits);
    return 0;
  }

  resized = ntrealloc(new_permits, sizeof(struct permitted) * loaded_lines);
  if(!resized) {
    MemError();
    ntfree(new_permits);
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
  sin.sin_port = htons(getcopyconfigitemintpositive("nterfacer", "port", NTERFACER_PORT));
  
  if(bind(fd, (struct sockaddr *) &sin, sizeof(sin))) {
    nterface_log(nrl, NL_ERROR, "Unable to bind listen socket (%d).", errno);
    return -1;
  }
  
  listen(fd, 5);
  
  if(ioctl(fd, FIONBIO, &opt)) {
    nterface_log(nrl, NL_ERROR, "Unable to set listen socket non-blocking.");
    return -1;
  }
  
  return fd;
}

struct service_node *register_service(char *name) {
  struct service_node *np = ntmalloc(sizeof(service_node));
  MemCheckR(np, NULL);

  np->name = getsstring(name, strlen(name));
  if(!np->name) {
    MemError();
    ntfree(np);
    return NULL;
  }

  np->handlers = NULL;
  np->next = tree;
  tree = np;

  return np;
}

struct handler *register_handler(struct service_node *service, char *command, int args, handler_function fp) {
  struct handler *hp = ntmalloc(sizeof(handler));
  MemCheckR(hp, NULL);

  hp->command = getsstring(command, strlen(command));
  if(!hp->command) {
    MemError();
    ntfree(hp);
    return NULL;
  }

  hp->function = fp;
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

  freesstring(service->name);

  ntfree(service);
}

void nterfacer_accept_event(struct esocket *socket) {
  struct sockaddr_in sin;
  unsigned int addrsize = sizeof(sin);
  int newfd = accept(socket->fd, (struct sockaddr *)&sin, &addrsize), i;
  struct sconnect *temp;
  struct permitted *item = NULL;
  struct esocket *newsocket;
  unsigned int opt = 1;

  if(newfd == -1) {
    nterface_log(nrl, NL_WARNING, "Unable to accept nterfacer fd!");
    return;
  }

  if(ioctl(newfd, FIONBIO, &opt)) {
    nterface_log(nrl, NL_ERROR, "Unable to set accepted socket non-blocking.");
    return;
  }
  
  for(i=0;i<permit_count;i++) {
    if(permits[i].ihost == sin.sin_addr.s_addr) {
      item = &permits[i];
      break;
    }
  }

  if(!item) {
    nterface_log(nrl, NL_INFO, "Unauthorised connection from %s closed", inet_ntoa(sin.sin_addr));
    close(newfd);
    return;
  }

  temp = (struct sconnect *)ntmalloc(sizeof(struct sconnect));
  if(!temp) {
    MemError();
    close(newfd);
    return;
  }
    
  /* do checks on hostname first */

  newsocket = esocket_add(newfd, ESOCKET_UNIX_DOMAIN_CONNECTED, &nterfacer_events, nterfacer_token);
  if(!newsocket) {
    ntfree(temp);
    close(newfd);
    return;
  }
  newsocket->tag = temp;

  nterface_log(nrl, NL_INFO, "New connection from %s.", item->hostname->content);

  temp->status = SS_IDLE;
  temp->permit = item;

  esocket_write_line(newsocket, "nterfacer " PROTOCOL_VERSION);
}

void derive_key(unsigned char *out, char *password, char *segment, unsigned char *noncea, unsigned char *nonceb, unsigned char *extra, int extralen) {
  SHA256_CTX c;
  SHA256_Init(&c);
  SHA256_Update(&c, (unsigned char *)password, strlen(password));
  SHA256_Update(&c, (unsigned char *)":", 1);
  SHA256_Update(&c, (unsigned char *)segment, strlen(segment));
  SHA256_Update(&c, (unsigned char *)":", 1);
  SHA256_Update(&c, noncea, 16);
  SHA256_Update(&c, (unsigned char *)":", 1);
  SHA256_Update(&c, nonceb, 16);
  SHA256_Update(&c, (unsigned char *)":", 1);
  SHA256_Update(&c, extra, extralen);
  SHA256_Final(out, &c);

  SHA256_Init(&c);
  SHA256_Update(&c, out, 32);
  SHA256_Final(out, &c);
}

int nterfacer_line_event(struct esocket *sock, char *newline) {
  struct sconnect *socket = sock->tag;
  char *response, *theirnonceh = NULL, *theirivh = NULL;
  unsigned char theirnonce[16], theiriv[16];
  int number, reason;

  switch(socket->status) {
    case SS_IDLE:
      if(strcasecmp(newline, ANTI_FULL_VERSION)) {
        nterface_log(nrl, NL_INFO, "Protocol mismatch from %s: %s", socket->permit->hostname->content, newline);
        return 1;
      } else {
        unsigned char challenge[32];
        char ivhex[16 * 2 + 1], noncehex[16 * 2 + 1];

        if(!get_entropy(challenge, 32) || !get_entropy(socket->iv, 16)) {
          nterface_log(nrl, NL_ERROR, "Unable to open challenge/IV entropy bin!");
          return 1;
        }

        int_to_hex(challenge, socket->challenge, 32);
        int_to_hex(socket->iv, ivhex, 16);

        memcpy(socket->response, challenge_response(socket->challenge, socket->permit->password->content), sizeof(socket->response));
        socket->response[sizeof(socket->response) - 1] = '\0'; /* just in case */

        socket->status = SS_VERSIONED;
        if(!generate_nonce(socket->ournonce, 1)) {
          nterface_log(nrl, NL_ERROR, "Unable to generate nonce!");
          return 1;
        }
        int_to_hex(socket->ournonce, noncehex, 16);

        if(esocket_write_line(sock, "%s %s %s", socket->challenge, ivhex, noncehex))
           return BUF_ERROR;
        return 0;
      }
      break;
    case SS_VERSIONED:
      for(response=newline;*response;response++) {
        if((*response == ' ') && (*(response + 1))) {
          *response = '\0';
          theirivh = response + 1;
          break;
        }
      }

      if(theirivh) {
        for(response=theirivh;*response;response++) {
          if((*response == ' ') && (*(response + 1))) {
            *response = '\0';
            theirnonceh = response + 1;
            break;
          }
        }
      }

      if(!theirivh || (strlen(theirivh) != 32) || !hex_to_int(theirivh, theiriv, sizeof(theiriv)) ||
         !theirnonceh || (strlen(theirnonceh) != 32) || !hex_to_int(theirnonceh, theirnonce, sizeof(theirnonce))) {
        nterface_log(nrl, NL_INFO, "Protocol error drop: %s", socket->permit->hostname->content);
        return 1;
      }

      if(!memcmp(socket->ournonce, theirnonce, sizeof(theirnonce))) {
        nterface_log(nrl, NL_INFO, "Bad nonce drop: %s", socket->permit->hostname->content);
        return 1;
      }

      if(!strncasecmp(newline, socket->response, sizeof(socket->response))) {
        unsigned char theirkey[32], ourkey[32];

        derive_key(ourkey, socket->permit->password->content, socket->challenge, socket->ournonce, theirnonce, (unsigned char *)"SERVER", 6);

        derive_key(theirkey, socket->permit->password->content, socket->response, theirnonce, socket->ournonce, (unsigned char *)"CLIENT", 6);
        nterface_log(nrl, NL_INFO, "Authed: %s", socket->permit->hostname->content);
        socket->status = SS_AUTHENTICATED;
        switch_buffer_mode(sock, ourkey, socket->iv, theirkey, theiriv);

        if(esocket_write_line(sock, "Oauth"))
          return BUF_ERROR;
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
          if(esocket_write_line(sock, "%d,E%d,%s", number, reason, request_error(reason)))
            return BUF_ERROR;
          return 0;
        } else {
          return 1;
        }
      }
      break;
  }

  return 0;
}

int nterfacer_new_rline(char *line, struct esocket *socket, int *number) {
  char *sp, *p, *parsebuf = NULL, *pp, commandbuf[MAX_BUFSIZE], *args[MAX_ARGS], *newp;
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
    parsebuf = (char *)ntmalloc(strlen(pp) + 1);
    MemCheckR(parsebuf, RE_MEM_ERROR);
  
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
          ntfree(parsebuf);
          return RE_TOO_MANY_ARGS;
        }
      } else {
        *newp++ = *pp;
      }
    }
    *newp = '\0';
  }
  if(argcount < hl->args) {
    if(argcount && parsebuf)
      ntfree(parsebuf);
    return RE_WRONG_ARG_COUNT;
  }

  prequest = (struct rline *)ntmalloc(sizeof(struct rline));
  if(!prequest) {
    MemError();
    if(argcount && parsebuf)
      ntfree(parsebuf);
    return RE_MEM_ERROR;
  }

  prequest->service = service;
  prequest->handler = hl;
  prequest->buf[0] = '\0';
  prequest->curpos = prequest->buf;
  prequest->tag = NULL;
  prequest->id = *number;
  prequest->next = rlines;
  prequest->socket = socket;
  prequest->callback = NULL;

  rlines = prequest;
  re = (hl->function)(prequest, argcount, args);
  
  if(argcount && parsebuf)
    ntfree(parsebuf);

  return re;
}

void nterfacer_disconnect_event(struct esocket *sock) {
  struct sconnect *socket = sock->tag;
  struct rline *li;
  /* not tested */

  nterface_log(nrl, NL_INFO, "Disconnected from %s.", socket->permit->hostname->content);

  /* not tested */
  for(li=rlines;li;li=li->next)
    if(li->socket && (li->socket->tag == socket))
      li->socket = NULL;

  ntfree(socket);
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
  int retval = RE_OK;

  if(li->socket || li->callback) {
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    for(tp=escapedbuf,p=buf;*p||(*tp='\0');*tp++=*p++)
      if((*p == ',') || (*p == '\\'))
        *tp++ = '\\';
    if(li->socket) {
      if(esocket_write_line(li->socket, "%d,OE%d,%s", li->id, error_code, escapedbuf))
        retval = RE_SOCKET_ERROR;
    } else {
      if(error_code == 0) /* :P */
        error_code = -10000;

      nterfacer_sendcallback(li, error_code, escapedbuf);
    }
  }

  for(pp=rlines;pp;lp=pp,pp=pp->next) {
    if(pp == li) {
      if(lp) {
        lp->next = li->next;
      } else {
        rlines = li->next;
      }
      ntfree(li);
      break;
    }
  }
  
  return retval;
}

int ri_final(struct rline *li) {
  struct rline *pp, *lp = NULL;
  int retval = RE_OK;

  if(li->socket) {
    if(esocket_write_line(li->socket, "%d,OO%s", li->id, li->buf))
      retval = RE_SOCKET_ERROR;
  } else if(li->callback) {
    nterfacer_sendcallback(li, 0, li->buf);
  }

  for(pp=rlines;pp;lp=pp,pp=pp->next) {
    if(pp == li) {
      if(lp) {
        lp->next = li->next;
      } else {
        rlines = li->next;
      }
      ntfree(li);
      break;
    }
  }

  return retval;
}

int ping_handler(struct rline *ri, int argc, char **argv) {
  ri_append(ri, "OK");
  return ri_final(ri);
}

struct sched_rline {
  rline rl;
  int argc;
  handler *hl;
  void *schedule;
  char argv[];
};

static const int XMAXARGS = 50;

static void execrline(void *arg) {
  struct sched_rline *sr = arg;
  int re, i;
  char *argv[XMAXARGS], *buf;

  sr->schedule = NULL;

  buf = sr->argv;
  for(i=0;i<sr->argc;i++) {
    argv[i] = buf;
    buf+=strlen(buf) + 1;
  }

  re = (sr->hl->function)(&sr->rl, sr->argc, argv);

  if(re)
    Error("nterfacer", ERR_WARNING, "sendline: error occured calling %p %d: %s", sr->hl->function, re, request_error(re));
}

void *nterfacer_sendline(char *service, char *command, int argc, char **argv, rline_callback callback, void *tag) {
  struct service_node *servicep;
  struct rline *prequest;
  struct sched_rline *sr;
  struct handler *hl;
  int totallen, i;
  char *buf;

  for(servicep=tree;servicep;servicep=servicep->next)
    if(!strcmp(servicep->name->content, service))
      break;

  if(argc > XMAXARGS)
    Error("nterfacer", ERR_STOP, "Over maximum arguments.");

  if(!servicep) {
    Error("nterfacer", ERR_WARNING, "sendline: service not found: %s", service);
    return NULL;
  }

  for(hl=servicep->handlers;hl;hl=hl->next)
    if(!strcmp(hl->command->content, command))
      break;

  if(!hl) {
    Error("nterfacer", ERR_WARNING, "sendline: command not found: %s", command);
    return NULL;
  }

  if(argc < hl->args) {
    Error("nterfacer", ERR_WARNING, "sendline: wrong number of arguments: %s", command);
    return NULL;
  }

  /* we have to create a copy of the arguments for reentrancy reasons, grr */
  totallen = 0;
  for(i=0;i<argc;i++)
    totallen+=strlen(argv[i]) + 1;

  /* HACKY but allows existing code to still work */
  sr = (struct sched_rline *)ntmalloc(sizeof(struct sched_rline) + totallen);
  if(!sr) {
    MemError();
    return NULL;
  }
  prequest = &sr->rl;

  sr->argc = argc;
  buf = sr->argv;
  for(i=0;i<argc;i++) {
    size_t len = strlen(argv[i]) + 1;
    memcpy(buf, argv[i], len);
    buf+=len;
  }
  sr->hl = hl;

  prequest->service = servicep;
  prequest->handler = hl;
  prequest->buf[0] = '\0';
  prequest->curpos = prequest->buf;
  prequest->tag = tag;
  prequest->id = 0;
  prequest->socket = NULL;
  prequest->callback = callback;

  prequest->next = rlines;
  rlines = prequest;

  scheduleoneshot(time(NULL), execrline, sr);

  return (void *)sr;
}

void nterfacer_freeline(void *tag) {
  struct sched_rline *prequest = tag;

  prequest->rl.callback = NULL;
  if(prequest->schedule)
    deleteschedule(prequest->schedule, execrline, NULL);
}

#define MAX_LINES 8192

/* this bites */
static void nterfacer_sendcallback(struct rline *ri, int error, char *buf) {
  char *lines[MAX_LINES+1];
  char newbuf[MAX_BUFSIZE+5];
  char *s, *d, *laststart;
  int linec = 0;

  for(s=buf,laststart=d=newbuf;*s;s++) {
    if((*s == '\\') && *(s + 1)) {
      if(*(s + 1) == ',') {
        *d++ = ',';
      } else if(*(s + 1) == '\\') {
        *d++ = '\\';
      }
      s++;
    } else if(*s == ',') {
      *d++ = '\0';
      if(linec >= MAX_LINES - 5) {
        nterfacer_sendcallback(ri, BF_OVER, "Buffer overflow.");
        return;
      }

      lines[linec++] = laststart;
      laststart = d;
    } else {
      *d++ = *s;
    }
  }
  *d = '\0';
  lines[linec++] = laststart;

  ri->callback(error, linec, lines, ri->tag);
}
