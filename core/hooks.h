/* hooks.h */

#ifndef __HOOKS_H
#define __HOOKS_H

#include <limits.h>

#define HOOKMAX 5000

/* This is the authoritative registry of all known hook numbers */

#define HOOK_CORE_REHASH             0  /* Argument is an int */
#define HOOK_CORE_STATSREQUEST       1
#define HOOK_CORE_STATSREPLY         2
#define HOOK_CORE_ENDOFHOOKSQUEUE    3
#define HOOK_CORE_STOPERROR          4
#define HOOK_CORE_ERROR	             5	/* Argument is a struct error_event * */
#define HOOK_CORE_SIGUSR1            6 
#define HOOK_CORE_SIGINT             7

#define HOOK_IRC_CONNECTED         100  /* Located in server.c now to fix burst bug */
#define HOOK_IRC_DISCON            101
#define HOOK_IRC_SENDBURSTSERVERS  102  /* Located in server.c now to fix burst bug */
#define HOOK_IRC_SENDBURSTNICKS    103  /* Located in server.c now to fix burst bug */
#define HOOK_IRC_SENDBURSTBURSTS   104  /* Located in server.c now to fix burst bug */
#define HOOK_IRC_PRE_DISCON        105

#define HOOK_SERVER_NEWSERVER      200  /* Argument is number of new server */
#define HOOK_SERVER_LOSTSERVER     201  /* Argument is number of lost server */
#define HOOK_SERVER_END_OF_BURST   202
#define HOOK_SERVER_PRE_LOSTSERVER 203  /* Argument is number of lost server */
#define HOOK_SERVER_LINKED         204  /* Argument is number of server */

#define HOOK_NICK_NEWNICK          300  /* Argument is nick* */
#define HOOK_NICK_RENAME           301  /* Argument is void*[2] (nick *, oldnick) */
#define HOOK_NICK_LOSTNICK         302  /* Argument is nick* */
#define HOOK_NICK_WHOISCHANNELS    303  /* Argument is void*[3] (sender, target, sourcenum) */
#define HOOK_NICK_ACCOUNT          304  /* Argument is nick* */
#define HOOK_NICK_QUIT             305  /* Argument is void*[2] (nick, reason) */
#define HOOK_NICK_SETHOST          306  /* Argument is nick* */
#define HOOK_NICK_MODEOPER         307  /* Argument is void*[2] (nick, modes) */
#define HOOK_NICK_KILL             308  /* Argument is void*[2] (nick, reason) */
#define HOOK_NICK_MASKPRIVMSG      309  /* Argument is void*[3] (nick, target, message) ** NICK COULD BE NULL ** */
#define HOOK_NICK_MODECHANGE       310  /* Argument is void*[2] (nick *, oldmodes) */
#define HOOK_NICK_MESSAGE          311  /* Argument is void*[3] (nick *, message, isnotice) */

#define HOOK_CHANNEL_BURST         400  /* Argument is channel pointer */
#define HOOK_CHANNEL_CREATE        401  /* Argument is void*[2] (channel, nick) */
#define HOOK_CHANNEL_JOIN          402  /* Argument is void*[2] (channel, nick) */
#define HOOK_CHANNEL_PART          403  /* Argument is void*[3] (channel, nick, reason) */
#define HOOK_CHANNEL_KICK          404  /* Argument is void*[4] (channel, kicked, kicker, reason) ** KICKER COULD BE NULL ***/
#define HOOK_CHANNEL_TOPIC         405  /* Argument is void*[2] (channel, nick) ** NICK COULD BE NULL ** */
#define HOOK_CHANNEL_MODECHANGE    406  /* Argument is void*[3] (channel, nick, flags, oldchanmodes) ** NICK COULD BE NULL ** */
#define HOOK_CHANNEL_BANSET        407  /* Argument is void*[2] (channel, nick) ** NICK COULD BE NULL **, ban will be first ban on channel */
#define HOOK_CHANNEL_BANCLEAR      408  /* Argument is void*[2] (channel, nick) ** NICK COULD BE NULL **, ban will be gone.  XXX - could we care what the ban was? */
#define HOOK_CHANNEL_OPPED         409  /* Argument is void*[3] (channel, nick, target) ** NICK COULD BE NULL ** */
#define HOOK_CHANNEL_DEOPPED       410  /* Argument is void*[3] (channel, nick, target) ** NICK COULD BE NULL ** */
#define HOOK_CHANNEL_VOICED        411  /* Argument is void*[3] (channel, nick, target) ** NICK COULD BE NULL ** */
#define HOOK_CHANNEL_DEVOICED      412  /* Argument is void*[3] (channel, nick, target) ** NICK COULD BE NULL ** */

