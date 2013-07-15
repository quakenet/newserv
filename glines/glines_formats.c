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

/** Find canonical user and host for a string.
 * If \a userhost starts with '$', assign \a userhost to *user_p and NULL to *host_p.
 * Otherwise, if \a userhost contains '@', assign the earlier part of it to *user_p and the rest to *host_p.
 * Otherwise, assign \a def_user to *user_p and \a userhost to *host_p.
 *
 * @param[in] userhost Input string from user.
 * @param[out] nick_p Gets pointer to nick part of hostmask.
 * @param[out] user_p Gets pointer to user (or channel/realname) part of hostmask.
 * @param[out] host_p Gets point to host part of hostmask (may be assigned NULL).
 * @param[in] def_user Default value for user part.
 */
static void
canon_userhost(char *userhost, char **nick_p, char **user_p, char **host_p, char *def_user) {
  char *tmp, *s;

  if (*userhost == '$') {
    *user_p = userhost;
    *host_p = NULL;
    *nick_p = NULL;
    return;
  }

  if ((tmp = strchr(userhost, '!'))) {
    *nick_p = userhost;
    *(tmp++) = '\0';
  } else {
    *nick_p = def_user;
    tmp = userhost;
  }

  if (!(s = strchr(tmp, '@'))) {
    *user_p = def_user;
    *host_p = tmp;
  } else {
    *user_p = tmp;
    *(s++) = '\0';
    *host_p = s;
  }
}

gline *makegline(const char *mask) {
  /* populate gl-> user,host,node,nick and set appropriate flags */
  gline *gl;
  char dupmask[512];
  char *nick, *user, *host;
  const char *pos;
  int count;

  /* Make sure there are no spaces in the mask */
  if (strchr(mask, ' ') != NULL)
    return NULL;

  gl = newgline();

  if (!gl) {
    Error("gline", ERR_ERROR, "Failed to allocate new gline");
    return NULL;
  }
  
  if (mask[0] == '#' || mask[0] == '&') {
    gl->flags |= GLINE_BADCHAN;
    gl->user = getsstring(mask, CHANNELLEN);
    return gl;
  }

  if (mask[0] == '$') {
    if (mask[1] != 'R') {
      freegline(gl);
      return NULL;
    }

    gl->flags |= GLINE_REALNAME;
    gl->user = getsstring(mask + 2, REALLEN);
    return gl;
  }

  strncpy(dupmask, mask, sizeof(dupmask));
  canon_userhost(dupmask, &nick, &user, &host, "*");

  /* Make sure it's not too long */
  if (strlen(nick) + strlen(user) + strlen(host) > NICKLEN + USERLEN + HOSTLEN) {
    freegline(gl);
    return NULL;
  }

  if (ipmask_parse(host, &gl->ip, &gl->bits))
    gl->flags |= GLINE_IPMASK;
  else
    gl->flags |= GLINE_HOSTMASK;

  /* Don't allow invalid IPv6 bans as those might match * on snircd 1.3.4 */
  count = 0;

  for (pos = host; *pos; pos++)
    if (*pos == ':')
      count++;

  if (count >= 8) {
    controlwall(NO_OPER, NL_GLINES, "Warning: Parsed invalid IPv6 G-Line: %s", mask);
    freegline(gl);
    return NULL;
  }

  if (strcmp(nick, "*") != 0)
    gl->nick = getsstring(nick, 512);

  if (strcmp(user, "*") != 0)
    gl->user = getsstring(user, 512);

  if (strcmp(host, "*") != 0)
    gl->host = getsstring(host, 512);

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

