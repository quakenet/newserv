/* new H user, a little more than N's user */
#ifndef HUSER_H
#define HUSER_H

#include <time.h>

#include "hchannel.h"
#include "haccount.h"

#include "hed.h"
#include "helpmod_entries.h"

#define H_USRFLAGS_DEFAULT 0

typedef struct huser_struct
{
    /* the real N user, channel information etc. can be gotten from this */
    nick *real_user;
    int flags;
    haccount *account;

    helpmod_entry state; /* for old H */

    time_t last_activity;

    /* for the lamer control, global -> can detect lame amsg's */
    char last_line[512];
    int last_line_repeats;

    double spam_val;
    int flood_val;

    int lc[5];
    /* end lamer control */

    helpmod_editor *editor;

    struct huser_channel_struct *hchannels;

    struct huser_struct *next;
} huser;

enum
{
    HCUMODE_OP =     1 << 0,
    HCUMODE_VOICE =  1 << 1,
    HQUEUE_DONE =    1 << 2,
    H_IDLE_WARNING = 1 << 3
};

typedef struct huser_channel_struct
{
    int flags;
    struct hchannel_struct *hchan;

    time_t last_activity;
    /* for queue, tells us who was the oper for this user */
    huser *responsible_oper;

    struct huser_channel_struct *next;
} huser_channel;


extern huser *husers;

huser *huser_add(nick*);
void huser_del(huser*);
void huser_del_all();
huser *huser_get(nick*);

/* indicates that the user did something, channel is optional */
void huser_activity(huser*, hchannel *);

void huser_clear_inactives(void);
int huser_count(void);
void huser_reset_states(void);

hlevel huser_get_level(huser*);
int huser_get_account_flags(huser*);

const char *huser_get_nick(huser *);
const char *huser_get_ident(huser *);
const char *huser_get_host(huser *);
const char *huser_get_auth(huser *);
const char *huser_get_realname(huser *);

/* defines the "being on queue" and being on desk (getting service) */
int on_queue(huser*, huser_channel*);
int on_desk(huser*, huser_channel*);

/* trojan removal */
int huser_is_trojan(huser*);

huser_channel *huser_add_channel(huser*, struct hchannel_struct*);
void huser_del_channel(huser*, struct hchannel_struct*);
/* returns 0 if the user is not on the channel, non-zero otherwise */
huser_channel *huser_on_channel(huser*, struct hchannel_struct*);

/* returns non-zero if the given huser pointer is valid */
int huser_valid(huser*);

#endif
