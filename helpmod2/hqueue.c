#include <assert.h>
#include <string.h>

#include "hqueue.h"
#include "hgen.h"
#include "hstat.h"
#include "huser.h"

#include "hcommands.h"

typedef struct hqueue_entry
{
    huser_channel *huserchan;
    hchannel_user *hchanuser;
} hqueue_entry;

static hqueue_entry *hqueue_get_next(hchannel *hchan)
{
    static struct hqueue_entry qentry;
    static hchannel *current_hchan = NULL;
    static hchannel_user *hchanuser = NULL;
    huser_channel *huserchan;

    if (hchan != NULL)
    {
        hchanuser = hchan->channel_users;
        current_hchan = hchan;
    }

    if (hchanuser == NULL)
        return NULL;

    huserchan = huser_on_channel(hchanuser->husr, current_hchan);

    assert(huserchan != NULL);

    qentry.huserchan = huserchan;
    qentry.hchanuser = hchanuser;

    hchanuser = hchanuser->next;

    return &qentry;
}

static int hqueue_on_queue(hqueue_entry *qentry)
{
    return on_queue(qentry->hchanuser->husr, qentry->huserchan);
}

static int hqueue_count_in_queue(hchannel *hchan)
{
    int count;
    hqueue_entry *tmp = hqueue_get_next(hchan);

    for (count = 0;tmp;tmp = hqueue_get_next(NULL))
        if (hqueue_on_queue(tmp))
            count++;

    return count;
}

static int hqueue_count_off_queue(hchannel *hchan)
{
    int count;
    hqueue_entry *tmp = hqueue_get_next(hchan);

    for (count = 0;tmp;tmp = hqueue_get_next(NULL))
        if (on_desk(tmp->hchanuser->husr, tmp->huserchan))
        count++;

    return count;
}

int hqueue_average_time(hchannel *hchan)
{
    int count, sum = 0;
    hqueue_entry *tmp = hqueue_get_next(hchan);

    for (count = 0;tmp;tmp = hqueue_get_next(NULL))
        if (hqueue_on_queue(tmp))
        {
            count++;
            sum+=(time(NULL) - tmp->hchanuser->time_joined);
        }
    if (count)
        return sum / count;
    else
        return 0;
}

void hqueue_report_positions(hchannel *hchan)
{
    int count;
    hqueue_entry *tmp = hqueue_get_next(hchan);

    for (count = 1;tmp;tmp = hqueue_get_next(NULL))
    {
        if (hqueue_on_queue(tmp) && ((count <= 5) || !(count % 5)))
            helpmod_reply(tmp->hchanuser->husr, hchan->real_channel, "You now have queue position #%d for channel %s", count, hchannel_get_name(hchan));
        if (hqueue_on_queue(tmp))
            count++;
    }
}

void hqueue_advance(hchannel *hchan, huser *oper, int nadv)
{
    char buffer[512];
    int counter = nadv;
    huser_channel *huserchan;

    if (hchan == NULL) return;

    hchannel_user *hchanuser = hchan->channel_users;

    buffer[0] = '\0';

    if (counter < 0 || counter > 25)
        return;

    if (!huser_valid(oper) || !huser_on_channel(oper, hchan) || (time(NULL) - huser_on_channel(oper,hchan)->last_activity > HELPMOD_QUEUE_TIMEOUT))
        oper = NULL;

    while (counter)
    {
        if (hchanuser == NULL) /* out of users */
        {
            if (counter == nadv)
            {
                helpmod_message_channel(hchan, "There are no users in the queue");
                return;
            }
            else
                break; /* we got something, print the message on channel */
        }
        huserchan = huser_on_channel(hchanuser->husr, hchan);

        assert(huserchan != NULL);

        if (huserchan->flags & HQUEUE_DONE || huser_get_level(hchanuser->husr) > H_PEON)
        {
            hchanuser = hchanuser->next; /* user was not in queue, proceed to next one */
            continue;
        }

        huserchan->flags |= HQUEUE_DONE;

        counter--;

        strcat(buffer, huser_get_nick(hchanuser->husr));
        strcat(buffer, " ");

        if (hchan->flags & H_DO_STATS)
            hstat_add_queue(hchan, 1);

        if ((hchan->flags & H_QUEUE_SPAMMY) && (oper != NULL))
        {
	    helpmod_reply(hchanuser->husr, hchan->real_channel, "It is now your time to state your problem. Please do so on channel %s and direct your questions to %s %s",  hchannel_get_name(hchan), hlevel_title(huser_get_level(oper)), huser_get_nick(oper));
            if (!(huser_get_account_flags(oper) & H_NOSPAM))
                helpmod_reply(oper, hchan->real_channel, "User %s (%s@%s) is yours, he has been in queue for %s", huser_get_nick(hchanuser->husr), huser_get_ident(hchanuser->husr), huser_get_host(hchanuser->husr), helpmod_strtime(time(NULL) - hchanuser->time_joined));
        }

        helpmod_channick_modes(hchanuser->husr, hchan, MC_VOICE, HLAZY);
        huserchan->responsible_oper = oper;

        hchanuser = hchanuser->next;
    }
    if (oper != NULL)
        helpmod_message_channel(hchan, "user%s %s: Please state your questions on this channel and direct them to %s %s", (nadv - counter == 1)?"":"s", buffer, hlevel_title(huser_get_level(oper)), huser_get_nick(oper));
    else
        helpmod_message_channel(hchan, "user%s %s: Please state your questions on this channel", (nadv - counter == 1)?"":"s", buffer);

    if (hchan->flags & H_QUEUE_SPAMMY)
        hqueue_report_positions(hchan);
}


