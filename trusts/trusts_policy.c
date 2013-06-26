#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef __USE_MISC
#define __USE_MISC /* inet_aton */
#endif

#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <errno.h>
#include <fcntl.h>

#include "../lib/version.h"
#include "../lib/hmac.h"
#include "../core/events.h"
#include "../core/schedule.h"
#include "../core/nsmalloc.h"
#include "../core/hooks.h"
#include "../core/config.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../glines/glines.h"
#include "trusts.h"

MODULE_VERSION("");

static int countext, enforcepolicy_irc, enforcepolicy_auth;

#define TRUSTBUFSIZE 8192
#define TRUSTPASSLEN 128
#define NONCELEN 16

typedef struct trustsocket {
  int fd;
  int authed;
  char authuser[SERVERLEN+1];
  char buf[TRUSTBUFSIZE];
  unsigned char nonce[NONCELEN];
  int size;
  time_t connected;
  time_t timeout;
  int accepted;
  int rejected;

  struct trustsocket *next;
} trustsocket;

static trustsocket *tslist;
static int listenerfd = -1;
static FILE *urandom;

typedef struct trustaccount {
  int used;
  char server[SERVERLEN+1];
  char password[TRUSTPASSLEN+1];
} trustaccount;

trustaccount trustaccounts[MAXSERVERS];

static int checkconnectionth(const char *username, struct irc_in_addr *ip, trusthost *th, int hooknum, int usercountadjustment, char *message, size_t messagelen) {
  trustgroup *tg;

  if(messagelen>0)
    message[0] = '\0';
  
  if(!th || !trustsdbloaded)
    return POLICY_SUCCESS;

  tg = th->group;

  /*
   * the purpose of this logic is to avoid spam like this:
   * WARNING: tgX exceeded limit: 11 connected vs 10 max
   * (goes back down to 10)
   * WARNING: tgX exceeded limit: 11 connected vs 10 max
   */

  if(hooknum == HOOK_TRUSTS_NEWNICK) {
    patricia_node_t *head, *node;
    int nodecount = 0;

    head = refnode(iptree, ip, th->nodebits);
    PATRICIA_WALK(head, node)
    {
      nodecount += node->usercount;
    }
    PATRICIA_WALK_END;
    derefnode(iptree, head);

    if(th->maxpernode && nodecount + usercountadjustment > th->maxpernode) {
      controlwall(NO_OPER, NL_TRUSTS, "Hard connection limit exceeded on subnet: %s (group: %s): %d connected, %d max.", trusts_cidr2str(ip, th->nodebits), tg->name->content, nodecount + usercountadjustment, th->maxpernode);
      snprintf(message, messagelen, "Too many connections from your host (%s) - see http://www.quakenet.org/help/trusts/connection-limit for details.", IPtostr(*ip));
      return POLICY_FAILURE_NODECOUNT;
    }

    if(tg->trustedfor && tg->count + usercountadjustment > tg->trustedfor) {
      if(tg->count > (long)tg->exts[countext]) {
        tg->exts[countext] = (void *)(long)tg->count;

        controlwall(NO_OPER, NL_TRUSTS, "Hard connection limit exceeded (group %s): %d connected, %d max.", tg->name->content, tg->count + usercountadjustment, tg->trustedfor);
        snprintf(message, messagelen, "Too many connections from your trust (%s) - see http://www.quakenet.org/help/trusts/connection-limit for details.", IPtostr(*ip));
      }

      snprintf(message, messagelen, "Too many connections from your trust (%s) - see http://www.quakenet.org/help/trusts/connection-limit for details.", IPtostr(*ip));
      return POLICY_FAILURE_GROUPCOUNT;
    }

    if((tg->flags & TRUST_ENFORCE_IDENT) && (username[0] == '~')) {
      controlwall(NO_OPER, NL_TRUSTS, "Ident required: %s@%s (group: %s).", username, IPtostr(*ip), tg->name->content);
      snprintf(message, messagelen, "IDENTD required from your host (%s) - see http://www.quakenet.org/help/trusts/connection-limit for details.", IPtostr(*ip));
      return POLICY_FAILURE_IDENTD;
    }

    if(tg->maxperident > 0) {
      int identcount = 0;
      trusthost *th2;
      nick *tnp;

      for(th2=tg->hosts;th2;th2=th2->next) {
        for(tnp=th2->users;tnp;tnp=nextbytrust(tnp)) {
          if(!ircd_strcmp(tnp->ident, username))
            identcount++;
        }
      }

      if(identcount + usercountadjustment > tg->maxperident) {
        controlwall(NO_OPER, NL_TRUSTS, "Hard ident limit exceeded: %s@%s (group: %s): %d connected, %d max.", username, IPtostr(*ip), tg->name->content, identcount + usercountadjustment, tg->maxperident);
        snprintf(message, messagelen, "Too many connections from your username (%s@%s) - see http://www.quakenet.org/help/trusts/connection-limit for details.", username, IPtostr(*ip));
        return POLICY_FAILURE_IDENTCOUNT;
      }
    }
  } else {
    if(tg->count < tg->maxusage)
      tg->exts[countext] = (void *)(long)tg->count;
  }

  if(tg->trustedfor > 0)
    snprintf(message, messagelen, "Trust has %d out of %d allowed connections.", tg->count + usercountadjustment, tg->trustedfor);

  return POLICY_SUCCESS;
}

