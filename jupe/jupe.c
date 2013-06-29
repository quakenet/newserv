#include <stdlib.h>
#include <sys/time.h>

#include "../core/hooks.h"
#include "../lib/sstring.h"
#include "../server/server.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../lib/irc_string.h"
#include "jupe.h"

jupe_t *jupes;

int handlejupe(void *source, int cargc, char **cargv);
void sendjupeburst(int hook, void *args);

void _init() {
	jupes = NULL;

        /* If we're connected to IRC, force a disconnect. */ 
        if (connected) {
                irc_send("%s SQ %s 0 :Resync [adding jupe support]",mynumeric->content,myserver->content); irc_disconnected();
        }

	registerhook(HOOK_IRC_SENDBURSTBURSTS, &sendjupeburst);
	
	registerserverhandler("JU", &handlejupe, 5);
}

void _fini() {
	jupe_t *next;

	while (jupes) {
		/* keep a pointer to the next item */
		next = jupes->ju_next;

		free(jupes);

		jupes = next;
	}

	deregisterhook(HOOK_IRC_SENDBURSTBURSTS, &sendjupeburst);
	
	deregisterserverhandler("JU", &handlejupe);
}

int handlejupe(void *source, int cargc, char **cargv) {
	char *target, *server, *expire, *modtime, *reason;
	jupe_t *jupe;
	unsigned int flags;

	if (cargc < 5)
		return CMD_OK; /* local jupe or not a valid.. we don't care */

	target = cargv[0];
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

		Error("jupe", ERR_WARNING, "jupe modified for %s (%s) expiring in %s,"
				                        " lastmod: %s, active: %s", server, reason, expire, modtime, flags ? "yes" : "no");

		return CMD_OK;
	}

	jupe = make_jupe(server, reason, getnettime() + atoi(expire),
			atoi(modtime), flags);

	if (jupe == NULL)
		return CMD_ERROR;

	Error("jupe", ERR_WARNING, "jupe added for %s (%s) expiring in %s,"
			" lastmod: %s, active: %s", server, reason, expire, modtime, flags ? "yes" : "no");

	return CMD_OK;
}

void sendjupeburst(int hook, void *args) {
	jupe_t *jupe = jupes;

	if (hook != HOOK_IRC_SENDBURSTBURSTS)
		return;

	while (jupe) {
		jupe_propagate(jupe);

		jupe = jupe->ju_next;
	}
}

jupe_t *make_jupe(char *server, char *reason, time_t expirets, time_t lastmod, unsigned int flags) {
	jupe_t *jupe, *last_jupe;

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
	if (jupes == NULL)
		jupes = jupe;
	else {
		last_jupe = jupes;

		while (last_jupe->ju_next)
			last_jupe = last_jupe->ju_next;

		last_jupe->ju_next = jupe;
	}

	return jupe;
}

void jupe_propagate(jupe_t *jupe) {
	irc_send("%s JU * %c%s %d %d :%s", mynumeric->content, 
			JupeIsRemActive(jupe) ? '+' : '-',
			JupeServer(jupe),
			jupe->ju_expire - getnettime(),
			JupeLastMod(jupe),
			JupeReason(jupe));
}

void jupe_expire(void) {
	jupe_find(NULL);
}

jupe_t *jupe_find(char *server) {
	jupe_t *jupe = jupes;

	while (jupe) {
		/* server == NULL if jupe_find() is used by jupe_expire */
		if (server && ircd_strcmp(server, JupeServer(jupe)) == 0)
			return jupe;

		if (jupe->ju_next && jupe->ju_next->ju_expire < getnettime())
			jupe_free(jupe->ju_next);
		
		jupe = jupe->ju_next;
	}

	if (jupes && jupes->ju_expire < getnettime())
		jupe_free(jupes);

	return NULL;
}

void jupe_free(jupe_t *jupe) {
	jupe_t *trav = jupes;

	if (jupe == jupes)
		jupes = jupe->ju_next;
	else {
		while (trav) {
			if (trav->ju_next == jupe) {
				trav->ju_next = jupe->ju_next;

				break;
			}

			trav = trav->ju_next;
		}
	}

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

	jupe = make_jupe(server, reason, getnettime() + duration,
			getnettime(), flags);
	
	if (jupe == NULL)
		return 0;

	jupe_propagate(jupe);

	return 1;
}