void helpmod_queue_handler (huser *sender, channel* returntype, hchannel *hchan, int hqueue_action, char* ostr, int argc, char *argv[])
{
    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Can not handle queue: Channel not defined or not found");
        return;
    }
    switch (hqueue_action) /* easy ones */
    {
    case HQ_ON:
        if (hchan->flags & H_QUEUE)
            helpmod_reply(sender, returntype, "Can not activate queue: Queue is already active on channel %s", hchannel_get_name(hchan));
        else
        {
            hchan->flags |= H_QUEUE;
            helpmod_reply(sender, returntype, "Queue activated for channel %s", hchannel_get_name(hchan));
	    hchannel_conf_change(hchan, hchan->flags & ~(H_QUEUE));
	    hchan->autoqueue = 0;
	}
        return;
    case HQ_OFF:
        if (!(hchan->flags & H_QUEUE))
            helpmod_reply(sender, returntype, "Can not deactive queue: Queue is not active on %s", hchannel_get_name(hchan));
        else
        {
            hchan->flags &= ~H_QUEUE;
            helpmod_reply(sender, returntype, "Queue deactivated for channel %s", hchannel_get_name(hchan));
	    hchannel_conf_change(hchan, hchan->flags | H_QUEUE);
	    hchan->autoqueue = 0;

	    {   /* devoice all users of level H_PEON */
		hchannel_user *hchanuser;
                huser_channel *huserchan;
		for (hchanuser = hchan->channel_users;hchanuser != NULL;hchanuser = hchanuser->next)
		{
		    huserchan = huser_on_channel(hchanuser->husr, hchan);
		    if (huser_get_level(hchanuser->husr) == H_PEON && huserchan->flags & HCUMODE_VOICE)
			helpmod_channick_modes(hchanuser->husr, hchan, MC_DEVOICE, HLAZY);
		}
	    }

	}
        return;
    }
    if (!(hchan->flags & H_QUEUE))
    {
        helpmod_reply(sender, returntype, "Can not handle queue: Queue not active on channel %s", hchannel_get_name(hchan));
        return;
    }
    /* now to the real deal */
    switch (hqueue_action)
    {
    case HQ_DONE:
        {
            int i;
            if (argc == 0)
            {
                helpmod_reply(sender, returntype, "Can not advance queue: User not specified");
                return;
            }
            if (argc > H_CMD_MAX_ARGS)
                argc = H_CMD_MAX_ARGS;

            for (i=0;i<argc;i++)
            {
                huser *husr = huser_get(getnickbynick(argv[i]));
                if (husr == NULL)
                {
                    helpmod_reply(sender, returntype, "Can not advance queue: User %s not found", argv[i]);
                    continue;
                }
                helpmod_channick_modes(husr, hchan, MC_DEVOICE, HLAZY);
            }

            hqueue_handle_queue(hchan, sender);
        }
        return;
    case HQ_NEXT:
        {
            int nnext = 1;
            if (argc > 0)
            {
                if (!sscanf(argv[0], "%d", &nnext) || nnext <= 0 || nnext > 25 /* magic number */)
                {
                    helpmod_reply(sender, returntype, "Can not advance queue: Integer [1, 25] expected");
                    return;
                }
            }
            hqueue_advance(hchan, sender, nnext);
        }
        return;
    case HQ_MAINTAIN:
        {
            int tmp;
            if (argc == 0)
            {
                helpmod_reply(sender, returntype, "Autoqueue for channel %s is currently %d (%s)", hchannel_get_name(hchan), hchan->autoqueue, hchannel_get_state(hchan, H_QUEUE_MAINTAIN));
                return;
            }
            if (!sscanf(argv[0], "%d", &tmp) || tmp < 0 || tmp > 25)
            {
                helpmod_reply(sender, returntype, "Can not set auto queue: Integer [0, 20] expected");
                return;
            }
            hchan->autoqueue = tmp;
            if (tmp == 0)
            {
                if (hchan->flags & H_QUEUE_MAINTAIN)
                {
                    hchan->flags &= ~(H_QUEUE_MAINTAIN);
                    helpmod_reply(sender, returntype, "Autoqueue is now disabled for channel %s", hchannel_get_name(hchan));
                }
                else
                    helpmod_reply(sender, returntype, "Autoqueue is not enabled for channel %s", hchannel_get_name(hchan));
            }
            else if (!(hchan->flags & H_QUEUE_MAINTAIN))
            {
                hchan->flags |= H_QUEUE_MAINTAIN;
                helpmod_reply(sender, returntype, "Autoqueue for channel %s activated and set to %d succesfully", hchannel_get_name(hchan), hchan->autoqueue);
            }
            else
                helpmod_reply(sender, returntype, "Autoqueue for channel %s set to %d succesfully", hchannel_get_name(hchan), hchan->autoqueue);
            hqueue_handle_queue(hchan, sender);
        }
        return;
    case HQ_LIST:
        {
            int count = hqueue_count_in_queue(hchan);
            hqueue_entry *hqueue = hqueue_get_next(hchan);

            char buffer[512];
            buffer[0] = '\0';

            helpmod_reply(sender, returntype, "Channel %s has following users in queue (%d user%s total):", hchannel_get_name(hchan), count, (count==1)?"":"s");

            for (;hqueue;hqueue = hqueue_get_next(NULL))
            {
                if (strlen(buffer) >= 250)
                {
                    helpmod_reply(sender, returntype, "%s", buffer);
                    buffer[0] = '\0';
                }
                if (hqueue_on_queue(hqueue))
                    sprintf(buffer+strlen(buffer) /* :) */, "%s (%s@%s) [%s] ", huser_get_nick(hqueue->hchanuser->husr), huser_get_ident(hqueue->hchanuser->husr), huser_get_host(hqueue->hchanuser->husr), helpmod_strtime(time(NULL) - hqueue->hchanuser->time_joined));
            }
            if  (buffer[0])
                helpmod_reply(sender, returntype, "%s", buffer);
        }
        return;
    case HQ_NONE: /* if no parameters are given print the summary */
    case HQ_SUMMARY:
        {
            int count_on = hqueue_count_in_queue(hchan);
            int count_off = hqueue_count_off_queue(hchan);
            int average = hqueue_average_time(hchan);

            helpmod_reply(sender, returntype, "Channel %s has %d user%s in queue, %d user%s being helped, average time in queue %s", hchannel_get_name(hchan), count_on, (count_on==1)?"":"s", count_off, (count_off==1)?"":"s", helpmod_strtime(average));
        }
        return;
    case HQ_RESET:
        {
            hchannel_user *hchanuser = hchan->channel_users;
            for (;hchanuser;hchanuser = hchanuser->next)
            {
                if (huser_get_level(hchanuser->husr) == H_PEON)
                {
                    huser_channel *huserchan = huser_on_channel(hchanuser->husr, hchan);
                    assert (huserchan != NULL);
                    if (huserchan->flags & HCUMODE_VOICE)
                        helpmod_channick_modes(hchanuser->husr, hchan, MC_DEVOICE, HLAZY);
                    huserchan->flags &= ~(HQUEUE_DONE | H_IDLE_WARNING);
                }
            }
            if (!IsModerated(hchan->real_channel))
                helpmod_simple_modes(hchan, CHANMODE_MODERATE, 0, 0);
            helpmod_message_channel(hchan, "Channel queue has been reset");
            helpmod_reply(sender, returntype, "Queue for channel %s has been reset", hchannel_get_name(hchan));
        }
        return;
    }
}

void hqueue_handle_queue(hchannel *hchan, huser *huser)
{
    if ((hchan->flags & (H_QUEUE | H_QUEUE_MAINTAIN)) && hchan->autoqueue && hqueue_count_in_queue(hchan))
    {
        int dif = hchan->autoqueue - hqueue_count_off_queue(hchan);
        if (dif > 0)
            hqueue_advance(hchan, huser, dif);
    }
    hcommit_modes();
}

int hqueue_get_position(hchannel *hchan, huser *target)
{
    int position;
    hqueue_entry *hqueue = hqueue_get_next(hchan);

    if (hchannel_on_channel(hchan, target) == NULL)
        return -1;

    if (!(hchan->flags & H_QUEUE))
        return -1;

    for (position = 0;hqueue;hqueue = hqueue_get_next(NULL))
    {
        if (hqueue_on_queue(hqueue))
        {
            position++;
            if (hqueue->hchanuser->husr == target)
                return position;
        }
    }
    return -1;
}
