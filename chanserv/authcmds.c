#include "chanserv.h"
#include "authlib.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <string.h>

int csa_dohello(void *source, int cargc, char **cargv);
int csa_doauth(void *source, int cargc, char **cargv);
int csa_doreqpw(void *source, int cargc, char **cargv);
int csa_doreqmasterpw(void *source, int cargc, char **cargv);
int csa_donewpw(void *source, int cargc, char **cargv);
int csa_doemail(void *source, int cargc, char **cargv);
int csa_dosetpw(void *source, int cargc, char **cargv);
int csa_dosetmail(void *source, int cargc, char **cargv);
int csa_dosetmasterpw(void *source, int cargc, char **cargv);

void _init() {
  csa_initregex();
  chanservaddcommand("hello",   QCMD_NOTAUTHED, 2, csa_dohello, "Creates a new user account.");
  chanservaddcommand("auth",    QCMD_ALIAS | QCMD_SECURE | QCMD_NOTAUTHED, 2, csa_doauth,  "Authenticates you on the bot.");
  chanservaddcommand("login",   QCMD_SECURE | QCMD_NOTAUTHED, 2, csa_doauth,  "Authenticates you on the bot.");
  chanservaddcommand("newpass", QCMD_AUTHED, 3, csa_donewpw, "Change your password.");
  chanservaddcommand("email",   QCMD_AUTHED, 3, csa_doemail, "Change your email address.");
  chanservaddcommand("requestpassword", QCMD_NOTAUTHED, 2, csa_doreqpw, "Requests the current password by email.");
  chanservaddcommand("requestmasterpassword", QCMD_AUTHED, 1, csa_doreqmasterpw, "Requests a new master password by email.");
  chanservaddcommand("setpassword",       QCMD_OPER, 2, csa_dosetpw, "Set a new password.");
  chanservaddcommand("setemail",          QCMD_OPER, 2, csa_dosetmail, "Set the email address.");
  chanservaddcommand("setmasterpassword", QCMD_OPER, 2, csa_dosetmasterpw, "Set the master password.");
//  chanservaddcommand("getpassword",       QCMD_DEV, 2, csa_dogetpw, "Get the password.");
//  chanservaddcommand("getmasterpassword", QCMD_DEV, 2, csa_dogetmasterpw, "Get the master password.");
}

void _fini() {
  csa_freeregex();
  chanservremovecommand("hello", csa_dohello);
  chanservremovecommand("auth",  csa_doauth);
  chanservremovecommand("login",  csa_doauth);
  chanservremovecommand("requestpassword", csa_doreqpw);
  chanservremovecommand("requestmasterpassword",  csa_doreqmasterpw);
  chanservremovecommand("newpass",  csa_donewpw);
  chanservremovecommand("email",  csa_doemail);
  chanservremovecommand("setpassword", csa_dosetpw);
  chanservremovecommand("setemail", csa_dosetmail);
  chanservremovecommand("setmasterpassword", csa_dosetmasterpw);
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

/*
 * /msg Q HELLO <email address> <email address>
 */
int csa_dohello(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup;
  char userhost[USERLEN+HOSTLEN+2];

  if (getreguserfromnick(sender))
    return CMD_ERROR;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "hello");
    return CMD_ERROR;
  }

  if (findreguserbynick(sender->nick)) {
    chanservstdmessage(sender, QM_AUTHNAMEINUSE, sender->nick);
    return CMD_ERROR;
  }

  if (strcmp(cargv[0],cargv[1])) {
    chanservstdmessage(sender, QM_EMAILDONTMATCH);
    cs_log(sender,"HELLO FAIL username %s email don't match (%s vs %s)",sender->nick,cargv[0],cargv[1]);
    return CMD_ERROR;
  }

  if (csa_checkeboy(sender, cargv[0]))
    return CMD_ERROR;

  rup=getreguser();
  rup->status=0;
  rup->ID=++lastuserID;
  strncpy(rup->username,sender->nick,NICKLEN); rup->username[NICKLEN]='\0';
  rup->created=time(NULL);
  rup->lastauth=time(NULL);
  rup->lastemailchange=time(NULL);
  rup->flags=QUFLAG_NOTICE;
  rup->languageid=0;
  rup->suspendby=0;
  rup->suspendexp=0;
  rup->password[0]='\0';
  rup->masterpass[0]='\0';
  rup->email=getsstring(cargv[0],EMAILLEN);
  rup->info=NULL;
  sprintf(userhost,"%s@%s",sender->ident,sender->host->name->content);
  rup->lastuserhost=getsstring(userhost,USERLEN+HOSTLEN+1);
  rup->suspendreason=NULL;
  rup->comment=NULL;
  rup->knownon=NULL;
  rup->checkshd=NULL;
  rup->stealcount=0;
  rup->fakeuser=NULL;
  rup->nicks=NULL;
  addregusertohash(rup);
  csa_createrandompw(rup->password, PASSLEN);
  csa_createrandompw(rup->masterpass, PASSLEN);
  chanservstdmessage(sender, QM_NEWACCOUNT, rup->username,rup->email->content);
  cs_log(sender,"HELLO OK created auth %s (%s)",rup->username,rup->email->content); 
  csdb_createuser(rup);
  csdb_createmail(rup, QMAIL_NEWACCOUNT);
 
  return CMD_OK;
}

