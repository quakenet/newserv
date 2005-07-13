#include <time.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "hchannel.h"
#include "hstat.h"
#include "helpmod.h"
#include "hgen.h"

int hstat_is_prime_time(void)
{
    time_t timer = time(NULL);
    struct tm *tstruct = localtime(&timer);

    if (tstruct->tm_hour > 12 && tstruct->tm_hour < 23)
        return !0;
    return 0;
}

void hstat_scheduler(void)
{
    time_t timer = time(NULL);
    struct tm *tstruct = localtime(&timer);
    int is_sunday = (tstruct->tm_wday == 0); /* is it sunday ? */
    int i;

    /* Fix the hstat_cycle in case it's broken */
    if (is_sunday)
        hstat_cycle = hstat_cycle - (hstat_cycle % 7) + 6;

    {   /* accounts */
        haccount *ptr = haccounts;
	for (;ptr;ptr = ptr->next)
	    if (ptr->level > H_PEON)
	    {
		hstat_account *ptr2 = ptr->stats;
		for (;ptr2;ptr2 = ptr2->next)
		{
		    if (is_sunday)
		    {
			HSTAT_ACCOUNT_ZERO(ptr2->longterm[(hstat_week() + 1) % 10]);
			for (i=0;i<7;i++)
			{
			    HSTAT_ACCOUNT_SUM(ptr2->longterm[(hstat_week() + 1) % 10], ptr2->longterm[(hstat_week() + 1) % 10], ptr2->week[i]);
			}
		    }
		    HSTAT_ACCOUNT_ZERO(ptr2->week[(hstat_day() + 1) % 7]);
		    /*                        ptr2->longterm[(hstat_week() - 1) % 10] = ptr2->longterm[hstat_week()];
		     for (i=0;i<7;i++)
		     {
		     HSTAT_ACCOUNT_ZERO(ptr2->day[i]);
		     }*/
		}
	    }
    }
    {   /* hchannels */
        hchannel *ptr = hchannels;
        for (;ptr;ptr = ptr->next)
        {
	    hstat_channel *ptr2;
	    if (ptr->flags & H_REPORT && hchannel_is_valid(ptr->report_to))
            {
                hstat_channel_entry *entry = &ptr->stats->week[hstat_day()];
                helpmod_message_channel(ptr->report_to, "Daily summary for channel %s: Time spent %s, joins %d and queue usage %d", hchannel_get_name(ptr),helpmod_strtime(entry->time_spent), entry->joins, entry->queue_use);
            }

	    ptr2 = ptr->stats;
	    if (is_sunday)
	    {
                HSTAT_CHANNEL_ZERO(ptr2->longterm[(hstat_week() + 1) % 10]);
                for (i=0;i<7;i++)
                {
                    HSTAT_CHANNEL_SUM(ptr2->longterm[(hstat_week() + 1) % 10], ptr2->longterm[(hstat_week() + 1) % 10], ptr2->week[i]);
                }
/*                ptr2->longterm[(hstat_week() - 1) % 10] = ptr2->longterm[hstat_week()];
                for (i=0;i<7;i++)
		{
                    HSTAT_CHANNEL_ZERO(ptr2->day[i]);
                }*/
	    }
	    HSTAT_CHANNEL_ZERO(ptr2->week[(hstat_day() + 1) % 7]);
        }
    }

    hstat_cycle++;
}

int hstat_get_schedule_time(void)
{
    time_t timer = time(NULL);
    struct tm *tstruct = localtime(&timer);

    return timer + (HDEF_d - (HDEF_h * tstruct->tm_hour + HDEF_m * tstruct->tm_min + tstruct->tm_sec));
}

hstat_channel *get_hstat_channel(void)
{
    hstat_channel *tmp;

    tmp = malloc(sizeof(hstat_channel));

    memset(tmp, 0x00, sizeof(hstat_channel));

    return tmp;
}

void hstat_del_channel(hchannel *hchan)
{
    haccount *hacc = haccounts;
    hstat_account **hs_acc;
    for (;hacc;hacc = hacc->next)
        for (hs_acc = &hacc->stats;*hs_acc;hs_acc = &(*hs_acc)->next)
            if ((*hs_acc)->hchan == hchan)
            {
                hstat_account *tmp = (*hs_acc)->next;
                free(*hs_acc);
                *hs_acc = tmp;
                break;
            }
}

