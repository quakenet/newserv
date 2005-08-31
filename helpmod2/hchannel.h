/* new H channel entries */
#ifndef HCHANNEL_H
#define HCHANNEL_H

#include <time.h>

#include "../channel/channel.h"

#include "hlamer.h"
#include "hcensor.h"
#include "hterm.h"
#include "htopic.h"
#include "hstat.h"
#include "hdef.h"
#include "hticket.h"

enum
{
    H_PASSIVE              = 1 << 0,
    H_WELCOME              = 1 << 1,
    H_JOINFLOOD_PROTECTION = 1 << 2,
    H_QUEUE                = 1 << 3,

    H_QUEUE_SPAMMY         = 1 << 4,
    H_QUEUE_MAINTAIN       = 1 << 5,
    H_REPORT               = 1 << 6,
    H_CENSOR               = 1 << 7,

    H_LAMER_CONTROL        = 1 << 8,
    H_ANTI_IDLE            = 1 << 9,
    H_MAINTAIN_M           = 1 << 10,
    H_MAINTAIN_I           = 1 << 11,

    H_HANDLE_TOPIC         = 1 << 12,
    H_DO_STATS             = 1 << 13,
    H_TROJAN_REMOVAL       = 1 << 14,
    H_CHANNEL_COMMANDS     = 1 << 15,

    H_OPER_ONLY            = 1 << 16,
    H_DISALLOW_LAME_TEXT   = 1 << 17,
    H_QUEUE_TIMEOUT        = 1 << 18,
    H_REQUIRE_TICKET       = 1 << 19,
    H_TICKET_MESSAGE       = 1 << 20,

    /* the following are not real channel flags, they're used only internally */
    H_UNUSED_1             = 1 << 28,
    H_UNUSED_2             = 1 << 29,
    H_JOIN_FLOOD           = 1 << 30,
    H_EVERYONEOUT          = 1 << 31
};

#define H_CHANFLAGS_DEFAULT (H_CHANNEL_COMMANDS)

#define HCHANNEL_CONF_COUNT 20

typedef struct hchannel_struct
{
    channel *real_channel;
    char welcome[400];
    int flags;

    int jf_control; /* join flood control */
    hcensor *censor; /* censor, keeps the bad words out */
    int autoqueue; /* H_QUEUE_MAINTAIN value */
    int max_idle; /* automatic idler removal */
    htopic *topic;
    hterm *channel_hterms; /* local terms */
    struct hchannel_struct *report_to; /* where to report this channels statistics */

    /* this is also the queue, so it's "sorted" */
    struct hchannel_user_struct *channel_users;

    time_t last_activity;
    time_t last_staff_activity;
    hstat_channel *stats;

    hlc_profile *lc_profile;

    struct hticket_struct *htickets;
    sstring *ticket_message;

    struct hchannel_struct *next;
} hchannel;

typedef struct hchannel_user_struct
{
    struct huser_struct *husr;
    time_t time_joined;
    struct hchannel_user_struct *next;
} hchannel_user;

extern hchannel *hchannels;

hchannel *hchannel_add(const char *);
int hchannel_del(hchannel *);
void hchannel_del_all(void);
hchannel *hchannel_get_by_channel(channel *);
hchannel *hchannel_get_by_name(const char *);
const char *hchannel_get_name(hchannel*);

hchannel_user *hchannel_on_channel(hchannel*, struct huser_struct *);
hchannel_user *hchannel_add_user(hchannel*, struct huser_struct *);
hchannel_user *hchannel_del_user(hchannel*, struct huser_struct *);

int hchannel_authority(hchannel*, struct huser_struct *);

/* returns the setting state for a channel, the int is the flag, not 1 << parameter */
const char *hchannel_get_state(hchannel*, int);
/* returns a setting name */
const char *hchannel_get_sname(int);
void hchannel_conf_change(hchannel *, int);
void hchannel_mode_check(hchannel*);

void hchannel_remove_inactive_users(void);
void hchannel_report(void);

void hchannel_set_topic(hchannel *);

/*int hchannel_on_queue(hchannel *, struct huser_struct *);*/
int hchannels_on_queue(struct huser_struct*);
int hchannels_on_desk(struct huser_struct*);

int hchannel_count_users(hchannel *, hlevel);
int hchannel_count_queue(hchannel *);

void hchannels_dnmo(struct huser_struct*);

int hchannel_is_valid(hchannel *);

/* somewhat of a hack, but channels have to be loaded before accounts */
void hchannels_match_accounts(void);

int hchannel_count(void);

void hchannel_activate_join_flood(hchannel*);
/* goes to schedule */
void hchannel_deactivate_join_flood();

#endif
