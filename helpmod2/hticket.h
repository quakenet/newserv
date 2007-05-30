/* hchannel invite ticket */
/* currently uses a list, if it's insufficient a faster datastructure will be used */
#ifndef HTICKET_H
#define HTICKET_H

#include <time.h>

#include "../irc/irc_config.h"

#include "hdef.h"

#define HTICKET_EXPIRATION_TIME (2 * HDEF_d)

typedef struct hticket_struct
{
    char authname[ACCOUNTLEN +1];
    time_t time_expiration;
    sstring *message;
    struct hticket_struct *next;
} hticket;

hticket *hticket_get(const char *, struct hchannel_struct*);
hticket *hticket_del(hticket *, struct hchannel_struct*);
hticket *hticket_add(const char *, time_t expiration, struct hchannel_struct*, const char *);
int hticket_count(void);
void hticket_remove_expired(void);

#endif
