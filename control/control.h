
#ifndef __CONTROL_H
#define __CONTROL_H

#include "../parser/parser.h"
#include "../nick/nick.h"
#include "../channel/channel.h"

void registercontrolhelpcmd(const char *name, int level, int maxparams, CommandHandler handler, char *help);
int deregistercontrolcmd(const char *name, CommandHandler handler);
void controlmessage(nick *target, char *message, ... );
void controlchanmsg(channel *cp, char *message, ...);
void controlnotice(nick *target, char *message, ...);
int controlshowcommands(void *sender, int cargc, char **cargv);
int controlrmmod(void *sender, int cargc, char **cargv);
void controlspecialrmmod(void *arg);
void controlspecialreloadmod(void *arg);
void controlhelp(nick *np, Command *cmd);

#define registercontrolcmd(a, b, c, d) registercontrolhelpcmd(a, b, c, d, NULL)

typedef void (*ControlMsg)(nick *, char *, ... );
typedef void (*ControlWall)(flag_t, flag_t, char *, ...);

extern ControlMsg controlreply;
extern ControlWall controlwall;

extern nick *mynick;

extern CommandTree *controlcmds;

struct specialsched {
  sstring *modulename;
  void *schedule;
};

/* NEVER USE THE FOLLOWING IN COMMANDS, you'll end up missing bits off and users'll end up being able to gline people */
#define __NO_ANYONE      0x000
#define __NO_AUTHED      0x001 /* must be authed with the network, don't know what use this is really */
#define __NO_OPERED      0x002 /* just means that they MUST be /oper'ed, for hello -- part of LEGACY */
#define __NO_ACCOUNT     0x004 /* for hello, must have a user account */
#define __NO_LEGACY      0x008 /* reserved for old newserv commands, their level is 10 */
#define __NO_STAFF       0x010 /* +s */
#define __NO_TRUST       0x020 /* +t */
#define __NO_OPER        0x040 /* +O */
#define __NO_SEC         0x080 /* +w */
#define __NO_DEVELOPER   0x100 /* +d */

/* These are dangerous, they don't include requiring /OPER or STAFF status, be careful */
#define NOD_ACCOUNT   __NO_ACCOUNT | NO_AUTHED /* must contain authed else account won't be checked */
#define NOD_STAFF     __NO_STAFF | NOD_ACCOUNT
#define NOD_TRUST     __NO_TRUST | NOD_ACCOUNT
#define NOD_OPER      __NO_OPER | NOD_ACCOUNT
#define NOD_SEC       __NO_SEC | NOD_ACCOUNT
#define NOD_DEVELOPER __NO_DEVELOPER | NOD_ACCOUNT

/* These ones are safe to use */
#define NO_ANYONE       __NO_ANYONE                /* don't have to be authed to Q, or us, or opered or anything */
#define NO_AUTHED       __NO_AUTHED                /* must be authed to Q */
#define NO_ACCOUNT      NOD_ACCOUNT              /* must have an account on the bot */
#define NO_OPERED       __NO_OPERED                /* must be /opered */
#define NO_STAFF        NOD_STAFF                  /* must be authed to Q and have staff level on bot */
#define NO_OPER         NO_OPERED | NOD_OPER       /* must be authed to Q, /opered, and have oper level on bot */
#define NO_DEVELOPER    NO_OPERED | NOD_DEVELOPER  /* must be authed to Q, /opered, and have dev level on bot */
#define NO_TRUST_STAFF  NO_STAFF | NOD_TRUST       /* must be authed to Q, and have staff and trust level on bot */
#define NO_TRUST_OPER   NO_OPER | NOD_TRUST        /* must be authed to Q, /opered, and have trust and oper levels on bot */
#define NO_SEC_STAFF    NO_STAFF | NOD_SEC         /* must be authed to Q, and have staff and sec level on bot */
#define NO_SEC_OPER     NO_OPER | NOD_SEC          /* must be authed to Q, /opered, and have sec and oper levels on bot */

#define NO_ALL_FLAGS    __NO_STAFF | __NO_TRUST | __NO_OPER | __NO_SEC | __NO_DEVELOPER
#define NO_OPER_FLAGS   __NO_STAFF
#define NO_DEV_FLAGS    NO_ALL_FLAGS

#define NL_MANAGEMENT      0x0001  /* hello, password, userflags, noticeflags */
#define NL_TRUSTS          0x0002  /* trust stuff... */
#define NL_KICKKILLS       0x0004  /* KICK/KILL command */
#define NL_MISC            0x0008  /* misc commands (resync, etc) */
#define NL_GLINES          0x0010  /* GLINE commands */
#define NL_HITS            0x0020  /* Where a gline or kill is set automatically by the bot */
#define NL_CLONING         0x0040  /* Clone detection */
#define NL_CLEARCHAN       0x0080  /* When someone clearchans */
#define NL_FAKEUSERS       0x0100  /* Fakeuser addition */
#define NL_BROADCASTS      0x0200  /* Broadcast/mbroadcast/sbroadcast */
#define NL_OPERATIONS      0x0400  /* insmod/rmmod/etc */
#define NL_OPERING         0x0800  /* when someone opers */
#define NL_NOTICES         0x1000  /* turn off to receive privmsgs instead of notices */
#define NL_ALL_COMMANDS    0x2000  /* every single command sent */

#endif  