/*
 * /msg Q AUTH <account> <password>
 */
int csa_doauth(void *source, int cargc, char **cargv) {
  reguser *rup;
  activeuser* aup;
  nick *sender=source;
  nicklist *nl;
  char userhost[USERLEN+HOSTLEN+2];
  int ucount=0;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "auth");
    return CMD_ERROR;
  }
  
  if (!(aup = getactiveuserfromnick(sender)))
    return CMD_ERROR;
  
  aup->authattempts++;
  if (aup->authattempts > MAXAUTHATTEMPT) {
    if ((aup->authattempts % 100) == 0)
      chanservwallmessage("Warning: User %s!%s@%s attempted to auth %d times. Last attempt: AUTH %s %s", 
        nl->np->nick, nl->np->ident, nl->np->host->name->content, cargv[0], cargv[1]);
    chanservstdmessage(sender, QM_AUTHFAIL);
    cs_log(sender,"AUTH FAIL too many auth attempts (last attempt: AUTH %s %s)",cargv[0], cargv[1]); 
    return CMD_ERROR;
  }

  if (!(rup=findreguserbynick(cargv[0]))) {
    chanservstdmessage(sender, QM_AUTHFAIL);
    cs_log(sender,"AUTH FAIL bad username %s",cargv[0]); 
    return CMD_ERROR;
  }

  if (!checkpassword(rup, cargv[1])) {
    chanservstdmessage(sender, QM_AUTHFAIL);
    cs_log(sender,"AUTH FAIL username %s bad password %s",rup->username,cargv[1]);
    return CMD_ERROR;
  }

  rup->lastauth=time(NULL);
  sprintf(userhost,"%s@%s",sender->ident,sender->host->name->content);
  if (rup->lastuserhost)
    freesstring(rup->lastuserhost);
  rup->lastuserhost=getsstring(userhost,USERLEN+HOSTLEN+1);
  
  if (UHasSuspension(rup) && rup->suspendexp && (time(0) >= rup->suspendexp)) {
    /* suspension has expired, remove it */
    rup->flags&=(~(QUFLAG_SUSPENDED|QUFLAG_GLINE|QUFLAG_DELAYEDGLINE));
    rup->suspendby=0;
    rup->suspendexp=0;
    freesstring(rup->suspendreason);
    rup->suspendreason=0;
  }
  
  if (UIsNeedAuth(rup))
    rup->flags&=~(QUFLAG_NEEDAUTH);
  csdb_updateuser(rup);
  
  if (UIsDelayedGline(rup)) {
    /* delayed-gline - schedule the user's squelching */
    deleteschedule(NULL, &chanservdgline, (void*)rup); /* icky, but necessary unless we stick more stuff in reguser structure */
    scheduleoneshot(time(NULL)+rand()%900, &chanservdgline, (void*)rup);
  }
  else if (UIsGline(rup)) {
    /* instant-gline - lets be lazy and set a schedule expiring now :) */
    deleteschedule(NULL, &chanservdgline, (void*)rup); /* icky, but necessary unless we stick more stuff in reguser structure */
    scheduleoneshot(time(NULL), &chanservdgline, (void*)rup);
  }
  else if (UIsSuspended(rup)) {
    /* plain suspend */
    chanservstdmessage(sender, QM_AUTHSUSPENDED);
    chanservstdmessage(sender, QM_REASON, rup->suspendreason->content);
    if (rup->suspendexp) {
      struct tm* tmp;
      char buf[200];
      
      tmp=gmtime(&(rup->suspendexp));
      strftime(buf, 15, "%d/%m/%y %H:%M", tmp);
      chanservstdmessage(sender, QM_EXPIRES, buf);
    }
    return CMD_ERROR;
  }
  
  if (!UHasHelperPriv(rup) && !UIsNoAuthLimit(rup)) {
    for (nl=rup->nicks; nl; nl=nl->next)
      ucount++;
    
    if (ucount >= MAXAUTHCOUNT) {
      chanservstdmessage(sender, QM_TOOMANYAUTHS);
      return CMD_ERROR;
    }
  }
  
  chanservstdmessage(sender, QM_AUTHOK, rup->username);
  cs_log(sender,"AUTH OK username %s", rup->username);
  localusersetaccount(sender, rup->username);

  return CMD_OK;
}

