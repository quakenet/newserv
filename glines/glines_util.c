#include "../irc/irc.h"
#include "glines.h"

int glinebyip(const char *user, struct irc_in_addr *ip, unsigned char bits, int duration, const char *reason, int flags, const char *creator) {
  glinebuf gbuf;
  int hits;

  glinebufinit(&gbuf, 1);
  glinebufaddbyip(&gbuf, user, ip, bits, flags, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);

  glinebufcounthits(&gbuf, &hits, NULL, NULL);

  if (flags & GLINE_SIMULATE)
    glinebufabandon(&gbuf);
  else
    glinebufflush(&gbuf, 1);

  return hits;
}

int glinebynick(nick *np, int duration, const char *reason, int flags, const char *creator) {
  glinebuf gbuf;
  int hits;

  glinebufinit(&gbuf, 1);
  glinebufaddbynick(&gbuf, np, flags, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);

  glinebufcounthits(&gbuf, &hits, NULL, NULL);

  if (flags & GLINE_SIMULATE)
    glinebufabandon(&gbuf);
  else
    glinebufflush(&gbuf, 1);

  return hits;
}

void glineunsetmask(const char *mask) {
  gline *gl;

  gl = findgline(mask);

  if (!gl)
    return;

  gline_deactivate(gl, 0, 1);
}