hstat_account *get_hstat_account(void)
{
    hstat_account *tmp;

    tmp = malloc(sizeof(hstat_account));

    memset(tmp, 0x00, sizeof(hstat_account));

    tmp->next = NULL; /* just for style */

    return tmp;
}

void hstat_calculate_general(hchannel *hchan, huser* husr, const char *message)
{
    hstat_account **acc_stat;
    hstat_account_entry *acc_entry;
    hstat_channel_entry *chan_entry;
    huser_channel *huserchan = huser_on_channel(husr, hchan);

    int wordc = 0, time_spent;

    if (huserchan == NULL || husr->account == NULL)
        return;

    for (acc_stat = &husr->account->stats;*acc_stat;acc_stat = &(*acc_stat)->next)
        if ((*acc_stat)->hchan == hchan)
            break;

    if (*acc_stat == NULL)
    { /* this user has no stats for the channel -> create them */
        *acc_stat = get_hstat_account();
        (*acc_stat)->hchan = hchan;
    }

    acc_entry = &(*acc_stat)->week[hstat_day()];
    chan_entry = &hchan->stats->week[hstat_day()];

    time_spent = time(NULL) - huserchan->last_activity;

    if (time_spent > 0 && time_spent < HSTAT_ACTIVITY_TRESHOLD)
    {
        acc_entry->time_spent+=time_spent;
        chan_entry->time_spent+=time_spent;

        if (hstat_is_prime_time())
        {
            acc_entry->prime_time_spent+=time_spent;
            chan_entry->prime_time_spent+=time_spent;
        }
    }

    acc_entry->lines++;
    chan_entry->lines++;

    wordc = hword_count(message);

    acc_entry->words+=wordc;
    chan_entry->words+=wordc;
}

void hstat_add_join(hchannel *hchan)
{
    hchan->stats->week[hstat_day()].joins++;
}

void hstat_add_queue(hchannel *hchan, int amount)
{
    hchan->stats->week[hstat_day()].queue_use+=amount;
}

const char *hstat_channel_print(hstat_channel_entry *entry, int type)
{
    static char buffer[256];
    buffer[0] = '\0';
    switch (type)
    {
    case HSTAT_CHANNEL_LONG:
        sprintf(buffer, "%-18s %-18s %-10d %-10d %-10d %-10d", helpmod_strtime(entry->time_spent), helpmod_strtime(entry->prime_time_spent), entry->joins, entry->queue_use, entry->lines, entry->words);
        break;
    case HSTAT_CHANNEL_SHORT:
        sprintf(buffer, "%-18s %-18s", helpmod_strtime(entry->time_spent), helpmod_strtime(entry->prime_time_spent));
        break;
    }
    return buffer;
}

const char *hstat_account_print(hstat_account_entry *entry, int type)
{
    static char buffer[256];
    buffer[0] = '\0';
    switch (type)
    {
    case HSTAT_ACCOUNT_LONG:
        sprintf(buffer, "%-18s %-18s %-10d %-10d", helpmod_strtime(entry->time_spent), helpmod_strtime(entry->prime_time_spent), entry->lines, entry->words);
        break;
    case HSTAT_ACCOUNT_SHORT:
        sprintf(buffer, "%-18s %-18s", helpmod_strtime(entry->time_spent), helpmod_strtime(entry->prime_time_spent));
        break;
    }
    return buffer;
}

const char *hstat_header(hstat_type type)
{
    switch (type)
    {
    case HSTAT_ACCOUNT_SHORT:
        return "TimeSpent          PrimeTimeSpent";
    case HSTAT_ACCOUNT_LONG:
        return "TimeSpent          PrimeTimeSpent     Lines      Words";
    case HSTAT_CHANNEL_SHORT:
        return "TimeSpent          PrimeTimeSpent";
    case HSTAT_CHANNEL_LONG:
        return "TimeSpent          PrimeTimeSpent     Joins      QueueUse   Lines      Words";
    default:
        return "Error: please contact strutsi";
    }
}

