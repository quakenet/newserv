#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "helpmod_entries.h"
#include "helpmod_alias.h"

#include "helpmod.h"
#include "hcommand.h"
#include "hcommands.h"
#include "hterm.h"
#include "hchanban.h"
#include "hcensor.h"
#include "hchannel.h"
#include "hlamer.h"
#include "hban.h"
#include "hconf.h"
#include "hhooks.h"
#include "hstat.h"
#include "hgen.h"
#include "hdef.h"
#include "hticket.h"
#include "hed.h"
#include "../lib/version.h"

MODULE_VERSION("")

int HELPMOD_ACCOUNT_EXPIRATION[] =
{
    14 * HDEF_d, /* H_LAMER */
    7  * HDEF_d, /* H_PEON */
    1  * HDEF_M, /* H_FRIEND */
    2  * HDEF_M, /* H_TRIAL */
    6  * HDEF_M, /* H_STAFF */
    1  * HDEF_y, /* H_OPER */
    2  * HDEF_y  /* H_ADMIN */
};

/* new H stuff */
hcommand *hcommands;
hchannel *hchannels;
haccount *haccounts;
hterm *hterms;
hlc_profile *hlc_profiles;
hban *hbans;
huser *husers;
hchanban *hchanbans;
helpmod_editor *helpmod_editors;

modechanges hmodechanges;
time_t helpmod_startup_time;

helpmod_entry helpmod_base;
alias_tree aliases;
nick *helpmodnick;
char helpmod_db[128] = "helpmod/default";
long helpmod_usage;

int hstat_cycle;
time_t hstat_last_cycle;

enum
{
    H_REPLY_PRIVMSG,
    H_REPLY_NOTICE
};

void hcommit_modes(void)
{
    if (hmodechanges.cp == NULL)
        return;
    if (hchannel_get_by_channel(hmodechanges.cp)->flags & H_PASSIVE)
    {
        localsetmodeinit(&hmodechanges, NULL, helpmodnick);
        return;
    }
    localsetmodeflush(&hmodechanges, 1);
    localsetmodeinit(&hmodechanges, NULL, helpmodnick);
}

void helpmod_reply(huser *target, channel* returntype, const char *message, ... )
{
    char buf[512];
    va_list va;
    int reply_type;

    if (helpmodnick==NULL) {
        return;
    }

    va_start(va,message);
    vsnprintf(buf,512,message,va);
    va_end(va);

    if (buf[0] == '\0' || buf[0] == '\n')
        return;

    if (returntype) /* channel */
        reply_type = H_REPLY_NOTICE;
    else            /* private */
        reply_type = H_REPLY_PRIVMSG;

    if (target->account != NULL)
    {
        if (target->account->flags & H_ALL_PRIVMSG)
            reply_type = H_REPLY_PRIVMSG;
        if (target->account->flags & H_ALL_NOTICE)
            reply_type = H_REPLY_NOTICE;
    }

    if (reply_type == H_REPLY_PRIVMSG)
        sendmessagetouser(helpmodnick,target->real_user, "%s", buf);
    else
        sendnoticetouser(helpmodnick,target->real_user, "%s", buf);
}

void helpmod_message_channel(hchannel *hchan, const char *message, ...)
{
    char buf[512];
    va_list va;

    if (helpmodnick==NULL || hchan == NULL) {
        return;
    }

    va_start(va,message);
    vsnprintf(buf,512,message,va);
    va_end(va);

    if (hchan->flags & H_PASSIVE)
        return;

    sendmessagetochannel(helpmodnick, hchan->real_channel, "%s", buf);
}

void helpmod_message_channel_long(hchannel *hchan, const char *message, ...)
{
    char buf[2048], *bp = buf;
    va_list va;
    int i;

    if (helpmodnick==NULL || hchan == NULL) {
        return;
    }

    va_start(va,message);
    vsnprintf(buf,2048,message,va);
    va_end(va);

    if (hchan->flags & H_PASSIVE)
        return;

    while (strlen(bp) > 450)
    {
        for (i=0;i<50;i++)
            if (isspace(bp[390 + i]))
            {
                bp[390 + i] = '\0';
                break;
            }
        if (i == 50)
            bp[390 + i] = '\0';
        sendmessagetochannel(helpmodnick, hchan->real_channel, "%s", bp);
        bp+=(390+i+1);
    }
    if (*bp)
        sendmessagetochannel(helpmodnick, hchan->real_channel, "%s", bp);
}

void helpmod_kick(hchannel *hchan, huser *target, const char *reason, ...)
{
    char buf[512];
    va_list va;

    if (hchan->flags & H_PASSIVE)
        return;

    va_start(va,reason);
    vsnprintf(buf,512,reason,va);
    va_end(va);

    localkickuser(helpmodnick, hchan->real_channel, target->real_user, buf);
}

void helpmod_invite(hchannel *hchan, huser *husr)
{
    if (hchan->flags & H_PASSIVE)
        return;

    localinvite(helpmodnick, hchan->real_channel->index, husr->real_user);
}

