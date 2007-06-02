#ifndef __LOCALUSERCHANNEL_H
#define __LOCALUSERCHANNEL_H

#include "localuser.h"
#include "../channel/channel.h"
#include "../irc/irc_config.h"

#define MC_OP          0x0001
#define MC_VOICE       0x0002
#define MC_DEOP        0x0004
#define MC_DEVOICE     0x0008

#define MCB_ADD        0x0001
#define MCB_DEL        0x0002

typedef struct modechanges {
  channel      *cp;
  nick         *source;
  int           changecount;
  flag_t        addflags;
  flag_t        delflags;
  sstring      *key;
  unsigned int  limit;
  struct {
    sstring *str;
    short  dir;
    char   flag;
  } changes[MAXMODEARGS];
} modechanges;

/* These functions are in localuserchannel.c */
int localburstontochannel(channel *cp, nick *np, time_t timestamp, flag_t modes, unsigned int limit, char *key);
int localjoinchannel(nick *np, channel *cp);
int localpartchannel(nick *np, channel *cp, char *reason);
int localcreatechannel(nick *np, char *channame);
int localgetops(nick *np, channel *cp);
int localgetvoice(nick *np, channel *cp);
int localsetmodes(nick *np, channel *cp, nick *target, short modes);
void localsettopic(nick *np, channel *cp, char *topic);
void localkickuser(nick *np, channel *cp, nick *target, const char *message);
void localusermodechange(nick *np, channel *cp, char *modes);
void sendmessagetochannel(nick *source, channel *cp, char *format, ... );
void localinvite(nick *source, channel *cp, nick *target);

void localsetmodeinit (modechanges *changes, channel *cp, nick *np);
void localdosetmode_nick (modechanges *changes, nick *target, short modes);
void localdosetmode_ban (modechanges *changes, const char *ban, short dir);
void localdosetmode_key (modechanges *changes, const char *key, short dir);
void localdosetmode_limit (modechanges *changes, unsigned int limit, short dir);
void localdosetmode_simple (modechanges *changes, flag_t addmodes, flag_t delmodes);
void localsetmodeflush (modechanges *changes, int force);

#endif