#define HOOK_CHANNEL_NEWCHANNEL    413  /* Argument is channel pointer */
#define HOOK_CHANNEL_LOSTCHANNEL   414  /* Argument is channel pointer */

#define HOOK_CHANNEL_NEWNICK       415  /* Argument is void*[2] (channel, nick) */
#define HOOK_CHANNEL_LOSTNICK      416  /* Argument is void*[2] (channel, nick) */

#define HOOK_CHANSERV_DBLOADED     500 /* No arg */
/* 501 spare for now */
#define HOOK_CHANSERV_RUNNING      502 /* No arg */

#define HOOK_CHANSERV_AUTH         503 /* Argument is void*[2] (nick *, lastauth) */
#define HOOK_CHANSERV_PWCHANGE     504 /* Argument is nick * */
#define HOOK_CHANSERV_CHANLEVMOD   505 /* Argument is void*[3] (nick *, regchanuser *, oldflags) */
#define HOOK_CHANSERV_CHANLEVDUMP  506 /* Argument is nick * */
#define HOOK_CHANSERV_WHOIS        507 /* Argument is nick * */
#define HOOK_CHANSERV_WHOAMI       508 /* Argument is nick * */
#define HOOK_CHANSERV_CMD          509 /* Argument is nick * */

#define HOOK_CONTROL_REGISTERED    600 /* Argument is nick* */
#define HOOK_CONTROL_WHOISREQUEST  601 /* Argument is nick* */
#define HOOK_CONTROL_WHOISREPLY    602 /* Argument is char* */

#define HOOK_SHADOW_SERVER         701 /* Argument is char* */

#define HOOK_AUTH_FLAGSUPDATED     801 /* Argument is void*[2] (authname*, u_int64_t*) */
#define HOOK_AUTH_LOSTAUTHNAME     802 /* Argument is authname* */

#define HOOK_TRUSTS_DB_CLOSED      900 /* No arg */
#define HOOK_TRUSTS_DB_LOADED      901 /* No arg */
#define HOOK_TRUSTS_NEWNICK        902 /* Argument is void*[2] (nick*, long) */
#define HOOK_TRUSTS_LOSTNICK       903 /* Argument is void*[2] (nick*, long) */
#define HOOK_TRUSTS_NEWGROUP       904 /* Argument is trustgroup* */
#define HOOK_TRUSTS_LOSTGROUP      905 /* Argument is trustgroup* */
#define HOOK_TRUSTS_ADDGROUP       906 /* Argument is trustgroup* */
#define HOOK_TRUSTS_DELGROUP       907 /* Argument is trustgroup* */
#define HOOK_TRUSTS_ADDHOST        908 /* Argument is trusthost* */
#define HOOK_TRUSTS_DELHOST        909 /* Argument is trusthost* */
#define HOOK_TRUSTS_MODIFYGROUP    910 /* Argument is trustgroup* */
#define HOOK_TRUSTS_LOSTHOST       911 /* Argument is trusthost* */
#define HOOK_TRUSTS_MODIFYHOST     912 /* Argument is trusthost* */

#define HOOK_SIGNONTRACKER_HAVETIME 1100 /* Argument is nick* */

#define HOOK_WHOWAS_NEWRECORD      1200 /* Argument is void*[2] (whowas *, nick *) */
#define HOOK_WHOWAS_LOSTRECORD     1201 /* Argument is whowas * */

#define PRIORITY_DEFAULT           0

#define PRIORITY_MAX               LONG_MIN
#define PRIORITY_MIN               LONG_MAX

typedef void (*HookCallback)(int, void *);

extern unsigned int hookqueuelength;

void inithooks();
int registerhook(int hooknum, HookCallback callback);
int deregisterhook(int hooknum, HookCallback callback);
void triggerhook(int hooknum, void *arg);
int registerpriorityhook(int hooknum, HookCallback callback, long priority);

#endif