static void hmode_set_channel(hchannel  *hchan)
{
    if (hmodechanges.cp && hmodechanges.cp != hchan->real_channel)
        hcommit_modes();
    hmodechanges.cp = hchan->real_channel;
}

void helpmod_channick_modes(huser *target, hchannel *hchan, short mode, int now)
{
    void *args[] = { hchan->real_channel, helpmodnick, target->real_user};
    hmode_set_channel(hchan);
    localdosetmode_nick(&hmodechanges, target->real_user, mode);

    if (huser_on_channel(target, hchan) == NULL)
    {
        Error("helpmod", ERR_WARNING, "Channick mode for user %s not on channel %s", huser_get_nick(target), hchannel_get_name(hchan));
        return;
    }

    if (hchan->flags & H_PASSIVE)
        return;

    switch (mode)
    {
    case MC_OP:
        triggerhook(HOOK_CHANNEL_OPPED, args);
        break;
    case MC_DEOP:
        triggerhook(HOOK_CHANNEL_DEOPPED, args);
        break;
    case MC_VOICE:
        triggerhook(HOOK_CHANNEL_VOICED, args);
        break;
    case MC_DEVOICE:
        triggerhook(HOOK_CHANNEL_DEVOICED, args);
        break;
    }

    if (now)
        hcommit_modes();
}

void helpmod_setban(hchannel *hchan, const char *banstr, time_t expiration, int type, int now)
{
    hmode_set_channel(hchan);

    if (hchan->flags & H_PASSIVE)
        return;

    localdosetmode_ban(&hmodechanges, banstr, type);

    if ((type == MCB_ADD) && (expiration > time(NULL)))
        scheduleoneshot(expiration, (ScheduleCallback)&hchanban_schedule_entry, hchanban_add(hchan, banstr, expiration));

    if (now)
        hcommit_modes();
}

void helpmod_simple_modes(hchannel *hchan, int add, int remove, int now)
{
    hmode_set_channel(hchan);

    if (hchan->flags & H_PASSIVE)
        return;

    localdosetmode_simple(&hmodechanges, add, remove);

    if (now)
        hcommit_modes();
}

void helpmod_set_topic(hchannel *hchan, const char* topic)
{
    if (hchan->flags & H_PASSIVE)
        return;

    localsettopic(helpmodnick, hchan->real_channel, (char*)topic);
}

void helpmod_privmsg(void **args)
{
    void *sender = args[0];
    void *message = args[1];

    nick *sender_nick;
    huser *sender_huser;

    sender_nick = getnickbynick((char*)sender);

    if ((sender_huser = huser_get(sender_nick)) == NULL)
        sender_huser = huser_add(sender_nick);

    huser_activity(sender_huser, NULL);

    helpmod_command(sender_huser, NULL, (char*)message);
}

void helpmod_chan_privmsg(void **args)
{
    void *sender = args[0];
    channel *chan = (channel*)args[1];
    void *message = args[2];

    nick *sender_nick;
    huser *sender_huser;
    hcensor *tmp;
    hchannel *hchan = hchannel_get_by_channel(chan);

    sender_nick = getnickbynick((char*)sender);

    if ((hchan == NULL || hchan->flags & H_PASSIVE))
        return;

    if ((sender_huser = huser_get(sender_nick)) == NULL)
        sender_huser = huser_add(sender_nick);

    if (hchan->flags & H_DO_STATS)
    {
        if (sender_huser->account != NULL)
	    hstat_calculate_account(hchan, sender_huser, message);
        hstat_calculate_channel(hchan, sender_huser, message);
    }
    huser_activity(sender_huser, hchan);

    if (huser_get_level(sender_huser) < H_FRIEND) /* staff, staff trials and friends are not subject to any control */
    {
        if ((hchan->flags & H_CENSOR) && (tmp = hcensor_check(hchan->censor, (char*)message)))
        { /* censor match */
            if (hcensor_match(hchan, sender_huser, tmp))
		return;
        }
        if ((hchan->flags & H_LAMER_CONTROL) && (hlc_check(hchan, sender_huser, (char*)message)))
            return;
        if ((hchan->flags & H_DISALLOW_LAME_TEXT) && helpmod_is_lame_line(message))
        {
            helpmod_kick(hchan, sender_huser, "Please only use normal text on %s", hchannel_get_name(hchan));
            return;
	}
	if ((hchan->flags & H_HIGHLIGHT_PREVENTION) && hchannel_highlight_detection(hchan, (char*)message))
	{
	    helpmod_kick(hchan, sender_huser, "Please do not abuse the highlight feature of IRC clients");
            return;
	}
    }

    if (hcommand_is_command((char*)message) && (hchan->flags & H_CHANNEL_COMMANDS))
        helpmod_command(sender_huser, chan, (char*)message);
}

void helpmod_kicked(void **args)
{
    /* just rejoin the channel, if an oper wants H gone he can use delchan */
    channel *cp = findchannel(args[1]);
    if (!cp)
        localcreatechannel(helpmodnick, args[1]);
    else
    {
        localjoinchannel(helpmodnick, cp);
        localgetops(helpmodnick, cp);
    }
}

