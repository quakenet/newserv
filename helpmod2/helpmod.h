/* some extern definitions */
#ifndef HELPMOD_H
#define HELPMOD_H

#include <time.h>

#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "huser.h"
#include "hversions.h"

/* configuration */

/* These should always be equal */
#define HELPMOD_VERSION_INTERNAL HELPMOD_VERSION_2_14
#define HELPMOD_VERSION "2.14"

#define HELPMOD_USER_TIMEOUT 1200

#define HELPMOD_DEFAULT_DB "./helpmod2/helpmod.db"
#define HELPMOD_FALLBACK_DB "./helpmod2/helpmod.db"

#define HELPMOD_HELP_DEFAULT_DB "./helpmod2/help.db"

#define HELPMOD_NICK "G"
#define HELPMOD_AUTH "G"

#define HELPMOD_QUEUE_TIMEOUT (7 * HDEF_m)

#define HELPMOD_BAN_DURATION (2 * HDEF_h)

extern int HELPMOD_ACCOUNT_EXPIRATION[6];

extern nick *helpmodnick;

extern modechanges hmodechanges;

extern time_t helpmod_startup_time;

void hcommit_modes(void);

void helpmod_reply(huser *target, channel* returntype, const char *message, ... );

void helpmod_message_channel_long(hchannel *hchan, const char *message, ...);
void helpmod_message_channel(hchannel *hchan, const char *message, ...);

void helpmod_kick(hchannel *hchan, huser *target, const char *reason, ...);

void helpmod_invite(hchannel *, huser *);

/* the last argument is a commit-now truth value */
void helpmod_channick_modes(huser *target, hchannel *hchan, short, int);
void helpmod_simple_modes(hchannel *hchan, int, int, int);
void helpmod_setban(hchannel *hchan, const char*, time_t, int, int);

void helpmod_set_topic(hchannel *hchan, const char* topic);

extern char helpmod_db[128];

extern long helpmod_usage;

#endif
