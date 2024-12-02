/* CMDNAME: resetpassword
 * CMDALIASES: resetpass
 * CMDLEVEL: QCMD_SECURE | QCMD_NOTAUTHED
 * CMDARGS: 4
 * CMDDESC: Resets the password.
 * CMDFUNC: csa_dorespw
 * CMDPROTO: int csa_dorespw(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: @UCOMMAND@ <account> <newpass> <newpass> <code>
 * CMDHELP: Resets your password using the code received on your registered email address, where:
 * CMDHELP: username - your username
 * CMDHELP: newpass  - your desired new password.  Must be entered the same both times.
 * CMDHELP: code     - the code received in the RESET email.
 */

#include "../chanserv.h"
#include "../authlib.h"
#include "../../lib/irc_string.h"
#include "../../lib/hmac.h"
#include <stdio.h>
#include <string.h>

int csa_dorespw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;
  unsigned int same=0;
  int pq;
  time_t t;

  if (cargc<4) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "resetpassword");
    return CMD_ERROR;
  }

  if (!(rup=findreguser(sender, cargv[0])))
    return CMD_ERROR;

  if (strcmp(cargv[1],cargv[2])) {
    chanservstdmessage(sender, QM_PWDONTMATCH); /* Sorry, passwords do not match */
    cs_log(sender,"RESETPASS FAIL username %s new passwords don't match (%s vs %s)",rup->username,cargv[1],cargv[2]);
    return CMD_ERROR;
  }

  if (!hmac_strcmp(rup->password,cargv[1])) {
    /* If they are the same then continue anyway but don't send the hook later. */
    same=1;
  }

  pq = csa_checkpasswordquality(cargv[1]);
  if(pq == QM_PWTOSHORT) {
    chanservstdmessage(sender, QM_PWTOSHORT); /* new password too short */
    cs_log(sender,"RESETPASS FAIL username %s password too short %s (%zu characters)",rup->username,cargv[1],strlen(cargv[1]));
    return CMD_ERROR;
  } else if(pq == QM_PWTOWEAK) {
    chanservstdmessage(sender, QM_PWTOWEAK); /* new password is weak */
    cs_log(sender,"RESETPASS FAIL username %s password too weak %s",rup->username,cargv[1]);
    return CMD_ERROR;
  } else if(pq == QM_PWTOLONG) {
    chanservstdmessage(sender, QM_PWTOLONG); /* new password too long */
    cs_log(sender,"RESETPASS FAIL username %s password too long %s",rup->username,cargv[1]);
    return CMD_ERROR;
  } else if(pq == -1) {
    /* all good */
  } else {
    chanservsendmessage(sender, "unknown error in resetpass.c... contact #help");
    return CMD_ERROR;
  }

  if(UHasStaffPriv(rup) || !rup->lockuntil || hmac_strcmp(cargv[3], csc_generateresetcode(rup->lockuntil, rup->username))) {
    chanservstdmessage(sender, QM_BADRESETCODE);
    return CMD_ERROR;
  }

  t=time(NULL);

  if(rup->lockuntil > t) {
    chanservstdmessage(sender, QM_ACCOUNTLOCKED, rup->lockuntil);
    return CMD_ERROR;
  }

  rup->lockuntil=t+7*24*3600;

  if(rup->lastemail) {
    freesstring(rup->lastemail);
    rup->lastemail=NULL;
  }

  rup->lastpasschange=t;
  csdb_accounthistory_insert(sender, rup->password, cargv[1], NULL, NULL);
  setpassword(rup, cargv[1]);

  rup->lastauth=t;
  chanservstdmessage(sender, QM_PWCHANGED);
  cs_log(sender,"RESETPASS OK username %s", rup->username);

#ifdef AUTHGATE_WARNINGS
  if(UHasOperPriv(rup))
    chanservsendmessage(sender, "WARNING FOR PRIVILEGED USERS: you MUST go to https://auth.quakenet.org and login successfully to update the cache, if you do not your old password will still be usable in certain circumstances.");
#endif

  csdb_updateuser(rup);
  csdb_createmail(rup, QMAIL_NEWPW);

  if (!same)
    triggerhook(HOOK_CHANSERV_PWCHANGE, sender);

  return CMD_OK;
}
