/* 
 * Nick protection system for the chanserv
 */

#include "chanserv.h"
#include "../core/schedule.h"
#include "../localuser/localuser.h"


#define PROTECTTIME    60 /* How long you have to renick if you encroach.. */

void csp_handlenick(int hooknum, void *arg);
void csp_freenick(int hooknum, void *arg);
void csp_timerfunc (void *arg);
int csp_doclaimnick(void *source, int cargc, char **cargv);

void _init() {
  registerhook(HOOK_NICK_NEWNICK, csp_handlenick);
  registerhook(HOOK_NICK_RENAME, csp_handlenick);

  registerhook(HOOK_NICK_NEWNICK, csp_freenick);
  registerhook(HOOK_NICK_ACCOUNT, csp_freenick);
  
  chanservaddcommand("claimnick", QCMD_HELPER, 0, csp_doclaimnick, "Reclaims your nickname if it has been stolen.","");
}

void _fini() {
  reguser *rup;
  int i;

  deregisterhook(HOOK_NICK_NEWNICK, csp_handlenick);
  deregisterhook(HOOK_NICK_RENAME, csp_handlenick);

  deregisterhook(HOOK_NICK_NEWNICK, csp_freenick);
  deregisterhook(HOOK_NICK_ACCOUNT, csp_freenick);

  chanservremovecommand("claimnick", csp_doclaimnick);
  deleteallschedules(csp_timerfunc);

  /* Kill off all fakeusers too */
  for (i=0;i<REGUSERHASHSIZE;i++)
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) 
      if (rup->fakeuser) {
	if (getnickbynick(rup->username) == rup->fakeuser) {
	  deregisterlocaluser(rup->fakeuser,NULL);
	}
	rup->fakeuser=NULL;
      }
}

void csp_handlenick(int hooknum, void *arg) {
  nick *np=arg;
  reguser *rup;

  /* Check that it's a protected nick */
  if (!(rup=findreguserbynick(np->nick)) || !UIsProtect(rup))
    return;

  /* If we already had a timer running, this means someone renamed off and back.
   * Clear the old timer.
   */
  if (rup->checkshd) {
    deleteschedule(rup->checkshd, csp_timerfunc, rup);
    rup->checkshd=NULL;
  }

  /* If they're an oper, or the legal user of the nick, it's OK */
  /* Also, don't warn them if they are a fakeuser */
  if (getreguserfromnick(np)==rup) {
    rup->stealcount=0;
    return;
  }

  if (IsOper(np) || homeserver(np->numeric)==mylongnum)
    return;

  /* OK, at this stage we've established that:
   *  - This is a protected nick
   *  - The person using it isn't authed as the correct user
   */

  /* Send warning */
  chanservstdmessage(np, QM_PROTECTEDNICK, rup->username);

  /* Schedule checkup */
  rup->checkshd=scheduleoneshot(time(NULL)+PROTECTTIME, csp_timerfunc, rup);
}

void csp_freenick(int hooknum, void *arg) {
  nick *np=arg;
  reguser *rup=getreguserfromnick(np);

  if (!rup || !UIsProtect(rup))
    return;

  if (rup->checkshd) {
    deleteschedule(rup->checkshd, csp_timerfunc, rup);
    rup->checkshd=NULL;
  }
  
  rup->stealcount=0;
  if (rup->fakeuser) {
    /* Before killing, check the user is valid */
    if (getnickbynick(rup->username) == rup->fakeuser) { 
      /* Free up the fakeuser */
      deregisterlocaluser(rup->fakeuser, NULL);
      rup->fakeuser=NULL;
      chanservstdmessage(np, QM_NICKWASFAKED, rup->username);
    }
  }
}

void csp_timerfunc (void *arg) {
  reguser *rup=arg;
  nick *np;
  
  rup->checkshd=NULL;

  /* Check that we still have a user with this name and 
   * that they're not now opered or authed.. */
  if (!(np=getnickbynick(rup->username)) || IsOper(np) || (getreguserfromnick(np)==rup))
    return;

  /* KILL! KILL! KILL! */
  killuser(chanservnick, np, "Protected nick.");
  
  rup->stealcount++;

  /* If it's been stolen too much, create a fake user.. */
  if (rup->stealcount >= 3) {
    rup->fakeuser=registerlocaluser(rup->username, "reserved", "nick", "Protected nick.", NULL, UMODE_INV, NULL);
  }
}

int csp_doclaimnick(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  nick *target;

  if (!rup)
    return CMD_ERROR;
  
  if (!UIsProtect(rup)) {
    chanservstdmessage(sender, QM_NOTPROTECTED,rup->username);
    return CMD_ERROR;
  }

  if (!(target=getnickbynick(rup->username))) {
    chanservstdmessage(sender, QM_UNKNOWNUSER, rup->username);
    return CMD_ERROR;
  }

  if (getreguserfromnick(target)==rup) {
    chanservstdmessage(sender, QM_SAMEAUTH, target->nick, rup->username);
    return CMD_ERROR;
  }

  if (rup->fakeuser==target) {
    deregisterlocaluser(rup->fakeuser, NULL);
    rup->fakeuser=NULL;
  } else {
    if (!IsOper(target))
      killuser(chanservnick, target, "Protected nick.");
  }

  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}