/*
 * /msg Q REQUESTPASSWORD <account> <email>
 */
int csa_doreqpw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "requestpassword");
    return CMD_ERROR;
  }

  if (!(rup=findreguser(sender, cargv[0])))
    return CMD_ERROR;

  if (ircd_strcmp(cargv[1],rup->email->content)) {
    chanservstdmessage(sender, QM_BADEMAIL, rup->username);
    cs_log(sender,"REQUESTPASSWORD FAIL wrong email, username %s email %s",rup->username,cargv[1]);
    return CMD_ERROR;
  }

  if (csa_checkthrottled(sender, rup, "REQUESTPASSWORD"))
    return CMD_ERROR;

  rup->lastemailchange=time(NULL);
  csdb_updateuser(rup);
  csdb_createmail(rup, QMAIL_REQPW);
  chanservstdmessage(sender, QM_MAILQUEUED, rup->username);
  cs_log(sender,"REQUESTPASSWORD OK username %s email %s", rup->username,rup->email->content);

  return CMD_OK;
}

/*
 * /msg Q NEWPASS <master password> <new password> <new password>
 */
int csa_donewpw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;

  if (cargc<3) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "newpass");
    return CMD_ERROR;
  }

  if (!(rup=getreguserfromnick(sender)))
    return CMD_ERROR;

  if (!checkmasterpassword(rup, cargv[0])) {
    chanservstdmessage(sender, QM_AUTHFAIL);
    cs_log(sender,"NEWPASS FAIL username %s bad masterpassword %s",rup->username,cargv[0]);
    return CMD_ERROR;
  }

  if (strcmp(cargv[1],cargv[2])) {
    chanservstdmessage(sender, QM_PWDONTMATCH); /* Sorry, passwords do not match */
    cs_log(sender,"NEWPASS FAIL username %s new passwords don't match (%s vs %s)",rup->username,cargv[1],cargv[2]);
    return CMD_ERROR;
  }

  if (strlen(cargv[1]) < 6) {
    chanservstdmessage(sender, QM_PWTOSHORT); /* new password to short */
    cs_log(sender,"NEWPASS FAIL username %s password to short %s (%d characters)",rup->username,cargv[1],strlen(cargv[1]));
    return CMD_ERROR;
  }

  setpassword(rup, cargv[1]);
  rup->lastauth=time(NULL);
  chanservstdmessage(sender, QM_PWCHANGED);
  cs_log(sender,"NEWPASS OK username %s", rup->username);
  csdb_updateuser(rup);
  csdb_createmail(rup, QMAIL_NEWPW);

  return CMD_OK;
}

