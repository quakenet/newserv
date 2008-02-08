#include <stdio.h>

#include "../chanserv.h"
#include "../../newsearch/newsearch.h"
#include "../../control/control.h"

void printnick_auth(nick *sender, nick *np) {
  struct reguser *rup;
  
  if (!(rup=getreguserfromnick(np))) {
    controlreply(sender,"%s (not authed)",np->nick);
  } else {
    controlreply(sender,"%s (%s/%u) (%s) (%s)",np->nick,rup->username,rup->ID,
        rup->email ? rup->email->content : "no email",
        rup->comment ? rup->comment->content : "no comment" );
  }
}

void printnick_authchans(nick *sender, nick *np) {
  struct reguser *rup;
  struct regchanuser *rcup;
  char thebuf[1024];
  char buf2[512];
  unsigned int bufpos=0, buf2len;
  unsigned char ch;
  
  printnick_auth(sender,np);
  
  if (!(rup=getreguserfromnick(np)))
    return;
  
  if (!rup->knownon) {
    controlreply(sender, " (no channels)");
  } else {
    for (rcup=rup->knownon;rcup;rcup=rcup->nextbyuser) {
      if (CUIsOwner(rcup))
        ch='*';
      else if (CUHasMasterPriv(rcup))
        ch='&';
      else if (CUHasOpPriv(rcup))
        ch='@';
      else if (CUHasVoicePriv(rcup))
        ch='+';
      else if (CUKnown(rcup))
        ch=' ';
      else 
        ch='!';
      
      buf2len=sprintf(buf2,"%c%s",ch,rcup->chan->index->name->content);

      if (buf2len+bufpos > 400) {
        controlreply(sender," %s", thebuf);
        bufpos=0;
      } 
      bufpos+=sprintf(thebuf+bufpos,"%s ",buf2);
    }
    if (bufpos)
      controlreply(sender," %s", thebuf);
  }
}
