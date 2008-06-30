#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "../core/hooks.h"
#include "../lib/splitline.h"
#include "../core/config.h"
#include "../irc/irc.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../core/error.h"
#include "../server/server.h"
#include "../control/control.h"
#include "xsb.h"

static const char *DEFAULT_SERVICE_MASKS[] = { "services*.*.quakenet.org", (char *)0 };
static array defaultservicemasks_a;
static sstring **defaultservicemasks;

static void handlemaskprivmsg(int, void *);
static CommandTree *cmds;

#define DEBUG

void _init(void) {
  const char **p;
  array *servicemasks;

  cmds = newcommandtree();
  if(cmds)
    registerhook(HOOK_NICK_MASKPRIVMSG, &handlemaskprivmsg);

  array_init(&defaultservicemasks_a, sizeof(sstring *));

  for(p=DEFAULT_SERVICE_MASKS;*p;p++) {
    int i = array_getfreeslot(&defaultservicemasks_a);
    defaultservicemasks = (sstring **)defaultservicemasks_a.content;

    defaultservicemasks[i] = getsstring(*p, strlen(*p));
  }

  servicemasks = getconfigitems("xsb", "servicemask");
  if(!servicemasks || !servicemasks->cursi)
    Error("xsb", ERR_WARNING, "No service masks in config file (xsb/servicemask), using defaults");
}

void _fini(void) {
  int i;

  for(i=0;i<defaultservicemasks_a.cursi;i++)
    freesstring(defaultservicemasks[i]);
  array_free(&defaultservicemasks_a);

  if(!cmds)
    return;

  deregisterhook(HOOK_NICK_MASKPRIVMSG, &handlemaskprivmsg);

  destroycommandtree(cmds);
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

#ifndef DEBUG
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

void xsb_addcommand(const char *name, const int maxparams, CommandHandler handler) {
  addcommandtotree(cmds, name, 0, maxparams, handler);
}

void xsb_delcommand(const char *name, CommandHandler handler) {
  deletecommandfromtree(cmds, name, handler);
}

void xsb_command(const char *command, const char *format, ...) {
  char buf[512];
  va_list va;
  sstring **servicemasks;
  array *aservicemasks;
  int i;
  char *controlnum;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  aservicemasks = getconfigitems("xsb", "servicemask");
  if(!aservicemasks || !aservicemasks->cursi)
    aservicemasks = &defaultservicemasks_a;

  servicemasks = (sstring **)aservicemasks->content;

  controlnum = longtonumeric(mynick->numeric, 5);

  for(i=0;i<aservicemasks->cursi;i++)
    irc_send("%s P $%s :XSB1 %s %s", controlnum, servicemasks[i]->content, command, buf);
}
