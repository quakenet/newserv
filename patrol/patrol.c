#include <ctype.h>
#include <string.h>
#include "../lib/strlfunc.h"
#include "../core/config.h"
#include "../core/schedule.h"
#include "../localuser/localuser.h"
#include "patrol.h"

static sstring *patrol_hostpool[PATROL_POOLSIZE], *patrol_tailpool[PATROL_POOLSIZE];
static unsigned int patrol_hostpoolsize, patrol_tailpoolsize;
static int patrol_min_hosts;
static char patrol_hostmode;

int patrol_minmaxrand(float min, float max) {
  return (int)((max - min + 1) * rand() / (RAND_MAX + min)) + min;
}

nick *patrol_selectuser(void) {
  int target = patrol_minmaxrand(0, 500), loops = 150, j;
  nick *np;

  do {
    for (j = patrol_minmaxrand(0, NICKHASHSIZE - 1); j < NICKHASHSIZE; j++)
      for (np = nicktable[j]; np; np = np->next)
        if (!--target)
          return np;
  } while (--loops > 0);

  return NULL;
}

host *patrol_selecthost(void) {
  int target = patrol_minmaxrand(0, 500), loops = 150, j;
  host *hp;

  do {
    for (j = patrol_minmaxrand(0, HOSTHASHSIZE - 1); j < HOSTHASHSIZE; j++)
      for (hp = hosttable[j]; hp; hp = hp->next)
        if (!--target)
          return hp;
  } while (--loops > 0);

  return NULL;
}

int patrol_isip(char *host) {
  char *p = host, components = 0, length = 0;

  for (; *p; p++) {
    if (*p == '.') {
      if (((!length) || (length = 0)) || (++components > 3))
        return 0;
    } else {
      if ((++length > 3) || !isdigit(*p))
        return 0;
    }
  }

  return components == 3;
}

static int specialuseronhost(host *hp) {
  nick *np;

  for (np = hp->nicks; np; np = np->nextbyhost)
    if (IsOper(np) || IsService(np) || IsXOper(np) || NickOnServiceServer(np))
      return 1;

  return 0;
}

char patrol_genchar(int ty) {
  /* hostname and realname characters*/
  if (!ty) {
    if (!(patrol_minmaxrand(0, 40) % 10)) {
      return patrol_minmaxrand(48, 57);
    } else {
      return patrol_minmaxrand(97, 122);
    }

    /* ident characters - without numbers*/
  } else if (ty == 1) {
    return patrol_minmaxrand(97, 122);
    /* ident characters - with numbers*/
  } else if (ty == 2) {
    ty = patrol_minmaxrand(97, 125);

    if (ty > 122) return patrol_minmaxrand(48, 57);

    return ty;
    /* nick characters - with and without numbers*/
  } else if (ty == 3 || ty == 4) {
    if (!(patrol_minmaxrand(0, 59) % 16)) {
      char weirdos[6] = { '\\', '|', '[', '{', ']', '}' };
      return weirdos[patrol_minmaxrand(0, 5)];
    }

    if (ty == 4) {
      ty = patrol_minmaxrand(65, 93);

      if (ty > 90) return patrol_minmaxrand(48, 57);
    } else {
      ty = patrol_minmaxrand(65, 90);
    }

    if (!(patrol_minmaxrand(0, 40) % 8)) return ty;

    return ty + 32;
    /* moron check */
  } else {
    return ' ';
  }
}

void patrol_gennick(char *ptc, char size) {
  int i;

  for (i = 0; i < size; i++) {
    if (i == 0) {
      ptc[i] = patrol_genchar(3);
    } else {
      ptc[i] = patrol_genchar(4);
    }
  }

  ptc[i] = '\0';
}

void patrol_genident(char *ptc, char size) {
  int i;

  for (i = 0; i < size; i++) {
    if (i == 0) {
      ptc[i] = patrol_genchar(1);
    } else {
      ptc[i] = patrol_genchar(2);
    }
  }

  ptc[i] = '\0';
}

void patrol_genhost(char *ptc, char size, struct irc_in_addr *ipaddress) {
  int dots = patrol_minmaxrand(2, 5), i, dotexist = 0, cur;

  while (!dotexist) {
    for (i = 0; i < size; i++) {
      ptc[i] = patrol_genchar(0);

      if ((i > 5) && (i < (size - 4))) {
        if ((ptc[i - 1] != '.') && (ptc[i - 1] != '-')) {
          cur = patrol_minmaxrand(1, size / dots);

          if (cur < 3) {
            if (cur == 1) {
              ptc[i] = '.';
              dotexist = 1;
            } else {
              ptc[i] = '-';
            }
          }
        }
      }
    }
  }

  ptc[i] = '\0';

  memset(ipaddress, 0, sizeof(ipaddress));
  ((unsigned short *)(ipaddress->in6_16))[5] = 65535;
  ((unsigned short *)(ipaddress->in6_16))[6] = patrol_minmaxrand(0, 65535);
  ((unsigned short *)(ipaddress->in6_16))[7] = patrol_minmaxrand(0, 65535);
}

