/* CMDNAME: resetpassword
 * CMDALIASES: resetpass
 * CMDLEVEL: QCMD_SECURE | QCMD_NOTAUTHED
 * CMDARGS: 4
 * CMDDESC: Change your password using a code that was sent to your email address.
 * CMDFUNC: csa_dorespw
 * CMDPROTO: int csa_dorespw(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: @UCOMMAND@ <username> <code> <newpassword> <newpassword>
 * CMDHELP: Changes your account password using a code that was sent to your email address.
 * CMDHELP: Your new password must be at least 6 characters long, contain at least one number
 * CMDHELP: and one letter, and may not contain sequences of letters or numbers. Also note
 * CMDHELP: that your password will be truncated to 10 characters.
 * CMDHELP: Your new password will be sent to your registered email address.
 * CMDHELP: Where:
 * CMDHELP: username - your username
 * CMDHELP: code - code you received in an email sent to your registered address
 * CMDHELP: newpassword - your desired new password.  Must be entered the same both times.
 * CMDHELP: Note: due to the sensitive nature of this command, you must send the message to
 * CMDHELP: Q@CServe.quakenet.org when using it.
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
  char *username, *code, *newpass, *newpassconfirm;
  unsigned int same=0;
  int pq;
  time_t t;

  if (cargc<4) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "resetpassword");
    return CMD_ERROR;
  }

  username=cargv[0];
  code=cargv[1];
  newpass=cargv[2];
  newpassconfirm=cargv[3];

  if (!(rup=findreguser(sender, username)))
    return CMD_ERROR;

  if (strcmp(newpass,newpassconfirm)) {
    chanservstdmessage(sender, QM_PWDONTMATCH); /* Sorry, passwords do not match */
    cs_log(sender,"RESETPASS FAIL username %s new passwords don't match (%s vs %s)",rup->username,newpass,newpassconfirm);
    return CMD_ERROR;
  }

  if (!hmac_strcmp(rup->password,newpass)) {
    /* If they are the same then continue anyway but don't send the hook later. */
    same=1;
  }

  pq = csa_checkpasswordquality(newpass);
  if(pq == QM_PWTOSHORT) {
    chanservstdmessage(sender, QM_PWTOSHORT); /* new password too short */
    cs_log(sender,"RESETPASS FAIL username %s password too short %s (%zu characters)",rup->username,newpass,strlen(newpass));
    return CMD_ERROR;
  } else if(pq == QM_PWTOWEAK) {
    chanservstdmessage(sender, QM_PWTOWEAK); /* new password is weak */
    cs_log(sender,"RESETPASS FAIL username %s password too weak %s",rup->username,newpass);
    return CMD_ERROR;
  } else if(pq == QM_PWTOLONG) {
    chanservstdmessage(sender, QM_PWTOLONG); /* new password too long */
    cs_log(sender,"RESETPASS FAIL username %s password too long %s",rup->username,newpass);
    return CMD_ERROR;
  } else if(pq == -1) {
    /* all good */
  } else {
    chanservsendmessage(sender, "Encountered an unknown error; please contact #help.");
    return CMD_ERROR;
  }

  if(UHasStaffPriv(rup) || !rup->lockuntil || hmac_strcmp(code, csc_generateresetcode(rup->lockuntil, rup->username))) {
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
  csdb_accounthistory_insert(sender, rup->password, newpass, NULL, NULL);
  setpassword(rup, newpass);

  rup->lastauth=t;
  chanservstdmessage(sender, QM_PWCHANGED);
  cs_log(sender,"RESETPASS OK username %s", rup->username);

  csdb_updateuser(rup);

  if (!same)
    triggerhook(HOOK_CHANSERV_PWCHANGE, sender);

  return CMD_OK;
}
