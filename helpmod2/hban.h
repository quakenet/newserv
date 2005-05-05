#ifndef HBAN_H
#define HBAN_H

#include <time.h>

#include "../channel/channel.h"

#include "../nick/nick.h"

#include "../lib/sstring.h"

typedef struct hban_struct
{
    chanban *real_ban;
    sstring *reason;
    time_t expiration;

    struct hban_struct *next;
} hban;

enum
{
    HBAN_NICK      = 1 << 0,
    HBAN_IDENT     = 1 << 1,
    HBAN_HOST      = 1 << 2,
    HBAN_REAL_HOST = 1 << 3
};

extern hban* hbans;

hban *hban_add(const char*, const char*, time_t, int);
hban *hban_del(hban*, int);
hban *hban_get(const char*);

int hban_count(void);
/* the first parameter is huser* */
hban *hban_huser(void*, const char*, time_t, int);

hban *hban_check(nick*);

void hban_remove_expired(void);

void hban_del_all(void);

const char *hban_get_reason(hban*);

const char *hban_ban_string(nick*, int);

#endif
