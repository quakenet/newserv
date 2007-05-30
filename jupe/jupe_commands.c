#include "../control/control.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "jupe.h"

int ju_addjupe(void *source, int cargc, char **cargv) {
	nick *np = (nick*)source;
	int result, duration;
	
	if (cargc < 3) {
		controlreply(np, "Syntax: addjupe <servername> <duration> <reason>");

		return CMD_OK;
	}

	if (jupe_find(cargv[0]) != NULL) {
		controlreply(np, "There is already a jupe for that server.");

		return CMD_OK;
	}

	duration = durationtolong(cargv[1]);
	
	if (duration > JUPE_MAX_EXPIRE) {
		controlreply(np, "A jupe's maximum duration is %s. Could not create jupe.", longtoduration(JUPE_MAX_EXPIRE));

		return CMD_OK;
	}

	result = jupe_add(cargv[0], cargv[2], duration, JUPE_ACTIVE);

	if (result)
		controlreply(np, "Done.");
	else
		controlreply(np, "Jupe could not be created.");
			
	return CMD_OK;
}

int ju_activatejupe(void *source, int cargc, char **cargv) {
	nick *np = (nick*)source;
	jupe_t *jupe;

	if (cargc < 1) {
		controlreply(np, "Syntax: activatejupe <servername>");

		return CMD_OK;
	}

	jupe = jupe_find(cargv[0]);

	if (jupe == NULL) {
		controlreply(np, "There is no such jupe.");

		return CMD_OK;
	}

	if (jupe->ju_flags & JUPE_ACTIVE) {
		controlreply(np, "This jupe is already activated.");

		return CMD_OK;
	}

	jupe_activate(jupe);

	controlreply(np, "Done.");

	return CMD_OK;
}

int ju_deactivatejupe(void *source, int cargc, char **cargv) {
	nick *np = (nick*)source;
	jupe_t *jupe;

	if (cargc < 1) {
		controlreply(np, "Syntax: deactivatejupe <servername>");

		return CMD_OK;
	}

	jupe = jupe_find(cargv[0]);

	if (jupe == NULL) {
		controlreply(np, "There is no such jupe.");

		return CMD_OK;
	}

	if ((jupe->ju_flags & JUPE_ACTIVE) == 0) {
		controlreply(np, "This jupe is already deactivated.");

		return CMD_OK;
	}

	jupe_deactivate(jupe);

	controlreply(np, "Done.");
	
	return CMD_OK;
}

int ju_jupelist(void *source, int cargc, char **cargv) {
	nick *np = (nick*)source;
	jupe_t *jupe;

	jupe_expire();

	jupe = jupes;
	
	controlreply(np, "Server Reason Expires Status");

	while (jupe) {
		controlreply(np, "%s %s %s %s", JupeServer(jupe), JupeReason(jupe), longtoduration(jupe->ju_expire - getnettime()), (jupe->ju_flags & JUPE_ACTIVE) ? "activated" : "deactivated");
		
		jupe = jupe->ju_next;
	}

	controlreply(np, "--- End of JUPE list.");

	return CMD_OK;
}

void _init(void) {
	registercontrolcmd("addjupe", 10, 3, ju_addjupe);
	registercontrolcmd("activatejupe", 10, 1, ju_activatejupe);
	registercontrolcmd("deactivatejupe", 10, 1, ju_deactivatejupe);
	registercontrolcmd("jupelist", 10, 0, ju_jupelist);
}

void _fini(void) {
	deregistercontrolcmd("addjupe", ju_addjupe);
	deregistercontrolcmd("activatejupe", ju_activatejupe);
	deregistercontrolcmd("deactivatejupe", ju_deactivatejupe);
	deregistercontrolcmd("jupelist", ju_jupelist);
}
