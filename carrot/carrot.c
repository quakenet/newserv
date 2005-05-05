#include "../control/control.h"
#include "../nick/nick.h"
#include "../channel/channel.h"

int ca_carrot(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  nick *victim;
  channel *cp;
  
  if (cargc<1) {
    controlreply(sender,"Usage: carrot channel and/or user");
    return CMD_ERROR;
  }
  
  if ((victim=getnickbynick(cargv[0]))!=NULL) {
    controlreply(victim,"%cACTION ger %s en morot%c",1,victim->nick,1);
    controlreply(sender,"Gave %s a carrot.",victim->nick); 
  } else if ((cp=findchannel(cargv[0]))!=NULL) {
    if (cargc>1) {
      controlchanmsg(cp,"%cACTION ger %s en morot%c",1,cargv[1],1);
    } else {
      controlchanmsg(cp,"%cACTION waves a carrot around menacingly.%c",1,1);
    }
    
    controlreply(sender,"Used carrot in %s.",cp->index->name->content);
  } else {
    controlreply(sender,"Couldn't find %s.",cargv[0]);
    return;
  }
  
  return CMD_OK;
}

void _init() {
  registercontrolcmd("carrot",10,2,ca_carrot);
}

void _fini() {
  deregistercontrolcmd("carrot",ca_carrot);
}

