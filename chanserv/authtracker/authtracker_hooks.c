/* authtracker module: Tracks when users come and go... */

#include "authtracker.h"
#include "../chanserv.h"
#include "../../core/hooks.h"
#include "../../nick/nick.h"
#include "../../lib/irc_string.h"

#include <time.h>
#include <string.h>

#define NTERFACER_AUTH	"nterfacer"

/* OK, we need to deal separately with users who have definitely gone (QUIT
 * or KILL) and those who has mysteriously vanished (lost in netsplit). 
 * There are hooks for all these things, but sadly QUIT and KILL also
 * trigger the generic "lost nick" hook.  Luckily, these things always
 * happen one after the other without any intervening quits, so we will keep
 * track of the last QUIT or KILL details and ignore the subsequent
 * LOSTNICK. */

unsigned int at_lastnum;
time_t at_lastauthts;
unsigned long at_lastuserid;

unsigned long at_getuserid(nick *np) {
  /* If they are not +r, forget it. */
  if (!IsAccount(np))
    return 0;

  /* Ignore that pesky nterfacer */
#ifdef NTERFACER_AUTH
  if (!ircd_strcmp(np->authname, NTERFACER_AUTH))
    return 0;
#endif
    
  /* Use the userid from authext only. */
  if (np->auth)
    return np->auth->userid;
  
  Error("authtracker",ERR_WARNING,"Unable to get userID from IsAccount() user %s",np->nick);
  return 0;
}

void at_handlequitorkill(int hooknum, void *arg) {
  void **args=arg;
  nick *np=args[0];
  char *reason=args[1];
  char *rreason;
  unsigned long userid;
  
  /* Ignore unauthed users, or those with no accountts */
  if (!(userid=at_getuserid(np)) || !np->accountts)
    return;

  at_lastuserid=userid;
  at_lastauthts=np->accountts;
  at_lastnum=np->numeric;

  if (hooknum==HOOK_NICK_KILL && (rreason=strchr(reason,'@')))
    reason=rreason;
  
  at_logquit(userid, np->accountts, time(NULL), reason);
}

void at_handlelostnick(int hooknum, void *arg) {
  nick *np=arg;
  unsigned long userid;
  
  if (!(userid=at_getuserid(np)) || !np->accountts)
    return;
  
  /* Don't do this for a user that just left normally (see above) */
  if ((userid==at_lastuserid) && (np->accountts == at_lastauthts) && (np->numeric==at_lastnum))
    return;
    
  at_lostnick(np->numeric, userid, np->accountts, time(NULL), AT_NETSPLIT);
}

void at_newnick(int hooknum, void *arg) {
  nick *np=arg;
  unsigned long userid;
  
  /* Ignore unauthed users, or those with no TS */
  if (!(userid=at_getuserid(np)) || !np->accountts)
    return;
  
  /* OK, we've seen an authed user appear.  If it's a known ghost we can zap it */
  if (!at_foundnick(np->numeric, userid, np->accountts)) {
    /* If we didn't zap a ghost then it's a new session */
    at_lognewsession(userid, np);
  }
}

void at_serverlinked(int hooknum, void *arg) {
  unsigned long servernum=(unsigned long)arg;
  
  at_serverback(servernum);
}

void at_hookinit() {
  registerhook(HOOK_NICK_QUIT, at_handlequitorkill);
  registerhook(HOOK_NICK_KILL, at_handlequitorkill);
  registerhook(HOOK_NICK_LOSTNICK, at_handlelostnick);
  registerhook(HOOK_NICK_NEWNICK, at_newnick);
  registerhook(HOOK_NICK_ACCOUNT, at_newnick);
  registerhook(HOOK_SERVER_LINKED, at_serverlinked);
}

void at_hookfini() {
  deregisterhook(HOOK_NICK_QUIT, at_handlequitorkill);
  deregisterhook(HOOK_NICK_KILL, at_handlequitorkill);
  deregisterhook(HOOK_NICK_LOSTNICK, at_handlelostnick);
  deregisterhook(HOOK_NICK_NEWNICK, at_newnick);
  deregisterhook(HOOK_NICK_ACCOUNT, at_newnick);
  deregisterhook(HOOK_SERVER_LINKED, at_serverlinked);
}      
