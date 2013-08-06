#include "../irc/irc.h"
#include "../trusts/trusts.h"
#include "glines.h"

int glinebyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int duration, const char *reason, int flags, const char *creator) {
  glinebuf gbuf;
  int hits;

  glinebufinit(&gbuf, 0);
  glinebufcommentf(&gbuf, "on IP mask %s@%s, set by %s", user, CIDRtostr(*ip, bits), creator);
  glinebufaddbyip(&gbuf, user, ip, bits, flags, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);

  glinebufcounthits(&gbuf, &hits, NULL);

  if (flags & GLINE_SIMULATE)
    glinebufabort(&gbuf);
  else
    glinebufcommit(&gbuf, 1);

  return hits;
}

int glinebynick(nick *np, int duration, const char *reason, int flags, const char *creator) {
  glinebuf gbuf;
  int hits;

  glinebufinit(&gbuf, 0);
  glinebufcommentf(&gbuf, "on nick %s!%s@%s, set by %s", np->nick, np->ident, np->host->name->content, creator);
  glinebufaddbynick(&gbuf, np, flags, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);

  glinebufcounthits(&gbuf, &hits, NULL);

  if (flags & GLINE_SIMULATE)
    glinebufabort(&gbuf);
  else
    glinebufcommit(&gbuf, 1);

  return hits;
}

void glineunsetmask(const char *mask) {
  gline *gl;

  gl = findgline(mask);

  if (!gl)
    return;

  gline_deactivate(gl, 0, 1);
}
