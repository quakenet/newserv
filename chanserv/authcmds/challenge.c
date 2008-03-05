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
#include "../../lib/hmac.h"
#include "../../lib/sha1.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int csa_dochallenge(void *source, int cargc, char **cargv) {
  nick *sender=source;
  activeuser* aup;
  time_t t = time(NULL);
  unsigned char digest[20];
  char hexbuf[20 * 2 + 1];
  
  SHA1_CTX ctx;

  if (!(aup=getactiveuserfromnick(sender)))
    return CMD_ERROR;

  if(t > aup->entropyttl) {
    cs_getrandbytes(aup->entropy,ENTROPYLEN);
    aup->entropyttl=t + 30;
  }

  SHA1Init(&ctx);
  /* we can maybe add a salt here in the future */
  SHA1Update(&ctx, aup->entropy, ENTROPYLEN);
  SHA1Final(digest, &ctx);

  chanservstdmessage(sender,QM_CHALLENGE,hmac_printhex(digest,hexbuf,sizeof(digest)),"HMAC_SHA1 HMAC_SHA256");
  cs_log(sender,"CHALLENGE");

  return CMD_OK;
}