static int checkconnection(const char *username, struct irc_in_addr *ip, int hooknum, int cloneadjustment, char *message, size_t messagelen) {
  struct irc_in_addr ip_canonicalized;
  ip_canonicalize_tunnel(&ip_canonicalized, ip);

  return checkconnectionth(username, &ip_canonicalized, th_getbyhost(&ip_canonicalized), hooknum, cloneadjustment, message, messagelen);
}

static int trustdowrite(trustsocket *sock, char *format, ...) {
  char buf[1024];
  va_list va;
  int r;

  va_start(va, format);
  r = vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  if(r >= sizeof(buf))
    r = sizeof(buf) - 1;

  buf[r] = '\n';

  if(write(sock->fd, buf, r + 1) != r + 1)
    return 0;
  return 1;
}

static int policycheck_auth(trustsocket *sock, const char *sequence_id, const char *username, const char *host) {
  char message[512];
  int verdict;
  struct irc_in_addr ip;
  unsigned char bits;
  
  if(!ipmask_parse(host, &ip, &bits)) {
    sock->accepted++;
    return trustdowrite(sock, "PASS %s", sequence_id);
  }
  
  verdict = checkconnection(username, &ip, HOOK_TRUSTS_NEWNICK, 1, message, sizeof(message));

  if(!enforcepolicy_auth)
    verdict = POLICY_SUCCESS;

  if (verdict == POLICY_SUCCESS) {
    sock->accepted++;

    if(message[0])
      return trustdowrite(sock, "PASS %s %s", sequence_id, message);
    else
      return trustdowrite(sock, "PASS %s", sequence_id);
  } else {
    sock->rejected++;

    controlwall(NO_OPER, NL_TRUSTS, "Rejected connection from %s@%s using IAuth: %s", username, host, message);
    return trustdowrite(sock, "KILL %s %s", sequence_id, message);
  }
}

static int trustkillconnection(trustsocket *sock, char *reason) {
  trustdowrite(sock, "QUIT %s", reason);
  return 0;
}

static void trustfreeconnection(trustsocket *sock, int unlink) {
  trustsocket **pnext = &tslist;

  if(!unlink) {
    deregisterhandler(sock->fd, 1);
    nsfree(POOL_TRUSTS, sock);
    return;
  }

  for(trustsocket *ts=tslist;ts;ts=ts->next) {
    if(ts == sock) {
      *pnext = sock->next;
      trustfreeconnection(sock, 0);
      break;
    }
    
    pnext = &(sock->next);
  }
}

static int handletrustauth(trustsocket *sock, char *server_name, char *mac) {
  int i;
  char *password = NULL;
  unsigned char digest[16];
  char noncehexbuf[NONCELEN * 2 + 1];
  char hexbuf[sizeof(digest) * 2 + 1];

  for(i=0;i<MAXSERVERS;i++) {
    if(trustaccounts[i].used && strcmp(trustaccounts[i].server, server_name) == 0) {
      password = trustaccounts[i].password;
      break;
    }
  }

  if (!password) {
    controlwall(NO_OPER, NL_TRUSTS, "Invalid servername for policy socket: '%s'", server_name);
    return trustkillconnection(sock, "Invalid servername.");
  }

  hmacmd5 h;
  hmacmd5_init(&h, (unsigned char *)password, strlen(password));
  hmacmd5_update(&h, (unsigned char *)hmac_printhex(sock->nonce, noncehexbuf, NONCELEN), NONCELEN * 2);
  hmacmd5_final(&h, digest);
  if(hmac_strcmp(mac, hmac_printhex(digest, hexbuf, sizeof(digest)))) {
    controlwall(NO_OPER, NL_TRUSTS, "Invalid password for policy socket with servername '%s'.", server_name);
    return trustkillconnection(sock, "Bad MAC.");
  }

  sock->authed = 1;
  strncpy(sock->authuser, server_name, SERVERLEN);

  controlwall(NO_OPER, NL_TRUSTS, "Successful authentication for policy socket with servername '%s'.", server_name);
  return trustdowrite(sock, "AUTHOK");
}

