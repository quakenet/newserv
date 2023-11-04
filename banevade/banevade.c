#include <stdio.h>
#include "../irc/irc.h"
#include "../lib/version.h"
#include "../lib/irc_string.h"
#include "../channel/channel.h"
#include "../bans/bans.h"
#include "../control/control.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"

#define BANEVADE_SPAM
//#define BANEVADE_ENFORCE

MODULE_VERSION("")

int ircdbanned(nick *n, channel *c);

static chanban *nickbanned2(nick *np, channel *cp, int visibleonly) {
	chanban *cbp;

	for (cbp=cp->bans;cbp;cbp=cbp->next) {
		if (nickmatchban(np,cbp,visibleonly))
			return cbp;
	}

	return NULL;
}

void be_onjoin(int hooknum, void *arg) {
	void **arglist = (void **)arg;
	channel *c = ((channel *)arglist[0]);
	nick *np = arglist[1];

	/* Verify both are valid */
	if(!c || !np)
		return;

	/* Services, opers and extended opers can bypass bans */
	if (IsService(np) || IsOper(np) || IsXOper(np) || NickOnServiceServer(np))
		return;

	int ircd_banned = ircdbanned(np, c);
	chanban *ns_banned = nickbanned2(np, c, 0);

#ifdef BANEVADE_SPAM
	Error("banevade", ERR_INFO, "Ban status ircd_banned=%d newserv_banned=%s", ircd_banned, (ns_banned ? "here" : "(null)"));
#endif

	/* If we're not considered banned as per the ircd code,
	 but banned according to newserv - trigger the hook */
	if(!ircd_banned && ns_banned) {
		/* We kinda dont need this hook anymore - but we'll trigger it anyway for future scripts/modules */
		triggerhook(HOOK_CHANNEL_JOIN_BYPASS_BAN, arg);

		channel *cp = findchannel("#qnet.banevade");
		if(cp)
			controlchanmsg(cp, "banevade: bad user in %s: %s!%s@%s vs. %s", c->index->name->content, np->nick, np->ident, np->host->name->content, bantostring(ns_banned));

		if(IsAccount(np)) {
			/* Reset host to [cc].HIS_HIDDENHOST */
			char host[HOSTLEN+1] = "";
			snprintf(host, sizeof(host), "%s.%s", np->authname, HIS_HIDDENHOST);

#ifdef BANEVADE_ENFORCE
			/* I didnt find a function for this, so we're doing it R A W */
			irc_send("%s SH %s %s %s", mynumeric->content, longtonumeric(np->numeric, 5), np->ident, host);

			/* Also remove them from the channel they're not supposed to be in */
			/* We're gonna go with the generic default-Q-kick-reason since we don't know the real reason for the kick */
			localkickuser(NULL, c, np, "Banned.");
#endif
		} else {
			/* Honest to god, this should never happen since the IRCD is in a state where it
			 * checks EITHER the sethost or the authname. So people shouldn't be able to avoid bans
			 * unauthed. But we still need to handle it, incase shit hits the fan. */

			/* User has a spoofed host but isn't authed, most likely a s:line (for some reason I want to write an s:line). Send them flying. */
			/* We're not gonna kill the user because they'll most likely just reconnect and rejoin, causing a buttload of spam. */
			/* So just trigger an extra line of spam into qnet.banevade so we'll investigate manually! */
			//killuser(NULL, np, "Please don't avoid bans on QuakeNet");

			/* Also send an additional spam line to the amazing OGs in qnet.banevade */
			if(cp) controlchanmsg(cp, "banevade: user %s appears to be un-authed, this might be a s:line!", np->nick);
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

/* This code is based on snircd 1.3.4a, which has the bug where authed users with +h
 could bypass *!*@[account].users.quakenet.org bans.
 It has been somewhat rewritten to fit the NewServ ban flags and structure
*/

/* The ircd checks 4 things:
 *
 * The clients nick!username (where username is current, original usernames are not checked)
 * The clients real ip towards IP bans
 * The real host (if client is using a cloaked host)
 * The current host of the client
 *
 * This causes the bug that [account].HIS_HIDDENHOST is never checked towards the banlist
 * if they have +h set.
 * We need to replicate that check in this code
*/

#ifdef BANEVADE_SPAM
#define IFNULLELSE(x, y) (x ? y : "(null)")
#endif

int ircdbanned(nick *n, channel *c) {
	char	*nick = n->nick;
	char	*ident = n->ident;
	char	tmphost[HOSTLEN + 1];
	char	*sr;
	chanban	*cb = NULL;

	/* Here is the bug in the ircd.
	 * In a perfect world we'd have one more variable to save both sethost and authhost
	*/
	if (!IsAccount(n))
		sr = NULL;
	else if (IsSetHost(n))
		sr = n->host->name->content;
	else {
		snprintf(tmphost, sizeof(tmphost), "%s.%s", n->authname, HIS_HIDDENHOST);
		sr = tmphost;
	}

	/* Bug 2020-10-22 anton
	 * snircd is only checking the VISIBLE host (not the real rdns, if any)
	 * we were checking real host, and if sethost'd we also checked the fake host.
	 * We gotta do it the other way around, check visible host and if sethost'd
	 * then check the real host to be as accurate as snircd as possible.
	*/
	/* Adding variable for visible host */
	sstring *vh = n->sethost ? n->sethost : n->host->name;

#ifdef BANEVADE_SPAM
	Error("banevade", ERR_INFO, "---------------------------");
	Error("banevade", ERR_INFO, "User %s has joined %s.", nick, c->index->name->content);
	Error("banevade", ERR_INFO, "We will be checking the following:");
	Error("banevade", ERR_INFO, " nick=%s ident=%s iphost=%s sr=%s host=%s", nick, ident, IPtostr(n->ipaddress), IFNULLELSE(sr, sr), vh->content);
#endif

	/* Walk through ban list. */
	for (cb = c->bans; cb; cb = cb->next) {
#ifdef BANEVADE_SPAM
		Error("banevade", ERR_INFO, "Ban for: %s!%s@%s", IFNULLELSE(cb->nick, cb->nick->content), IFNULLELSE(cb->user, cb->user->content), IFNULLELSE(cb->host, cb->host->content));
#endif

		/* Compare nick and user portion of ban. */
		/* ircd code:
		 banlist->banstr[banlist->nu_len] = '\0';
		 res = match(banlist->banstr, nu);
		 banlist->banstr[banlist->nu_len] = '@';
		*/

		/* Our code - since we don't save nick!user in our ban structure - we need to check it using other tools!
		 * I've only used match2strings, I've not used the flags for CHANBAN_NICKEXACT etc.
		 * The only issue I could think of is the lack of irc_compare for {[]} etc.
		 * But the ircd uses match, so I went with this to be somewhat 1:1 close to it.
		 * Give me a poke if you want this to be more like nickbanned() with those checks.
		*/
		if(cb->nick) {
			if(!match2strings(cb->nick->content, nick)) {
#ifdef BANEVADE_SPAM
				Error("banevade", ERR_INFO, " nick is present but not matching");
#endif
				continue;
			}
		}
		if(cb->user) {
			if(!match2strings(cb->user->content, ident)) {
#ifdef BANEVADE_SPAM
				Error("banevade", ERR_INFO, " user is present but not matching");
#endif
				continue;
			}
		}

		/* Compare host portion of ban. */
		/* ircd code:
		 if (!((cb->flags & BAN_IPMASK)
			&& ipmask_check(&cli_ip(cptr), &cb->address, cb->addrbits))
			&& match(hostmask, cli_user(cptr)->host)
			&& !(sr && !match(hostmask, sr)))
				continue;
		*/
		if(cb->host) {
			if(
				/* Check current host */
				!match2strings(cb->host->content, vh->content)
				/* Check IP if ip-ban */
				&& !((cb->flags & CHANBAN_IP) && ipmask_check(&(n->ipnode->prefix->sin), &(cb->ipaddr), cb->prefixlen))
				/* Check our real host (if +h/+x) */
				&& !(sr && match2strings(cb->host->content, sr))
			) {
#ifdef BANEVADE_SPAM
				Error("banevade", ERR_INFO, " host is present but not matching.");
#endif
				continue;
			}
		}

		/* If we got this far, everything that exists in the ban matches! */
#ifdef BANEVADE_SPAM
		Error("banevade", ERR_INFO, " -- Ban appears to hit the user.");
#endif
		return 1;
	}

	return 0;
}
