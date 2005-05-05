/* testmod.c */

#include <stdio.h>
#include "../core/hooks.h"
#include "../server/server.h"
#include "../nick/nick.h"
#include "../core/error.h"
#include "../core/schedule.h"
#include "../localuser/localuser.h"

void spamserverstate(int hook, void *servernum) {
  switch(hook) {
    case HOOK_SERVER_NEWSERVER:
      Error("testmod",ERR_INFO,"New server: %d (%d maxusers)",(int)servernum,serverlist[(int)servernum].maxusernum+1);
      break;

    case HOOK_SERVER_LOSTSERVER:
      Error("testmod",ERR_INFO,"Lost server: %d",(int)servernum);
      break;
  }
}

void printnick(int hook, void *vp) {
/*
//  nick *np=(nick *)vp;
  
//  printf("New nick %s: %s!%s@%s (%s)\n",longtonumeric(np->numeric,5),np->nick,np->ident,np->host->name->content,np->realname->name->content);
*/
} 

void printstats(int hook, void *arg) {
  printf("%s\n",(char *)arg);
}

void requeststats(void *arg) {
  triggerhook(HOOK_CORE_STATSREQUEST,(void *)20);
} 

void _init() {
  registerhook(HOOK_SERVER_NEWSERVER,&spamserverstate);
  registerhook(HOOK_SERVER_LOSTSERVER,&spamserverstate);
  registerhook(HOOK_NICK_NEWNICK,&printnick);
  registerhook(HOOK_CORE_STATSREPLY,&printstats);
  
  schedulerecurring(time(NULL),0,10,&requeststats,NULL);
}

void _fini() {
  deregisterhook(HOOK_SERVER_NEWSERVER,&spamserverstate);
  deregisterhook(HOOK_SERVER_LOSTSERVER,&spamserverstate);
  deregisterhook(HOOK_NICK_NEWNICK,&printnick);
  deregisterhook(HOOK_CORE_STATSREPLY,&printstats);
  
  deleteschedule(NULL,&requeststats,NULL);
}
