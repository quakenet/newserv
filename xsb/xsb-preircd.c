#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "../core/hooks.h"
#include "../lib/splitline.h"
#include "../core/config.h"
#include "../irc/irc.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../localuser/localuser.h"
#include "../lib/strlfunc.h"
#include "../lib/sha1.h"
#include "../irc/irc.h"
#include "../lib/hmac.h"
#include "../core/schedule.h"
#include "xsb.h"

static const char *DEFAULT_SERVICE_MASKS[] = { "services*.*.quakenet.org", (char *)0 };

static array defaultservicemasks_a;
static sstring **defaultservicemasks;

static void handlemaskprivmsg(int, void *);
static CommandTree *cmds;

struct messagequeue {
  struct messagequeue *next;
  long numeric;
  char buf[];
};

struct messagequeue *head, *tail;

static nick *xsbnick;
static void *xsbnicksched;

static void handler(nick *target, int messagetype, void **args);
static void flushqueue(void);

static char *servicenick(char *server) {
  static char nickbuf[NICKLEN+1];
  char digest[SHA1_DIGESTSIZE*2+1];
  unsigned char digestbuf[SHA1_DIGESTSIZE];
  SHA1_CTX s;

  SHA1Init(&s);
  SHA1Update(&s, (unsigned char *)server, strlen(server));
  SHA1Final(digestbuf, &s);

  snprintf(nickbuf, sizeof(nickbuf), "XSB1-%s", hmac_printhex(digestbuf, digest, SHA1_DIGESTSIZE));

  return nickbuf;
}

static void setuplocaluser(void *arg) {
  xsbnicksched = NULL;

  xsbnick = registerlocaluser(servicenick(myserver->content), "xsb", "xsb.quakenet.org", "eXtended service broadcast localuser (v1)", NULL, UMODE_OPER | UMODE_SERVICE | UMODE_INV, handler);
  if(!xsbnick) {
    xsbnicksched = scheduleoneshot(time(NULL)+1, setuplocaluser, NULL);
  } else {
    flushqueue();
  }
}

void _init(void) {
  const char **p;
  array *servicemasks;

  cmds = newcommandtree();
  if(!cmds)
    return;

  xsbnicksched = scheduleoneshot(time(NULL)+1, setuplocaluser, NULL);
  
  registerhook(HOOK_NICK_MASKPRIVMSG, &handlemaskprivmsg);
  
  array_init(&defaultservicemasks_a, sizeof(sstring *));

  for(p=DEFAULT_SERVICE_MASKS;*p;p++) {
    int i = array_getfreeslot(&defaultservicemasks_a);
    defaultservicemasks = (sstring **)defaultservicemasks_a.content;

    defaultservicemasks[i] = getsstring(*p, strlen(*p));
  }

  servicemasks = getconfigitems("xsb", "servicemask");
  if(!servicemasks || !servicemasks->cursi)
    Error("xsb", ERR_WARNING, "No service masks in config file (xsb/servicemask), using defaults.");
}

void _fini(void) {
  int i;
  struct messagequeue *q, *nq;

  if(xsbnick)
    deregisterlocaluser(xsbnick, NULL);

  if(xsbnicksched)
    deleteschedule(xsbnicksched, setuplocaluser, NULL);

  if(!cmds)
    return;

  destroycommandtree(cmds);

  deregisterhook(HOOK_NICK_MASKPRIVMSG, &handlemaskprivmsg);

  for(i=0;i<defaultservicemasks_a.cursi;i++)
    freesstring(defaultservicemasks[i]);
  array_free(&defaultservicemasks_a);

  for(q=head;q;q=nq) {
    nq = q->next;
    free(q);
  }
  
  head = NULL;
  tail = NULL;
}

void xsb_addcommand(const char *name, const int maxparams, CommandHandler handler) {
  addcommandtotree(cmds, name, 0, maxparams, handler);
}

void xsb_delcommand(const char *name, CommandHandler handler) {
  deletecommandfromtree(cmds, name, handler);
}

