#include "../control/control.h"
#include "../nick/nick.h"
#include "../channel/channel.h"

int fa_facepalm(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  nick *victim;
  channel *cp;

  if (cargc<1) {
    controlreply(sender,"Usage: facepalm <target>");
    return CMD_ERROR;
  }

  if ((victim=getnickbynick(cargv[0]))!=NULL) {
        controlreply(victim,"  .-'---`-.");
        controlreply(victim,",'          `.");
        controlreply(victim,"|             \\");
        controlreply(victim,"|              \\");
        controlreply(victim,"\\           _  \\");
        controlreply(victim,",\\  _    ,'-,/-)\\");
        controlreply(victim,"( * \\ \\,' ,' ,'-)");
        controlreply(victim," `._,)     -',-')");
        controlreply(victim,"   \\/         ''/");
        controlreply(victim,"    )        / /");
        controlreply(victim,"   /       ,'-'");
        controlreply(sender,"Gave %s a facepalm.",victim->nick);
  } else if ((cp=findchannel(cargv[0]))!=NULL) {
    	controlchanmsg(cp,"  .-'---`-.");
    	controlchanmsg(cp,",'          `.");
    	controlchanmsg(cp,"|             \\");
    	controlchanmsg(cp,"|              \\");
    	controlchanmsg(cp,"\\           _  \\");
    	controlchanmsg(cp,",\\  _    ,'-,/-)\\");
    	controlchanmsg(cp,"( * \\ \\,' ,' ,'-)");
    	controlchanmsg(cp," `._,)     -',-')");
    	controlchanmsg(cp,"   \\/         ''/");
    	controlchanmsg(cp,"    )        / /");
    	controlchanmsg(cp,"   /       ,'-'");
    	controlreply(sender,"Spammed facepalm in %s.",cp->index->name->content);
  } else {
    	controlreply(sender,"Can't find %s.",cargv[0]);
  }

  return CMD_OK;
}

void _init() {
  registercontrolhelpcmd("facepalm",NO_OPERED,2,fa_facepalm,"Usage: facepalm <target>\nSpams a facepalm at target.");
}

void _fini() {
  deregistercontrolcmd("facepalm",fa_facepalm);
}