/*
 * /msg Q REQUESTMASTERPASSWORD
 */
int csa_doreqmasterpw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;

  if (!(rup=getreguserfromnick(sender)))
    return CMD_ERROR;

  if (csa_checkthrottled(sender, rup, "REQUESTMASTERPASSWORD"))
    return CMD_ERROR;

  csa_createrandompw(rup->masterpass, PASSLEN);
  chanservstdmessage(sender, QM_MASTERPWCHANGED);
  cs_log(sender,"REQUESTMASTERPASSWORD OK username %s",rup->username);
  csdb_updateuser(rup);
  csdb_createmail(rup, QMAIL_NEWMASTERPW);

  return CMD_OK;
}

/*
 * /msg Q EMAIL <master password> <email address> <email address>
 */
int csa_doemail(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;

  if (cargc<3) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "email");
    return CMD_ERROR;
  }

  if (!(rup=getreguserfromnick(sender)))
    return CMD_ERROR;

  if (!checkmasterpassword(rup, cargv[0])) {
    chanservstdmessage(sender, QM_AUTHFAIL);
    cs_log(sender,"EMAIL FAIL username %s bad masterpass %s",rup->username,cargv[0]);
    return CMD_ERROR;
  }

  if (strcmp(cargv[1],cargv[2])) {
    chanservstdmessage(sender, QM_EMAILDONTMATCH);
    cs_log(sender,"EMAIL FAIL username %s email don't match (%s vs %s)",rup->username,cargv[1],cargv[2]);
    return CMD_ERROR;
  }

  if (csa_checkeboy(sender, cargv[1]))
    return CMD_ERROR;

//  rup2->ID = rup->ID;
//  rup2->email=getsstring(rup->email->content,EMAILLEN); /* save previous email addy */

  csdb_createmail(rup, QMAIL_NEWEMAIL);
  freesstring(rup->email);
  rup->email=getsstring(cargv[1],EMAILLEN);
  rup->lastemailchange=time(NULL);
  chanservstdmessage(sender, QM_EMAILCHANGED, cargv[1]);
  cs_log(sender,"EMAIL OK username %s",rup->username);
  csdb_updateuser(rup);

  return CMD_OK;
}

/*
 * /msg Q SETPASSWORD <account> <new password>
 */
int csa_dosetpw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "setpassword");
    return CMD_ERROR;
  }

  if (!(rup=findreguser(sender, cargv[0])))
    return CMD_ERROR;

  strncpy(rup->password,cargv[1],PASSLEN);
  rup->password[PASSLEN]='\0';
  chanservstdmessage(sender, QM_PWCHANGED);
  cs_log(sender,"SETPASSWORD OK username %s",rup->username);
  csdb_updateuser(rup);

  return CMD_OK;
}

/*
 * /msg Q SETEMAIL <account> <email address>
 */
int csa_dosetmail(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "setemail");
    return CMD_ERROR;
  }

  if (!(rup=findreguser(sender, cargv[0])))
    return CMD_ERROR;

  if (csa_checkeboy(sender, cargv[1]))
    return CMD_ERROR;

  freesstring(rup->email);
  rup->email=getsstring(cargv[1],EMAILLEN);
  rup->lastemailchange=time(NULL);
  chanservstdmessage(sender, QM_EMAILCHANGED, cargv[1]);
  cs_log(sender,"SETEMAIL OK username %s <%s>",rup->username,rup->email->content);
  csdb_updateuser(rup);

  return CMD_OK;
}

/*
 * /msg Q SETMASTERPASSWORD <account>
 */
int csa_dosetmasterpw(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "setmasterpassword");
    return CMD_ERROR;
  }

  if (!(rup=findreguser(sender, cargv[0])))
    return CMD_ERROR;

  csa_createrandompw(rup->masterpass, PASSLEN);
  chanservstdmessage(sender, QM_MASTERPWCHANGED);
  cs_log(sender,"SETMASTERPASSWORD OK username %s",rup->username);
  csdb_updateuser(rup);

  return CMD_OK;
}