int hstat_week(void)
{
    return (hstat_cycle / 7) % 10;
}

int hstat_day(void)
{
    time_t timer = time(NULL);
    struct tm *tstruct = localtime(&timer);

    return tstruct->tm_wday;
}

int hstat_account_count(hstat_account *hs_acc)
{
    int count = 0;
    for (;hs_acc;hs_acc = hs_acc->next)
        count++;
    return count;
}

hstat_account_entry_sum hstat_account_last_week(hstat_account *hs_acc)
{
    hstat_account_entry_sum tmp = {0,0,0,0};
    int i;
    for (i=0;i<7;i++)
    {
        HSTAT_ACCOUNT_SUM(tmp, tmp, hs_acc->week[i]);
    }
    return tmp;
}

hstat_account_entry_sum hstat_account_last_month(hstat_account *hs_acc)
{
    hstat_account_entry_sum tmp = {0,0,0,0};
    int i;

    //HSTAT_ACCOUNT_SUM(tmp, tmp, hstat_account_last_week(hs_acc));
    for (i=0;i<4;i++)
    {
        HSTAT_ACCOUNT_SUM(tmp, tmp, hs_acc->longterm[(hstat_week() + i) % 10]);
    }
    return tmp;
}

hstat_channel_entry hstat_channel_last_week(hstat_channel *hs_chan)
{
    hstat_channel_entry tmp = {0,0,0,0,0,0};
    int i;
    for (i=0;i<7;i++)
    {
        HSTAT_CHANNEL_SUM(tmp, tmp, hs_chan->week[i]);
    }
    return tmp;
}

hstat_channel_entry hstat_channel_last_month(hstat_channel *hs_chan)
{
    hstat_channel_entry tmp = {0,0,0,0,0,0};
    int i;

    //HSTAT_CHANNEL_SUM(tmp, tmp, hstat_channel_last_week(hs_chan));
    for (i=0;i<4;i++)
    {
        HSTAT_CHANNEL_SUM(tmp, tmp, hs_chan->longterm[(hstat_week() + i) % 10]);
    }
    return tmp;
}

static int hstat_account_compare(hstat_account_entry_sum *e1, hstat_account_entry_sum *e2)
{
    return e2->prime_time_spent - e1->prime_time_spent;
}

hstat_accounts_array create_hstat_account_array(hchannel *hchan, hlevel lvl)
{
    hstat_accounts_array arr = {NULL, 0};
    hstat_account *ptr;
    hstat_account_entry_sum tmp1, tmp2;
    haccount *hacc = haccounts;
    int initial_arrlen = 0;

    if (lvl == H_OPER)
        initial_arrlen = haccount_count(H_OPER) + haccount_count(H_ADMIN);
    else if (lvl == H_STAFF)
        initial_arrlen = haccount_count(H_STAFF) + haccount_count(H_TRIAL);
    else
        initial_arrlen = haccount_count(lvl);

    if (!initial_arrlen)
        return arr;

    arr.array = (hstat_account_entry_sum*)malloc(sizeof(hstat_account_entry_sum) * initial_arrlen);
    for (;hacc;hacc = hacc->next)
        if ((lvl == H_OPER && (hacc->level == H_OPER || hacc->level == H_ADMIN)) ||
            (lvl == H_STAFF && (hacc->level == H_TRIAL || hacc->level == H_STAFF)))
	    for (ptr = hacc->stats;ptr;ptr = ptr->next)
		if (ptr->hchan == hchan)
		{
		    assert(arr.arrlen < initial_arrlen);
		    tmp1 = hstat_account_last_month(ptr);
		    tmp2 = hstat_account_last_week(ptr);
		    HSTAT_ACCOUNT_SUM(arr.array[arr.arrlen], tmp1, tmp2);
		    arr.array[arr.arrlen].owner = hacc;
		    arr.arrlen++;
		}

    qsort(arr.array, arr.arrlen, sizeof(hstat_account_entry_sum), (int(*)(const void*, const void*))hstat_account_compare);
    return arr;
}
