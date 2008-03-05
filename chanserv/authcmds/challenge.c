/*
 * CMDNAME: challenge
 * CMDLEVEL: QCMD_SECURE | QCMD_NOTAUTHED
 * CMDARGS: 0
 * CMDDESC: Returns a challenge for use in challengeauth.
 * CMDFUNC: csa_dochallenge
 * CMDPROTO: int csa_dochallenge(void *source, int cargc, char **cargv);
 */

#include "../chanserv.h"
#include "../authlib.h"
#include "../../lib/irc_string.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int csa_dochallenge(void *source, int cargc, char **cargv) {
  nick *sender=source;
  activeuser* aup;
  time_t t;
  
  if (!(aup=getactiveuserfromnick(sender)))
    return CMD_ERROR;

  t = time(NULL);
  if(t > aup->entropyttl) {
    cs_getrandbytes(aup->entropy,ENTROPYLEN);
    aup->entropyttl=t + 30;
  }

  chanservstdmessage(sender,QM_CHALLENGE,cs_calcchallenge(aup->entropy),cs_cralgorithmlist());
  cs_log(sender,"CHALLENGE");

  return CMD_OK;
}