#define MAXTOKENS 10
static int handletrustline(trustsocket *sock, char *line) {
  char *command, *p, *lastpos;
  char *tokens[MAXTOKENS];
  int tokensfound = -1;

  for(command=lastpos=p=line;*p;p++) {
    if(*p == ' ') {
      *p = '\0';
      if(tokensfound == MAXTOKENS)
        return trustkillconnection(sock, "too many tokens");

      if(tokensfound >= 0) {
        tokens[tokensfound++] = lastpos;
      } else {
        tokensfound++;
      }
      lastpos = p + 1;
    }
  }
  if(lastpos != p) {
    if(tokensfound == MAXTOKENS)
      return trustkillconnection(sock, "too many tokens");
    tokens[tokensfound++] = lastpos;
  }

  if(!sock->authed && !strcmp("AUTH", command)) {
    if(tokensfound != 2)
      return trustkillconnection(sock, "incorrect arg count for command.");

    return handletrustauth(sock, tokens[0], tokens[1]);
  } else if(sock->authed && !strcmp("CHECK", command)) {
    if(tokensfound != 3)
      return trustkillconnection(sock, "incorrect arg count for command.");

    policycheck_auth(sock, tokens[0], tokens[1], tokens[2]);
    return 1;
  } else {
    Error("trusts_policy", ERR_WARNING, "Bad command: %s", command);
    return 0;
  }
}

static trustsocket *findtrustsocketbyfd(int fd) {
  for(trustsocket *ts=tslist;ts;ts=ts->next)
    if(ts->fd==fd)
      return ts;
    
  return NULL;
}

static int handletrustclient(trustsocket *sock) {
  int r, remaining = TRUSTBUFSIZE - sock->size, i;
  char *lastpos, *c;

  if(!remaining) {
    trustkillconnection(sock, "Buffer overflow.");
    return 0;
  }

  r = read(sock->fd, sock->buf + sock->size, remaining);
  if(r <= 0)
    return 0;

  sock->size+=r;
  lastpos = sock->buf;

  for(c=sock->buf,i=0;i<sock->size;i++,c++) {
    if(*c != '\n')
      continue;
    *c = '\0';
    if(!handletrustline(sock, lastpos))
      return 0;

    lastpos = c + 1; /* is this ok? */
  }
  sock->size-=lastpos - sock->buf;
  memmove(sock->buf, lastpos, sock->size);
  
  return 1;
}

static void processtrustclient(int fd, short events) {
  trustsocket *sock = findtrustsocketbyfd(fd);

  if(!sock)
    return;
  
  if(events & POLLIN)
    if(!handletrustclient(sock))
      trustfreeconnection(sock, 1);
}

static void trustdotimeout(void *arg) {
  time_t t = time(NULL);
  trustsocket **pnext, *next, *sock;

  pnext = &tslist;
    
  for(sock=tslist;sock;) {
    next = sock->next;

    if(!sock->authed && t >= sock->timeout) {
      trustkillconnection(sock, "Auth timeout.");
      *pnext = sock->next;
      trustfreeconnection(sock, 0);
    } else {
      pnext = &(sock->next);
    }
    
    sock=next;
  }
}

static void processtrustlistener(int fd, short events) {
  if(events & POLLIN) {
    trustsocket *sock;
    char buf[NONCELEN * 2 + 1];

    int newfd = accept(fd, NULL, NULL), flags;
    if(newfd == -1)
      return;

    flags = fcntl(newfd, F_GETFL, 0);
    if(flags < 0) {
      Error("trusts_policy", ERR_WARNING, "Unable to set socket non-blocking.");
      close(newfd);
      return;
    }

    if(fcntl(fd, F_SETFL, flags|O_NONBLOCK) < 0) {
      Error("trusts_policy", ERR_WARNING, "Unable to set socket non-blocking.");
      close(newfd);
      return;
    }

    registerhandler(newfd, POLLIN|POLLERR|POLLHUP, processtrustclient);
      
    sock = nsmalloc(POOL_TRUSTS, sizeof(trustsocket));
    if(!sock) {
      deregisterhandler(newfd, 1);
      return;
    }
    
    sock->fd = newfd;
    sock->next = tslist;
    tslist = sock;
    
    if(fread((char *)sock->nonce, 1, NONCELEN, urandom) != NONCELEN) {
      Error("trusts_policy", ERR_WARNING, "Error getting random bytes.");
      deregisterhandler(newfd, 1);
      tslist = sock->next;
      nsfree(POOL_TRUSTS, sock);
    } else {
      sock->authed = 0;
      sock->size = 0;
      sock->connected = time(NULL);
      sock->timeout = time(NULL) + 30;
      sock->accepted = 0;
      sock->rejected = 0;
      if(!trustdowrite(sock, "AUTH %s", hmac_printhex(sock->nonce, buf, NONCELEN))) {
        Error("trusts_policy", ERR_WARNING, "Error writing auth to fd %d.", newfd);
        deregisterhandler(newfd, 1);
        tslist = sock->next;
        nsfree(POOL_TRUSTS, sock);
        return;
      }
    }
  }
}

