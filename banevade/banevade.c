#include "../irc/irc.h"
#include "../lib/version.h"
#include "../channel/channel.h"

MODULE_VERSION("")

void be_onjoin(int hooknum, void *arg) {
	void **arglist = (void **)arg;
	channel *c = ((channel *)arglist[0]);
	nick *np = arglist[1];

	/* Verify both are valid */
	if(!c || !np)
		return;

	/* Only check if user has a spoofed host and is authed */
	if(IsSetHost(np) && IsAccount(np)) {
		if(nickbanned(np, c, 0)) {
			/* User is considered as banned by newserv
			 * Note: nickbanned doesn't check against old ident
			 * So if user was "badword@a.b" and sethost'd to "goodword@a.b"
			 * they'd be able to bypass *!badword@* bans
			*/
			triggerhook(HOOK_CHANNEL_JOIN_BYPASS_BAN, arg);
		}
	}

}

void _init() {
	registerhook(HOOK_CHANNEL_JOIN, &be_onjoin);
	registerhook(HOOK_CHANNEL_CREATE, &be_onjoin);
}

void _fini() {
	deregisterhook(HOOK_CHANNEL_JOIN, &be_onjoin);
	deregisterhook(HOOK_CHANNEL_CREATE, &be_onjoin);
}

