/* CMDNAME: sendpassword
 * CMDALIASES: sendpass
 * CMDLEVEL: QCMD_HELPER
 * CMDARGS: 1
 * CMDDESC: Sends the user a reset code to the email.
 * CMDFUNC: csa_dosendpw
 * CMDPROTO: int csa_dosendpw(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: @UCOMMAND@ <username>
 * CMDHELP: Sends the password for the specified account to the user's email address.
 */

#include "../chanserv.h"
#include "../authlib.h"
#include "../../lib/irc_string.h"
#include <stdio.h>
#include <string.h>

int csa_dosendpw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;
  time_t t;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "sendpassword");
    return CMD_ERROR;
  }

  if (!(rup=findreguser(sender, cargv[0])))
    return CMD_ERROR;

  if(UHasStaffPriv(rup)) {
    chanservstdmessage(sender, QM_REQUESTPASSPRIVUSER);
    cs_log(sender,"SENDPASSWORD FAIL privilidged user %s",rup->username);
    return CMD_ERROR;
  }

  t = time(NULL);

  if(rup->lockuntil && rup->lockuntil > t) {
    // Send same reset code.
    csdb_createmail(rup, QMAIL_NEWPW);
  } else {
    rup->lockuntil=t;
    rup->lastemailchange=t;
    csdb_updateuser(rup);

    if(rup->lastauth) {
      csdb_createmail(rup, QMAIL_LOSTPW);
    } else {
      csdb_createmail(rup, QMAIL_NEWACCOUNT); /* user hasn't authed yet and needs to do the captcha */
    }
  }

  chanservstdmessage(sender, QM_MAILQUEUED);
  cs_log(sender,"SENDPASSWORD username %s", rup->username);

  return CMD_OK;
}
