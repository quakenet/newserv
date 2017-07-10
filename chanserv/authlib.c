/* authlib.c */

#include "chanserv.h"
#include "../lib/irc_string.h"
#include "authlib.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <pcre.h>

static pcre *remail, *raccount;

void csa_initregex(void) {
  const char *errptr;
  int erroffset;

  if (!(remail=pcre_compile(VALID_EMAIL, PCRE_CASELESS, &errptr, &erroffset, NULL)))
    Error("chanserv", ERR_STOP, "Unable to compile email regex (error: %s, position: %d).", errptr, erroffset);

  if (!(raccount=pcre_compile(VALID_ACCOUNT_NAME, PCRE_CASELESS, &errptr, &erroffset, NULL)))
    Error("chanserv", ERR_STOP, "Unable to compile account name regex (error: %s, position: %d).", errptr, erroffset);
}

void csa_freeregex(void) {
  pcre_free(remail);
  pcre_free(raccount);
}

/*
 * use regex matching to determine if it's a valid eboy or not
 */
int csa_checkeboy_r(char *eboy)
{
  int len;

  len = (((strlen(eboy)) < (EMAILLEN)) ? (strlen(eboy)) : (EMAILLEN));
  if (len <= 4) {
    return QM_EMAILTOOSHORT;
  }

  /* catch some real lame attempts */
  if (!ircd_strncmp("user@mymailhost.xx", eboy, len) || !ircd_strncmp("info@quakenet.org", eboy, len)
      || !ircd_strncmp("user@mymail.xx", eboy, len) || !ircd_strncmp("user@mail.cc", eboy, len)
      || !ircd_strncmp("user@host.com", eboy, len) || !ircd_strncmp("Jackie@your.isp.com", eboy, len)
      || !ircd_strncmp("QBot@QuakeNet.org", eboy, len) || !ircd_strncmp("Q@CServe.quakenet.org", eboy, len)
      || !ircd_strncmp("badger@example.com", eboy, len)) {
    return QM_NOTYOUREMAIL;
  }

  if (pcre_exec(remail, NULL, eboy, strlen(eboy), 0, 0, NULL, 0) < 0) {
    return QM_INVALIDEMAIL;
  }

  return -1;
}

int csa_checkeboy(nick *sender, char *eboy)
{
  int r = csa_checkeboy_r(eboy);
  if (r == -1)
    return 0;

  if(sender)
    chanservstdmessage(sender, r, eboy);

  return 1;
}

/*
 * use regex matching to determine if it's a valid account name or not
 * returns 1 if bad, 0 if good
 */
int csa_checkaccountname_r(char *accountname) {
  if (pcre_exec(raccount, NULL, accountname, strlen(accountname), 0, 0, NULL, 0) < 0) {
    return (1);
  }
  return (0);
}

int csa_checkaccountname(nick *sender, char *accountname) {
  int r = csa_checkaccountname_r(accountname);
  if(r && sender)
    chanservstdmessage(sender, QM_INVALIDACCOUNTNAME);

  return r;
}

/*
 * create a random pw. code stolen from fox's O
 */
void csa_createrandompw(char *pw, int n)
{
  int i;
  char upwdchars[] = "ABCDEFGHIJKLMNPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz123456789-!";
  
  for (i = 0; i < n; i++) {
    pw[i] = upwdchars[rand() % (sizeof(upwdchars) - 1)];
  }
  pw[n] = '\0';
}

/*
 * check if account is "throttled"
 */
int csa_checkthrottled(nick *sender, reguser *rup, char *s)
{
  time_t now;
  long d;
  float t;

  now=time(NULL);
  d=MAX_RESEND_TIME+rup->lastemailchange-now;

  if (d>MAX_RESEND_TIME)
    d=MAX_RESEND_TIME;

  if (d>0L) {
    t = ((float) d) / ((float) 3600);
    chanservstdmessage(sender, QM_MAILTHROTTLED, t);
    cs_log(sender,"%s FAIL username %s, new request throttled for %.1f hours",s,rup->username,t);
    return 1;
  }
  return 0;
}

int csa_checkpasswordquality(char *password) {
  int i, cntweak = 0, cntdigits = 0, cntletters = 0;
  if (strlen(password) < 6)
    return QM_PWTOSHORT;

  if (strlen(password) > 10)
    return QM_PWTOLONG;

  for ( i = 0; password[i] && i < PASSLEN; i++ ) {
    if ( password[i] == password[i+1] || password[i] + 1 == password[i+1] || password[i] - 1 == password[i+1] )
      cntweak++;
    if(isdigit(password[i]))
      cntdigits++;
    if(islower(password[i]) || isupper(password[i]))
      cntletters++;
    if(password[i] < 32 || password[i] > 127)
      return QM_PWINVALID;
  }

  if( cntweak > 3 || !cntdigits || !cntletters)
    return QM_PWTOWEAK;

  return -1;
}

