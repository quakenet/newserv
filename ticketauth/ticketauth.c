/* ticketauth.c */

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../control/control.h"
#include "../core/config.h"
#include "../nick/nick.h"
#include "../core/error.h"
#include "../lib/sha1.h"
#include "../lib/version.h"

#include "../core/hooks.h"
#include "../irc/irc.h"

#define WARN_CHANNEL "#fishcowcow"

MODULE_VERSION("$Id")

sstring *sharedsecret = NULL;

/* here as we're not currently using TS, this should be REMOVED and the code updated to use localusersetaccount instead */
void localusersetaccountnots(nick *np, char *accname) {
  if (IsAccount(np)) {
    Error("localuser",ERR_WARNING,"Tried to set account on user %s already authed", np->nick);
    return;
  }

  SetAccount(np);
  strncpy(np->authname, accname, ACCOUNTLEN);
  np->authname[ACCOUNTLEN]='\0';

  if (connected) {
    irc_send("%s AC %s %s",mynumeric->content, longtonumeric(np->numeric,5), np->authname);
  }

  triggerhook(HOOK_NICK_ACCOUNT, np);
}

int ta_ticketauth(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  char buffer[1024], *hmac, *acc;
  unsigned char shabuf[20];
  int expiry, acclen;
  SHA1_CTX context;

  if(IsAccount(np)) {
    controlreply(np, "You're already authed.");
    return CMD_ERROR;
  }

  if(cargc != 3)
    return CMD_USAGE;

  hmac = cargv[0];
  acc = cargv[1];
  acclen = strlen(acc);
  expiry = atoi(cargv[2]);
  if((acclen <= 1) || (acclen > ACCOUNTLEN)) {
    controlreply(np, "Bad account.");
    return CMD_ERROR;
  }

  if(time(NULL) > expiry) {
    controlwall(NO_OPER, NL_MISC, "%s!%s@%s attempted to TICKETAUTH as %s (expired)", np->nick, np->ident, np->host->name->content, acc);
    controlreply(np, "Ticket time is bad or has expired.");
    return CMD_ERROR;
  }
  
  snprintf(buffer, sizeof(buffer), "%s %d", acc, expiry);

  SHA1Init(&context);
  SHA1Update(&context, (unsigned char *)buffer, strlen(buffer));
  SHA1Final(shabuf, &context);
  
  /* ha! */
  snprintf(buffer, sizeof(buffer), "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", shabuf[0], shabuf[1],  shabuf[2], shabuf[3], shabuf[4], shabuf[5], shabuf[6], shabuf[7], shabuf[8], shabuf[9], shabuf[10], shabuf[11], shabuf[12], shabuf[13], shabuf[14], shabuf[15], shabuf[16], shabuf[17], shabuf[18], shabuf[19]);

  if(strcasecmp(buffer, hmac)) {
    controlwall(NO_OPER, NL_MISC, "%s!%s@%s attempted to TICKETAUTH as %s (bad HMAC)", np->nick, np->ident, np->host->name->content, acc);
    controlreply(np, "Bad HMAC.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_MISC, "%s!%s@%s TICKETAUTH'ed as %s", np->nick, np->ident, np->host->name->content, acc);
  controlreply(np, "Ticket valid, authing. . .");

  localusersetaccountnots(np, acc);

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

  registercontrolhelpcmd("ticketauth", NO_OPERED, 3, ta_ticketauth, "Usage: ticketauth <ticket>");
}

void _fini() {
  if(!sharedsecret)
    return;

  deregistercontrolcmd("ticketauth", ta_ticketauth);

  freesstring(sharedsecret);
}
