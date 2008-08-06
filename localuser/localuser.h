/* localuser.h */

#ifndef __LOCALUSER_H
#define __LOCALUSER_H

#include "../nick/nick.h"
#include "../lib/flags.h"
#include "../irc/irc.h"

#define LU_PRIVMSG     0x0001 /* (sender, message) */
#define LU_PRIVNOTICE  0x0002 /* (sender, message) */
#define LU_SECUREMSG   0x0003 /* (sender, message) */
#define LU_CHANMSG     0x0004 /* (sender, target, message) */
#define LU_CHANNOTICE  0x0005 /* (sender, target, message) */
#define LU_INVITE      0x0006 /* (sender, channel) */
#define LU_KICKED      0x0007 /* (kicker, channel, reason) */
#define LU_KILLED      0x0010 /* () */
#define LU_STATS       0x0020 /* (server, numeric, data) */
#define LU_STATS_END   0x0021 /* server */

/* Callback function for privmsg etc. */
typedef void (*UserMessageHandler)(nick *,int,void **);

extern UserMessageHandler umhandlers[MAXLOCALUSER+1];

#define registerlocaluser(nickname, ident, host, realname, authname, umodes, handler) registerlocaluserflags(nickname, ident, host, realname, authname, 0, 0, umodes, handler)
nick *registerlocaluserflags(char *nickname, char *ident, char *host, char *realname, char *authname, unsigned long authid, flag_t accountflags, flag_t umodes, UserMessageHandler hander);
int renamelocaluser(nick *np, char *newnick);
int deregisterlocaluser(nick *np, char *reason);
UserMessageHandler hooklocaluserhandler(nick *np, UserMessageHandler newhandler);
void sendnickmsg(nick *np);
void sendnickburst(int hooknum, void *arg);
int handleprivatemsgcmd(void *source, int cargc, char **cargv);
int handleprivatenoticecmd(void *source, int cargc, char **cargv);
int handlemessageornotice(void *source, int cargc, char **cargv, int isnotice);
void sendmessagetouser(nick *source, nick *target, char *format, ... );
void sendsecuremessagetouser(nick *source, nick *target, char *servername, char *format, ... );
void sendnoticetouser(nick *source, nick *target, char *format, ... );
void killuser(nick *source, nick *target, char *format, ... );
#define localusersetaccount(np, accname) localusersetaccountflags(np, accname, 0)
void localusersetaccountflags(nick *np, char *accname, unsigned long accid, flag_t accountflags, time_t authTS);
void localusersetumodes(nick *np, flag_t newmodes);
void sethostuser(nick *target, char *ident, char *host);

#endif