void patrol_genreal(char *ptc, char size) {
  int spaces = patrol_minmaxrand(2, 4), i;

  for (i = 0; i < size; i++) {
    ptc[i] = patrol_genchar(0);

    if ((i > 5) && (i < (size - 4))) {
      if (ptc[i - 1] != ' ') {
        if (patrol_minmaxrand(1, size / spaces) == 1) ptc[i] = ' ';
      }
    }
  }

  ptc[i] = '\0';
}

void patrol_generatehost(char *buf, int maxsize, struct irc_in_addr *ipaddress) {
  if (PATROL_HOST_MODE == PATROL_STEAL_HOST) {
    host *hp;
    int loops = 20;

    buf[0] = '\0';

    do {
      hp = patrol_selecthost();

      if (hp && (hp->clonecount <= PATROL_MAX_CLONE_COUNT) && !patrol_isip(hp->name->content) && !specialuseronhost(hp)) {
        strlcpy(buf, hp->name->content, maxsize + 1);

        if (hp->nicks) {
          memcpy(ipaddress, &hp->nicks->ipaddress, sizeof(struct irc_in_addr));
        } else {
          memset(ipaddress, 0, sizeof(struct irc_in_addr));
          ((unsigned short *)(ipaddress->in6_16))[5] = 65535;
          ((unsigned short *)(ipaddress->in6_16))[6] = patrol_minmaxrand(0, 65535);
          ((unsigned short *)(ipaddress->in6_16))[7] = patrol_minmaxrand(0, 65535);
        }

        break;
      }
    } while (--loops > 0);
  } else {
    char *cpos;
    int pieces = patrol_minmaxrand(2, 4), totallen = 0, a = 0, i;
    int *choices = malloc(sizeof(int) * (pieces + 1));
    int *lengths = malloc(sizeof(int) * (pieces + 1));

    choices[pieces] = patrol_minmaxrand(0, patrol_tailpoolsize - 1);
    lengths[pieces] = strlen(patrol_tailpool[choices[pieces]]->content) + 1;
    totallen += lengths[pieces];

    for (i = 0; i < pieces; i++) {
      choices[i] = patrol_minmaxrand(0, patrol_hostpoolsize - 1);
      lengths[i] = strlen(patrol_hostpool[choices[i]]->content) + 1;

      if (totallen + lengths[i] > maxsize) {
        choices[i] = choices[pieces];
        lengths[i] = lengths[pieces];
        pieces -= (pieces - i);
        break;
      }

      totallen += lengths[i];
    }

    for (i = 0; i < pieces; i++) {
      for (cpos = patrol_hostpool[choices[i]]->content; *cpos;)
        buf[a++] = *cpos++;

      buf[a++] = '.';
    }

    for (cpos = patrol_tailpool[choices[i]]->content; *cpos;) {
      buf[a++] = *cpos++;
    }

    buf[a] = '\0';
    free(choices);
    free(lengths);

    memset(ipaddress, 0, sizeof(struct irc_in_addr));
    ((unsigned short *)(ipaddress->in6_16))[5] = 65535;
    ((unsigned short *)(ipaddress->in6_16))[6] = patrol_minmaxrand(0, 65535);
    ((unsigned short *)(ipaddress->in6_16))[7] = patrol_minmaxrand(0, 65535);
  }
}

void patrol_generatenick(char *buf, int maxsize) {
  int bits = patrol_minmaxrand(2, 3), loops = 0, wanttocopy, len = 0, i, d = 0, newmaxsize = maxsize - patrol_minmaxrand(0, 7);
  nick *np;

  if (newmaxsize > 2)
    maxsize = newmaxsize;

  do {
    np = patrol_selectuser();

    if (np) {
      wanttocopy = patrol_minmaxrand(1, (strlen(np->nick) / 2) + 3);

      for (i = 0; ((i < wanttocopy) && (len < maxsize)); i++)
        buf[len++] = np->nick[i];

      if (++d > bits) {
        buf[len] = '\0';
        return;
      }
    }
  } while (++loops < 10);

  buf[0] = '\0';
}

void patrol_generateident(char *buf, int maxsize) {
  nick *np = patrol_selectuser();
  buf[0] = '\0';

  if (np)
    strlcpy(buf, np->ident, maxsize + 1);
}

void patrol_generaterealname(char *buf, int maxsize) {
  nick *np = patrol_selectuser();
  buf[0] = '\0';

  if (np)
    strlcpy(buf, np->realname->name->content, maxsize + 1);
}