void helpmodmessagehandler(nick *sender, int messagetype, void **args)
{
    switch (messagetype)
    {
    case LU_PRIVMSG:
        helpmod_privmsg(args);
        break;
    case LU_CHANMSG:
        helpmod_chan_privmsg(args);
        break;
    case LU_KICKED:
        helpmod_kicked(args);
        break;
    }

    hcommit_modes();
}

void helpconnect(void) {
    /* register H */
    helpmodnick=registerlocaluser(HELPMOD_NICK,
				  "help",
				  "quakenet.org",
				  "NewServ HelpMod 2, /msg "HELPMOD_NICK" help",
				  HELPMOD_AUTH,
				  UMODE_OPER|UMODE_ACCOUNT|UMODE_SERVICE,&helpmodmessagehandler);
    /* register hooks */
    helpmod_registerhooks();

    /* continue with the database */
    if (helpmod_config_read(HELPMOD_DEFAULT_DB))
    {
        Error("helpmod", ERR_WARNING, "Error reading the default database '%s'", HELPMOD_DEFAULT_DB);
        if (helpmod_config_read(HELPMOD_FALLBACK_DB))
            Error("helpmod", ERR_ERROR, "Error reading the fallback database '%s'", HELPMOD_FALLBACK_DB);
    }
    if (!haccounts)
        Error("helpmod", ERR_ERROR, "Read 0 accounts from database (something is broken)");

    /* old H */
    helpmod_init_entry(&helpmod_base);
    helpmod_load_entries(HELPMOD_HELP_DEFAULT_DB);
}

void _init()
{
    helpmod_usage = 0;
    helpmod_base = NULL;
    aliases = NULL;

    hcommands = NULL;
    hchannels = NULL;
    hlc_profiles = NULL;
    husers = NULL;
    haccounts = NULL;
    hbans = NULL;
    hterms = NULL;
    hchanbans = NULL;

    helpmod_startup_time = time(NULL);
    /* add the supported commands, needs to be done like this since 3 arrays would just become a mess */
    /* first the legacy old-H commands */

    hcommands_add();

    srand(time(NULL));

    /* init hmodechanges */
    localsetmodeinit(&hmodechanges, NULL, helpmodnick);

    schedulerecurring(time(NULL)+1,0,HDEF_h,(ScheduleCallback)&huser_clear_inactives,NULL);
    schedulerecurring(time(NULL)+1,0,HDEF_d,(ScheduleCallback)&haccount_clear_inactives,NULL);
    schedulerecurring(time(NULL)+1,0,HDEF_m,(ScheduleCallback)&hban_remove_expired,NULL);
    schedulerecurring(time(NULL)+1,0,30 * HDEF_s, (ScheduleCallback)&hchannel_remove_inactive_users, NULL);
    schedulerecurring(time(NULL)+1,0,60 * HDEF_m,(ScheduleCallback)&hchannel_report, NULL);
    schedulerecurring(time(NULL) + HDEF_h, 0, 6 * HDEF_h, (ScheduleCallback)&helpmod_config_scheduled_events, NULL);
    schedulerecurring(time(NULL)+1,0,10 * HDEF_m, (ScheduleCallback)&hticket_remove_expired, NULL);
    schedulerecurring(hstat_get_schedule_time() - 5 * HDEF_m, 0, HDEF_d, (ScheduleCallback)&hstat_scheduler, NULL);

    helpconnect();
}

void _fini()
{
    helpmod_deregisterhooks();
    /* write the database so that we don't lose anything
     existance of atleast one account is required (sanity check)
     */
    if (haccounts)
        helpmod_config_write(HELPMOD_DEFAULT_DB);

    /* delete schedule stuff */
    deleteallschedules((ScheduleCallback)&helpconnect);
    deleteallschedules((ScheduleCallback)&huser_clear_inactives);
    deleteallschedules((ScheduleCallback)&haccount_clear_inactives);
    deleteallschedules((ScheduleCallback)&hban_remove_expired);
    deleteallschedules((ScheduleCallback)&hchannel_remove_inactive_users);
    deleteallschedules((ScheduleCallback)&hchannel_report);
    deleteallschedules((ScheduleCallback)&helpmod_config_scheduled_events);
    deleteallschedules((ScheduleCallback)&hstat_scheduler);
    deleteallschedules((ScheduleCallback)&hchannel_deactivate_join_flood);
    deleteallschedules((ScheduleCallback)&hchanban_schedule_entry);
    deleteallschedules((ScheduleCallback)&hticket_remove_expired);

    hcommand_del_all();
    hchannel_del_all();
    huser_del_all();
    hlc_del_all();
    hban_del_all();
    hterm_del_all(NULL);
    hchanban_del_all();
    helpmod_clear_aliases(&aliases);
    helpmod_clear_all_entries();

    deregisterlocaluser(helpmodnick, "Module unloaded");
}