static void handlemaskprivmsg(int hooknum, void *args) {
  void **hargs = (void **)args;
  nick *source = hargs[0];
  char *message = hargs[2];
  Command *cmd;
  int cargc;
  char *cargv[50];
  server *s;

  if(!source)
    return;

  if(!IsOper(source) || !IsService(source))
    return;

  cargc = splitline(message, cargv, 50, 0);
  if(cargc < 2)
    return;

  if(strcmp(cargv[0], "XSB1"))
    return;

  s = &serverlist[homeserver(source->numeric)];
  if(s->linkstate != LS_LINKED) {
    Error("xsb", ERR_WARNING, "Got XSB message from unlinked server (%s): %s", s->name->content, source->nick);
    return;
  }

#ifndef XSB_DEBUG
  if(!(s->flags & SMODE_SERVICE)) {
    Error("xsb", ERR_WARNING, "Got XSB message from non-service server (%s): %s", s->name->content, source->nick);
    return;
  }
#endif

  cmd = findcommandintree(cmds, cargv[1], 1);
  if(!cmd)
    return;

  if(cmd->maxparams < (cargc - 2)) {
    rejoinline(cargv[cmd->maxparams], cargc - (cmd->maxparams));
    cargc = (cmd->maxparams) + 2;
  }

  (cmd->handler)(source, cargc - 2, &cargv[2]);
}

static void directsend(struct messagequeue *q) {
  nick *np = getnickbynumeric(q->numeric);
  if(!np || !IsOper(np) || !IsService(np))
    return;

#ifndef XSB_DEBUG
  if(!(serverlist[homeserver(np->numeric)].flags & SMODE_SERVICE))
    return;
#endif

  sendmessagetouser(xsbnick, np, "XSB1 %s", q->buf);
}

static void flushqueue(void) {
  struct messagequeue *q, *nq;
    
  for(q=head;q;q=nq) {
    nq = q->next;

    directsend(q);
    free(q);
  }
  head = NULL;
  tail = NULL;
}

static int getservicemasks(sstring ***masks) {
  array *aservicemasks = getconfigitems("xsb", "servicemask");
  if(!aservicemasks || !aservicemasks->cursi)
    aservicemasks = &defaultservicemasks_a;

  *masks = (sstring **)aservicemasks->content;

  return aservicemasks->cursi;
}

static void xsb_send(nick *target, const char *format, ...) {
  char buf[512];
  va_list va;
  size_t len;
  struct messagequeue *q;

  va_start(va, format);
  len = vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);
  if(len >= sizeof(buf))
    len = sizeof(buf);

  /* sucks */
  q = (struct messagequeue *)malloc(sizeof(struct messagequeue) + len + 5);
  q->numeric = target->numeric;
  strlcpy(q->buf, buf, len + 2);

  if(xsbnick) {
    directsend(q);
    free(q);
  } else {
    q->next = NULL;
    if(tail) {
      tail->next = q;
    } else {
      head = q;
    }
    tail = q;
  }
}

void xsb_broadcast(const char *command, server *service, const char *format, ...) {
  char buf[512];
  va_list va;
  sstring **servicemasks;
  int i, j;
  nick *np;
  unsigned int marker;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  if(service) {
    np = getnickbynick(servicenick(service->name->content));
    if(!np)
      return;

    xsb_send(np, "%s %s", command, buf);
    return;
  }

  marker = nextservermarker();

  for(i=getservicemasks(&servicemasks)-1;i>=0;i--) {
    char *mask = servicemasks[i]->content;
    for(j=0;j<MAXSERVERS;j++) {
      if(!serverlist[j].name || serverlist[j].marker == marker)
        continue;

      if(!match(mask, serverlist[j].name->content)) {
        serverlist[j].marker = marker;

        np = getnickbynick(servicenick(serverlist[j].name->content));
        if(np)
          xsb_send(np, "%s %s", command, buf);
      }
    }
  }
}

void xsb_unicast(const char *command, nick *np, const char *format, ...) {
  char buf[512];
  va_list va;
  
  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  xsb_send(np, "%s %s", command, buf);
}

int xsb_isservice(server *service) {
  int i;
  sstring **servicemasks;
  char *name = service->name->content;

  if(!(service->flags & SMODE_SERVICE))
    return 0;

  for(i=getservicemasks(&servicemasks)-1;i>=0;i--)
    if(match2strings(servicemasks[i]->content, name))
      return 1;

  return 0;
}

static void handler(nick *target, int messagetype, void **args) {
  switch(messagetype) {
    case LU_PRIVMSG: {
      void *newargs[3];
      nick *sender = args[0];
      char *cargv = args[1];

      newargs[0] = sender;
      newargs[1] = NULL;
      newargs[2] = cargv;

      handlemaskprivmsg(0, newargs);
      break;
    }
    case LU_KILLED:
      xsbnick = NULL;
      xsbnicksched = scheduleoneshot(time(NULL)+1, setuplocaluser, NULL);
      break;
  }
}