static int createlistenersock(int port) {
  struct sockaddr_in s;
  int fd;
  int optval;

  memset(&s, 0, sizeof(struct sockaddr_in));
  s.sin_family = AF_INET;
  s.sin_addr.s_addr = INADDR_ANY;
  s.sin_port = htons(port);

  fd = socket(PF_INET, SOCK_STREAM, 0);
  if(fd < 0) {
    Error("trusts_policy", ERR_WARNING, "Unable to get socket for trustfd.");
    return -1;
  }

  optval = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  if(bind(fd, (struct sockaddr *)&s, sizeof(struct sockaddr_in)) < 0) {
    Error("trusts_policy", ERR_WARNING, "Unable to bind trustfd.");
    close(fd);
    return -1;
  }

  if(listen(fd, 5) < 0) {
    Error("trusts_policy", ERR_WARNING, "Unable to listen on trustfd.");
    close(fd);
    return -1;
  }

  registerhandler(fd, POLLIN, processtrustlistener);

  return fd;
}

static void policycheck_irc(int hooknum, void *arg) {
  void **args = arg;
  nick *np = args[0];
  long moving = (long)args[1];
  char message[512];
  int verdict;

  if(moving)
    return;

  verdict = checkconnectionth(np->ident, &np->p_nodeaddr, gettrusthost(np), hooknum, 0, message, sizeof(message));
    
  if(!enforcepolicy_irc)
    verdict = POLICY_SUCCESS;

  switch (verdict) {
    case POLICY_FAILURE_NODECOUNT:
      glinebynick(np, POLICY_GLINE_DURATION, message, GLINE_IGNORE_TRUST, "trusts_policy");
      break;
    case POLICY_FAILURE_IDENTD:
      glinebyip("~*", &np->p_ipaddr, 128, POLICY_GLINE_DURATION, message, GLINE_ALWAYS_USER|GLINE_IGNORE_TRUST, "trusts_policy");
      break;
    case POLICY_FAILURE_IDENTCOUNT:
      glinebynick(np, POLICY_GLINE_DURATION, message, GLINE_ALWAYS_USER|GLINE_IGNORE_TRUST, "trusts_policy");
      break;
  }
}

static int trusts_cmdtrustpolicyirc(void *source, int cargc, char **cargv) {
  nick *sender = source;

  if(cargc < 1) {
    controlreply(sender, "Use of glines for trust policy enforcement is currently %s.", enforcepolicy_irc?"enabled":"disabled");
    return CMD_OK;
  }

  enforcepolicy_irc = atoi(cargv[0]);
  controlwall(NO_OPER, NL_TRUSTS, "%s %s use of glines for trust policy enforcement.", controlid(sender), enforcepolicy_irc?"enabled":"disabled");
  controlreply(sender, "Use of glines for trust policy enforcement is now %s.", enforcepolicy_irc?"enabled":"disabled");

  return CMD_OK;
}

static int trusts_cmdtrustpolicyauth(void *source, int cargc, char **cargv) {
  nick *sender = source;

  if(cargc < 1) {
    controlreply(sender, "Trust policy enforcement with IAuth is currently %s.", enforcepolicy_auth?"enabled":"disabled");
    return CMD_OK;
  }

  enforcepolicy_auth = atoi(cargv[0]);
  controlwall(NO_OPER, NL_TRUSTS, "%s %s trust policy enforcement with IAuth.", controlid(sender), enforcepolicy_auth?"enabled":"disabled");
  controlreply(sender, "Trust policy enforcement with IAuth is now %s.", enforcepolicy_auth?"enabled":"disabled");

  return CMD_OK;
}


