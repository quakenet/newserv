/*
  nterfacer newserv control module
  Copyright (C) 2004-2007 Chris Porter.
*/

#include <string.h>

#include "../localuser/localuserchannel.h"
#include "../channel/channel.h"
#include "../lib/strlfunc.h"
#include "../control/control.h"
#include "../core/hooks.h"
#include "../nick/nick.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../authext/authext.h"

#include "library.h"
#include "nterfacer_control.h"

MODULE_VERSION("");

int handle_ison(struct rline *li, int argc, char **argv);
int handle_isaccounton(struct rline *li, int argc, char **argv);
int handle_whois(struct rline *li, int argc, char **argv);
int handle_message(struct rline *li, int argc, char **argv);
int handle_notice(struct rline *li, int argc, char **argv);
int handle_channel(struct rline *li, int argc, char **argv);
int handle_onchan(struct rline *li, int argc, char **argv);
int handle_status(struct rline *li, int argc, char **argv);
int handle_servicesonchan(struct rline *li, int argc, char **argv);
int handle_counthost(struct rline *li, int argc, char **argv);

struct rline *grli; /* used inline for status */
struct service_node *n_node;

void _init(void) {
  n_node = register_service("N");
  if(!n_node)
    return;

  register_handler(n_node, "ison", 1, handle_ison);
  register_handler(n_node, "isaccounton", 1, handle_isaccounton);
  register_handler(n_node, "whois", 1, handle_whois);
  register_handler(n_node, "msg", 2, handle_message);
  register_handler(n_node, "notice", 2, handle_notice);
  register_handler(n_node, "chanmsg", 2, handle_channel);
  register_handler(n_node, "onchan", 2, handle_onchan);
  register_handler(n_node, "status", 0, handle_status);
  register_handler(n_node, "servicesonchan", 1, handle_servicesonchan);
  register_handler(n_node, "counthost", 1, handle_counthost);
}

void _fini(void) {
  if(n_node)
    deregister_service(n_node);
}

int handle_ison(struct rline *li, int argc, char **argv) {
  int i;
  for(i=0;i<argc;i++)
    if(ri_append(li, "%d", getnickbynick(argv[i])?1:0) == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow");

  return ri_final(li);
}

int handle_isaccounton(struct rline *li, int argc, char **argv) {
  int i;
  for(i=0;i<argc;i++)
    if(ri_append(li, "%d", findauthnamebyname(argv[i])?1:0) == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow");

  return ri_final(li);
}

int handle_whois(struct rline *li, int argc, char **argv) {
  nick *np = getnickbynick(argv[0]);
  channel **channels;

  int i;

  if(!np)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "User not online");

  ri_append(li, "%s", np->nick);
  ri_append(li, "%s", np->ident);
  ri_append(li, "%s", np->host->name->content);
  ri_append(li, "%s", np->realname->name->content);
  ri_append(li, "%s", np->authname);
  ri_append(li, "%s", printflags(np->umodes, umodeflags));
  
  channels = np->channels->content;

  for(i=0;i<np->channels->cursi;i++)
    if(ri_append(li, "%s", channels[i]->index->name->content) == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow");

  return ri_final(li);
}

int handle_message(struct rline *li, int argc, char **argv) {
  int realargc = abs(argc), i;
  nick *np = getnickbynick(argv[0]);  
  if(!np)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "User not online");

  for(i=1;i<realargc;i++) {
    if(argc < 0) {
      controlnotice(np, "%s", argv[i]);
    } else {
      controlreply(np, "%s", argv[i]);
    }
  }

  ri_append(li, "Done.");
  return ri_final(li);
}

int handle_notice(struct rline *li, int argc, char **argv) {
  return handle_message(li, -argc, argv);
}

int handle_channel(struct rline *li, int argc, char **argv) {
  int i;
  channel *cp = findchannel(argv[0]);  
  if(!cp)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  for(i=1;i<argc;i++)
    controlchanmsg(cp, "%s", argv[i]);

  ri_append(li, "Done.");
  return ri_final(li);
}

int handle_onchan(struct rline *li, int argc, char **argv) {
  int i;
  nick *np = getnickbynick(argv[0]);
  channel **channels;
  channel *cp;
  if(!np)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Nickname not found");  
  
  cp = findchannel(argv[1]);
  if(!cp)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  channels = (channel **)np->channels->content;
  for(i=0;i<np->channels->cursi;i++) {
    if(channels[i]->index->channel == cp) {
      ri_append(li, "1");
      return ri_final(li);
    }
  }

  ri_append(li, "0");
  return ri_final(li);
}

void handle_nterfacerstats(int hooknum, void *arg) {
  /* hmm, not much we can do here, @ppa */
  ri_append(grli, "%s", (char *)arg);
}

int handle_status(struct rline *li, int argc, char **argv) {
  grli = li;
  
  registerhook(HOOK_CORE_STATSREPLY, &handle_nterfacerstats);
  triggerhook(HOOK_CORE_STATSREQUEST, (void *)5);
  deregisterhook(HOOK_CORE_STATSREPLY, &handle_nterfacerstats);
  return ri_final(li);
}

/* assumes services have a single char nickname and +k set */
int handle_servicesonchan(struct rline *li, int argc, char **argv) {
  int i;
  nick *np;
  channel *cp = findchannel(argv[0]);
  if(!cp)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found"); 

#if NICKLEN >= 1 /* if not we have a buffer overflow */
  for(i=0;i<=cp->users->hashsize;i++) {
    if(cp->users->content[i] != nouser) {      
      np = getnickbynumeric(cp->users->content[i]);
      if(np && IsService(np) && np->nick[0] && !np->nick[1])
        if(ri_append(li, "%s", np->nick) == BF_OVER)
          return ri_error(li, BF_OVER, "Buffer overflow");
    }
  }
#endif

  return ri_final(li);
}

int handle_counthost(struct rline *li, int argc, char **argv) {
  int i, j;
  unsigned int results[100];
  host *hp;
  if(argc > 100)
    return ri_error(li, ERR_TOO_MANY_ARGS, "Too many arguments");

  memset(results, 0, sizeof(results));

  for(j=0;j<argc;j++)
    collapse(argv[j]);

  for(i=0;i<HOSTHASHSIZE;i++)
    for(hp=hosttable[i];hp;hp=hp->next)
      for(j=0;j<argc;j++)
        if(!match(argv[j], hp->name->content))
          results[j]+=hp->clonecount;

  for(j=0;j<argc;j++)
    if(ri_append(li, "%d", results[j]) == BF_OVER)
      return ri_error(li, BF_OVER, "Buffer overflow");


  return ri_final(li);
}


