/* TODO: catch control messages */

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
#include "xsb.h"

static const char *DEFAULT_SERVICE_MASKS[] = { "services*.*.quakenet.org", (char *)0 };

static array defaultservicemasks_a;
static sstring **defaultservicemasks;

static void handlemaskprivmsg(int, void *);
static void handlecontrolregistered(int, void *);
static CommandTree *cmds;

static char controlnum[6];

struct messagequeue {
  struct messagequeue *next;
  char buf[];
};

struct messagequeue *head, *tail;

void _init(void) {
  const char **p;
  array *servicemasks;

  controlnum[0] = '\0';
  
  cmds = newcommandtree();
  if(!cmds)
    return;
  
  registerhook(HOOK_NICK_MASKPRIVMSG, &handlemaskprivmsg);
  registerhook(HOOK_CONTROL_REGISTERED, &handlecontrolregistered);
  
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
  
  if(!cmds)
    return;

  destroycommandtree(cmds);

  deregisterhook(HOOK_NICK_MASKPRIVMSG, &handlemaskprivmsg);
  deregisterhook(HOOK_CONTROL_REGISTERED, &handlecontrolregistered);

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

static void directsend(char *buf) {
  irc_send("%s P %s", controlnum, buf);
}

static void handlecontrolregistered(int hooknum, void *args) {
  nick *np = (nick *)args;

  if(np) {
    struct messagequeue *q, *nq;
    
    longtonumeric2(np->numeric, 5, controlnum);
    
    for(q=head;q;q=nq) {
      nq = q->next;
      
      directsend(q->buf);
      free(q);
    }
    head = NULL;
    tail = NULL;
  } else {
    controlnum[0] = '\0';
  }
}

static int getservicemasks(sstring ***masks) {
  array *aservicemasks = getconfigitems("xsb", "servicemask");
  if(!aservicemasks || !aservicemasks->cursi)
    aservicemasks = &defaultservicemasks_a;

  *masks = (sstring **)aservicemasks->content;

  return aservicemasks->cursi;
}

static void xsb_send(const char *format, ...) {
  char buf[512];
  va_list va;
  size_t len;
  
  va_start(va, format);
  len = vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);
  if(len >= sizeof(buf))
    len = sizeof(buf);
  
  if(controlnum[0]) {
    directsend(buf);
  } else {
    struct messagequeue *q = (struct messagequeue *)malloc(sizeof(struct messagequeue) + len + 5);
    
    strlcpy(q->buf, buf, len + 2);
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
  int i;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  if(service) {
    xsb_send("$%s :XSB1 %s %s", service->name->content, command, buf);
    return;
  }

  for(i=getservicemasks(&servicemasks)-1;i>=0;i--)
    xsb_send("$%s :XSB1 %s %s", servicemasks[i]->content, command, buf);
}

void xsb_unicast(const char *command, nick *np, const char *format, ...) {
  char buf[512];
  va_list va;
  
  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  /* TODO */
  xsb_send("$%s :XSB1 %s %s", serverlist[homeserver(np->numeric)].name, command, buf);
  /* xsb_send("%s :XSB1 %s %s", longtonumeric(np->numeric, 5), command, buf);*/
  
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

