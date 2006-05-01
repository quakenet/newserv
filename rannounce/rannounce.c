
#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../lib/irc_string.h"

nick *rannouncenick;

void rannouncehandler(nick *me, int type, void **args);

void _init() {
  channel *cp;
  rannouncenick=registerlocaluser("R","relay","quakenet.org","Relay announcer",NULL,0,rannouncehandler);
  
  if ((cp=findchannel("#twilightzone"))) {
    localjoinchannel(rannouncenick, cp);
  } else {
    localcreatechannel(rannouncenick, "#twilightzone");
  }

  if ((cp=findchannel("#qnet.queue"))) {
    localjoinchannel(rannouncenick, cp);
  } else {
    localcreatechannel(rannouncenick, "#qnet.queue");
  }

  if ((cp=findchannel("#qrequest"))) {
    localjoinchannel(rannouncenick, cp);
  } else {
    localcreatechannel(rannouncenick, "#qrequest");
  }
}

void _fini() {
  deregisterlocaluser(rannouncenick,NULL);
}

void rannouncehandler(nick *me, int type, void **args) {
  nick *np;
  char *text;
  channel *cp;
  int items;

  if (type==LU_PRIVMSG) {
    np=args[0];
    text=args[1];

    if (IsOper(np) && !ircd_strncmp(text,"announce ",9)) {
      text+=9;
      items=strtoul(text,NULL,10);

      if (items) {
	if ((cp=findchannel("#twilightzone"))) {
	  sendmessagetochannel(me, cp, "%d item%s in queue - https://www.quakenet.org/secure/queue/",items,items==1?"":"s");
	}
	if ((cp=findchannel("#qnet.queue"))) {
	  sendmessagetochannel(me, cp, "%d item%s in queue - https://www.quakenet.org/secure/queue/",items,items==1?"":"s");
	}
	if ((cp=findchannel("#qrequest"))) {
	  sendmessagetochannel(me, cp, "%d item%s in queue",items,items==1?"":"s");
	}
      }
    }
  } else if (type==LU_KILLED) {
    rannouncenick=NULL;
  }
}

