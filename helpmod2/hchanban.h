#ifndef HCHANBAN_H
#define HCHANBAN_H

#include <time.h>
#include "hchannel.h"
#include "../lib/sstring.h"

typedef struct hchanban_struct
{
    hchannel *hchan;
    sstring *banmask;
    time_t expiration;

    struct hchanban_struct *next;
} hchanban;

extern hchanban *hchanbans;

hchanban *hchanban_add(hchannel*, const char*, time_t);
hchanban *hchanban_del(hchanban*);
hchanban *hchanban_del_all(void);

hchanban *hchanban_get(hchannel*, const char*);

void hchanban_schedule_entry(hchanban*);

#endif