nick *patrol_generateclone(int extraumodes, UserMessageHandler handler) {
  int loops = 0, modes = UMODE_XOPER | UMODE_INV | extraumodes;
  char c_nick[NICKLEN + 1], c_ident[USERLEN + 1], c_host[HOSTLEN + 1], c_real[REALLEN + 1];
  struct irc_in_addr ipaddress;

  /* PPA: unlikely to be infinite */
  do {
    c_nick[0] = '\0';

    if (!loops && patrol_hostmode) /* only have one go at this */
      patrol_generatenick(c_nick, NICKLEN);

    if (!c_nick[0])
      patrol_gennick(c_nick, patrol_minmaxrand(7, PATROL_MMIN(13, NICKLEN)));

    loops++;
  } while ((getnickbynick(c_nick) != NULL));

  patrol_generateident(c_ident, USERLEN);

  if (!c_ident[0])
    patrol_genident(c_ident, patrol_minmaxrand(4, PATROL_MMIN(8, USERLEN)));

  if (patrol_hostmode) {
    patrol_generatehost(c_host, HOSTLEN, &ipaddress);

    if (!c_host[0])
      patrol_genhost(c_host, HOSTLEN, &ipaddress);
  } else {
    patrol_genhost(c_host, HOSTLEN, &ipaddress);
  }

  patrol_generaterealname(c_real, REALLEN);

  if (!c_real[0])
    patrol_genreal(c_real, patrol_minmaxrand(15, PATROL_MMIN(50, REALLEN)));

  return registerlocaluserflagsip(c_nick, c_ident, c_host, c_real, NULL, 0, 0, modes, &ipaddress, handler);
}

void patrol_nickchange(nick *np) {
  char c_nick[NICKLEN + 1];
  int loops = 0;

  /* PPA: unlikely to be infinite */
  do {
    if ((loops++ < 10) && patrol_hostmode) {
      patrol_generatenick(c_nick, NICKLEN);
    } else {
      patrol_gennick(c_nick, patrol_minmaxrand(7, PATROL_MMIN(13, NICKLEN)));
    }
  } while (c_nick[0] && (getnickbynick(c_nick) != NULL));

  renamelocaluser(np, c_nick);
}

int patrol_is_not_octet(char *begin, int length) {
  int i;

  if (length > 3)
    return 0;

  for (i = 0; i < length; i++) {
    if (!((*begin >= '0') && (*begin <= '9')))
      return 0;

    begin++;
  }

  return 1;
}

int patrol_generatepool(void) {
  int i, k = 0, j = 0, loops = 0;
  char *p, *pp;
  nick *np;

  for (i = 0; i < NICKHASHSIZE; i++)
    for (np = nicktable[i]; np; np = np->next)
      j++;

  if (j < patrol_min_hosts)
    return 0;

  if (PATROL_HOST_MODE == PATROL_STEAL_HOST)
    return PATROL_MINPOOLSIZE;

  i = 0;

  do {
    for (j = patrol_minmaxrand(0, NICKHASHSIZE - 1); j < NICKHASHSIZE; j++) {
      if (nicktable[j]) {
        for (p = nicktable[j]->host->name->content, pp = p; *p;) {
          if (*++p == '.') {
            if (!patrol_is_not_octet(pp, p - pp)) {
              if (i < PATROL_POOLSIZE) {
                if (i < patrol_hostpoolsize)
                  freesstring(patrol_hostpool[i]);

                patrol_hostpool[i] = getsstring(pp, p - pp);
                i++;
              } else {
                if (k >= PATROL_POOLSIZE)
                  break;
              }
            }

            pp = ++p;
          }
        }

        if (!patrol_is_not_octet(pp, p - pp)) {
          if (k < PATROL_POOLSIZE) {
            if (k < patrol_tailpoolsize)
              freesstring(patrol_tailpool[k]);

            patrol_tailpool[k] = getsstring(pp, p - pp);
            k++;
          } else {
            if (i >= PATROL_POOLSIZE)
              break;
          }
        }
      }
    }

    loops++;
  } while ((loops < 5) && ((i < PATROL_POOLSIZE) || (k < PATROL_POOLSIZE)));

  patrol_hostpoolsize = i;
  patrol_tailpoolsize = k;
  return i;
}

int patrol_repool(void) {
  if (patrol_generatepool() < PATROL_MINPOOLSIZE) {
    patrol_hostmode = 0;
    return 0;
  } else {
    patrol_hostmode = 1;
    return 1;
  }
}

void patrol_sched_repool(void *arg) {
  int delta;

  if (patrol_repool())
    delta = PATROL_POOL_REGENERATION;
  else
    delta = 10;

  scheduleoneshot(time(NULL) + delta, &patrol_sched_repool, NULL);
}

void _init(void) {
  char buf[32];
  sstring *m;

  snprintf(buf, sizeof(buf), "%d", PATROL_MINIMUM_HOSTS_BEFORE_POOL);
  m = getcopyconfigitem("patrol", "minpoolhosts", buf, 32);
  patrol_min_hosts = atoi(m->content);
  freesstring(m);

  scheduleoneshot(time(NULL) + 5, &patrol_sched_repool, NULL);
}

void _fini(void) {
  int i;

  deleteallschedules(&patrol_sched_repool);

  for (i = 0; i < patrol_hostpoolsize; i++)
    freesstring(patrol_hostpool[i]);

  for (i = 0; i < patrol_tailpoolsize; i++)
    freesstring(patrol_tailpool[i]);
}

