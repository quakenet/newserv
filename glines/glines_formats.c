#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../trusts/trusts.h"
#include "../control/control.h"
#include "glines.h"

gline *makegline(const char *mask) {
  /* populate gl-> user,host,node,nick and set appropriate flags */
  gline *gl;
  char nick[512], user[512], host[512];
  const char *pnick = NULL, *puser = NULL, *phost = NULL;

  /* Make sure there are no spaces in the mask, glstore_* depends on this */
  if (strchr(mask, ' ') != NULL)
    return NULL;

  gl = newgline();

  if (!gl) {
    Error("gline", ERR_ERROR, "Failed to allocate new gline");
    return NULL;
  }

  if (*mask == '#' || *mask == '&') {
    gl->flags |= GLINE_BADCHAN;
    gl->user = getsstring(mask, CHANNELLEN);
    return gl;
  }

  if (*mask == '$') {
    if (mask[1] != 'R') {
      freegline(gl);
      return NULL;
    }

    gl->flags |= GLINE_REALNAME;
    gl->user = getsstring(mask + 2, REALLEN);
    return gl;
  }

  if (sscanf(mask, "%[^!]!%[^@]@%s", nick, user, host) == 3) {
    pnick = nick;
    puser = user;
    phost = host;
  } else if (sscanf(mask, "%[^@]@%s", user, host) == 2) {
    puser = user;
    phost = host;
  } else {
    phost = mask;
  }

  /* validate length of the mask components */
  if ((pnick && (pnick[0] == '\0' || strlen(pnick) > NICKLEN)) ||
      (puser && (puser[0] == '\0' || strlen(puser) > USERLEN)) ||
      (phost && (phost[0] == '\0' || strlen(phost) > HOSTLEN))) {
    freegline(gl);
    return NULL;
  }

  /* ! and @ are not allowed in the mask components */
  if ((pnick && (strchr(pnick, '!') || strchr(pnick, '@'))) ||
      (puser && (strchr(puser, '!') || strchr(puser, '@'))) ||
      (phost && (strchr(phost, '!') || strchr(phost, '@')))) {
    freegline(gl);
    return NULL;
  }

  if (phost && ipmask_parse(phost, &gl->ip, &gl->bits))
    gl->flags |= GLINE_IPMASK;
  else
    gl->flags |= GLINE_HOSTMASK;

  if (pnick && strcmp(pnick, "*") != 0)
    gl->nick = getsstring(pnick, NICKLEN);

  if (puser && strcmp(puser, "*") != 0)
    gl->user = getsstring(puser, USERLEN);

  if (phost && strcmp(phost, "*") != 0)
    gl->host = getsstring(phost, HOSTLEN);

  return gl;
}

char *glinetostring(gline *gl) {
  static char mask[512]; /* check */

  if (gl->flags & GLINE_REALNAME) {
   if (gl->user)
     snprintf(mask, sizeof(mask), "$R%s", gl->user->content);
   else
     strncpy(mask, "$R*", sizeof(mask));

   return mask;
  }

  if (gl->flags & GLINE_BADCHAN) {
    assert(gl->user);

    strncpy(mask, gl->user->content, sizeof(mask));
    return mask;
  }

  snprintf(mask, sizeof(mask), "%s!%s@%s",
    gl->nick ? gl->nick->content : "*",
    gl->user ? gl->user->content : "*",
    gl->host ? gl->host->content : "*");

  return mask;
}

