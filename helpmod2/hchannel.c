
#include <stdlib.h>
#include <assert.h>

#include "../lib/sstring.h"
#include "../lib/irc_string.h"
#include "../channel/channel.h"
#include "../localuser/localuserchannel.h"
#include "../core/schedule.h"

#include "hchannel.h"
#include "haccount.h"
#include "helpmod.h"
#include "hqueue.h"
#include "hgen.h"
#include "hchanban.h"
#include "hban.h"

hchannel *hchannel_add(const char *cname)
{
    channel *cp;
    hchannel *hchan;

    cp = findchannel((char*)cname);
    if (!cp)
    {
        localcreatechannel(helpmodnick, (char*)cname);
        cp = findchannel((char*)cname);
    }
    else
    {
        localjoinchannel(helpmodnick, cp);
        localgetops(helpmodnick, cp);
    }

    hchan = (hchannel*)malloc(sizeof(hchannel));

    hchan->welcome[0] = '\0';
    hchan->real_channel = cp;
    hchan->flags = H_CHANFLAGS_DEFAULT;
    hchan->channel_users = NULL;
    hchan->channel_hterms = NULL;
    hchan->max_idle = 5 * HDEF_m;
    hchan->topic = NULL;
    hchan->report_to = NULL;

    hchan->autoqueue = 0;
    hchan->jf_control = time(NULL);
    hchan->lc_profile = NULL;
    hchan->censor = NULL;

    hchan->htickets = NULL;

    hchan->stats = get_hstat_channel();

    hchan->next = hchannels;
    hchannels = hchan;

    {
        int i;
        nick *nck;
        huser *husr;
        huser_channel *tmp;
        for (i=0;i < hchan->real_channel->users->hashsize;i++)
        {
            nck = getnickbynumeric(hchan->real_channel->users->content[i]);
            if (!nck) /* it's a hash, not an array */
                continue;

            if ((husr = huser_get(nck)) == NULL)
                husr = huser_add(nck);
/*
            fprintf(hdebug_file, "%d ADD (hchannel_add) %s to %s\n", time(NULL), husr->real_user->nick, hchannel_get_name(hchan));
            fflush(hdebug_file);
*/
            tmp = huser_add_channel(husr, hchan);
            hchannel_add_user(hchan, husr);

            if (hchan->real_channel->users->content[i] & CUMODE_OP)
                tmp->flags |= HCUMODE_OP;
            if (hchan->real_channel->users->content[i] & CUMODE_VOICE)
                tmp->flags |= HCUMODE_VOICE;
        }
    }
    return hchan;
}

int hchannel_del(hchannel *hchan)
{
    hchannel *tmp, **ptr = &hchannels;

    for (;*ptr;ptr = &(*ptr)->next)
        if (*ptr == hchan)
            break;

    assert(*ptr != NULL);

    tmp = hchan->next;

    hcensor_del_all(&(hchan->censor));
    hterm_del_all(&hchan->channel_hterms);
    htopic_del_all(&hchan->topic);
    hstat_del_channel(hchan);
    free(hchan->stats);

    while (hchan->htickets)
        hticket_del(hchan->htickets, hchan);

    localpartchannel(helpmodnick, hchan->real_channel);

    free(hchan);

    *ptr = tmp;

    return 0;
}

int hchannel_authority(hchannel *hchan, struct huser_struct *husr)
{
    if ((hchan->flags & H_OPER_ONLY) && (huser_get_level(husr) < H_OPER))
        return 0;
    return 1;
}

hchannel *hchannel_get_by_name(const char *cname)
{
    hchannel *tmp = hchannels;
    for (;tmp;tmp=tmp->next)
        if (!ircd_strcmp(cname, tmp->real_channel->index->name->content))
            return tmp;
    return NULL;
}

hchannel *hchannel_get_by_channel(channel *chan)
{
    hchannel *tmp;

    if (chan == NULL)
        return NULL;

    tmp = hchannels;
    for (;tmp;tmp=tmp->next)
        if (tmp->real_channel == chan)
            return tmp;
    return NULL;
}

const char *hchannel_get_state(hchannel* hchan, int mask)
{
    if (hchan->flags & mask)
        return "active";
    else
        return "inactive";
}

const char *hchannel_get_name(hchannel *hchan)
{
    return hchan->real_channel->index->name->content;
}

