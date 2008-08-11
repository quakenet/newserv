/* ticketauth.c */

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../control/control.h"
#include "../core/config.h"
#include "../nick/nick.h"
#include "../core/error.h"
#include "../lib/hmac.h"
#include "../lib/version.h"
#include "../localuser/localuser.h"
#include "../core/hooks.h"
#include "../irc/irc.h"
#include "../chanserv/chanserv.h"

#define WARN_CHANNEL "#twilightzone"

MODULE_VERSION("");

sstring *sharedsecret = NULL;

int ta_ticketauth(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  char buffer[1024], *uhmac, *acc, *junk, *flags;
  unsigned char digest[32];
  int expiry, acclen, id;
  hmacsha256 hmac;
  channel *wcp;

  if(IsAccount(np)) {
    controlreply(np, "You're already authed.");
    return CMD_ERROR;
  }

  if(cargc != 6)
    return CMD_USAGE;

  acc = cargv[0];
  expiry = atoi(cargv[1]);
  id = atoi(cargv[2]);
  acclen = strlen(acc);
  flags = cargv[3];
  junk = cargv[4];
  uhmac = cargv[5];

  if((acclen <= 1) || (acclen > ACCOUNTLEN)) {
    controlreply(np, "Bad account.");
    return CMD_ERROR;
  }

  if(time(NULL) > expiry + 30) {
    controlwall(NO_OPER, NL_MISC, "%s!%s@%s attempted to TICKETAUTH as %s (expired)", np->nick, np->ident, np->host->name->content, acc);
    controlreply(np, "Ticket time is bad or has expired.");
    return CMD_ERROR;
  }

  hmacsha256_init(&hmac, (unsigned char *)sharedsecret->content, sharedsecret->length);
  snprintf(buffer, sizeof(buffer), "%s %d %d %s", acc, expiry, id, junk);
  hmacsha256_update(&hmac, (unsigned char *)buffer, strlen(buffer));
  hmacsha256_final(&hmac, digest);
  
  /* hahahaha */
  snprintf(buffer, sizeof(buffer), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", digest[0], digest[1],  digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15], digest[16], digest[17], digest[18], digest[19], digest[20], digest[21], digest[22], digest[23], digest[24], digest[25], digest[26], digest[27], digest[28], digest[29], digest[30], digest[31]);

  if(strcasecmp(buffer, uhmac)) {
    controlwall(NO_OPER, NL_MISC, "%s!%s@%s attempted to TICKETAUTH as %s (bad HMAC)", np->nick, np->ident, np->host->name->content, acc);
    controlreply(np, "Bad HMAC.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_MISC, "%s!%s@%s TICKETAUTH'ed as %s", np->nick, np->ident, np->host->name->content, acc);

  wcp = findchannel(WARN_CHANNEL);
  if(wcp)
    controlchanmsg(wcp, "WARNING: %s!%s@%s TICKETAUTH'ed as %s", np->nick, np->ident, np->host->name->content, acc);

  controlreply(np, "Ticket valid, authing. . .");

  localusersetaccount(np, acc, id, 0, cs_accountflagmap_str(flags));

  controlreply(np, "Done.");
  return CMD_OK;
}

void _init() {
  sharedsecret = getcopyconfigitem("ticketauth", "sharedsecret", "", 512);
  if(!sharedsecret || !sharedsecret->content || !sharedsecret->content[0]) {
    Error("ticketauth", ERR_ERROR, "Shared secret not defined in config file.");
    if(sharedsecret) {
      freesstring(sharedsecret);
      sharedsecret = NULL;
    }

    return;
  }

  registercontrolhelpcmd("ticketauth", NO_OPERED, 5, ta_ticketauth, "Usage: ticketauth <ticket>");
}

void _fini() {
  if(!sharedsecret)
    return;

  deregistercontrolcmd("ticketauth", ta_ticketauth);

  freesstring(sharedsecret);
}