reguser *csa_createaccount(char *username, char *password, char *email) {
  time_t t = time(NULL);
  char *local, *dupemail;

  dupemail = strdup(email);
  local=strchr(dupemail, '@');
  if(!local) {
    free(dupemail);
    return NULL;
  }
  *(local++)='\0';

  reguser *rup=getreguser();
  rup->status=0;
  rup->ID=++lastuserID;
  strncpy(rup->username,username,NICKLEN); rup->username[NICKLEN]='\0';
  rup->created=t;
  rup->lastauth=0;
  rup->lastemailchange=t;
  rup->lastpasschange=t;
  rup->flags=QUFLAG_NOTICE;
  rup->languageid=0;
  rup->suspendby=0;
  rup->suspendexp=0;
  rup->suspendtime=0;
  rup->lockuntil=0;
  strncpy(rup->password,password,PASSLEN); rup->password[PASSLEN]='\0';
  rup->email=getsstring(email,EMAILLEN);
  rup->lastemail=NULL;

  rup->localpart=getsstring(dupemail,EMAILLEN);
  free(dupemail);

  rup->domain=findorcreatemaildomain(email);
  addregusertomaildomain(rup, rup->domain);
  rup->info=NULL;
  rup->lastuserhost=NULL;
  rup->suspendreason=NULL;
  rup->comment=NULL;
  rup->knownon=NULL;
  rup->checkshd=NULL;
  rup->stealcount=0;
  rup->fakeuser=NULL;
  addregusertohash(rup);

  return rup;
}

int csa_completeauth2(reguser *rup, char *nickname, char *ident, char *hostname, char *authtype, void (*reply)(nick *, int, ...), nick *reply_to) {
  int toomanyauths=0;
  time_t now;
  char userhost[USERLEN+HOSTLEN+2];
  nick *onp;
  authname *anp;

  /* This should never fail but do something other than crashing if it does. */
  if (!(anp=findauthname(rup->ID))) {
    reply(reply_to, QM_AUTHFAIL);
    return 0;
  }

  /* Check for too many auths.  Don't return immediately, since we will still warn
   * other users on the acct in this case. */
  if (!UHasStaffPriv(rup) && !UIsNoAuthLimit(rup)) {
    if (anp->usercount >= MAXAUTHCOUNT) {
      reply(reply_to, QM_TOOMANYAUTHS);
      toomanyauths=1;
    }
  }

  for (onp=anp->nicks;onp;onp=onp->nextbyauthname) {
    if (toomanyauths) {
      chanservstdmessage(onp, QM_OTHERUSERAUTHEDLIMIT, nickname, ident, hostname, MAXAUTHCOUNT);
    } else {
      chanservstdmessage(onp, QM_OTHERUSERAUTHED, nickname, ident, hostname);
    }
  }

  if (toomanyauths)
    return 0;

  now=time(NULL);

  if (UHasSuspension(rup) && rup->suspendexp && (now >= rup->suspendexp)) {
    /* suspension has expired, remove it */
    rup->flags&=(~(QUFLAG_SUSPENDED|QUFLAG_GLINE|QUFLAG_DELAYEDGLINE));
    rup->suspendby=0;
    rup->suspendexp=0;
    freesstring(rup->suspendreason);
    rup->suspendreason=0;
    csdb_updateuser(rup);
  }

  if (UIsSuspended(rup)) {
    /* plain suspend */
    reply(reply_to, QM_AUTHSUSPENDED);
    if(rup->suspendreason)
      reply(reply_to, QM_REASON, rup->suspendreason->content);
    if (rup->suspendexp)
      reply(reply_to, QM_EXPIRES, rup->suspendexp);
    return 0;
  }
  if (UIsInactive(rup)) {
    reply(reply_to, QM_INACTIVEACCOUNT);
    return 0;
  }

  /* Guarantee a unique auth timestamp for each account */
  if (rup->lastauth < now)
    rup->lastauth=now;
  else
    rup->lastauth++;

  sprintf(userhost,"%s@%s",ident,hostname);
  if (rup->lastuserhost)
    freesstring(rup->lastuserhost);
  rup->lastuserhost=getsstring(userhost,USERLEN+HOSTLEN+1);

  csdb_updateuser(rup);

  cs_log(NULL,"%s!%s@%s [noauth] %s OK username %s",nickname,ident,hostname,authtype,rup->username);

  return 1;
}