void hchannel_del_all(void)
{
    while (hchannels)
        hchannel_del(hchannels);
}

hchannel_user *hchannel_on_channel(hchannel *hchan, struct huser_struct *husr)
{
    hchannel_user *ptr = hchan->channel_users;
    for (;ptr;ptr = ptr->next)
        if (ptr->husr == husr)
            return ptr;
    return NULL;
}

hchannel_user *hchannel_add_user(hchannel *hchan, struct huser_struct *husr)
{
    hchannel_user **tmp = &(hchan->channel_users);
    assert(hchannel_on_channel(hchan, husr) == NULL);

    for (;*tmp;tmp = &(*tmp)->next);

    *tmp = (hchannel_user*)malloc(sizeof(hchannel_user));
    (*tmp)->husr = husr;
    (*tmp)->time_joined = time(NULL);
    (*tmp)->next = NULL;

    assert(hchannel_on_channel(hchan, husr) != NULL);

    return *tmp;
}

hchannel_user *hchannel_del_user(hchannel *hchan, struct huser_struct *husr)
{
    hchannel_user **tmp = &(hchan->channel_users);
    assert(hchannel_on_channel(hchan, husr) != NULL);

    for (;*tmp;tmp = &(*tmp)->next)
        if ((*tmp)->husr == husr)
        {
            hchannel_user *ptr = (*tmp)->next;
            free(*tmp);
            *tmp = ptr;

            assert(hchannel_on_channel(hchan, husr) == NULL);
            return NULL;
        }
    return NULL;
}

void hchannel_remove_inactive_users(void)
{
    hchannel *hchan = hchannels;

    for (;hchan;hchan = hchan->next)
    {
        if (hchan->flags & H_ANTI_IDLE && !(hchan->flags & H_PASSIVE))
        {
            hchannel_user **hchanuser = &hchan->channel_users;
            while (*hchanuser)
            {
		if (
		    (huser_get_level((*hchanuser)->husr) == H_PEON) &&
		    (time(NULL) - huser_on_channel((*hchanuser)->husr,hchan)->last_activity >= hchan->max_idle) &&
		    !(on_queue((*hchanuser)->husr, huser_on_channel((*hchanuser)->husr, hchan))) &&
                    !IsSetHost((*hchanuser)->husr->real_user)
		   )
		{
		    if (huser_on_channel((*hchanuser)->husr, hchan)->flags & H_IDLE_WARNING)
                    {
                        const char *banmask = hban_ban_string((*hchanuser)->husr->real_user, HBAN_HOST);
                        helpmod_setban(hchan, banmask, time(NULL) + 10 * HDEF_m, MCB_ADD, HNOW);

                        helpmod_kick(hchan, (*hchanuser)->husr, "Please do not idle in %s", hchannel_get_name(hchan));
                        continue;
                    }
                    else
                    {
                        helpmod_reply((*hchanuser)->husr, NULL, "You are currently idle in %s. Please part the channel if you have nothing to do there", hchannel_get_name(hchan));
                        huser_on_channel((*hchanuser)->husr, hchan)->flags |= H_IDLE_WARNING;
                    }
                }
                hchanuser = &(*hchanuser)->next;
            }
        }
        /* Additionally, test if the channel has queue but no idle opers / staff */
        if (hchan->flags & H_QUEUE && hchan->flags & H_QUEUE_TIMEOUT)
        {
            hchannel_user *tmp;
            for (tmp = hchan->channel_users;tmp;tmp = tmp->next)
                if (huser_get_level(tmp->husr) > H_PEON)
                {
                    huser_channel *huserchan = huser_on_channel(tmp->husr, hchan);
                    if ((time(NULL) - huserchan->last_activity < HELPMOD_QUEUE_TIMEOUT) && (huserchan->last_activity != tmp->time_joined))
                        break;
                }
            if (!tmp)
            {
                hchan->flags &= ~H_QUEUE;
                if (hchan->flags & H_REPORT && hchannel_is_valid(hchan->report_to))
                    helpmod_message_channel(hchan->report_to, "%s: Channel queue deactivated because of inactivity", hchannel_get_name(hchan));
                hchannel_conf_change(hchan, hchan->flags | H_QUEUE);
            }
        }
    }
}