static int trusts_cmdtrustsockets(void *source, int cargc, char **cargv) {
  nick *sender = source;
  time_t now;
  trustsocket *sock;

  time(&now);

  controlreply(sender, "Server                   Connected for             Accepted        Rejected");

  for(sock=tslist;sock;sock=sock->next)
    controlreply(sender, "%-20s %20s %10d %15d", sock->authed?sock->authuser:"<not authenticated>", longtoduration(now - sock->connected, 0), sock->accepted, sock->rejected);

  controlreply(sender, "-- End of list.");
  return CMD_OK;
}

void loadtrustaccounts(void) {
  array *accts;

  memset(trustaccounts, 0, sizeof(trustaccounts));

  accts = getconfigitems("trusts_policy", "server");
  if(!accts) {
    Error("trusts_policy", ERR_INFO, "No servers added.");
  } else {
    sstring **servers = (sstring **)(accts->content);
    int i;
    for(i=0;i<accts->cursi;i++) {
      char server[512];
      char *pos;

      if(i>=MAXSERVERS) {
        Error("trusts_policy", ERR_INFO, "Too many servers specified.");
        break;
      }

      strncpy(server, servers[i]->content, sizeof(server));

      pos = strchr(server, ',');

      if(!pos) {
        Error("trusts_policy", ERR_INFO, "Server line is missing password: %s", server);
        continue;
      }

      *pos = '\0';

      trustaccounts[i].used = 1;
      strncpy(trustaccounts[i].server, server, SERVERLEN);
      strncpy(trustaccounts[i].password, pos+1, TRUSTPASSLEN);
    }
  }
}

static void trustaccounts_rehash(int hooknum, void *arg) {
  loadtrustaccounts();
}

void _init(void) {
  sstring *m;
  int trustport;

  countext = registertgext("count");
  if(countext == -1)
    return;

  m = getconfigitem("trusts_policy", "enforcepolicy_irc");
  if(m)
    enforcepolicy_irc = atoi(m->content);

  m = getconfigitem("trusts_policy", "enforcepolicy_auth");
  if(m)
    enforcepolicy_auth = atoi(m->content);

  m = getconfigitem("trusts_policy", "trustport");
  if(m)
    trustport = atoi(m->content);
  else
    trustport = DEFAULT_TRUSTPORT;

  if(trustport)
    listenerfd = createlistenersock(trustport);

  loadtrustaccounts();

  registerhook(HOOK_TRUSTS_NEWNICK, policycheck_irc);
  registerhook(HOOK_TRUSTS_LOSTNICK, &policycheck_irc);
  registerhook(HOOK_CORE_REHASH, trustaccounts_rehash);

  registercontrolhelpcmd("trustpolicyirc", NO_DEVELOPER, 1, trusts_cmdtrustpolicyirc, "Usage: trustpolicyirc ?1|0?\nEnables or disables policy enforcement (IRC). Shows current status when no parameter is specified.");
  registercontrolhelpcmd("trustpolicyauth", NO_DEVELOPER, 1, trusts_cmdtrustpolicyauth, "Usage: trustpolicyauth ?1|0?\nEnables or disables policy enforcement (IAuth). Shows current status when no parameter is specified.");
  registercontrolhelpcmd("trustsockets", NO_DEVELOPER, 0, trusts_cmdtrustsockets, "Usage: trustsockets\nLists all currently active TRUST sockets.");

  schedulerecurring(time(NULL)+1, 0, 5, trustdotimeout, NULL);
  
  urandom = fopen("/dev/urandom", "rb");
  if(!urandom)
    Error("trusts_policy", ERR_ERROR, "Couldn't open /dev/urandom.");
}

void _fini(void) {
  trustsocket *sock, *next;

  if(countext == -1)
    return;

  releasetgext(countext);

  deregisterhook(HOOK_TRUSTS_NEWNICK, policycheck_irc);
  deregisterhook(HOOK_TRUSTS_LOSTNICK, policycheck_irc);
  deregisterhook(HOOK_CORE_REHASH, trustaccounts_rehash);

  deregistercontrolcmd("trustpolicyirc", trusts_cmdtrustpolicyirc);
  deregistercontrolcmd("trustpolicyauth", trusts_cmdtrustpolicyauth);
  deregistercontrolcmd("trustsockets", trusts_cmdtrustsockets);
  
  deleteallschedules(trustdotimeout); 
 
  if (urandom)
    fclose(urandom);

  if (listenerfd != -1)
    deregisterhandler(listenerfd, 1);

  for(sock=tslist;sock;) {
    next = sock->next;

    trustkillconnection(sock, "Unloading module.");
    trustfreeconnection(sock, 0);

    sock = next;
  }
}
