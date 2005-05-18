/*
  nterfacer newserv control module
  Copyright (C) 2004 Chris Porter.

  v1.05
    - modified whois to take into account channels with ','
    - made ison take multiple arguments
    - added isaccounton
  v1.04
    - added status/onchan/servicesonchan
  v1.03
    - added multiple targets for channel/nick notice/message commands
  v1.02
    - added channel message, nick message/notice commands
    - generalised error messages
  v1.01
    - whois fixed to notice BUF_OVER
*/

#include "../chanstats/chanstats.h"
#include "../localuser/localuserchannel.h"
#include "../channel/channel.h"
#include "../lib/strlfunc.h"
#include "../control/control.h"
#include "../core/hooks.h"
#include "../lib/irc_string.h"

#include "library.h"
#include "nterfacer_control.h"

struct service_node *node;

int handle_chanstats(struct rline *li, int argc, char **argv);
int handle_ison(struct rline *li, int argc, char **argv);
int handle_whois(struct rline *li, int argc, char **argv);
int handle_message(struct rline *li, int argc, char **argv);
int handle_notice(struct rline *li, int argc, char **argv);
int handle_channel(struct rline *li, int argc, char **argv);
int handle_onchan(struct rline *li, int argc, char **argv);
int handle_status(struct rline *li, int argc, char **argv);
int handle_servicesonchan(struct rline *li, int argc, char **argv);
int handle_isaccounton(struct rline *li, int argc, char **argv);

struct rline *grli; /* used inline for status */

void _init(void) {
  node = register_service("N");
  if(!node)
    return;

  register_handler(node, "chanstats", 1, handle_chanstats);
  register_handler(node, "ison", 1, handle_ison);
  register_handler(node, "whois", 1, handle_whois);
  register_handler(node, "msg", 2, handle_message);
  register_handler(node, "notice", 2, handle_notice);
  register_handler(node, "chanmsg", 2, handle_channel);
  register_handler(node, "onchan", 2, handle_onchan);
  register_handler(node, "status", 0, handle_status);
  register_handler(node, "servicesonchan", 1, handle_servicesonchan);
  register_handler(node, "isaccounton", 1, handle_isaccounton);
}

void _fini(void) {
  if(node)
    deregister_service(node);
}

int handle_chanstats(struct rline *li, int argc, char **argv) {
  chanstats *csp;
  chanindex *cip;
  int i,j,k,l;
  int tot,emp;
  int themax;
  float details[13];
  
  cip=findchanindex(argv[0]);
  
  if (cip==NULL)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  csp=cip->exts[csext];

  if (csp==NULL)
    return ri_error(li, ERR_CHANSTATS_STATS_NOT_FOUND, "Stats not found");

  if (uponehour==0) {
    details[0] = -1;
    details[1] = -1;
  } else {
    tot=0; emp=0;
    for(i=0;i<SAMPLEHISTORY;i++) {
      tot+=csp->lastsamples[i];
      if (csp->lastsamples[i]==0) {
        emp++;
      }
    }
    details[0] = tot/SAMPLEHISTORY;
    details[1] = emp/SAMPLEHISTORY * 100;

  }

  details[2] = csp->todayusers/todaysamples;
  details[3] = ((float)(todaysamples-csp->todaysamples)/todaysamples)*100;
  details[4] = csp->todaymax;

  themax=csp->lastmax[0];

  details[5] = csp->lastdays[0]/10;
  details[6] = ((float)(lastdaysamples[0]-csp->lastdaysamples[0])/lastdaysamples[0])*100;
  details[7] = themax;
  
  /* 7-day average */
  j=k=l=0;
  for (i=0;i<7;i++) {
    j+=csp->lastdays[i];
    k+=csp->lastdaysamples[i];
    l+=lastdaysamples[i];
    if (csp->lastmax[i]>themax) {
      themax=csp->lastmax[i];
    }
  }

  details[8] = j/70;
  details[9] = ((l-k)*100)/l;
  details[10] = themax;

  /* 14-day average: continuation of last loop */
  for (;i<14;i++) {
    j+=csp->lastdays[i];
    k+=csp->lastdaysamples[i];
    l+=lastdaysamples[i];
    if (csp->lastmax[i]>themax) {
      themax=csp->lastmax[i];
    }
  }

  details[11] = j/140;
  details[12] = ((l-k)*100)/l;
  details[13] = themax;

  ri_append(li, "%.1f", details[0]);
  ri_append(li, "%.1f%%", details[1]);
  for(j=2;j<14;) {
    ri_append(li, "%.1f", details[j++]);
    ri_append(li, "%.1f%%", details[j++]);
    ri_append(li, "%d%%", details[j++]);
  }

  return ri_final(li);
}

int handle_ison(struct rline *li, int argc, char **argv) {
  int i;
  for(i=0;i<argc;i++)
    ri_append(li, "%d", getnickbynick(argv[i])?1:0);
  return ri_final(li);
}

int handle_isaccounton(struct rline *li, int argc, char **argv) {
  int i;
  for(i=0;i<argc;i++)
    ri_append(li, "%d", getnickbynick(argv[i])?1:0);
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
        ri_append(li, "%s", np->nick);
    }
  }
#endif

  return ri_final(li);
}