void hchannel_report(void)
{
    hchannel *hchan = hchannels;
    for (;hchan;hchan = hchan->next)
        if (hchan->flags & H_REPORT && !(hchan->flags & H_PASSIVE) && hchannel_is_valid(hchan->report_to))
        {
            int total = hchannel_count_users(hchan, H_ANY);
            int peons = hchannel_count_users(hchan, H_PEON);
            int services = hchannel_count_users(hchan, H_SERVICE);

            if (peons == 0)
                return;

            if (hchan->flags & H_QUEUE)
            {
                int peons_queue = hchannel_count_queue(hchan);
                helpmod_message_channel(hchan->report_to, "%s: %d user%s in queue and %d user%s currently receiving support. %d Non-user%s. Average queue time %s", hchannel_get_name(hchan), peons_queue, (peons_queue==1)?"":"s", peons - peons_queue, (peons - peons_queue == 1)?"":"s", total-peons-services, (total-peons-services == 1)?"":"s", helpmod_strtime(hqueue_average_time(hchan)));
            }
            else
                helpmod_message_channel(hchan->report_to, "%s: %d user%s and %d non-user%s", hchannel_get_name(hchan), peons, (peons == 1)?"":"s",  total-peons-services, (total-peons-services == 1)?"":"s");
        }
}

void hchannel_set_topic(hchannel *hchan)
{
    if (hchan->flags & H_HANDLE_TOPIC)
        helpmod_set_topic(hchan, htopic_construct(hchan->topic));
}

void hchannels_match_accounts(void)
{
    hchannel *hchan = hchannels;
    hchannel_user *hchanuser;
    for (;hchan;hchan = hchan->next)
        for (hchanuser = hchan->channel_users;hchanuser;hchanuser = hchanuser->next)
            if (hchanuser->husr->account == NULL && IsAccount(hchanuser->husr->real_user))
                hchanuser->husr->account = haccount_get_by_name(hchanuser->husr->real_user->authname);
}

int hchannels_on_queue(huser *husr)
{
    huser_channel *huserchan = husr->hchannels;
    for (;huserchan;huserchan = huserchan->next)
        if (on_queue(husr, huserchan))
            return 1;
    return 0;
}

int hchannels_on_desk(struct huser_struct* husr)
{
    huser_channel *huserchan = husr->hchannels;
    for (;huserchan;huserchan = huserchan->next)
        if (on_desk(husr, huserchan))
            return 1;
    return 0;
}

void hchannels_dnmo(struct huser_struct *husr)
{
    hchannel *hchan = hchannels;
    for (;hchan;hchan = hchan->next)
    {
        huser_channel *huserchan = huser_on_channel(husr, hchan);
        /*if (on_queue(husr, huserchan) || on_desk(husr, huserchan))*/
        if (huserchan != NULL)
        {
            hchannel_user *tmp, **hchanuser = &hchan->channel_users;
            for (;*hchanuser;hchanuser = &(*hchanuser)->next)
                if ((*hchanuser)->husr == husr)
                {
                    tmp = *hchanuser;
                    *hchanuser = (*hchanuser)->next;
                    if (!*hchanuser)
                        break;
                }
            *hchanuser = tmp;
            (*hchanuser)->next = NULL;
            if (on_desk(husr, huserchan))
            {
                helpmod_channick_modes(husr, hchan, MC_DEVOICE, HLAZY);
                huserchan->flags &= ~HQUEUE_DONE;
            }
        }
    }
}

int hchannel_count_users(hchannel *hchan, hlevel lvl)
{
    int count = 0;
    hchannel_user *hchanuser = hchan->channel_users;
    for (;hchanuser;hchanuser = hchanuser->next)
        if (lvl == H_ANY)
            count++;
        else if (huser_get_level(hchanuser->husr) == lvl)
            count++;
    return count;
}

int hchannel_count_queue(hchannel *hchan)
{
    int count = 0;
    hchannel_user *hchanuser = hchan->channel_users;
    for (;hchanuser;hchanuser = hchanuser->next)
    {
        if (on_queue(hchanuser->husr, huser_on_channel(hchanuser->husr, hchan)))
            count++;
    }
    return count;
}

int hchannel_is_valid(hchannel *hchan)
{
    hchannel *ptr = hchannels;
    for (;ptr;ptr = ptr->next)
        if (hchan == ptr)
            return 1;
    return 0;
}

