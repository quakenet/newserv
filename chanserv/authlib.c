/* authlib.c */

#include "authlib.h"
#include "chanserv.h"
#include "../lib/irc_string.h"

#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

regex_t preg;
static int regexinit;

int csa_initregex() {
  if (regexinit)
    return 1;

  if (!regcomp(&preg, VALID_EMAIL, REG_EXTENDED | REG_NOSUB | REG_ICASE))
    return 0;

  regexinit = 1;
  return 1;
}

void csa_freeregex() {
  regfree(&preg);
}

/*
 * use regex matching to determine if it's a valid eboy or not
 */
int csa_checkeboy(nick *sender, char *eboy)
{ 
  int i, len;

  len = (((strlen(eboy)) < (EMAILLEN)) ? (strlen(eboy)) : (EMAILLEN));
  if (len <= 4) {
    if (sender)
      chanservstdmessage(sender, QM_EMAILTOOSHORT, eboy);
    return (1);
  }

  if (strstr(&eboy[1], "@") == NULL) {
    if (sender)
      chanservstdmessage(sender, QM_EMAILNOAT, eboy);
    return (1);
  }

  if (eboy[len - 1] == '@') {
    if (sender)
      chanservstdmessage(sender, QM_EMAILATEND, eboy);
    return (1);
  }

  for (i = 0; i < len; i++) {
    if (!isalpha(eboy[i]) && !isdigit(eboy[i])
        && !(eboy[i] == '@') && !(eboy[i] == '.')
        && !(eboy[i] == '_') && !(eboy[i] == '-')) {
      if (sender)
        chanservstdmessage(sender, QM_EMAILINVCHR, eboy);
      return (1);
    }
  }

  /* catch some real lame attempts */
  if (!ircd_strncmp("user@mymailhost.xx", eboy, len) || !ircd_strncmp("info@quakenet.org", eboy, len)
      || !ircd_strncmp("user@mymail.xx", eboy, len) || !ircd_strncmp("user@mail.cc", eboy, len)
      || !ircd_strncmp("user@host.com", eboy, len) || !ircd_strncmp("Jackie@your.isp.com", eboy, len)
      || !ircd_strncmp("QBot@QuakeNet.org", eboy, len)) {
    if (sender)
      chanservstdmessage(sender, QM_NOTYOUREMAIL, eboy);
    return (1);
  }

  csa_initregex();
  if (regexec(&preg, eboy, (size_t) 0, NULL, 0)) {
    if (sender)
      chanservstdmessage(sender, QM_INVALIDEMAIL, eboy);
    return (1);
  }

  return (0);
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
