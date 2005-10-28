#include "../control/control.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../lib/irc_string.h"
#include <string.h>

int do_countusers(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source, *np;
  host *hp;
  unsigned int count=0;
  char *chp,*host,*ident;

  if (cargc<1)
    return CMD_USAGE;

  for (chp=cargv[0]; ;chp++) {
    if (*chp=='@') {
      /* found @, so it's user@host */
      ident=cargv[0];
      *chp='\0';
      host=chp+1;
      break;
    } else if (*chp=='\0') {
      /* found NULL, so just host */
      host=cargv[0];
      ident=NULL;
      break;
    }
  }
  
  if ((hp=findhost(host))==NULL) {
    controlreply(sender,"0 users match %s%s%s",ident?ident:"",ident?"@":"",host);
    return CMD_OK;
  }
  
  if (!ident) {
    /* We want *@host, so it's just the clonecount we have for free */
    controlreply(sender,"%d users match %s",hp->clonecount,host);
  } else {
    /* Do it the slow way */
    for (np=hp->nicks;np;np=np->nextbyhost) {
      if (!ircd_strcmp(np->ident,ident))
        count++;
    }
    controlreply(sender,"%d users match %s@%s",count,ident,host);
  }
  
  return CMD_OK;
} 

void _init() {
  registercontrolhelpcmd("countusers", NO_OPER, 1, do_countusers, "Usage: countusers <hostmask>\nReturns users on specified hostmask.");
}

void _fini() {
  deregistercontrolcmd("countusers", do_countusers);
}
