#include "chanserv.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"

void chanservdgline(void* arg) {
  reguser *rup=(reguser*)arg;
  nick *np, *nl;
  authname *anp;
  int i;
  unsigned int ucount;
  
  if (!rup || (!UIsDelayedGline(rup) && !UIsGline(rup)))
    return;
  
  if (!(anp=findauthname(rup->ID)))
    return;
    
  for (nl=anp->nicks;nl;nl=nl->nextbyauthname) {
    for (i=0, ucount=0; i<NICKHASHSIZE; i++)
      for (np=nicktable[i];np;np=np->next)
        if (np->ipnode==nl->ipnode && !ircd_strcmp(np->ident, nl->ident))
          ucount++;
    
    if (ucount >= MAXGLINEUSERS) {
      chanservwallmessage("Delayed GLINE \"*!%s@%s\" (account %s) would hit %d users, aborting.", 
        nl->ident, IPtostr(nl->p_ipaddr), rup->username, ucount);
    } else {
      char *reason = "Network abuse";
      if(rup->suspendreason)
        reason = rup->suspendreason->content;

      glinebynick(nl, 3600, reason, 0, "chanserv");
      chanservwallmessage("Delayed GLINE \"*!%s@%s\" (authed as %s) expires in 60 minute/s (hit %d user%s) (reason: %s)", 
        nl->ident, IPtostr(nl->p_ipaddr), rup->username, ucount, ucount==1?"":"s", reason);
    }
  }
}
