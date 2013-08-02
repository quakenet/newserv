#ifndef __SERVER_LIST_H
#define __SERVER_LIST_H

#include "../server/server.h"
#include "../lib/flags.h"

#define SERVERTYPEFLAG_CLIENT_SERVER    0x00000001 /* +c */
#define SERVERTYPEFLAG_HUB              0x00000002 /* +h */
#define SERVERTYPEFLAG_SERVICE          0x00000004 /* +s */
#define SERVERTYPEFLAG_CHANSERV         0x00000008 /* +Q */
#define SERVERTYPEFLAG_SPAMSCAN         0x00000010 /* +S */
#define SERVERTYPEFLAG_CRITICAL_SERVICE 0x00000020 /* +X */

#define SERVERTYPEFLAG_USER_STATE       SERVERTYPEFLAG_CLIENT_SERVER|SERVERTYPEFLAG_HUB|SERVERTYPEFLAG_CRITICAL_SERVICE

extern const flag servertypeflags[];
flag_t getservertype(server *server);

#endif