void hchannel_mode_check(hchannel *hchan)
{
    if (((hchan->flags & H_MAINTAIN_M) || (hchan->flags & H_QUEUE)) && !IsModerated(hchan->real_channel))
        helpmod_simple_modes(hchan, CHANMODE_MODERATE, 0,0);
    else if (!((hchan->flags & H_MAINTAIN_M) || (hchan->flags & H_QUEUE)) && IsModerated(hchan->real_channel))
        helpmod_simple_modes(hchan, 0, CHANMODE_MODERATE ,0);
    if (hchan->flags & H_MAINTAIN_I && !IsInviteOnly(hchan->real_channel))
        helpmod_simple_modes(hchan, CHANMODE_INVITEONLY, 0,0);
    else if (!(hchan->flags & H_MAINTAIN_I) && IsInviteOnly(hchan->real_channel))
        helpmod_simple_modes(hchan, 0, CHANMODE_INVITEONLY, 0);
}

void hchannel_conf_change(hchannel *hchan, int old_flags)
{
    int i;
    hflagchange change;

    for (i=0;i<HCHANNEL_CONF_COUNT;i++)
    {
        if ((hchan->flags ^ old_flags) & (1 << i))
        {
            change = (hchan->flags & (1 << i))?H_ON:H_OFF;
            switch (1 << i)
            {
            case H_QUEUE:
                hchannel_mode_check(hchan);
                if (change == H_ON)
                    helpmod_message_channel(hchan, "Channel queue has been activated, all users enqueued");
                else
                    helpmod_message_channel(hchan, "Channel queue has been deactivated");
                break;
            case H_MAINTAIN_I:
            case H_MAINTAIN_M:
                hchannel_mode_check(hchan);
                break;
            }
        }
    }
}

int hchannel_count(void)
{
    hchannel *hchan = hchannels;
    int count = 0;
    for (;hchan;hchan = hchan->next)
        count++;
    return count;
}

void hchannel_activate_join_flood(hchannel *hchan)
{
    hchannel_user **hchanuser = &hchan->channel_users;
    helpmod_simple_modes(hchan, CHANMODE_REGONLY, 0, 1);

    /* clean the channel of the already joined clients */
    while (*hchanuser)
        if (((*hchanuser)->time_joined > (time(NULL) - 5)) && huser_get_level((*hchanuser)->husr) < H_STAFF)
            helpmod_kick(hchan, (*hchanuser)->husr, "Join flood");
        else
            hchanuser = &(*hchanuser)->next;

    hchan->flags |= H_JOIN_FLOOD;

    scheduleoneshot(time(NULL) + 60, &hchannel_deactivate_join_flood, NULL);
}
/* goes to schedule */
void hchannel_deactivate_join_flood()
{
    hchannel *hchan = hchannels;
    for (;hchan;hchan = hchan->next)
        if (hchan->flags & H_JOIN_FLOOD)
        {
            helpmod_simple_modes(hchan, 0, CHANMODE_REGONLY, 1);
            hchan->flags &= ~H_JOIN_FLOOD;
        }
        /*if (IsRegOnly(hchan->real_channel) && hchan->jf_control < time(NULL))
        {
            helpmod_simple_modes(hchan, 0, CHANMODE_REGONLY, 1);
            return;
        }

     scheduleoneshot(time(NULL) + 60, &hchannel_deactivate_join_flood, NULL);
     */
}

const char *hchannel_get_sname(int flag)
{
    if (flag < 0 || flag > HCHANNEL_CONF_COUNT)
        return NULL;

    switch (flag)
    {
    case 0:
        return "Passive state";
    case 1:
        return "Welcome message";
    case 2:
        return "JoinFlood protection";
    case 3:
        return "Queue";
    case 4:
        return "Verbose queue (requires queue)";
    case 5:
        return "Auto queue (requires queue)";
    case 6:
        return "Channel status reporting";
    case 7:
        return "Pattern censor";
    case 8:
        return "Lamer control";
    case 9:
        return "Idle user removal";
    case 10:
        return "Keep channel moderated";
    case 11:
        return "Keep channel invite only";
    case 12:
        return "Handle channel topic";
    case 13:
        return "Calculate statistic";
    case 14:
        return "Remove joining trojans";
    case 15:
        return "Channel commands";
    case 16:
        return "Oper only channel";
    case 17:
        return "Disallow bold, underline, etc.";
    case 18:
        return "Queue inactivity deactivation";
    case 19:
        return "Require a ticket to join";
    default:
        return "error, please contact strutsi";
    }
}
