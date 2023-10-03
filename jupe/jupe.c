#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>

#include "../core/hooks.h"
#include "../lib/sstring.h"
#include "../server/server.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../lib/irc_string.h"
#include "jupe.h"

static jupe_t *jupes = NULL;

static int handlejupe(void *source, int cargc, char **cargv);
static jupe_t *make_jupe(char *server, char *reason, time_t expirets, time_t lastmod, unsigned int flags);
static void jupe_free(jupe_t *);

void _init() {
  /* If we're connected to IRC, force a disconnect. */
  if (connected) {
    irc_send("%s SQ %s 0 :Resync [adding jupe support]", mynumeric->content, myserver->content);
    irc_disconnected(0);
  }

  registerserverhandler("JU", &handlejupe, 5);
}

void _fini() {
  while (jupes) {
    jupe_t *next = jupes->ju_next;

    jupe_free(jupes);

    jupes = next;
  }

  deregisterserverhandler("JU", &handlejupe);
}

static int handlejupe(void *source, int cargc, char **cargv) {
  char *server, *expire, *modtime, *reason;
  jupe_t *jupe;
  unsigned int flags;

  if (cargc < 5)
    return CMD_OK; /* local jupe or not a valid.. we don't care */

  server = cargv[1];
  expire = cargv[2];
  modtime = cargv[3];
  reason = cargv[4];

  if (atoi(expire) > JUPE_MAX_EXPIRE || atoi(expire) <= 0)
    return CMD_ERROR; /* jupe's expiry date is not valid */

  if (server[0] != '+' && server[0] != '-')
    return CMD_OK; /* not a valid jupe either */
  else {
    flags = (server[0] == '+') ? JUPE_ACTIVE : 0;
    server++;
  }

  jupe = jupe_find(server);

  if (jupe != NULL && atoi(modtime) > jupe->ju_lastmod) {
    jupe->ju_flags = flags;
    jupe->ju_lastmod = atoi(modtime);

    Error("jupe", ERR_INFO, "jupe modified for %s (%s) expiring in %s,"
                               " lastmod: %s, active: %s", server, reason, expire, modtime, flags ? "yes" : "no");
    return CMD_OK;
  }

  jupe = make_jupe(server, reason, getnettime() + atoi(expire), atoi(modtime), flags);

  if (jupe == NULL)
    return CMD_ERROR;

  Error("jupe", ERR_INFO, "jupe added for %s (%s) expiring in %s,"
               " lastmod: %s, active: %s", server, reason, expire, modtime, flags ? "yes" : "no");

  return CMD_OK;
}

jupe_t *make_jupe(char *server, char *reason, time_t expirets, time_t lastmod, unsigned int flags) {
  jupe_t *jupe;

  jupe = (jupe_t*)malloc(sizeof(jupe_t));

  if (jupe == NULL)
    return NULL;

  jupe->ju_server = getsstring(server, HOSTLEN);
  jupe->ju_reason = getsstring(reason, TOPICLEN);
  jupe->ju_expire = expirets;
  jupe->ju_lastmod = lastmod;
  jupe->ju_flags = flags;
  jupe->ju_next = NULL;

  /* add the jupe to our linked list */
  jupe->ju_next = jupes;
  jupes = jupe;

  return jupe;
}

void jupe_propagate(jupe_t *jupe) {
  irc_send("%s JU * %c%s %jd %jd :%s", mynumeric->content, 
           JupeIsRemActive(jupe) ? '+' : '-',
           JupeServer(jupe),
           (intmax_t)(jupe->ju_expire - getnettime()),
           (intmax_t)JupeLastMod(jupe),
           JupeReason(jupe));
}

static void jupe_expire(void) {
  time_t nettime = getnettime();

  for (jupe_t **p = &jupes, *j = *p; j; j = *p) {
    if (j->ju_expire <= nettime) {
      *p = j->ju_next;
      jupe_free(j);
    } else {
      p = &j->ju_next;
    }
  }
}

jupe_t *jupe_next(jupe_t *current) {
  if (current == NULL) {
    jupe_expire();
    return jupes;
  }

  return current->ju_next;
}

jupe_t *jupe_find(char *server) {
  for (jupe_t *jupe = jupe_next(NULL); jupe; jupe = jupe_next(jupe)) {
    if (ircd_strcmp(server, JupeServer(jupe)) == 0) {
      return jupe;
    }
  }

  return NULL;
}

static void jupe_free(jupe_t *jupe) {
  freesstring(jupe->ju_server);
  freesstring(jupe->ju_reason);
  free(jupe);
}

void jupe_activate(jupe_t *jupe) {
  if (jupe->ju_flags & JUPE_ACTIVE)
    return;

  jupe->ju_flags |= JUPE_ACTIVE;
  jupe->ju_lastmod = getnettime();

  jupe_propagate(jupe);
}

void jupe_deactivate(jupe_t *jupe) {
  if ((jupe->ju_flags & JUPE_ACTIVE) == 0)
    return;

  jupe->ju_flags &= ~JUPE_ACTIVE;
  jupe->ju_lastmod = getnettime();

  jupe_propagate(jupe);
}

int jupe_add(char *server, char *reason, time_t duration, unsigned int flags) {
  jupe_t *jupe;

  if (jupe_find(server) != NULL)
    return 0;

  if (duration > JUPE_MAX_EXPIRE || duration <= 0)
    return 0;

  jupe = make_jupe(server, reason, getnettime() + duration, getnettime(), flags);

  if (jupe == NULL)
    return 0;

  jupe_propagate(jupe);

  return 1;
}
