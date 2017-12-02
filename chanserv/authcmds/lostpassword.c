/* CMDNAME: lostpassword
 * CMDALIASES: lostpass
 * CMDLEVEL: QCMD_NOTAUTHED
 * CMDARGS: 1
 * CMDDESC: Sends instructions for resetting your account to your registered email address.
 * CMDFUNC: csa_dolostpw
 * CMDPROTO: int csa_dolostpw(void *source, int cargc, char **cargv);
 * CMDHELP: Usage: @UCOMMAND@ <email>
 * CMDHELP: Sends instructions for resetting your account to your registered email address, where:
 * CMDHELP: email    - your email address
 */

#include "../chanserv.h"
#include "../authlib.h"
#include "../../lib/irc_string.h"
#include <stdio.h>
#include <string.h>

int csa_dolostpw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;
  time_t t;
  int i, matched = 0;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "lostpassword");
    return CMD_ERROR;
  }

  t=time(NULL);

  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) {
      if(!rup->email || strcasecmp(cargv[0],rup->email->content))
        continue;

      if(UHasStaffPriv(rup)) {
        cs_log(sender,"LOSTPASSWORD FAIL privileged email %s",cargv[0]);
        continue;
      }

      matched = 1;

      if(rup->lockuntil && rup->lockuntil > t) {
        chanservstdmessage(sender, QM_ACCOUNTLOCKED, rup->lockuntil);
        continue;
      }

      if(csa_checkthrottled(sender, rup, "LOSTPASSWORD"))
        continue;

      rup->lockuntil=t;
      rup->lastemailchange=t;
      csdb_updateuser(rup);

      if(rup->lastauth) {
        csdb_createmail(rup, QMAIL_LOSTPW);
      } else {
        csdb_createmail(rup, QMAIL_NEWACCOUNT); /* user hasn't authed yet and needs to do the captcha */
      }

      cs_log(sender,"LOSTPASSWORD OK username %s email %s", rup->username, rup->email->content);
      chanservstdmessage(sender, QM_MAILQUEUED);
    }
  }

  if(!matched) {
    cs_log(sender,"LOSTPASSWORD FAIL email %s",cargv[0]);
    chanservstdmessage(sender, QM_BADEMAIL);
    return CMD_ERROR;
  } else {
    chanservstdmessage(sender, QM_DONE);
  }

  return CMD_OK;
}
