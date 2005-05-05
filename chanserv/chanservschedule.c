#include "chanserv.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"

void chanservdgline(void* arg) {
  reguser* rup=(reguser*)arg;
  nicklist* nl;
  nick* np;
  int i;
  unsigned int ucount;
  
  if (!rup)
    return;
  
  for (nl=rup->nicks; nl; nl=nl->next) {
    for (i=0, ucount=0; i<NICKHASHSIZE; i++)
      for (np=nicktable[i];np;np=np->next)
        if (!ircd_strcmp(np->ident, nl->np->ident) && np->ipaddress==nl->np->ipaddress)
          ucount++;
    
    if (ucount >= MAXGLINEUSERS)
      chanservwallmessage("Delayed GLINE \"*!%s@%s\" (account %s) would hit %d users, aborting.", 
        nl->np->ident, IPtostr(nl->np->ipaddress), rup->username, ucount);
    else {
      irc_send("%s GL * +*!%s@%s 3600 :%s\r\n", mynumeric->content, nl->np->ident, 
        IPtostr(nl->np->ipaddress), rup->suspendreason->content);
      chanservwallmessage("Delayed GLINE \"*!%s@%s\" (authed as %s) expires in 60 minute/s (hit %d user%s) (reason: %s)", 
        nl->np->ident, IPtostr(nl->np->ipaddress), rup->username, ucount, ucount==1?"":"s", rup->suspendreason->content);
    }
  }
}
