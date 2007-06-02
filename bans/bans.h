#ifndef _BANS_H
#define _BANS_H

#include "../lib/flags.h"
#include "../lib/sstring.h"
#include <time.h>

#define CHANBAN_NICKEXACT   0x0001  /* Ban includes an exact nick (no wildcards) */
#define CHANBAN_NICKMASK    0x0002  /* Ban includes a nick mask with wildcards */
#define CHANBAN_NICKANY     0x0004  /* Ban is *!.. */
#define CHANBAN_NICKNULL    0x0008  /* Ban has no nick */
#define CHANBAN_USEREXACT   0x0010  /* Ban includes an exact user (no wildcards) */
#define CHANBAN_USERMASK    0x0020  /* Ban includes a user mask with wildcards */
#define CHANBAN_USERANY     0x0040  /* Ban is ..!*@.. */
#define CHANBAN_USERNULL    0x0080  /* Ban has no user */
#define CHANBAN_HOSTEXACT   0x0100  /* Ban includes an exact host */
#define CHANBAN_HOSTMASK    0x0200  /* Ban includes a host mask */
#define CHANBAN_HOSTANY     0x0400  /* Ban is ..@* */
#define CHANBAN_HOSTNULL    0x0800  /* Ban has no host */
#define CHANBAN_IP          0x1000  /* Ban could match against IP address */
#define CHANBAN_CIDR        0x2000  /* Ban includes a CIDR mask (e.g. 192.168.0.0/16 ) */
#define CHANBAN_INVALID     0x4000  /* Ban is nonsensical, i.e. has at least one really NULL component*/
#define CHANBAN_HIDDENHOST  0x8000  /* Ban could possibly match hidden host */


typedef struct chanban {
  flag_t          flags;
  sstring        *nick;
  sstring        *user;
  sstring        *host;
  time_t          timeset;
  struct chanban *next;
} chanban;
            
chanban *getchanban();
void freechanban(chanban *cbp);

chanban *makeban(const char *mask);

int banoverlap(const chanban *bana, const chanban *banb);
int banequal(chanban *bana, chanban *banb);
char *bantostring(chanban *bp);
char *bantostringdebug(chanban *bp);


#endif
