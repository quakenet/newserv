#ifndef HSTAT_H
#define HSTAT_H

#include "hdef.h"

extern int hstat_cycle;
extern time_t hstat_last_cycle;

/* forward declarations */
struct haccount_struct;
struct hchannel_struct;
struct huser_struct;

#define HSTAT_ACTIVITY_TRESHOLD (3 * HDEF_m)

#define HSTAT_ACCOUNT_SUM(sum, op1, op2)\
{\
    (sum).time_spent = (op1).time_spent + (op2).time_spent;\
    (sum).prime_time_spent = (op1).prime_time_spent + (op2).prime_time_spent;\
    (sum).lines = (op1).lines + (op2).lines;\
    (sum).words = (op1).words + (op2).words;\
}

#define HSTAT_ACCOUNT_ZERO(target)\
{\
    (target).time_spent = 0;\
    (target).prime_time_spent = 0;\
    (target).lines = 0;\
    (target).words = 0;\
}

#define HSTAT_CHANNEL_SUM(sum, op1, op2)\
{\
    (sum).time_spent = (op1).time_spent + (op2).time_spent;\
    (sum).prime_time_spent = (op1).prime_time_spent + (op2).prime_time_spent;\
    (sum).lines = (op1).lines + (op2).lines;\
    (sum).words = (op1).words + (op2).words;\
    (sum).joins = (op1).joins + (op2).joins;\
    (sum).queue_use = (op1).queue_use + (op2).queue_use;\
}

#define HSTAT_CHANNEL_ZERO(target)\
{\
    (target).time_spent = 0;\
    (target).prime_time_spent = 0;\
    (target).lines = 0;\
    (target).words = 0;\
    (target).joins = 0;\
    (target).queue_use = 0;\
}

typedef enum
{
    HSTAT_ACCOUNT_SHORT,
    HSTAT_ACCOUNT_LONG,
    HSTAT_CHANNEL_SHORT,
    HSTAT_CHANNEL_LONG
} hstat_type;

typedef struct hstat_account_entry_struct
{
    int time_spent;
    int prime_time_spent;
    int lines;
    int words;
} hstat_account_entry;

typedef struct hstat_account_entry_sum_struct
{
    int time_spent;
    int prime_time_spent;
    int lines;
    int words;
    struct haccount_struct *owner; /* haccount* */
} hstat_account_entry_sum;

typedef struct hstat_channel_entry_struct
{
    int time_spent;
    int prime_time_spent;

    int joins;
    int queue_use;

    int lines;
    int words;
} hstat_channel_entry;

typedef struct hstat_channel_struct
{
    hstat_channel_entry week[7]; /* 7 days */
    hstat_channel_entry longterm[10]; /* 10 weeks */

} hstat_channel;

typedef struct hstats_account_struct
{
    struct hchannel_struct *hchan;

    hstat_account_entry week[7]; /* 7 days */
    hstat_account_entry longterm[10]; /* 10 weeks */

    struct hstats_account_struct *next;
} hstat_account;

typedef struct hstat_accounts_array_struct
{
    hstat_account_entry_sum *array;
    int arrlen;
} hstat_accounts_array;

/* free() this.entries, arguments: channel, level to list */
hstat_accounts_array create_hstat_account_array(struct hchannel_struct*, hlevel);

hstat_channel *get_hstat_channel(void);
hstat_account *get_hstat_account(void);

const char *hstat_header(hstat_type);

const char *hstat_channel_print(hstat_channel_entry*, int);

const char *hstat_account_print(hstat_account_entry*, int);

void hstat_calculate_general(struct hchannel_struct*, struct huser_struct*, const char *);

void hstat_add_join(struct hchannel_struct*);
void hstat_del_channel(struct hchannel_struct*);

void hstat_add_queue(struct hchannel_struct*, int);

int is_prime_time(void); /* tells if now is the "main" time in #feds */

int hstat_week(void);
int hstat_day(void);

hstat_account_entry_sum hstat_account_last_week(hstat_account*);
hstat_account_entry_sum hstat_account_last_month(hstat_account*);
hstat_channel_entry hstat_channel_last_week(hstat_channel*);
hstat_channel_entry hstat_channel_last_month(hstat_channel*);

int hstat_account_count(hstat_account*);

/* handles the cycle and today -> week -> longterm changes */
void hstat_scheduler(void);
int hstat_get_schedule_time(void);

#endif
