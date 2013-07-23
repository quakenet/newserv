#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>

#include "../lib/strlfunc.h"

#include "hcommands.h"
#include "hcommand.h"

#include "helpmod.h"
#include "klingon.h"
#include "helpmod_alias.h"

#include "haccount.h"
#include "hqueue.h"
#include "hterm.h"

#include "hgen.h"
#include "hban.h"
#include "hchanban.h"

#include "hticket.h"
#include "hconf.h"

/* following are macros for use ONLY IN HERE
 they may not look pretty, but work surprisingly well */

#define SKIP_WORD \
{\
    if (*ostr == '"' && strchr(ostr+1, '"'))\
        ostr = strchr(ostr+1, '"');\
    while (!isspace(*ostr) && *ostr)\
        ostr++;\
    while (isspace(*ostr) && *ostr)\
        ostr++;\
        argc--;\
        argv++;\
}

#define DEFINE_HCHANNEL \
{\
    if (argc == 0)\
        hchan = hchannel_get_by_channel(returntype);\
    else if (*argv[0] == '#')\
    {\
        hchan = hchannel_get_by_name(argv[0]);\
        SKIP_WORD;\
    }\
    else\
        hchan = hchannel_get_by_channel(returntype);\
}

#define HCHANNEL_VERIFY_AUTHORITY(c, u)\
{\
    if ((c) != NULL && !hchannel_authority((c), (u)))\
    {\
        helpmod_reply(u, returntype, "Sorry, channel %s is oper only", hchannel_get_name((c)));\
        return;\
    }\
}

enum
{
    H_CMD_OP,
    H_CMD_DEOP,
    H_CMD_VOICE,
    H_CMD_DEVOICE
};

enum
{
    H_TERM_FIND,
    H_TERM_MINUS,
    H_TERM_PLUS
};

static void helpmod_cmd_addchan (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot add channel: Channel not defined");
        return;
    }

    if (*argv[0] != '#')
    {
        helpmod_reply(sender, returntype, "Can not add channel: Channel name must start with '#'");
        return;
    }

    if (hchannel_get_by_name(argv[0]) != NULL)
    {
        helpmod_reply(sender, returntype, "Can not add channel: Channel %s already active", argv[0]);
        return;
    }

    if (hchannel_add(argv[0]) != NULL)
    {
        helpmod_reply(sender, returntype, "Channel %s added succesfully", argv[0]);
        return;
    }
}

static void helpmod_cmd_delchan (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    char buffer[256];
    DEFINE_HCHANNEL;

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Can not delete channel: Channel not defined or not found");
        return;
    }
    strcpy(buffer, hchan->real_channel->index->name->content);
    if (argc == 0 || strcmp(argv[0], "YesImSure"))
    {
        helpmod_reply(sender, returntype, "Can not delete channel: Sanity check. Please add a parameter \"YesImSure\" to confirm channel deletion");
        return;
    }
    hchannel_del(hchan);

    helpmod_reply(sender, returntype, "Channel %s deleted succesfully", buffer);
}   

static void helpmod_cmd_whoami (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_reply(sender, returntype, "You are %s", huser_get_nick(sender));
    helpmod_reply(sender, returntype, "Your userlevel is %s", hlevel_name(huser_get_level(sender)));
    if (sender->account == NULL)
        helpmod_reply(sender, returntype, "You do not have an account with me");
    else
        helpmod_reply(sender, returntype, "Your accounts name is %s", sender->account->name->content);
    if (huser_get_level(sender) < H_TRIAL)
    {
	if (sender->lc[0] || sender->lc[1] || sender->lc[2] || sender->lc[3] || sender->lc[4])
	    helpmod_reply(sender, returntype, "You violated the following rules: Excessive use of capital letters: %d, repeating: %d, improper use of language: %d, flooding: %d and spamming: %d", sender->lc[0], sender->lc[1], sender->lc[2], sender->lc[3], sender->lc[4]);
	else
            helpmod_reply(sender, returntype, "You have not violated any rules");
    }

    {
        int pos;
        huser_channel *huserchan = sender->hchannels;
        for (;huserchan;huserchan = huserchan->next)
            if ((pos = hqueue_get_position(huserchan->hchan, sender)) > -1)
                helpmod_reply(sender, returntype, "You have queue position #%d on channel %s", pos, hchannel_get_name(huserchan->hchan));
    }

    if (IsAccount(sender->real_user))
    {
        hchannel *hchan;
        hticket *htick;
        for (hchan = hchannels;hchan;hchan = hchan->next)
            if (hchan->flags & H_REQUIRE_TICKET)
            {
                htick = hticket_get(huser_get_auth(sender), hchan);
                if (htick != NULL)
                    helpmod_reply(sender, returntype, "You have an invite ticket for channel %s that expires in %s", hchannel_get_name(hchan), helpmod_strtime(time(NULL) - htick->time_expiration));
            }
    }
}

static void helpmod_cmd_whois (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    huser *husr;
    int i;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot get user information: User not specified");
        return;
    }
    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;

    for (i=0;i<argc;i++)
    {
        husr = huser_get(getnickbynick(argv[i]));
        if (husr == NULL)
	{
	    if (getnickbynick(argv[i]) == NULL)
                helpmod_reply(sender, returntype, "Cannot get user information: User %s does not exist in the network", argv[i]);
	    else
		helpmod_reply(sender, returntype, "Cannot get user information: User %s exists but is not known to me", argv[i]);
	    continue;
        }
        helpmod_reply(sender, returntype, "User %s has userlevel %s", huser_get_nick(husr), hlevel_name(huser_get_level(husr)));
	if (husr->account == NULL)
            helpmod_reply(sender, returntype, "User %s does not have an account with me", huser_get_nick(husr));
        else
            helpmod_reply(sender, returntype, "User %s has account named %s", huser_get_nick(husr),husr->account->name->content);
        if (huser_get_level(husr) < H_TRIAL)
        {
	    if (husr->lc[0] || husr->lc[1] || husr->lc[2] || husr->lc[3] || husr->lc[4])
		helpmod_reply(sender, returntype, "User %s has lamercontrol entries: Excessive use of capital letters: %d, repeating: %d, improper use of language: %d, flooding: %d and spamming: %d", huser_get_nick(husr), husr->lc[0], husr->lc[1], husr->lc[2], husr->lc[3], husr->lc[4]);
	    else
		helpmod_reply(sender, returntype, "User %s has no lamercontrol entries", huser_get_nick(husr));
        }
        {
            int pos;
            huser_channel *huserchan = husr->hchannels;
            for (;huserchan;huserchan = huserchan->next)
            {
                if ((pos = hqueue_get_position(huserchan->hchan, husr)) > -1)
                    helpmod_reply(sender, returntype, "User %s has queue queue position #%d on channel %s", huser_get_nick(husr), pos, hchannel_get_name(huserchan->hchan));
                if (on_desk(husr, huserchan))
                    helpmod_reply(sender, returntype, "User %s is receiving support on channel %s", huser_get_nick(husr), hchannel_get_name(huserchan->hchan));
            }
        }
        if (IsAccount(husr->real_user))
        {
            hchannel *hchan;
            hticket *htick;
            for (hchan = hchannels;hchan;hchan = hchan->next)
                if (hchan->flags & H_REQUIRE_TICKET)
                {
                    htick = hticket_get(huser_get_auth(husr), hchan);
                    if (htick != NULL)
                        helpmod_reply(sender, returntype, "User %s has an invite ticket for channel %s that expires in %s", huser_get_nick(husr), hchannel_get_name(hchan), helpmod_strtime(time(NULL) - htick->time_expiration));
                }
        }
    }
}
/*
void helpmod_cmd_megod (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    if (sender->account == NULL)
        sender->account = haccount_add(huser_get_nick(sender), H_ADMIN);

    sender->account->level = H_ADMIN;
    }
    */

static void helpmod_cmd_seen (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    huser *target_huser;
    haccount *target_haccount;
    int i;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "No targets specified for seen");
        return;
    }
    for (i=0;i < argc;i++)
    {
        if (*argv[i] == '#')
        { /* account */
            if (!ci_strcmp(argv[i] + 1, HELPMOD_AUTH))
            {   /* A nice special case */
                helpmod_reply(sender, returntype, "I'm right here");
                continue;
            }
            target_haccount = haccount_get_by_name(argv[i]);
            if (target_haccount == NULL)
            {
                helpmod_reply(sender, returntype, "Account %s not found", argv[i] + 1);
                continue;
            }

            helpmod_reply(sender, returntype, "Account %s's last recorded activity was %s ago", target_haccount->name->content, helpmod_strtime(time(NULL) - target_haccount->last_activity));
        }
        else
        {
            if (getnickbynick(argv[i]) == helpmodnick)
            {   /* A nice special case */
                helpmod_reply(sender, returntype, "I'm right here");
                continue;
            }
            target_huser = huser_get(getnickbynick(argv[i]));
            if (target_huser == NULL)
            {
                helpmod_reply(sender, returntype, "User %s not found", argv[i]);
                continue;
            }
            helpmod_reply(sender, returntype, "User %s's last recorded activity was %s ago", huser_get_nick(target_huser), helpmod_strtime(time(NULL) - target_huser->last_activity));
        }
    }
}

static void helpmod_cmd_change_userlevel(huser *sender, hlevel target_level, channel* returntype, char* ostr, int argc, char *argv[])
{
    huser *target_huser;
    haccount *target_haccount;
    int i;

    if (argc == 0)
    {
	helpmod_reply(sender, returntype, "Incorrect syntax: Nick or account required");
        return ;
    }

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;

    for (i = 0;i < argc;i++)
    {
        if (*argv[i] == '#') /* account */
        {
            target_haccount = haccount_get_by_name(argv[i]);
            if (target_haccount == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: Account '%s' not found", argv[i]);
                continue;
            }
            if (target_haccount->level > huser_get_level(sender))
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: Account '%s' has a userlevel higher than yours", argv[i]);
                continue;
            }

            if (target_haccount->level == target_level)
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: Account '%s' already has userlevel %s", argv[i], hlevel_name(target_level));
                continue;
            }

            target_haccount->level = target_level;

	    helpmod_reply(sender, returntype, "Userlevel changed: Account '%s' now has userlevel %s", argv[i], hlevel_name(target_level));

        }
        else
        {
            target_huser = huser_get(getnickbynick(argv[i]));
            if (target_huser == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: User '%s' not found", argv[i]);
                continue;
            }
            if (huser_get_level(target_huser) > huser_get_level(sender))
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: User '%s' has a userlevel higher than yours", argv[i]);
                continue;
            }
            if (huser_get_level(target_huser) == H_STAFF && huser_get_level(sender) == H_STAFF)
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: User '%s' has the same userlevel as you have", argv[i]);
                continue;
            }

            if (huser_get_level(target_huser) == target_level)
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: User '%s' already has userlevel %s", argv[i], hlevel_name(target_level));
                continue;
            }
            if (target_huser == sender)
            {
                helpmod_reply(sender, returntype, "Cannot change userlevel: Sanity check, you're changing your own userlevel, use #account instead of nick if you really wish to do this");
                continue;
	    }

	    if (target_level == H_LAMER)
	    {
		const char *banmask = hban_ban_string(target_huser->real_user, HBAN_HOST);
		hban_add(banmask, "Improper user", time(NULL) + HCMD_OUT_DEFAULT, 1);
		if (!IsAccount(target_huser->real_user))
		{
		    helpmod_reply(sender, returntype, "Cannot change userlevel: User '%s' is not authed. Banning user instead.", argv[i]);
		    continue;
		}
	    }

	    if (!IsAccount(target_huser->real_user))
	    {
                helpmod_reply(sender, returntype, "Cannot change userlevel: User '%s' is not authed", argv[i]);
                continue;
            }

	    if (target_huser->account == NULL)
	    {
                if (haccount_get_by_name(huser_get_auth(target_huser)) != NULL)
                {
                    helpmod_reply(sender, returntype, "Cannot change userlevel: Unable to create an account. Account %s already exists", huser_get_auth(target_huser));
                    continue;
                }
                else
                    target_huser->account = haccount_add(huser_get_auth(target_huser), target_level);
            }

            target_huser->account->level = target_level;

	    helpmod_reply(sender, returntype, "Userlevel changed: User '%s' now has userlevel %s", argv[i], hlevel_name(target_level));

            if (target_level != H_LAMER)
		helpmod_reply(target_huser, NULL, "Your userlevel has been changed, your current userlevel is %s", hlevel_name(target_level));
        }
    }
}

/* pseudo commands for the above */
static void helpmod_cmd_improper (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_change_userlevel(sender, H_LAMER, returntype, ostr, argc, argv); }
static void helpmod_cmd_peon (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_change_userlevel(sender, H_PEON, returntype, ostr, argc, argv); }
static void helpmod_cmd_friend (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_change_userlevel(sender, H_FRIEND, returntype, ostr, argc, argv); }
static void helpmod_cmd_trial (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_change_userlevel(sender, H_TRIAL, returntype, ostr, argc, argv); }
static void helpmod_cmd_staff (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_change_userlevel(sender, H_STAFF, returntype, ostr, argc, argv); }
static void helpmod_cmd_oper (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_change_userlevel(sender, H_OPER, returntype, ostr, argc, argv); }
static void helpmod_cmd_admin (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_change_userlevel(sender, H_ADMIN, returntype, ostr, argc, argv); }

static void helpmod_cmd_deluser (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    int i;
    huser *target_huser;
    haccount *target_haccount;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Usage: deluser [nick|#account]");
        return;
    }

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;

    for (i = 0;i < argc;i++)
    {
        if (*argv[i] != '#')
        {
            target_huser = huser_get(getnickbynick(argv[i]));
            if (target_huser == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot delete the account: User %s does not exist", argv[i]);
                continue;
            }
            if (target_huser->account == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot delete the account: User %s does not have an account", argv[i]);
                continue;
            }
            target_haccount = target_huser->account;
        }
        else
            target_haccount = haccount_get_by_name(argv[i]);

        if (target_haccount == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot delete the account: Account %s does not exist", argv[i]);
            continue;
        }
        if (target_haccount->level > huser_get_level(sender))
        {
            helpmod_reply(sender, returntype, "Cannot delete the account: Account %s has higher userlevel than you", target_haccount->name->content);
            continue;
        }
        helpmod_reply(sender, returntype, "Account %s deleted", target_haccount->name->content);
        haccount_del(target_haccount);
    }
}

static void helpmod_cmd_listuser (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    char *pattern;
    haccount *hack = haccounts;
    int count = 0, lvl = -1;
    time_t timer;

    if (argc > 0 && sscanf(argv[0], "%d", &lvl) && lvl >= H_LAMER && lvl <= H_ADMIN)
    {
        SKIP_WORD;
    }
    else
        lvl = -1;

    if (argc == 0)
        pattern = "*";
    else
        pattern = argv[0];

    if (lvl >= 0)
        helpmod_reply(sender, returntype, "%s accounts matching pattern %s: (account name, userlevel, expires in)", hlevel_name(lvl), pattern);
    else
        helpmod_reply(sender, returntype, "Accounts matching pattern %s: (account name, userlevel, expires in)", pattern);
    for (;hack;hack = hack->next)
    {
        if (strregexp(hack->name->content, pattern))
        {
            if (lvl >= 0 && hack->level != lvl)
                continue;
            timer = hack->last_activity + HELPMOD_ACCOUNT_EXPIRATION[hack->level];
            helpmod_reply(sender, returntype, "%-16s %-32s %s", hack->name->content, hlevel_name(hack->level), asctime(localtime(&timer)));
            count++;
        }
    }
    if (lvl >= 0)
        helpmod_reply(sender, returntype, "%s accounts matching pattern %d", hlevel_name(lvl), count);
    else
        helpmod_reply(sender, returntype, "Accounts matching pattern %d", count);
}

static void helpmod_cmd_censor (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hcensor *hcens;
    int i = 0;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot handle the censor: Channel not specified or found");
        return;
    }

    if (argc < 2 || !ci_strcmp(argv[0], "list")) /* view */
    {
        if (hchan->censor == NULL)
            helpmod_reply(sender, returntype, "Nothing is censored on channel %s", hchan->real_channel->index->name->content);
        else
        {
            helpmod_reply(sender, returntype, "Censored patterns for channel %s (%s):", hchan->real_channel->index->name->content, (hchan->flags & H_CENSOR)?"active":"inactive");
            for (hcens = hchan->censor;hcens;hcens = hcens->next)
                helpmod_reply(sender, returntype, "#%-4d %-8s %s%s%s", i++, hcensor_get_typename(hcens->type), hcens->pattern->content, hcens->reason?" :: ":"", hcens->reason?hcens->reason->content:"");
        }
    }
    else
    {
        if (!ci_strcmp(argv[0], "add"))
        {
            char *pattern;
            char *reason;
	    int type = HCENSOR_KICK;

            SKIP_WORD;

            /* not very elegant but will have to do */
	    if (argc > 1)
	    {
		if (!ci_strcmp(argv[0], "warn"))
		{
		    type = HCENSOR_WARN;
		    if (argc <= 2)
		    {
			helpmod_reply(sender, returntype, "Cannot add censor entry: Entries of type warn require a reason");
			return;
		    }
                    SKIP_WORD;
		}
		else if (!ci_strcmp(argv[0], "kick"))
		{
                    type = HCENSOR_KICK;
                    SKIP_WORD;
		}
		else if (!ci_strcmp(argv[0], "chanban"))
		{
                    type = HCENSOR_CHANBAN;
                    SKIP_WORD;
		}
		else if (!ci_strcmp(argv[0], "ban"))
		{
                    type = HCENSOR_BAN;
                    SKIP_WORD;
		}
	    }

	    pattern = argv[0];

	    if (strlen(pattern) == 0)
	    {
		helpmod_reply(sender, returntype, "Cannot add censor entry: Pattern must be non-empty");
                return;
	    }

	    SKIP_WORD;
	    if (argc && strlen(ostr) > 0)
		reason = ostr;
            else
                reason = NULL;

            if (hcensor_get_by_pattern(hchan->censor, pattern))
            {
                helpmod_reply(sender, returntype, "Cannot add censor entry: Pattern '%s' is already censored", pattern);
                return;
            }

            if (hcensor_add(&hchan->censor, pattern, reason, type))
		helpmod_reply(sender, returntype, "Pattern '%s' (%s) with type %s, censored succesfully", pattern, reason?reason:"no reason specified", hcensor_get_typename(type));
	    else
		helpmod_reply(sender, returntype, "Cannot add censor entry: Pattern '%s' is already censored", pattern);

        }
        else if (!ci_strcmp(argv[0], "del"))
        {
            SKIP_WORD;
            if (*argv[0] == '#')
            {
                int tmp;
                if (!sscanf(argv[0], "#%d", &tmp))
                {
                    helpmod_reply(sender, returntype, "Cannot delete censor entry: Bad index, integer expected");
                    return;
                }
                hcens = hcensor_get_by_index(hchan->censor, tmp);
            }
            else
                hcens = hcensor_get_by_pattern(hchan->censor, argv[0]);

            if (hcens == NULL)
                helpmod_reply(sender, returntype, "Cannot delete censor entry: Entry not found");
            else
            {
                hcensor_del(&hchan->censor, hcens);
                helpmod_reply(sender, returntype, "Censor entry deleted succesfully");
            }
        }
        else
            helpmod_reply(sender, returntype, "Unknown censor command %s", argv[0]);
    }
}

static void helpmod_cmd_chanconf (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    int old_flags=0;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot change or view channel configuration: Channel not specified or found");
        return;
    }

    if (argc == 0) /* view, spammy but with pretty formatting  */
    {
        int i;
        helpmod_reply(sender, returntype, "Channel configuration for %s:", hchan->real_channel->index->name->content);

        for (i=0;i<=HCHANNEL_CONF_COUNT;i++)
            helpmod_reply(sender, returntype, "(%02d) %-32s : %s", i, hchannel_get_sname(i), hchannel_get_state(hchan, 1 << i));
    }
    else /* change */
    {
        int i, tmp, type;
        old_flags = hchan->flags;

        if (argc > H_CMD_MAX_ARGS)
            argc = H_CMD_MAX_ARGS;

        for (i=0;i<argc;i++)
        {
            if (*argv[i] == '+')
            {
                type = 1;
                argv[i]++;
            }
            else if (*argv[i] == '-')
            {
                type = -1;
                argv[i]++;
            }
            else
                type = 0;

            if (!sscanf(argv[i], "%d", &tmp) || (tmp < 0) || (tmp > HCHANNEL_CONF_COUNT))
            {
                helpmod_reply(sender, returntype, "Cannot change channel configuration: Expected integer between [0, %d]", HCHANNEL_CONF_COUNT);
                continue;
            }

            switch (type)
            {
            case -1: /* unset */
                hchan->flags &= ~(1 << tmp);
                helpmod_reply(sender, returntype, "Channel configuration for %s changed: %s set to %s",hchannel_get_name(hchan), hchannel_get_sname(tmp), hchannel_get_state(hchan, 1 << tmp));
                break;
            case 0: /* view */
                helpmod_reply(sender, returntype, "(%02d) %-32s : %s", tmp, hchannel_get_sname(tmp), hchannel_get_state(hchan, 1 << tmp));
                break;
            case 1:  /* set */
                hchan->flags |= (1 << tmp);
                helpmod_reply(sender, returntype, "Channel configuration for %s changed: %s set to %s", hchannel_get_name(hchan), hchannel_get_sname(tmp), hchannel_get_state(hchan, 1 << tmp));
                break;
            }
        }
        hchannel_conf_change(hchan, old_flags);
    }
}

static void helpmod_cmd_acconf (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    haccount *hacc = sender->account;

    if (hacc == NULL)
    {
        helpmod_reply(sender, returntype, "Account configuration impossible: You do not have an account");
        return;
    }

    if (argc == 0) /* view, spammy but with pretty formatting  */
    {
        int i;
        helpmod_reply(sender, returntype, "Account configuration for %s:", hacc->name->content);

        for (i=0;i<=HACCOUNT_CONF_COUNT;i++)
            helpmod_reply(sender, returntype, "(%02d) %-35s : %s", i, haccount_get_sname(i), haccount_get_state(hacc, 1 << i));
    }
    else /* change */
    {
        int i, tmp, type;

        if (argc > H_CMD_MAX_ARGS)
            argc = H_CMD_MAX_ARGS;

        for (i=0;i<argc;i++)
        {
            if (*argv[i] == '+')
            {
                type = 1;
                argv[i]++;
            }
            else if (*argv[i] == '-')
            {
                type = -1;
                argv[i]++;
            }
            else
                type = 0;

            if (!sscanf(argv[i], "%d", &tmp) || (tmp < 0) || (tmp > HACCOUNT_CONF_COUNT))
            {
                helpmod_reply(sender, returntype, "Cannot change account configuration: Expected integer between [0, %d]", HACCOUNT_CONF_COUNT);
                continue;
            }

            switch (type)
            {
            case -1: /* unset */
                hacc->flags &= ~(1 << tmp);
                helpmod_reply(sender, returntype, "Account configuration for %s changed: %s set to %s",hacc->name->content, haccount_get_sname(tmp), haccount_get_state(hacc, 1 << tmp));
                break;
            case 0: /* view */
                helpmod_reply(sender, returntype, "(%02d) %-35s : %s", tmp, haccount_get_sname(tmp), haccount_get_state(hacc, 1 << tmp));
                break;
            case 1:  /* set */
                hacc->flags |= (1 << tmp);
                helpmod_reply(sender, returntype, "Account configuration for %s changed: %s set to %s", hacc->name->content, haccount_get_sname(tmp), haccount_get_state(hacc, 1 << tmp));
                break;
            }
        }
    }
}

static void helpmod_cmd_welcome (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot change or view the welcome message: Channel not specified or found");
        return;
    }

    if (argc == 0) /* view  */
    {
        helpmod_reply(sender, returntype, "Welcome message for channel %s (%s): %s", hchan->real_channel->index->name->content, hchannel_get_state(hchan, H_WELCOME), hchan->welcome);
    }
    else
    {
        strlcpy(hchan->welcome, ostr, HCHANNEL_WELCOME_LEN);
        helpmod_reply(sender, returntype, "Welcome message for channel %s (%s) is now: %s", hchan->real_channel->index->name->content, hchannel_get_state(hchan, H_WELCOME), hchan->welcome);
    }
}

static void helpmod_list_aliases(huser *sender, channel *returntype, char *buf, int *p_i, alias_tree node)
{
    if (*p_i > 256)
    {
        helpmod_reply(sender, returntype, "%s", buf);
        *p_i = 0;
    }
    if (!node)
        return;
    sprintf(buf+*p_i,"%.200s ",node->name->content);
    *p_i+=(1+strlen(node->name->content));
    helpmod_list_aliases(sender, returntype, buf, p_i, node->left);
    helpmod_list_aliases(sender, returntype, buf, p_i, node->right);
}

static void helpmod_cmd_aliases (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    char buf[512];
    int i = 0;
    helpmod_list_aliases(sender, returntype, buf, &i, aliases);
    if (i)
	helpmod_reply(sender, returntype, "%s", buf);
}

static void helpmod_cmd_showcommands (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    int level = H_PEON, previous_level = H_PEON;
    hcommand *tmp;

    if (!(argc && (sscanf(argv[0], "%d", &level)) && (level >= 0) && (level <= huser_get_level(sender))))
        level = huser_get_level(sender);

    helpmod_reply(sender, returntype, "HelpMod %s commands for userlevel %s:", HELPMOD_VERSION, hlevel_name(level));

    hcommand_list(H_NONE);

    while ((tmp = hcommand_list(level)) != NULL)
    {
	if (tmp->level > previous_level)
	{
	    helpmod_reply(sender, returntype, "--- Additional commands for userlevel %s ---", hlevel_name(tmp->level));
	    previous_level = tmp->level;
	}
	helpmod_reply(sender, returntype, "%-16s %s", tmp->name->content, tmp->help->content);
    }

    return;
}

static void helpmod_cmd_lamercontrol (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hlc_profile *ptr;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL) /* list profiles */
    {
        helpmod_reply(sender, returntype, "Following lamercontrol profiles are available:");
        for (ptr = hlc_profiles;ptr;ptr = ptr->next)
            helpmod_reply(sender, returntype, "%s", ptr->name->content);
        return;
    }
    if (argc == 0)
    {
        if (hchan->lc_profile == NULL)
            helpmod_reply(sender, returntype, "Channel %s has no lamercontrol profile set", hchannel_get_name(hchan));
        else
            helpmod_reply(sender, returntype, "Channel %s is using lamercontrol profile %s (%s)", hchannel_get_name(hchan), hchan->lc_profile->name->content, hchannel_get_state(hchan, H_LAMER_CONTROL));
        return;
    }
    if (!ci_strcmp(argv[0], "list"))
    {
        helpmod_reply(sender, returntype, "Following lamercontrol profiles are available:");
        for (ptr = hlc_profiles;ptr;ptr = ptr->next)
            helpmod_reply(sender, returntype, "%s", ptr->name->content);
        return;
    }
    ptr = hlc_get(argv[0]);
    if (ptr == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot set the lamercontrol profile: Profile %s does not exist:", argv[0]);
        return;
    }
    else
    {
        hchan->lc_profile = ptr;
        helpmod_reply(sender, returntype, "Lamercontrol profile %s set to channel %s succesfully",ptr->name->content,hchannel_get_name(hchan));
    }

}

static void helpmod_cmd_term_find_general (huser *sender, channel* returntype, int type, char* ostr, int argc, char *argv[])
{
    hterm *htrm;
    hterm *source;
    hchannel *hchan;
    huser *targets[6];
    int i,ntargets = 0;

    DEFINE_HCHANNEL;

    if (hchan == NULL)
        source = hterms;
    else
        source = hchan->channel_hterms;

    if (argc == 0)
    {
        if (type == H_TERM_PLUS)
            hqueue_advance(hchan, sender, 1);
        else
            helpmod_reply(sender, returntype, "Cannot find term: Term not specified");
        return;
    }
    htrm = hterm_get_and_find(source, argv[0]);
    if (htrm == NULL)
    {
        helpmod_reply(sender, returntype, "No term found matching '%s'", argv[0]);
        return;
    }
    if (returntype != NULL && huser_get_level(sender) >= H_TRIAL)
    {
        HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

        if (argc > 1)
        {
            SKIP_WORD;
            if (argc > H_CMD_MAX_ARGS)
                argc = H_CMD_MAX_ARGS;
            for (i=0;i<argc;i++)
            {
                targets[ntargets] = huser_get(getnickbynick(argv[i]));
                if (targets[ntargets] == NULL)
                {
                    helpmod_reply(sender, returntype, "Error using ?: User %s not found", argv[i]);
                    continue;
                }
                else
                    ntargets++;
            }
        }

        if (ntargets == 0)
            helpmod_message_channel_long(hchannel_get_by_channel(returntype), "(%s): %s", htrm->name->content, htrm->description->content);
        else
        {
            char buffer[256] = "";
            for (i=0;i<ntargets;i++)
            {
                if (i != 0)
		    strcat(buffer, " ");
		strcat(buffer, huser_get_nick(targets[i]));
            }

            helpmod_message_channel_long(hchannel_get_by_channel(returntype), "%s: (%s) %s", buffer, htrm->name->content, htrm->description->content);
            if (type != H_TERM_FIND)
            {
                if (hchan->flags & H_QUEUE)
                {
                    for (i=0;i<ntargets;i++)
                    {
                        huser_channel *huserchan = huser_on_channel(targets[i], hchan);
                        huserchan->flags |= HQUEUE_DONE;
                        if (huserchan->flags & HCUMODE_VOICE)
                            helpmod_channick_modes(targets[i], hchan, MC_DEVOICE, HLAZY);
                    }
                    if (type == H_TERM_PLUS)
                            hqueue_advance(hchan, sender, ntargets);
                }
            }
        }
    }
    else
        helpmod_reply(sender, returntype, "(%s): %s", htrm->name->content, htrm->description->content);
}

static void helpmod_cmd_term_find (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_cmd_term_find_general(sender, returntype, H_TERM_FIND, ostr, argc, argv);
}

static void helpmod_cmd_term_find_minus (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_cmd_term_find_general(sender, returntype, H_TERM_MINUS, ostr, argc, argv);
}

static void helpmod_cmd_term_find_plus (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_cmd_term_find_general(sender, returntype, H_TERM_PLUS, ostr, argc, argv);
}

static void helpmod_cmd_klingon (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    huser *target = NULL;
    int rand_val;

    DEFINE_HCHANNEL;

    if (hchan == NULL)
    {
        rand_val = rand() % KLINGON_NTARGETED;
        helpmod_reply(sender, NULL, "%s: %s", huser_get_nick(sender), klingon_targeted[rand_val]);
        return;
    }

    if (argc)
        target = huser_get(getnickbynick(argv[0]));

    if (target)
    {
        rand_val = rand() % KLINGON_NTARGETED;
        helpmod_message_channel(hchan, "%s: %s", huser_get_nick(target), klingon_targeted[rand_val]);
    }
    else
    {
        rand_val = rand() % KLINGON_NGENERAL;
        helpmod_message_channel(hchan, "%s", klingon_general[rand_val]);
    }
}

void helpmod_cmd_term (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hterm *htrm;
    hterm **source;
    hchannel *hchan;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
        source = &hterms;
    else
        source = &hchan->channel_hterms;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot handle terms: Operation not specified");
        return;
    }
    if (!ci_strcmp(argv[0], "list"))
    {
        char buffer[512];
        int count = 0;
        char *pattern = "*";

        buffer[0] = '\0';
        htrm = *source;

        if (argc >= 2)
            pattern = argv[1];
        else
            pattern = "*";

        helpmod_reply(sender, returntype, "Terms matching pattern '%s'", pattern);
        for (;htrm;htrm = htrm->next)
        {
            if (strlen(buffer) >= 250)
            {
                helpmod_reply(sender, returntype, "%s", buffer);
                buffer[0] = '\0';
            }
            if (strregexp(htrm->description->content, pattern) || strregexp(htrm->name->content, pattern))
            {
                sprintf(buffer+strlen(buffer) /* :) */, "%s(%u) ", htrm->name->content, (unsigned int)strlen(htrm->description->content));
                count++;
            }
        }
        if  (buffer[0])
            helpmod_reply(sender, returntype, "%s", buffer);
        helpmod_reply(sender, returntype, "%d term%s match%s pattern '%s'", count, (count == 1)?"":"s", (count == 1)?"es":"", pattern);
    }
    else if (!ci_strcmp(argv[0], "listfull"))
    {
        htrm = *source;
        helpmod_reply(sender, returntype, "Following terms have been entered:");
        for (;htrm;htrm = htrm->next)
            helpmod_reply(sender, returntype, "(%s): %s", htrm->name->content, htrm->description->content);
    }
    else if (!ci_strcmp(argv[0], "get"))
    {
        if (argc < 2)
        {
            helpmod_reply(sender, returntype, "Cannot get term: Term not specified");
            return;
        }
        htrm = hterm_get(*source, argv[1]);
        if (htrm == NULL)
            helpmod_reply(sender, returntype, "Cannot get term: Term %s not found", argv[1]);
        else
            helpmod_reply(sender, returntype, "(%s): %s", htrm->name->content, htrm->description->content);
    }
    else if (!ci_strcmp(argv[0], "add"))
    {
        if (argc < 2)
            helpmod_reply(sender, returntype, "Cannot add term: Term name not specified");
        else if (argc < 3)
            helpmod_reply(sender, returntype, "Cannot add term: Term description not specified");
        else if ((htrm = hterm_get(*source, argv[1])) != NULL)
            helpmod_reply(sender, returntype, "Cannot add term: Term %s is already added", argv[1]);
        else
        {
            char *name = argv[1], *description;
            SKIP_WORD; SKIP_WORD;
            description = ostr;
            htrm = hterm_add(source, name, description);
            helpmod_reply(sender, returntype, "Term %s added succesfully", name);
        }
    }
    else if (!ci_strcmp(argv[0], "del"))
    {
        int i;
        if (argc < 2)
        {
            helpmod_reply(sender, returntype, "Cannot delete term: Term name not specified");
            return;
        }
        if (argc > H_CMD_MAX_ARGS)
            argc = H_CMD_MAX_ARGS;
        for (i=1;i<argc;i++)
        {
            htrm = hterm_get(*source, argv[i]);
            if (htrm == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot delete term: Term %s not found", argv[i]);
                continue;
            }
            hterm_del(source != NULL?source:&hterms, htrm);
            helpmod_reply(sender, returntype, "Term %s deleted succesfully", argv[i]);
        }
    }
    else if (!ci_strcmp(argv[0], "find"))
    {
        if (argc < 2)
        {
            helpmod_reply(sender, returntype, "Cannot find term: Term name not specified");
            return;
        }
        htrm = hterm_get_and_find(*source, argv[1]);

        if (htrm == NULL)
        {
            helpmod_reply(sender, returntype, "No term found matching '%s'", argv[1]);
            return;
        }
        if (returntype != NULL && huser_get_level(sender) >= H_TRIAL)
            helpmod_message_channel(hchannel_get_by_channel(returntype), "(%s): %s", htrm->name->content, htrm->description->content);
        else
            helpmod_reply(sender, returntype, "(%s): %s", htrm->name->content, htrm->description->content);
    }
    else
    {
        helpmod_reply(sender, returntype, "Cannot handle terms: Operation not specified");
    }
}

static void helpmod_cmd_queue (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    int operation;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot handle queue: Operation not specified");
        return;
    }

    if (!ci_strcmp(argv[0], "done"))
        operation = HQ_DONE;
    else if (!ci_strcmp(argv[0], "next"))
        operation = HQ_NEXT;
    else if (!ci_strcmp(argv[0], "on"))
        operation = HQ_ON;
    else if (!ci_strcmp(argv[0], "off"))
        operation = HQ_OFF;
    else if (!ci_strcmp(argv[0], "maintain"))
        operation = HQ_MAINTAIN;
    else if (!ci_strcmp(argv[0], "list"))
        operation = HQ_LIST;
    else if (!ci_strcmp(argv[0], "summary"))
        operation = HQ_SUMMARY;
    else if (!ci_strcmp(argv[0], "reset"))
        operation = HQ_RESET;
    else
        operation = HQ_NONE;

    SKIP_WORD;

    helpmod_queue_handler(sender, returntype, hchan, operation, ostr, argc, argv);
}

static void helpmod_cmd_done (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    DEFINE_HCHANNEL;
    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);
    helpmod_queue_handler(sender, returntype, hchan, HQ_DONE, ostr, argc, argv);
}

static void helpmod_cmd_next (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    DEFINE_HCHANNEL;
    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);
    helpmod_queue_handler(sender, returntype, hchan, HQ_NEXT, ostr, argc, argv);
}

static void helpmod_cmd_enqueue (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    DEFINE_HCHANNEL;
    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);
    helpmod_queue_handler(sender, returntype, hchan, HQ_ON, ostr, argc, argv);
}

static void helpmod_cmd_dequeue (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    DEFINE_HCHANNEL;
    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);
    helpmod_queue_handler(sender, returntype, hchan, HQ_OFF, ostr, argc, argv);
}

static void helpmod_cmd_autoqueue (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    DEFINE_HCHANNEL;
    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);
    helpmod_queue_handler(sender, returntype, hchan, HQ_MAINTAIN, ostr, argc, argv);
}

static void helpmod_cmd_dnmo (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    int i;
    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot correct the luser: User not defined");
        return;
    }
    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;
    for (i=0;i<argc;i++)
    {
        huser *husr = huser_get(getnickbynick(argv[i]));
        if (husr == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot correct the luser: User %s not found", argv[i]);
            continue;
	}
	if (huser_get_level(husr) > H_PEON)
	{
	    helpmod_reply(sender, returntype, "Cannot correct the luser: User %s is not a peon", argv[i]);
            continue;
	}
        /*
        if (!hchannels_on_queue(husr) && !hchannels_on_desk(husr))
        {
            helpmod_reply(sender, returntype, "Cannot correct the luser: User %s is not in any queue", argv[i]);
            continue;
        }
        */
        hchannels_dnmo(husr);

        helpmod_reply(husr, NULL, "You contacted a channel operator without permission. This is a violation of the channel rules. As a result you have been set to the last queue position");
        helpmod_reply(sender, returntype, "User %s has been informed of his mistake", argv[i]);
    }
    { /* fix the autoqueue for the channel(s) */
        hchannel *hchan;
        for (hchan = hchannels;hchan;hchan = hchan->next)
            hqueue_handle_queue(hchan, sender);
    }
}

static void helpmod_cmd_ban (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    if (argc == 0 || !ci_strcmp(argv[0], "list"))
    {
        hban *ptr = hbans;
        char *pattern;
        int count = 0;

        if (argc <= 1)
            pattern = "*";
        else
            pattern = argv[1];

        helpmod_reply(sender, returntype, "Global bans matching pattern %s", pattern);

        for (;ptr;ptr = ptr->next)
            if (strregexp(bantostring(ptr->real_ban), pattern))
            {
                helpmod_reply(sender, returntype, "%-64s %-20s %s", bantostring(ptr->real_ban), helpmod_strtime(ptr->expiration - time(NULL)), ptr->reason?ptr->reason->content:"");
                count++;
            }

        helpmod_reply(sender, returntype, "%d Global bans match pattern %s", count, pattern);
    }
    else if (!ci_strcmp(argv[0], "add"))
    {
        int duration = 4 * HDEF_h;
        char *reason = "Banned";
        char *target = argv[1];

        if (argc == 1)
        {
            helpmod_reply(sender, returntype, "Cannot add global ban: Target hostmask not defined");
            return;
        }
        if (argc >= 3)
        {
            if ((duration = helpmod_read_strtime(argv[2])) < 0)
            {
                helpmod_reply(sender, returntype, "Cannot add global ban: Invalid time %s", argv[2]);
                return;
            }
        }
        if (argc >= 4)
        {
            SKIP_WORD;
            SKIP_WORD;
            SKIP_WORD;

            reason = ostr;
        }
        hban_add(target, reason, time(NULL) + duration, 0);
        helpmod_reply(sender, returntype, "Added ban for %s (%s), expires in %s", target, reason, helpmod_strtime(duration));
    }
    else if (!ci_strcmp(argv[0], "del"))
    {
        int i;
        if (argc == 1)
        {
            helpmod_reply(sender, returntype, "Cannot remove global ban: Target hostmask not defined");
            return;
        }
        if (argc > H_CMD_MAX_ARGS)
            argc = H_CMD_MAX_ARGS;
        for (i=1;i<argc;i++)
        {
            hban *ptr = hban_get(argv[i]);
            if (ptr == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot remove global ban: Hostmask %s is not banned", argv[i]);
                continue;
            }
            helpmod_reply(sender, returntype, "Global ban for hostmask %s removed", argv[i]);
            hban_del(ptr,0);
        }
    }
    else
    {
        helpmod_reply(sender, returntype, "Cannot handle global bans: Unknown operation %s", argv[0]);
        return;
    }
}

static void helpmod_cmd_chanban (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    int i;

    DEFINE_HCHANNEL;

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot handle channel bans: Channel not defined or not found");
        return;
    }

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);
/*
    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot handle channel bans: Operation not defined");
        return;
    }
*/
    if (argc == 0 || !ci_strcmp(argv[0], "list"))
    {
        char *pattern, *cban;
        chanban *ptr = hchan->real_channel->bans;
        int count = 0;

        if (argc <=  1)
            pattern = "*";
        else
            pattern = argv[1];

        helpmod_reply(sender, returntype, "Bans matching pattern %s for channel %s", pattern, hchannel_get_name(hchan));
        for (;ptr;ptr = ptr->next)
            if (strregexp((cban = bantostring(ptr)), pattern))
            {
                count++;
                if (hchanban_get(hchan,cban) != NULL)
		    helpmod_reply(sender, returntype, "%s Expires in %s", bantostring(ptr), helpmod_strtime(hchanban_get(hchan, cban)->expiration - time(NULL)));
                else
                    helpmod_reply(sender, returntype, "%s", bantostring(ptr));
            }
        helpmod_reply(sender, returntype, "%d bans match pattern %s on channel %s", count, pattern, hchannel_get_name(hchan));
    }
    else if (!ci_strcmp(argv[0], "add"))
    {
        if (argc == 1)
        {
            helpmod_reply(sender, returntype, "Cannot add channel bans: Pattern not defined");
            return;
        }
        if (argc > H_CMD_MAX_ARGS)
            argc = H_CMD_MAX_ARGS;
        for (i=1;i<argc;i++)
        {
            /* POSSIBLE BUG ? */
            helpmod_setban(hchan, argv[i], H_ETERNITY, MCB_ADD, HLAZY);
            helpmod_reply(sender, returntype, "Added ban %s to channel %s", argv[i], hchannel_get_name(hchan));
        }
    }
    else if (!ci_strcmp(argv[0], "del"))
    {
        if (argc == 1)
        {
            helpmod_reply(sender, returntype, "Cannot remove channel bans: Pattern not defined");
            return;
        }
        if (argc > H_CMD_MAX_ARGS)
            argc = H_CMD_MAX_ARGS;
        for (i=1;i<argc;i++)
        {
            chanban *ptr = hchan->real_channel->bans;
            for (;ptr;ptr = ptr->next)
                if (strregexp(bantostring(ptr), argv[i]))
                {
                    helpmod_setban(hchan, argv[i], 0, MCB_DEL, HLAZY);
                    helpmod_reply(sender, returntype, "Channel ban %s removed from channel %s", argv[i], hchannel_get_name(hchan));
                    break;
                }
            if (ptr == NULL)
                helpmod_reply(sender, returntype, "Cannot remove channel ban: Pattern %s not banned on channel %s", argv[i], hchannel_get_name(hchan));
        }
    }
    else
    {
        helpmod_reply(sender, returntype, "Cannot handle channel bans: Unknown operation %s", argv[0]);
        return;
    }
}

static void helpmod_cmd_idlekick (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    int tmp;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot handle the idlekick: Channel not defined or not found");
        return;
    }
    if (argc == 0) /* view */
    {
        helpmod_reply(sender, returntype, "Idlekick for channel %s is set to %s", hchannel_get_name(hchan), helpmod_strtime(hchan->max_idle));
        return;
    }
    else if ((tmp = helpmod_read_strtime(argv[0])) < 0 || tmp < HDEF_m || tmp > HDEF_w)
    {
        helpmod_reply(sender, returntype, "Cannot set the idlekick: Invalid time given '%s'", argv[0]);
        return;
    }
    else /* set it ! */
    {
        hchan->max_idle = tmp;
        helpmod_reply(sender, returntype, "Idlekick for channel %s set to %s succesfully", hchannel_get_name(hchan), helpmod_strtime(hchan->max_idle));
    }
}

static void helpmod_cmd_topic (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot handle the topic: Channel not defined or not found");
        return;
    }
    if (!(hchan -> flags & H_HANDLE_TOPIC))
        helpmod_reply(sender, returntype, "Note: I'm not set to handle the topic on channel %s", hchannel_get_name(hchan));

    if (argc == 0) /* check the topic */
    {
        helpmod_reply(sender, returntype, "Topic of channel %s is currently: %s", hchannel_get_name(hchan), htopic_construct(hchan->topic));
        return;
    }
    if (!ci_strcmp(argv[0], "erase"))
    {
        htopic_del_all(&hchan->topic);
        hchannel_set_topic(hchan);
        helpmod_reply(sender, returntype, "Topic of channel %s erased", hchannel_get_name(hchan));
    }
    else if (!ci_strcmp(argv[0], "refresh"))
    {
        hchannel_set_topic(hchan);
        helpmod_reply(sender, returntype, "Topic of channel %s refreshed", hchannel_get_name(hchan));
    }
    else if (!ci_strcmp(argv[0], "add"))
    {
        int pos;
        SKIP_WORD;
        if (argc == 0)
        {
            helpmod_reply(sender, returntype, "Cannot handle the topic of channel %s: Additional arguments required", hchannel_get_name(hchan));
            return;
        }
        if (sscanf(argv[0], "%d", &pos) && pos >= 0)
        {
            SKIP_WORD;
        }
        else
            pos = 0;
        if (argc == 0) /* lame repeat :( */
        {
            helpmod_reply(sender, returntype, "Cannot handle the topic of channel %s: Additional arguments required", hchannel_get_name(hchan));
            return;
        }
        if (htopic_len(hchan->topic) + strlen(ostr) + 3 > TOPICLEN)
        {
            helpmod_reply(sender, returntype, "Cannot add to the topic of channel %s: Maximum topic length exceeded", hchannel_get_name(hchan));
            return;
        }

        htopic_add(&hchan->topic, ostr, pos);
        hchannel_set_topic(hchan);
        helpmod_reply(sender, returntype, "Added '%s' to the topic of channel %s", ostr, hchannel_get_name(hchan));
    }
    else if (!ci_strcmp(argv[0], "del"))
    {
        int pos;
        if (argc == 1)
        {
            helpmod_reply(sender, returntype, "Cannot handle the topic of channel %s: Additional arguments required", hchannel_get_name(hchan));
            return;
        }
        SKIP_WORD;
        if (!sscanf(argv[0], "%d", &pos) || pos < 0)
        {
            helpmod_reply(sender, returntype, "Cannot handle the topic of channel %s: Expected positive integer", hchannel_get_name(hchan));
            return;
        }
        if (htopic_count(hchan->topic) < pos)
        {
            helpmod_reply(sender, returntype, "Cannot delete from the topic of channel %s: No such topic element %d", hchannel_get_name(hchan), pos);
            return;
        }
        htopic_del(&hchan->topic, htopic_get(hchan->topic, pos));
        hchannel_set_topic(hchan);
    }
    else if (!ci_strcmp(argv[0], "set"))
    {
        if (argc == 1)
        {
            helpmod_reply(sender, returntype, "Cannot handle the topic of channel %s: Additional arguments required", hchannel_get_name(hchan));
            return;
        }
        SKIP_WORD;
        htopic_del_all(&hchan->topic);
        htopic_add(&hchan->topic, ostr, 0);
        hchannel_set_topic(hchan);
    }
    else
        helpmod_reply(sender, returntype, "Cannot handle the topic of channel %s: Unknown operation %s", hchannel_get_name(hchan), argv[0]);
}

static void helpmod_cmd_out (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    huser *husr;
    int i;
    char *reason = "Banned";
    huser *targets[H_CMD_MAX_ARGS];
    int ntargets = 0;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot get rid of the user: User not specified");
        return;
    }

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;
    for (i=0;i<argc;i++)
    {
	if (argv[i][0] == ':')
	{
	    if (i == 0)
	    {
		helpmod_reply(sender, returntype, "Cannot get rid of users: No users specified");
		return;
	    }
	    while (i--)
	    {
		SKIP_WORD;
	    }
	    reason = ostr + 1;

	    if (!strncmp(reason, "?? ", 3))
	    { /* obtain reason from hterms */
		hchannel *hchan = NULL;
                hterm *new_reason;
		if (returntype != NULL)
		{ /* if hchan is NULL here then everything is broken already */
		    hchan = hchannel_get_by_channel(returntype);
		    new_reason = hterm_get_and_find(hchan->channel_hterms, reason + 3);
		}
		else
		    new_reason = hterm_get_and_find(hterms, reason + 3);

		if (new_reason != NULL)
		    reason = new_reason->description->content;
	    }
	    break;
	}

	husr = huser_get(getnickbynick(argv[i]));
        if (husr == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot get rid of the user: User %s not found", argv[i]);
            continue;
        }
        if (huser_get_level(husr) > H_PEON)
        {
            helpmod_reply(sender, returntype, "Cannot get rid of the user: User %s is not a peon", huser_get_nick(husr));
            continue;
	}
        targets[ntargets++] = husr;
    }

    for (i=0;i<ntargets;i++)
    {
	const char *banmask = hban_ban_string(targets[i]->real_user, HBAN_HOST);

	hban_add(banmask, reason, time(NULL) + HCMD_OUT_DEFAULT, HLAZY);

	helpmod_reply(sender, returntype, "User %s is now gone", huser_get_nick(targets[i]));
    }
    hcommit_modes();
}

static void helpmod_cmd_everyoneout (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hchannel_user **hchanuser;
    char *reason = "clearing channel";
    int autoqueue_tmp = -1;
    enum
    {
	HELPMOD_KICKMODE_ALL,
        HELPMOD_KICKMODE_UNAUTHED
    } kickmode = HELPMOD_KICKMODE_ALL;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot clear channel: Channel not defined or not found");
        return;
    }

    if (argc)
    {
	if (!ci_strcmp(argv[0], "all"))
	{
	    kickmode = HELPMOD_KICKMODE_ALL;
	    SKIP_WORD;
	}
	else if (!ci_strcmp(argv[0], "unauthed"))
	{
            kickmode = HELPMOD_KICKMODE_UNAUTHED;
            SKIP_WORD;
	}
	if (ostr[0] == ':')
	    ostr++;
	reason = ostr;
    }

    hchan->flags |= H_MAINTAIN_I;
    hchannel_mode_check(hchan);

    hchanuser = &hchan->channel_users;

    if ((hchan->flags & H_QUEUE) && (hchan->flags & H_QUEUE_MAINTAIN))
    {
        autoqueue_tmp = hchan->autoqueue;
        hchan->autoqueue = 0;
    }

    while (*hchanuser)
    {
	if (huser_get_level((*hchanuser)->husr) < H_TRIAL)
	    if (kickmode == HELPMOD_KICKMODE_ALL || (kickmode == HELPMOD_KICKMODE_UNAUTHED && !IsAccount((*hchanuser)->husr->real_user)))
	    {
		helpmod_kick(hchan, (*hchanuser)->husr, "%s", reason);
		continue;
	    }
	hchanuser = &(*hchanuser)->next;
    }

    if (autoqueue_tmp > 0)
        hchan->autoqueue = autoqueue_tmp;

    helpmod_reply(sender, returntype, "Channel %s has been cleared of normal users", hchannel_get_name(hchan));
}

static void helpmod_cmd_kick (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    int i;
    huser *husr, *targets[H_CMD_MAX_ARGS];
    int ntargets = 0;
    char *reason = "out";
    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot kick the user: Channel not defined or not found");
	return;
    }

    if (argc == 0)
    {
	helpmod_reply(sender, returntype, "Cannot kick users: No users specified");
	return;
    }

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;
    for (i=0;i<argc;i++)
    {
        if (*argv[i] == ':')
        {
	    if (i == 0)
	    {
		helpmod_reply(sender, returntype, "Cannot kick the user: No users defined");
		return;
	    }
	    while (i--)
            {
                SKIP_WORD;
            }
            reason = ostr+1;
            break;
        }
        husr = huser_get(getnickbynick(argv[i]));
        if (husr == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot kick the user: User %s not found", argv[i]);
            continue;
        }
        if (huser_on_channel(husr, hchan) == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot kick the user: User %s is not on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
            continue;
        }
        if (huser_get_level(husr) > H_PEON)
        {
            helpmod_reply(sender, returntype, "Cannot kick the user: User %s is not a peon", huser_get_nick(husr));
            continue;
        }
        targets[ntargets++] = husr;
    }

    for (i=0;i<ntargets;i++)
        helpmod_kick(hchan, targets[i], "%s", reason);
}

static void helpmod_cmd_stats (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    haccount *target = sender->account;
    hstat_account *ptr;
    hstat_account_entry *stat_entry;
    int days = 1, weeks = 0;
    int type = HSTAT_ACCOUNT_SHORT;

    time_t timer = time(NULL);
    struct tm *tstruct = localtime(&timer);

    int i = 0;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot show user statistics: Channel not defined or not found");
        return;
    }

    if (argc > 0 && huser_get_level(sender) == H_ADMIN)
    { /* not very elegant */
        if (argv[0][0] == '#') /* account */
        {
            target = haccount_get_by_name(argv[0]+1);
            if (target == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot show user statistics: Account %s not found", argv[0]);
                return;
            }
            SKIP_WORD;
        }
    }

    if (target == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot show user statistics: You do not have an account");
        return;
    }
    
    if (argc > 0)
    {
        if (!ci_strcmp(argv[0], "short") || !ci_strcmp(argv[0], "s"))
        {
            type = HSTAT_ACCOUNT_SHORT;
            SKIP_WORD;
        }
        else if (!ci_strcmp(argv[0], "long") || !ci_strcmp(argv[0], "l"))
        {
            type = HSTAT_ACCOUNT_LONG;
            SKIP_WORD;
        }
    }

    if (argc > 0)
        if (sscanf(argv[0], "%d", &days))
        {
            if (days < 0 || days > 7)
            {
                helpmod_reply(sender, returntype, "Cannot show user statistics: Expected integer between [0, 7]");
                return;
            }
            else
            {
                SKIP_WORD;
            }
        }

    if (argc > 0)
        if (sscanf(argv[0], "%d", &weeks))
        {
            if (weeks < 0 || weeks > 10)
            {
                helpmod_reply(sender, returntype, "Cannot show user statistics: Expected integer between [0, 10]");
                return;
            }
            else
            {
                SKIP_WORD;
            }
        }

    for (ptr = target->stats;ptr;ptr = ptr->next)
        if (ptr->hchan == hchan)
            break;

    if (ptr == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot show user statistics: User %s has no statistics for channel %s", target->name->content, hchannel_get_name(hchan));
        return;
    }

    if (!days && !weeks)
        return;

    helpmod_reply(sender, returntype, "Statistics for user %s on channel %s", target->name->content, hchannel_get_name(hchan));

    if (days)
    {
        helpmod_reply(sender, returntype, "Last %d day%s", days, (days==1)?"":"s");
        helpmod_reply(sender, returntype, "%s", hstat_header(type));
        for (i=0;i<days;i++)
        {
            stat_entry = &ptr->week[(tstruct->tm_wday - i + 7)  % 7];
            helpmod_reply(sender, returntype, "%s", hstat_account_print(stat_entry, type));
        }
    }

    if (weeks)
    {
        helpmod_reply(sender, returntype, "Last %d week%s", weeks, (weeks==1)?"":"s");
        helpmod_reply(sender, returntype, "%s", hstat_header(type));
        for (i=0;i<weeks;i++)
        {
            stat_entry = &ptr->longterm[(hstat_week() - i + 10) % 10];
            helpmod_reply(sender, returntype, "%s", hstat_account_print(stat_entry, type));
        }
    }
}

static void helpmod_cmd_chanstats (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hstat_channel *channel_stats;
    hstat_channel_entry *stat_entry;

    time_t timer = time(NULL);
    struct tm *tstruct = localtime(&timer);

    int days=1;
    int type = HSTAT_CHANNEL_SHORT;
    int weeks=0;

    int i = 7;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot show channel statistics: Channel not defined or not found");
        return;
    }

    if (argc > 0)
    {
        if (!ci_strcmp(argv[0], "short") || !ci_strcmp(argv[0], "s"))
        {
            type = HSTAT_CHANNEL_SHORT;
            SKIP_WORD;
        }
        else if (!ci_strcmp(argv[0], "long") || !ci_strcmp(argv[0], "l"))
        {
            type = HSTAT_CHANNEL_LONG;
            SKIP_WORD;
        }
    }

    if (argc > 0)
        if (sscanf(argv[0], "%d", &days))
        {
            if (days < 0 || days > 7)
            {
                helpmod_reply(sender, returntype, "Cannot show channel statistics: Expected integer between [0, 7]");
                return;
            }
            else
            {
                SKIP_WORD;
            }
        }

    if (argc > 0)
        if (sscanf(argv[0], "%d", &weeks))
        {
            if (weeks < 0 || weeks > 10)
            {
                helpmod_reply(sender, returntype, "Cannot show channel statistics: Expected integer between [0, 10]");
                return;
            }
            else
            {
                SKIP_WORD;
            }
        }

    channel_stats = hchan->stats;

    if (!days && !weeks)
        return;

    helpmod_reply(sender, returntype, "Statistics for channel %s", hchannel_get_name(hchan));

    if (days)
    {
        helpmod_reply(sender, returntype, "Last %d day%s", days, (days==1)?"":"s");
        helpmod_reply(sender, returntype, "%s", hstat_header(type));
        for (i=0;i<days;i++) /* latest week */ 
        {
            stat_entry = &hchan->stats->week[(tstruct->tm_wday - i + 7)  % 7];
            helpmod_reply(sender, returntype, "%s", hstat_channel_print(stat_entry, type));
        }
    }

    if (weeks)
    {
        helpmod_reply(sender, returntype, "Last %d week%s", weeks, (weeks==1)?"":"s");
        helpmod_reply(sender, returntype, "%s", hstat_header(type));
        for (i=0;i<weeks;i++) /* latest weeks */
        {
            stat_entry = &hchan->stats->longterm[(hstat_week() - i + 10)  % 10];
	    helpmod_reply(sender, returntype, "%s", hstat_channel_print(stat_entry, type));
        }
    }
}

static void helpmod_cmd_activestaff (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hstat_accounts_array arr;
    hlevel lvl = H_OPER;
    int listtype = 0;
    int i;

    DEFINE_HCHANNEL;

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot list active staff: Channel not specified or not found");
        return;
    }

    if (argc == 1)
    {
        if (!ci_strcmp(argv[0], "opers") || !ci_strcmp(argv[0], "o"))
        {
            lvl = H_OPER;
            SKIP_WORD;
        }
        else if (!ci_strcmp(argv[0], "staff") || !ci_strcmp(argv[0], "s"))
        {
            lvl = H_STAFF;
            SKIP_WORD;
	}
	else if (!ci_strcmp(argv[0], "staff") || !ci_strcmp(argv[0], "s"))
	{
            lvl = H_ANY;
            SKIP_WORD;
	}
    }

    if (argc == 1)
    {
        if (!ci_strcmp(argv[0], "active") || !ci_strcmp(argv[0], "a"))
        {
            listtype = 0;
            SKIP_WORD;
        }
        else if (!ci_strcmp(argv[0], "inactive") || !ci_strcmp(argv[0], "i"))
        {
            listtype = 1;
            SKIP_WORD;
        }
    }

    arr = create_hstat_account_array(hchan, lvl, HSTAT_ACCOUNT_ARRAY_TOP10);

    helpmod_reply(sender, returntype, "%s %ss for channel %s", listtype?"Inactive":"Active", hlevel_name(lvl), hchannel_get_name(hchan));
    switch (listtype)
    {
    case 0:
        for (i=0;i < arr.arrlen;i++)
            if (arr.array[i].prime_time_spent > H_ACTIVE_LIMIT)
                helpmod_reply(sender, returntype, "#%-2d %-20s %-20s %-20s", i+1,((haccount*)(arr.array[i].owner))->name->content, helpmod_strtime(arr.array[i].prime_time_spent), helpmod_strtime(arr.array[i].time_spent));
        break;
    case 1:
        for (i=arr.arrlen-1;i >= 0;i--)
            if (arr.array[i].prime_time_spent < H_ACTIVE_LIMIT)
                helpmod_reply(sender, returntype, "#%-2d %-20s %-20s %-20s", (arr.arrlen - i),((haccount*)(arr.array[i].owner))->name->content, helpmod_strtime(arr.array[i].prime_time_spent), helpmod_strtime(arr.array[i].time_spent));
        break;
    }

    free(arr.array);
}

static void helpmod_cmd_top10 (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hstat_accounts_array arr;
    hlevel lvl = H_OPER;
    int i, top_n = 10;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot list channel Top10: Channel not specified or not found");
        return;
    }

    if (argc >= 1)
    {
        if (!ci_strcmp(argv[0], "opers") || !ci_strcmp(argv[0], "o"))
            lvl = H_OPER;
	else if (!ci_strcmp(argv[0], "staff") || !ci_strcmp(argv[0], "s"))
	    lvl = H_STAFF;
	else if (!ci_strcmp(argv[0], "all") || !ci_strcmp(argv[0], "a"))
            lvl = H_ANY;
    }
    if (argc == 2)
    {
        int tmp;
        if (sscanf(argv[1], "%d", &tmp) && (tmp >= 10) && (tmp <= 50))
            top_n = tmp;
    }

    arr = create_hstat_account_array(hchan, lvl, HSTAT_ACCOUNT_ARRAY_TOP10);

    helpmod_reply(sender, returntype, "Top%d most active %ss of channel %s", top_n, hlevel_name(lvl), hchannel_get_name(hchan));
    for (i=0;i < arr.arrlen && i < top_n;i++)
	helpmod_reply(sender, returntype, "#%-2d %-20s %-20s %-20s",i+1,((haccount*)(arr.array[i].owner))->name->content, helpmod_strtime(arr.array[i].prime_time_spent), helpmod_strtime(arr.array[i].time_spent));

    free(arr.array);
}

static void helpmod_cmd_report (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan, *target;
    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan != NULL && (argc == 2))
    {
        hchan = hchannel_get_by_name(argv[0]);
        SKIP_WORD;
    }

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot view or set channel reporting: Channel not defined or not found");
        return;
    }
    if (argc != 1)
    {
        if (hchan->report_to == NULL || !hchannel_is_valid(hchan->report_to))
            helpmod_reply(sender, returntype, "Channel %s is not reported anywhere (%s)", hchannel_get_name(hchan), hchannel_get_state(hchan, H_REPORT));
        else
            helpmod_reply(sender, returntype, "Channel %s is reported to channel %s (%s)", hchannel_get_name(hchan), hchannel_get_name(hchan->report_to), hchannel_get_state(hchan, H_REPORT));
        return;
    }
    if ((target = hchannel_get_by_name(argv[0])) == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot set channel reporting: Channel %s not found", argv[0]);
        return;
    }
    hchan->report_to = target;
    helpmod_reply(sender, returntype, "Channel %s is now reported to channel %s (%s)", hchannel_get_name(hchan), hchannel_get_name(hchan->report_to), hchannel_get_state(hchan, H_REPORT));
}

static void helpmod_cmd_mode(huser *sender, channel* returntype, int change, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    huser_channel *huserchan;
    huser *husr;
    int i;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot change mode: Channel not specified or not found");
        return;
    }

    if (argc==0) /* for a simple opme */
    {
        argc = 1;
        argv[0] = (char*)huser_get_nick(sender);
    }

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;

    for (i=0;i<argc;i++)
    {
        husr = huser_get(getnickbynick(argv[i]));
        if (husr == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot change mode: User %s not found", argv[i]);
            continue;
        }
        huserchan = huser_on_channel(husr, hchan);
        if (huserchan == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot change mode: User %s it not on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
            continue;
        }

        switch (change)
        {
        case H_CMD_OP:
            {
                int j;
                for (j=0;j < hchan->real_channel->users->hashsize;j++)
                    if (getnickbynumeric(hchan->real_channel->users->content[j]) == husr->real_user)
                        break;
                if ((huserchan->flags & HCUMODE_OP) && !(hchan->real_channel->users->content[j] & CUMODE_OP))
                {
                    huserchan->flags &= ~HCUMODE_OP;
                    Error("helpmod", ERR_ERROR, "userchannelmode inconsistency (+o when should be -o)");
                }
                if (huserchan->flags & HCUMODE_OP)
                {
                    helpmod_reply(sender, returntype, "Cannot change mode: User %s is already +o on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
                    continue;
                }
                if (huser_get_level(husr) < H_STAFF)
                {
                    helpmod_reply(sender, returntype, "Cannot change mode: User %s is not allowed to have +o on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
                    continue;
                }
                helpmod_channick_modes(husr, hchan, MC_OP, HLAZY);
            }
            break;
        case H_CMD_DEOP:
            {
                int j;
                for (j=0;j < hchan->real_channel->users->hashsize;j++)
                    if (getnickbynumeric(hchan->real_channel->users->content[j]) == husr->real_user)
                        break;
                if (!(huserchan->flags & HCUMODE_OP) && (hchan->real_channel->users->content[j] & CUMODE_OP))
                {
                    huserchan->flags |= HCUMODE_OP;
                    Error("helpmod", ERR_ERROR, "userchannelmode inconsistency (-o when should be +o)");
                }
                if (!(huserchan->flags & HCUMODE_OP))
                {
                    helpmod_reply(sender, returntype, "Cannot change mode: User %s is already -o on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
                    continue;
                }
                if (huser_get_level(husr) > huser_get_level(sender))
                {
                    helpmod_reply(sender, returntype, "Cannot change mode: User %s is meant to have +o on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
                    continue;
                }
                helpmod_channick_modes(husr, hchan, MC_DEOP, HLAZY);
            }
            break;
        case H_CMD_VOICE:
            {
                int j;
                for (j=0;j < hchan->real_channel->users->hashsize;j++)
                    if (getnickbynumeric(hchan->real_channel->users->content[j]) == husr->real_user)
                        break;
                if ((huserchan->flags & HCUMODE_VOICE) && !(hchan->real_channel->users->content[j] & CUMODE_VOICE))
                {
                    huserchan->flags &= ~HCUMODE_VOICE;
                    Error("helpmod", ERR_ERROR, "userchannelmode inconsistency (+v when should be -v)");
                }
                if (huserchan->flags & HCUMODE_VOICE)
                {
                    helpmod_reply(sender, returntype, "Cannot change mode: User %s is already +v on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
                    continue;
                }
                helpmod_channick_modes(husr, hchan, MC_VOICE, HLAZY);
            }
            break;
        case H_CMD_DEVOICE:
            {
                int j;
                for (j=0;j < hchan->real_channel->users->hashsize;j++)
                    if (getnickbynumeric(hchan->real_channel->users->content[j]) == husr->real_user)
                        break;
                if (!(huserchan->flags & HCUMODE_VOICE) && (hchan->real_channel->users->content[j] & CUMODE_VOICE))
                {
                    huserchan->flags |= HCUMODE_VOICE;
                    Error("helpmod", ERR_ERROR, "userchannelmode inconsistency (-v when should be +v)");
                }
                if (!(huserchan->flags & HCUMODE_VOICE))
                {
                    helpmod_reply(sender, returntype, "Cannot change mode: User %s is already -v on channel %s", huser_get_nick(husr), hchannel_get_name(hchan));
                    continue;
                }
                helpmod_channick_modes(husr, hchan, MC_DEVOICE, HLAZY);
            }
            break;
        }
    }
}

static void helpmod_cmd_op (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_mode(sender, returntype, H_CMD_OP, ostr, argc, argv); }
static void helpmod_cmd_deop (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_mode(sender, returntype, H_CMD_DEOP, ostr, argc, argv); }
static void helpmod_cmd_voice (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_mode(sender, returntype, H_CMD_VOICE, ostr, argc, argv); }
static void helpmod_cmd_devoice (huser *sender, channel* returntype, char* ostr, int argc, char *argv[]) { helpmod_cmd_mode(sender, returntype, H_CMD_DEVOICE, ostr, argc, argv); }

static void helpmod_cmd_invite (huser *sender, channel *returntype, char* arg, int argc, char *argv[])
{
    hchannel *hchan;
    int i;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot invite: Channel not defined or not found");
        return;
    }

    if (huser_get_level(sender) < H_STAFF)
    {
        hticket *htick;
        hchan = hchannel_get_by_name(argv[0]);
        if (hchan == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot invite: Unknown channel %s", argv[0]);
            return;
        }
        /* if tickets don't work, it's better that the user doesn't know that the channel really exists */
        if (!(hchan->flags & H_REQUIRE_TICKET))
        {
            helpmod_reply(sender, returntype, "Cannot invite: Unknown channel %s", argv[0]);
            return;
        }
        htick = hticket_get(huser_get_auth(sender), hchan);

        if (htick == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot invite: You do not have an invite ticket for channel %s", argv[0]);
            return;
        }

        if (nickbanned(sender->real_user, hchan->real_channel, 0))
        {
            helpmod_reply(sender, returntype, "Cannot invite: You are banned from channel %s", argv[0]);
            return;
        }

        helpmod_invite(hchan, sender);
        helpmod_reply(sender, returntype, "Invited you to channel %s", hchannel_get_name(hchan));
        return;
    }

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;

    for (i = 0;i < argc; i++)
    {
        hchan = hchannel_get_by_name(argv[i]);
        if (hchan == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot invite: Unknown channel %s", argv[i]);
            continue;
        }
	if (huser_on_channel(sender, hchan) != NULL)
        {
            helpmod_reply(sender, returntype, "Cannot invite: You are already on channel %s", hchannel_get_name(hchan));
            continue;
        }
	if (!hchannel_authority(hchan, sender))
	{
	    if (huser_get_level(sender) >= H_STAFF && (hchan->flags & H_REQUIRE_TICKET));
	    else
	    {
		helpmod_reply(sender, returntype, "Sorry, channel %s is oper only", hchannel_get_name(hchan));
		continue;
	    }
        }
        helpmod_invite(hchan, sender);
        helpmod_reply(sender, returntype, "Invited you to channel %s", hchannel_get_name(hchan));
    }
}

static void helpmod_cmd_ticket (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    int expiration = HTICKET_EXPIRATION_TIME;
    hchannel *hchan;
    huser *husr;
    hticket *htick;
    const char *message = NULL;

    if (argc < 1)
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: Channel not specified");
        return;
    }

    hchan = hchannel_get_by_name(argv[0]);
    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: Unknown channel %s", argv[0]);
        return;
    }
    if (!(hchan->flags & H_REQUIRE_TICKET))
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: Tickets are not enabled for channel %s", argv[0]);
        return;
    }
    if (argc < 2)
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: Target user not specified");
        return;
    }

    husr = huser_get(getnickbynick(argv[1]));
    if (husr == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: Unknown user %s", argv[1]);
        return;
    }
    if (!IsAccount(husr->real_user))
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: User %s is not authed", argv[1]);
        return;
    }
    if (huser_get_level(husr) < H_PEON)
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: User %s is considered improper and not worthy of a ticket", argv[1]);
        return;
    }
    if (huser_get_level(husr) > H_PEON)
    {
        helpmod_reply(sender, returntype, "Cannot issue a ticket: User %s does not require a ticket", argv[1]);
        return;
    }
    if (argc > 3)
    {
	int tmp;
        tmp = helpmod_read_strtime(argv[2]);
        if (tmp > HDEF_m && tmp < 2 * HDEF_M)
	    expiration = tmp;
    }
    if (argc >= 4 && strlen(argv[3]) < 128)
    {
	if (argv[3][0] != '\0')
            message = argv[3];
    }

    htick = hticket_get(huser_get_auth(husr), hchan);

    if (htick != NULL)
        htick->time_expiration = time(NULL) + expiration;
    else
        hticket_add(huser_get_auth(husr), time(NULL) + expiration, hchan, message);

    helpmod_reply(sender, returntype, "Issued an invite ticket to user %s for channel %s expiring in %s", huser_get_nick(husr), hchannel_get_name(hchan), helpmod_strtime(expiration));
    helpmod_reply(husr, NULL, "You have been issued an invite ticket for channel %s. This ticket is valid for a period of %s. You can use my invite command to get to the channel now. Type /msg %s invite %s",hchannel_get_name(hchan), helpmod_strtime(HTICKET_EXPIRATION_TIME), helpmodnick->nick, hchannel_get_name(hchan));
    if (hchan->flags & H_TICKET_MESSAGE && hchan->ticket_message != NULL)
	helpmod_reply(husr, NULL, "Ticket information for %s: %s", hchannel_get_name(hchan), hchan->ticket_message->content);
}

static void helpmod_cmd_resolve (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    int i;
    hchannel *hchan;
    hticket *htick;
    huser *husr;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot resolve a ticket: The channel is not specified");
        return;
    }

    for (i = 0;i< argc;i++)
    {
        if (argv[i][0] == '#')
        {
            htick = hticket_get(&argv[i][1], hchan);
            if (htick == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot resolve a ticket: Authname %s does not have a ticket for channel %s", &argv[i][1], hchannel_get_name(hchan));
                continue;
            }
            hticket_del(htick, hchan);
            helpmod_reply(sender, returntype, "Resolved authname %s's ticket for channel %s", &argv[i][1], hchannel_get_name(hchan));
        }
        else
        {
            husr = huser_get(getnickbynick(argv[i]));
            if (husr == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot resolve a ticket: User %s not found", argv[i]);
                continue;
            }
            if (!IsAccount(husr->real_user))
            {
                helpmod_reply(sender, returntype, "Cannot resolve a ticket: User %s is not authed", argv[i]);
                continue;
            }
            htick = hticket_get(huser_get_auth(husr),hchan);
            if (htick == NULL)
            {
                helpmod_reply(sender, returntype, "Cannot resolve a ticket: User %s does not have a ticket for channel %s", argv[i], hchannel_get_name(hchan));
                continue;
            }
            hticket_del(htick, hchan);
            helpmod_reply(sender, returntype, "Resolved user %s's ticket for channel %s", argv[i], hchannel_get_name(hchan));
        }
    }
}

static void helpmod_cmd_tickets (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hticket *htick;
    int i;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot list tickets: Channel not defined or not found");
        return;
    }

    if (!(hchan->flags & H_REQUIRE_TICKET))
    {
        helpmod_reply(sender, returntype, "Cannot list tickets: Channel %s does not use the ticket system", hchannel_get_name(hchan));
        return;
    }

    htick = hchan->htickets;

    if (htick == NULL)
    {
        helpmod_reply(sender, returntype, "Channel %s has no valid tickets", hchannel_get_name(hchan));
        return;
    }

    helpmod_reply(sender, returntype, "Valid tickets for channel %s", hchannel_get_name(hchan));

    for (i = 0;htick;htick = htick->next, i++)
        helpmod_reply(sender, returntype, "%4d %16s %48s", i, htick->authname, helpmod_strtime(time(NULL) - htick->time_expiration));

    helpmod_reply(sender, returntype, "Done listing tickets. Channel %s had %d valid ticket%s", hchannel_get_name(hchan), i, (i==1)?"":"s");
}

static void helpmod_cmd_showticket (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    huser *husr;
    hticket *htick;
    int i;

    DEFINE_HCHANNEL;

    if (argc > H_CMD_MAX_ARGS)
        argc = H_CMD_MAX_ARGS;

    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot show the ticket: Channel not defined or not found");
        return;
    }
    for (i = 0;i < argc;i++)
    {
        husr = huser_get(getnickbynick(argv[i]));
        if (husr == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot show the ticket: User %s not found", argv[i]);
            continue;
        }
        if (!IsAccount(husr->real_user))
        {
            helpmod_reply(sender, returntype, "Cannot show the ticket: User %s is not authed", argv[i]);
            continue;
        }
        htick = hticket_get(huser_get_auth(husr), hchan);
        if (htick == NULL)
        {
            helpmod_reply(sender, returntype, "Cannot show the ticket: User %s does not have a valid ticket for channel %s", argv[i], hchannel_get_name(hchan));
            continue;
	}
	if (htick->message == NULL)
	    helpmod_reply(sender, returntype, "User %s has a ticket for channel %s expiring in %s. No message is attached.", argv[i], hchannel_get_name(hchan), helpmod_strtime(htick->time_expiration - time(NULL)));
	else
	    helpmod_reply(sender, returntype, "User %s has a ticket for channel %s expiring in %s. With message: %s", argv[i], hchannel_get_name(hchan), helpmod_strtime(htick->time_expiration - time(NULL)), htick->message->content);
    }
}

static int helpmod_cmd_termstats_sort(const void *left, const void *right)
{
    return (*((hterm**)right))->usage - (*((hterm**)left))->usage;
}

static void helpmod_cmd_termstats(huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hterm **arr, *origin;
    hchannel *hchan;
    int i, count;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
        origin = hterms;
    else
        origin = hchan->channel_hterms;

    count = hterm_count(origin);

    if (count == 0)
    {
        helpmod_reply(sender, returntype, "Cannot list term usage statistics: No terms available");
        return;
    }

    arr = malloc(sizeof(hterm*) * count);
    assert(arr != NULL);

    for (i=0;i < count;i++,origin = origin->next)
        arr[i] = origin;

    qsort(arr, count, sizeof(hterm*), helpmod_cmd_termstats_sort);

    if (hchan == NULL)
        helpmod_reply(sender, returntype, "10 Most used global terms");
    else
        helpmod_reply(sender, returntype, "10 Most used terms for channel %s", hchannel_get_name(hchan));

    for (i=0;i < 10 && i < count;i++)
        helpmod_reply(sender, returntype, "#%02d %32s :%d",i+1, arr[i]->name->content,arr[i]->usage);

    free(arr);
}

static int helpmod_cmd_checkchannel_nicksort(const void *left, const void *right)
{
    return ci_strcmp(getnickbynumeric(*((unsigned long*)left))->nick, getnickbynumeric(*((unsigned long*)right))->nick);
}

static int helpmod_cmd_checkchannel_statussort(const void *left, const void *right)
{
    int level1 = 0, level2 = 0;

    if (*((unsigned long*)left) & CUMODE_VOICE)
        level1 = 1;
    if (*((unsigned long*)left) & CUMODE_OP)
        level1 = 2;

    if (*((unsigned long*)right) & CUMODE_VOICE)
	level2 = 1;
    if (*((unsigned long*)right) & CUMODE_OP)
        level2 = 2;

    return level2 - level1;
}

static void helpmod_cmd_checkchannel(huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    channel *chan;
    nick *nck;
    int i, j, nick_count = 0, authed_count = 0, o_limit = 0, v_limit = 0, summary_only = 0;
    unsigned long *numeric_array;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Cannot check channel: Channel not defined");
        return;
    }

    chan = findchannel(argv[0]);
    if (chan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot check channel: Channel %s not found", argv[0]);
        return;
    }
    if (argc > 1 && !ci_strcmp(argv[1], "summary"))
        summary_only = 1;

    /* first pass - verify validity and count nicks */
    for (i=0;i < chan->users->hashsize;i++)
    {
            nck = getnickbynumeric(chan->users->content[i]);
            if (!nck) /* it's a hash, not an array */
                continue;

	    nick_count++;

	    if (IsAccount(nck))
                authed_count++;

            if (IsOper(nck) && strlen(nck->nick) > 1 && (IsSecret(chan) || IsPrivate(chan) || IsKey(chan) || IsInviteOnly(chan)))
            {
                helpmod_reply(sender, returntype, "Cannot check channel: Permission denied. Channel %s has an oper on it and one or more of +i/+k/+p/+s", argv[0]);
                return;
            }
	}

    numeric_array = (unsigned long*)malloc(nick_count * sizeof(unsigned long));

    /* second pass - construct array */
    for (i=0,j=0;i < chan->users->hashsize;i++)
    {
	if (getnickbynumeric(chan->users->content[i]) == NULL) /* it's a hash, not an array */
	    continue;

        numeric_array[j++] = chan->users->content[i];
    }

    qsort(numeric_array, nick_count, sizeof(unsigned long), helpmod_cmd_checkchannel_statussort);

    /* third pass - find status boundaries */
    {
	for (;o_limit < nick_count && numeric_array[o_limit] & CUMODE_OP; o_limit++);
        v_limit = o_limit;
	for(v_limit = o_limit; (v_limit < nick_count) && numeric_array[v_limit] & CUMODE_VOICE; v_limit++);
    }

    helpmod_reply(sender, returntype, "Information on channel %s", argv[0]);
    helpmod_reply(sender, returntype, "Channel created %s ago", helpmod_strtime(time(NULL) - chan->timestamp));
    if (!IsKey(chan) && !IsInviteOnly(chan))
	helpmod_reply(sender, returntype, "Channel topic: %s", chan->topic?chan->topic->content:"Not set");
    helpmod_reply(sender, returntype, "Channel modes: %s", printflags(chan->flags, cmodeflags));


    /* sort the sub arrays */

    if (o_limit > 0)
	qsort(numeric_array, o_limit, sizeof(unsigned long), helpmod_cmd_checkchannel_nicksort);
    if (v_limit - o_limit > 0)
	qsort(numeric_array + o_limit, v_limit - o_limit, sizeof(unsigned long), helpmod_cmd_checkchannel_nicksort);
    if (nick_count - v_limit > 0)
	qsort(numeric_array + v_limit, nick_count - v_limit, sizeof(unsigned long), helpmod_cmd_checkchannel_nicksort);

    /* fourth pass - print results */
    if (!summary_only)
	for (i=0;i < nick_count;i++)
	{
	    char buf[256], status;

	    if (numeric_array[i] & CUMODE_OP)
		status = '@';
	    else if (numeric_array[i] & CUMODE_VOICE)
		status = '+';
	    else
		status = ' ';

	    visiblehostmask(getnickbynumeric(numeric_array[i]), buf);
	    if (IsAccount(getnickbynumeric(numeric_array[i])))
		helpmod_reply(sender, returntype, "%c%s (%s)", status, buf, getnickbynumeric(numeric_array[i])->authname);
	    else
		helpmod_reply(sender, returntype, "%c%s", status, buf);
	}

    helpmod_reply(sender, returntype, "Users: %d  Clones: %d  Opped: %d  Voiced: %d  Authed: %3.0f%%", nick_count, nick_count - countuniquehosts(chan), o_limit, v_limit - o_limit, ((float)authed_count / (float)nick_count) * 100.0);

    free(numeric_array);
}

static void helpmod_cmd_statsdump (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    haccount *hacc = haccounts;
    hstat_account *ptr;
    int i;

    helpmod_reply(sender, returntype, "Account statistics");
    for (;hacc;hacc = hacc->next)
        for (ptr = hacc->stats;ptr;ptr = ptr->next)
        {
            helpmod_reply(sender, returntype, "Account %s channel %s short-term", hacc->name->content, hchannel_get_name(ptr->hchan));
            for (i = 0;i < 7;i++)
                helpmod_reply(sender, returntype, "%d %d %d %d", ptr->week[i].time_spent, ptr->week[i].prime_time_spent, ptr->week[i].lines, ptr->week[i].words);
            helpmod_reply(sender, returntype, "Account %s channel %s long-term", hacc->name->content, hchannel_get_name(ptr->hchan));
            for (i = 0;i < 10;i++)
                helpmod_reply(sender, returntype, "%d %d %d %d", ptr->longterm[i].time_spent, ptr->longterm[i].prime_time_spent, ptr->longterm[i].lines, ptr->longterm[i].words);
        }
}

static void helpmod_cmd_statsrepair (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    haccount *hacc = haccounts;
    hstat_account *ptr;
    int i;

    helpmod_reply(sender, returntype, "Repairing account statistics");
    for (;hacc;hacc = hacc->next)
        for (ptr = hacc->stats;ptr;ptr = ptr->next)
	{
	    for (i = 0;i < 7;i++)
	    {
		if (ptr->week[i].time_spent > HDEF_d)
		{
		    ptr->week[i].time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired short term TimeSpent %s @ %s : Greater than one day",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
		if (ptr->week[i].time_spent < 0)
		{
		    ptr->week[i].time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired short term TimeSpent %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}

		if (ptr->week[i].prime_time_spent > HDEF_d)
		{
		    ptr->week[i].prime_time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired short term PrimeTimeSpent %s @ %s : Greater than one day",hacc->name->content, hchannel_get_name(ptr->hchan));
		}                    
		if (ptr->week[i].prime_time_spent < 0)
		{
		    ptr->week[i].prime_time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired short term PrimeTimeSpent %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}

		if (ptr->week[i].lines > 10000)
		{
		    ptr->week[i].lines = 0;
		    helpmod_reply(sender, returntype, "repaired short term Lines %s @ %s : Greater than 10000",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
		if (ptr->week[i].lines < 0)
		{
		    ptr->week[i].lines = 0;
		    helpmod_reply(sender, returntype, "repaired short term Lines %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}

		if (ptr->week[i].words > 50000)
		{
		    ptr->week[i].words = 0;
		    helpmod_reply(sender, returntype, "repaired short term Words %s @ %s : Greater than 50000",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
		if (ptr->week[i].words < 0)
		{
		    ptr->week[i].words = 0;
		    helpmod_reply(sender, returntype, "repaired short term Words %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
	    }
	    for (i = 0;i < 10;i++)
	    {
		if (ptr->longterm[i].time_spent > HDEF_w)
		{
		    ptr->longterm[i].time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired long term TimeSpent %s @ %s : Greater than one week",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
		if (ptr->longterm[i].time_spent < 0)
		{
		    ptr->longterm[i].time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired long term TimeSpent %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}

		if (ptr->longterm[i].prime_time_spent > HDEF_w)
		{
		    ptr->longterm[i].prime_time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired long term PrimeTimeSpent %s @ %s : Greater than one week",hacc->name->content, hchannel_get_name(ptr->hchan));
		}                    
		if (ptr->longterm[i].prime_time_spent < 0)
		{
		    ptr->longterm[i].prime_time_spent = 0;
		    helpmod_reply(sender, returntype, "repaired long term PrimeTimeSpent %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}

		if (ptr->longterm[i].lines > 50000)
		{
		    ptr->longterm[i].lines = 0;
		    helpmod_reply(sender, returntype, "repaired long term Lines %s @ %s : Greater than 50000",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
		if (ptr->longterm[i].lines < 0)
		{
		    ptr->longterm[i].lines = 0;
		    helpmod_reply(sender, returntype, "repaired long term Lines %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}

		if (ptr->longterm[i].words > 50000)
		{
		    ptr->longterm[i].words = 0;
		    helpmod_reply(sender, returntype, "repaired long term Words %s @ %s : Greater than 50000",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
		if (ptr->longterm[i].words < 0)
		{
		    ptr->longterm[i].words = 0;
		    helpmod_reply(sender, returntype, "repaired long term Words %s @ %s : Less than zero",hacc->name->content, hchannel_get_name(ptr->hchan));
		}
	    }
	}
    helpmod_reply(sender, returntype, "Account statistics repaired");

}

static void helpmod_cmd_statsreset (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    haccount *hacc = haccounts;
    hstat_account *ptr;
    hchannel *hchan;
    int i, short_del, long_del;

    if (argc < 2)
    {
	helpmod_reply(sender, returntype, "Insufficient parameters");
	return;
    }

    if (sscanf(argv[0], "%d", &short_del) == 0)
    {
	helpmod_reply(sender, returntype, "Invalid parameter");
	return;
    }
    if (sscanf(argv[1], "%d", &long_del) == 0)
    {
	helpmod_reply(sender, returntype, "Invalid parameter");
	return;
    }

    for (;hacc;hacc = hacc->next)
        for (ptr = hacc->stats;ptr;ptr = ptr->next)
	{
            for (i = 1;i < short_del + 1;i++)
		HSTAT_ACCOUNT_ZERO(ptr->week[(hstat_day() + i) % 7]);
	    for (i = 1;i < long_del + 1;i++)
                HSTAT_ACCOUNT_ZERO(ptr->longterm[(hstat_week() + i) % 10]);
	}

    for (hchan = hchannels;hchan;hchan = hchan->next)
    {
	for (i = 1;i < short_del + 1;i++)
	    HSTAT_CHANNEL_ZERO(hchan->stats->week[(hstat_day() + i) % 7]);
	for (i = 1;i < long_del + 1;i++)
	    HSTAT_CHANNEL_ZERO(hchan->stats->longterm[(hstat_week() + i) % 10]);

    }

    helpmod_reply(sender, returntype, "Statistics reset complete");
}

static void helpmod_cmd_message (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;

    if (argc < 2)
    {
        helpmod_reply(sender, returntype, "Cannot send a message: Insufficient arguments");
        return;
    }
    hchan = hchannel_get_by_name(argv[0]);
    if (hchan == NULL)
    {
        helpmod_reply(sender, returntype, "Cannot send a message: Invalid channel %s", argv[0]);
        return;
    }
    SKIP_WORD;
    helpmod_message_channel(hchan, "(%s) %s", huser_get_nick(sender), ostr);
    helpmod_reply(sender, returntype, "Message sent to %s", hchannel_get_name(hchan));
}

static void helpmod_cmd_version (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_reply(sender, returntype, "HelpMod version " HELPMOD_VERSION " by strutsi (strutsi@quakenet.org)");
}

static void helpmod_cmd_ticketmsg (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
	helpmod_reply(sender, returntype, "Can not view or set ticket message: No channel is specified.");
	return;
    }

    if (argc == 0)
    { /* view */
	if (hchan->ticket_message == NULL)
	    helpmod_reply(sender, returntype, "No ticket message is set for channel %s", hchannel_get_name(hchan));
	else
	    helpmod_reply(sender, returntype, "Ticket message for channel %s is: %s", hchannel_get_name(hchan), hchan->ticket_message->content);
	return;
    }

    /* set */
    if (hchan->ticket_message != NULL)
	freesstring(hchan->ticket_message);

    hchan->ticket_message = getsstring(ostr, strlen(ostr));
    helpmod_reply(sender, returntype, "Ticket message for channel %s set to: %s", hchannel_get_name(hchan), hchan->ticket_message->content);
}

static void helpmod_cmd_lcedit (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hlc_profile *profile;

    if (argc == 0)
    {
	helpmod_reply(sender, returntype, "Can not handle lamer control profiles: Operation not defined");
        return;
    }

    if (!ci_strcmp(argv[0], "list"))
    {
	if (hlc_profiles == NULL)
	{
	    helpmod_reply(sender, returntype, "Can not list lamer control profiles: No profiles");
            return;
	}
	helpmod_reply(sender, returntype, "Following lamer control profiles are currently available:");
	for (profile = hlc_profiles;profile != NULL;profile = profile->next)
	    helpmod_reply(sender, returntype, "%s", profile->name->content);
    }
    else if (!ci_strcmp(argv[0], "add"))
    {
	if (argc < 2)
	{
	    helpmod_reply(sender, returntype, "Can not add a lamer control profile: Profile name not defined");
            return;
	}

	profile = hlc_get(argv[1]);

	if (profile != NULL)
	{
	    helpmod_reply(sender, returntype, "Can not add a lamer control profile: Profile named %s already exists", argv[1]);
	    return;
	}
	profile = hlc_add(argv[1]);

	{ /* set the default values */
	    profile->caps_max_percentage = 40;
	    profile->caps_min_count = 20;

	    profile->repeats_max_count = 3;
	    profile->repeats_min_length = 7;

	    profile->symbol_repeat_max_count = 6;
	    profile->character_repeat_max_count = 7;
	    profile->symbol_max_count = 9;

	    profile->tolerance_flood = 5;

	    profile->tolerance_spam = 0.008;
	    profile->constant_spam = 10;

	    profile->tolerance_warn = 1;
	    profile->tolerance_kick = 3;
	    profile->tolerance_remove = 5;
	}

	helpmod_reply(sender, returntype, "Lamer control profile %s added", argv[1]);
    }
    else if (!ci_strcmp(argv[0], "del"))
    {
        hchannel *hchan;
	if (argc < 2)
	{
	    helpmod_reply(sender, returntype, "Can not delete a lamer control profile: Profile name not defined");
            return;
	}

	profile = hlc_get(argv[1]);

	if (profile == NULL)
	{
	    helpmod_reply(sender, returntype, "Can not delete a lamer control profile: Profile named %s does not exist", argv[1]);
	    return;
	}
	for (hchan = hchannels;hchan != NULL;hchan = hchan->next)
	    if (hchan->lc_profile == profile)
	    {
		helpmod_reply(sender, returntype, "Can not delete a lamer control profile: Profile %s is in use", argv[1]);
		return;
	    }
	hlc_del(profile);
	helpmod_reply(sender, returntype, "Lamer control profile %s deleted", argv[1]);
    }
    else if (!ci_strcmp(argv[0], "view"))
    {
	if (argc < 2)
	{
	    helpmod_reply(sender, returntype, "Can not view a lamer control profile: Profile name not defined");
	    return;
	}

	profile = hlc_get(argv[1]);

	if (profile == NULL)
	{
	    helpmod_reply(sender, returntype, "Can not view a lamer control profile: Profile named %s not found", argv[1]);
	    return;
	}

	helpmod_reply(sender, returntype, "Lamer control profile %s:", profile->name->content);
	helpmod_reply(sender, returntype, "Maximum caps percentage:     %d", profile->caps_max_percentage);
	helpmod_reply(sender, returntype, "Caps minimum count:          %d", profile->caps_min_count);
	helpmod_reply(sender, returntype, "Repeat tolerance:            %d", profile->repeats_max_count);
        helpmod_reply(sender, returntype, "Repeat minimum length:       %d", profile->repeats_min_length);
        helpmod_reply(sender, returntype, "Symbol repeat tolerance:     %d", profile->symbol_repeat_max_count);
	helpmod_reply(sender, returntype, "Character repeat tolerance:  %d", profile->character_repeat_max_count);
	helpmod_reply(sender, returntype, "Continuous symbol tolerance: %d", profile->symbol_max_count);
	helpmod_reply(sender, returntype, "Flood tolerance:             %d", profile->tolerance_flood);
	helpmod_reply(sender, returntype, "Spam tolerance:              %d", profile->tolerance_spam);
	helpmod_reply(sender, returntype, "Spam multiplier:             %f", profile->constant_spam);
	helpmod_reply(sender, returntype, "Warning limit:               %d", profile->tolerance_warn);
	helpmod_reply(sender, returntype, "Kick limit:                  %d", profile->tolerance_kick);
	helpmod_reply(sender, returntype, "Ban limit:                   %d", profile->tolerance_remove);
    }
    else if (!ci_strcmp(argv[0], "edit"))
    {
	int int_val = -1;
	double dbl_val = -1;

	if (argc != 4)
	{
	    helpmod_reply(sender, returntype, "Can not edit a lamer control profile: Syntax error");
            return;
	}

	profile = hlc_get(argv[1]);

	if (profile == NULL)
	{
	    helpmod_reply(sender, returntype, "Can not edit a lamer control profile: Profile %s not found", argv[1]);
	    return;
	}

	sscanf(argv[3], "%d", &int_val);
        sscanf(argv[3], "%lf", &dbl_val);

	if (dbl_val < 0)
	{   /* All int values are also valid double values */
	    helpmod_reply(sender, returntype, "Can not edit a lamer control profile: Invalid argument");
            return;
	}

	if (!ci_strcmp(argv[2], "caps_percentage"))
	    profile->caps_max_percentage = int_val;
        else if (!ci_strcmp(argv[2], "caps_count"))
	    profile->caps_min_count = int_val;
        else if (!ci_strcmp(argv[2], "repeat_tolerance"))
	    profile->repeats_max_count = int_val;
        else if (!ci_strcmp(argv[2], "repeat_length"))
	    profile->repeats_min_length = int_val;
        else if (!ci_strcmp(argv[2], "symbol_repeat"))
	    profile->symbol_repeat_max_count = int_val;
        else if (!ci_strcmp(argv[2], "character_repeat"))
	    profile->character_repeat_max_count = int_val;
	else if (!ci_strcmp(argv[2], "continuous_symbol"))
	    profile->symbol_max_count = int_val;
	else if (!ci_strcmp(argv[2], "flood_tolerance"))
	    profile->tolerance_flood = int_val;
	else if (!ci_strcmp(argv[2], "spam_tolerance"))
	    profile->tolerance_spam = int_val;
	else if (!ci_strcmp(argv[2], "spam_multiplier"))
	    profile->constant_spam = dbl_val;
	else if (!ci_strcmp(argv[2], "warn_limit"))
	    profile->tolerance_warn = int_val;
	else if (!ci_strcmp(argv[2], "kick_limit"))
	    profile->tolerance_kick = int_val;
	else if (!ci_strcmp(argv[2], "ban_limit"))
	    profile->tolerance_remove = int_val;
	else
	{
	    helpmod_reply(sender, returntype, "Can not edit a lamer control profile: No component named %s", argv[2]);
	    return;
	}
	helpmod_reply(sender, returntype, "Lamer control profile component value changed succesfully.");
    }
}

static void helpmod_cmd_ged (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_editor *editor;

    if (sender->editor == NULL)
    { /* Start a new editor */
	if (argc == 0)
	{
	    helpmod_reply(sender, returntype, "Can not use ged: Filename not specified");
	    return;
	}
	editor = hed_open(sender, argv[0]);
	if (editor == NULL)
	{
	    helpmod_reply(sender, returntype, "Can not use ged: Invalid filename %s", argv[0]);
            return;
	}
	if (editor->user != sender)
	{
	    helpmod_reply(sender, returntype, "Can not use ged: File %s is currently being edited by %s", editor->filename, huser_get_nick(editor->user));
            return;
	}
	editor->user = sender;
	sender->editor = editor;

	helpmod_reply(sender, returntype, "Ged: %d", hed_byte_count(editor));
    }
    else
    {
	hed_command(sender, returntype, ostr, argc, argv);
    }
}

static void helpmod_cmd_text (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    DEFINE_HCHANNEL;
    FILE *in;

    if (argc == 0 || !ci_strcmp(argv[0], "list"))
    {
	DIR *dir;
	struct dirent *dent;
	char buffer[384], **lines, *start;
        int nwritten, bufpos = 0, nlines = 0,i;

	dir = opendir(HELPMOD_TEXT_DIR);
	assert(dir != NULL);

        /* First pass, count */
	for (dent = readdir(dir);dent != NULL;dent = readdir(dir))
	{
            /* Skip stuff like . and .. */
	    if (!strncmp(dent->d_name, ".", 1))
		continue;
            nlines++;
	}

	if (nlines == 0)
	{
	    helpmod_reply(sender, returntype, "No texts are available.");
            return;
	}

	helpmod_reply(sender, returntype, "Following texts are available:");

	{
	    lines = (char **)malloc(sizeof(char*) * nlines);
            *lines = (char *)malloc(sizeof(char) * (HED_FILENAME_LENGTH+1) * nlines);
            /* We need the start to free this array */
	    start = *lines;
	    for(i=0;i < nlines;i++)
		lines[i] = &lines[0][(HED_FILENAME_LENGTH+1) * i];
	}
        
	/* Second pass, create array */
        rewinddir(dir);
	for (dent = readdir(dir), i = 0;dent != NULL && i < nlines;dent = readdir(dir))
	{
	    /* Skip stuff like . and .. */
	    if (!strncmp(dent->d_name, ".", 1))
		continue;
	    strncpy(lines[i], dent->d_name, 64);
            i++;
	}
	/* Sort */
	qsort(*lines, nlines, sizeof(char)*(HED_FILENAME_LENGTH+1), (int (*)(const void*, const void*))&strcmp);

	for (i = 0;i < nlines;i++)
	{
	    if (bufpos)
	    {
		buffer[bufpos] = ' ';
		bufpos++;
	    }
	    sprintf(buffer + bufpos, "%s%n", lines[i], &nwritten);
	    bufpos+=nwritten;

	    if (bufpos > (384 - (HED_FILENAME_LENGTH+1)))
	    {
		helpmod_reply(sender, returntype, "%s", buffer);
		bufpos = 0;
	    }
	}

	if (bufpos)
	    helpmod_reply(sender, returntype, "%s", buffer);

	free(start);
        free(lines);

	return;
    }
    else if (!ci_strcmp(argv[0], "show"))
    {
        char fname_buffer[128], buffer[512];
	if (argc < 2)
	{
	    helpmod_reply(sender, returntype, "Can not show text: Text not specified");
            return;
	}
	if (!hed_is_valid_filename(argv[1]))
	{
	    helpmod_reply(sender, returntype, "Can not show text: Invalid filename");
            return;
	}
	sprintf(fname_buffer, HELPMOD_TEXT_DIR"/%s" ,argv[1]);
	in = fopen(fname_buffer, "rwt");
	if (in == NULL)
	{
	    helpmod_reply(sender, returntype, "Can not show text: Text %s not found", argv[1]);
            return;
	}
	while (!feof(in))
	{
	    if (!fgets(buffer, 512, in))
                break;
	    if (returntype != NULL && huser_get_level(sender) >= H_TRIAL)
		helpmod_message_channel(hchannel_get_by_channel(returntype), "%s", buffer);
	    else
		helpmod_reply(sender, returntype, "%s", buffer);
	}
	fclose (in);
        return;
    }

    if (huser_get_level(sender) < H_STAFF)
    {
	helpmod_reply(sender, returntype, "Can not handle text: Insufficient user level");
        return;
    }

    if (!ci_strcmp(argv[0], "add"))
    {
	char fname_buffer[128];
	if (argc < 2)
	{
	    helpmod_reply(sender, returntype, "Can not add text: Text not specified");
            return;
	}
	if (!hed_is_valid_filename(argv[1]))
	{
	    helpmod_reply(sender, returntype, "Can not add text: Invalid filename");
	    return;
	}
	sprintf(fname_buffer, HELPMOD_TEXT_DIR"/%s" ,argv[1]);
	if ((in = fopen(fname_buffer, "rt")) != NULL)
	{
	    helpmod_reply(sender, returntype, "Can not add text: Text %s already exists", argv[1]);
            return;
	}
	else
	{
	    if ((in = fopen(fname_buffer, "wt")) == NULL)
	    {
		helpmod_reply(sender, returntype, "Can not add text: Unexpected error, please contact strutsi");
                return;
	    }
	    fclose(in);
	}
	helpmod_reply(sender, returntype, "Text %s added succesfully", argv[1]);
    }
    else if (!ci_strcmp(argv[0], "del"))
    {
	char fname_buffer[128];
	if (argc < 2)
	{
	    helpmod_reply(sender, returntype, "Can not delete text: Text not specified");
            return;
	}
	if (!hed_is_valid_filename(argv[1]))
	{
	    helpmod_reply(sender, returntype, "Can not delete text: Invalid filename");
	    return;
	}
	sprintf(fname_buffer, HELPMOD_TEXT_DIR"/%s" ,argv[1]);

	if (!remove(fname_buffer))
	    helpmod_reply(sender, returntype, "Text %s removed", argv[1]);
	else
	    helpmod_reply(sender, returntype, "Can not delete text: Text %s does not exist or can not be deleted", argv[1]);
    }
    else
    {
	helpmod_reply(sender, returntype, "Can not handle text: Unknown operation %s", argv[0]);
        return;
    }
}

static void helpmod_cmd_rating (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hstat_account_entry sum = {0,0,0,0};
    hstat_account *ptr;
    int i;

    DEFINE_HCHANNEL;

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (hchan == NULL)
    {
	helpmod_reply(sender, returntype, "Can not show rating: Unknown channel");
        return;
    }
    if (sender->account == NULL)
    {
	helpmod_reply(sender, returntype, "Can not show rating: You do not have an account");
        return;
    }
    for (ptr = sender->account->stats;ptr != NULL;ptr = ptr->next)
	if (ptr->hchan == hchan)
	{
	    if (hstat_day() == 0)
	    { /* Sunday screws the indexing */
		for (i = 0;i < 7;i++)
		    HSTAT_ACCOUNT_SUM(sum, sum, ptr->week[i]);
	    }
	    else
	    { /* Normal case */
		for (i = 1;i <= hstat_day();i++)
		    HSTAT_ACCOUNT_SUM(sum, sum, ptr->week[i]);
	    }

	    helpmod_reply(sender, returntype, "Your rating for channel %s for this week is: %s", hchannel_get_name(hchan), helpmod_strtime(sum.time_spent));
	    return;
	}
    helpmod_reply(sender, returntype, "Can not show rating: You do not have any statistics for channel %s", hchannel_get_name(hchan));
}

static void helpmod_cmd_writedb (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    rename(HELPMOD_DEFAULT_DB, HELPMOD_DEFAULT_DB".old");
    helpmod_config_write(HELPMOD_DEFAULT_DB);

    helpmod_reply(sender, returntype, "Database written");
}

static void helpmod_cmd_evilhack1 (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    int tmp;

    if (argc == 0)
	helpmod_reply(sender, returntype, "hstat_cycle: %d", hstat_cycle);
    else
    {
	if (!sscanf(argv[0], "%d", &tmp) || tmp < 0)
	{
            helpmod_reply(sender, returntype, "Invalid argument");
            return;
	}
	hstat_cycle = tmp;
	helpmod_reply(sender, returntype, "hstat_cycle is now: %d", hstat_cycle);
    }
}

static void helpmod_cmd_evilhack2 (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    haccount *hacc = haccounts;
    hchannel *hchan;
    hstat_account *ptr;
    hstat_account_entry tmp_account;
    hstat_channel_entry tmp_channel;
    int first, second;

    if (argc != 2)
    {
	helpmod_reply(sender, returntype, "Syntax error: evilhack2 <first> <second>");
	return;
    }

    if (!sscanf(argv[0],"%d", &first) || first < 0 || first > 9)
    {
	helpmod_reply(sender, returntype, "Syntax error: Invalid first");
	return;
    }

    if (!sscanf(argv[1],"%d", &second) || second < 0 || second > 9)
    {
	helpmod_reply(sender, returntype, "Syntax error: Invalid second");
	return;
    }

    for (;hacc;hacc = hacc->next)
        for (ptr = hacc->stats;ptr;ptr = ptr->next)
	{
	    tmp_account = ptr->longterm[first];
	    ptr->longterm[first] = ptr->longterm[second];
            ptr->longterm[second] = tmp_account;
	}

    for (hchan = hchannels;hchan;hchan = hchan->next)
    {
	    tmp_channel = hchan->stats->longterm[first];
	    hchan->stats->longterm[first] = hchan->stats->longterm[second];
	    hchan->stats->longterm[second] = tmp_channel;
    }

    helpmod_reply(sender, returntype, "Evilhack2 done: Swapped %d and %d", first, second);
}

static void helpmod_cmd_channel (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hchannel_user *hchanuser;

    DEFINE_HCHANNEL;

    if (hchan == NULL)
    {
	helpmod_reply(sender, returntype, "Can not show channel: Channel not specified");
        return;
    }

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    helpmod_reply(sender, returntype, "Users for channel %s", hchannel_get_name(hchan));
    helpmod_reply(sender, returntype, "Nick             Account          User level               Idle time");

    for (hchanuser = hchan->channel_users;hchanuser;hchanuser=hchanuser->next)
	helpmod_reply(sender, returntype, "%-16s %-16s %-24s %s",hchanuser->husr->real_user->nick,hchanuser->husr->account?hchanuser->husr->account->name->content:"-",hlevel_name(huser_get_level(hchanuser->husr)), helpmod_strtime(time(NULL)-huser_on_channel(hchanuser->husr, hchan)->last_activity));

    helpmod_reply(sender, returntype, "Listed %d users for channel %s", hchannel_count_users(hchan, H_ANY), hchannel_get_name(hchan));
}

static void helpmod_cmd_weekstats (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    hchannel *hchan;
    hstat_accounts_array arr;
    int i;
    hlevel lvl = H_ANY;

    DEFINE_HCHANNEL;

    if (hchan == NULL)
    {
	helpmod_reply(sender, returntype, "Can not list weekly stats: Channel not specified");
        return;
    }

    HCHANNEL_VERIFY_AUTHORITY(hchan, sender);

    if (argc >= 1)
    {
        if (!ci_strcmp(argv[0], "opers") || !ci_strcmp(argv[0], "o"))
            lvl = H_OPER;
	else if (!ci_strcmp(argv[0], "staff") || !ci_strcmp(argv[0], "s"))
	    lvl = H_STAFF;
	else if (!ci_strcmp(argv[0], "all") || !ci_strcmp(argv[0], "a"))
            lvl = H_ANY;
    }

    arr = create_hstat_account_array(hchan, lvl, HSTAT_ACCOUNT_ARRAY_WEEKSTATS);

    helpmod_reply(sender, returntype, "Weekly statistics for %ss on channel %s", hlevel_name(lvl), hchannel_get_name(hchan));

    for (i=0;i < arr.arrlen;i++)
      if (arr.array[i].time_spent > HDEF_m)
	helpmod_reply(sender, returntype, "%-20s %-20s %-20s",((haccount*)(arr.array[i].owner))->name->content, helpmod_strtime(arr.array[i].prime_time_spent), helpmod_strtime(arr.array[i].time_spent));

    free(arr.array);
}

/* old H stuff */
void helpmod_cmd_load (huser *sender, channel *returntype, char* arg, int argc, char *argv[])
{
    FILE *tmp_file;
    char buf[128] = "helpmod/default";

    if (!arg)
        helpmod_reply(sender, returntype, "Warning: No config specified, using default");
    else
    {
	if (strlen(arg) >= 100)
	    return;
	else
	    sprintf(buf, "helpmod/%s", arg);
    }

    if (!(tmp_file = fopen(buf, "rt")))
    {
	helpmod_reply(sender, returntype, "File %s not found", buf);
	return;
    }
    else
	fclose(tmp_file);

    helpmod_clear_aliases(&aliases);
    helpmod_clear_all_entries();
    helpmod_init_entry(&helpmod_base);
    huser_reset_states();
    helpmod_load_entries(buf);
    strcpy(helpmod_db, buf);
    helpmod_reply(sender, returntype, "New config loaded and system reset");
}

void helpmod_cmd_status (huser *sender, channel* returntype, char* ostr, int argc, char *argv[])
{
    helpmod_reply(sender, returntype, "HelpMod version %s (built %s, up %s)", HELPMOD_VERSION, __DATE__, helpmod_strtime(time(NULL) - helpmod_startup_time));
    helpmod_reply(sender, returntype, "Channels          %d", hchannel_count());
    helpmod_reply(sender, returntype, "Accounts          %d", haccount_count(H_ANY));
    helpmod_reply(sender, returntype, "Users             %d", huser_count());
    helpmod_reply(sender, returntype, "Help entries      %ld", helpmod_entry_count(helpmod_base));
    helpmod_reply(sender, returntype, "Bans              %d", hban_count());
    helpmod_reply(sender, returntype, "Help provided     %ld", helpmod_usage);
    helpmod_reply(sender, returntype, "Tickets           %d", hticket_count());
}

/* not really a command, but help wants this one */
void helpmod_send_help(huser* target)
{
    int i;
    helpmod_usage++;
    for (i=0;i<target->state->text_lines;i++)
	helpmod_reply(target, NULL, "%s", target->state->text[i]->content);
    for (i=0;i<target->state->option_count;i++)
	helpmod_reply(target, NULL, "%d) %s", i+1, target->state->options[i]->description->content);
    if (!target->state->option_count)
    {
	helpmod_reply(target, NULL, "This concludes the help for this topic, if you want to, you can restart the service with the 'help' command, or return to the previous entry by selecting 0");
    }
}

void helpmod_cmd_command(huser* sender, channel* returntype, char* arg, int argc, char *argv[])
{
    hcommand *hcom;
    char buffer[512], *ptr = argv[0];
    FILE *in;

    if (argc == 0)
    {
        helpmod_reply(sender, returntype, "Usage: command [name of the command] for a list see 'showcommands'");
        return;
    }

    hcom = hcommand_get(argv[0], huser_get_level(sender));

    if (hcom == NULL)
    {
        helpmod_reply(sender, returntype, "Unknown command '%s'", argv[0]);
        return;
    }

    /* tolower */
    while (*(ptr++))
        *ptr = tolower(*ptr);

    /* ?,?+.?- is handled like this because windows is shit */
    if (!ci_strcmp(argv[0], "?"))
	sprintf(buffer, "./helpmod2/commands/questionmark");
    else if (!ci_strcmp(argv[0], "?+"))
	sprintf(buffer, "./helpmod2/commands/questionmarkplus");
    else if (!ci_strcmp(argv[0], "?-"))
	sprintf(buffer, "./helpmod2/commands/questionmarkminus");
    else
	sprintf(buffer, "./helpmod2/commands/%s", argv[0]);

    if ((in = fopen(buffer, "rt")) == NULL)
    {
        helpmod_reply(sender, returntype, "No help available for command '%s' (Ask strutsi to write it)", argv[0]);
        return;
    }

    while (!feof(in))
    {
        fgets(buffer, 512, in);
        if (feof(in))
            break;
        helpmod_reply(sender, returntype, "%s", buffer);
    }

    fclose(in);
}

void helpmod_cmd_help (huser* sender, channel* returntype, char* arg, int argc, char *argv[])
{
    int hlp_target;

    if (helpmod_base == NULL || !helpmod_base->option_count)
    {
	helpmod_reply(sender, returntype, "The help service is not available at this time, please try again later");
	return;
    }

    if (!argc)
    {
	sender->state = helpmod_base;
	helpmod_send_help(sender);
        return;
    }
    else
	if (!sscanf(arg, "%d", &hlp_target))
	{
            helpmod_entry tmp;

            tmp = helpmod_get_alias(arg);
	    if (!tmp)
                helpmod_reply(sender, returntype, "Invalid value. Either use 'help' to restart or give an integer as a valid selection");
            else
            {
                sender->state = tmp;
                helpmod_send_help(sender);
            }
            return;
        }

    hlp_target--;
    if (!helpmod_valid_selection(sender->state, hlp_target))
    {
	if (!sender->state->option_count)
	    helpmod_reply(sender, returntype, "There are no more options to choose from, you can restart the service by giving the 'help' command or select 0 to return to the previous entry");
	else if (!sender->state->parent)
            helpmod_reply(sender, returntype, "Bad selection, please enter your selection as an integer from %d to %d", 1, sender->state->option_count);
	else
	    helpmod_reply(sender, returntype, "Bad selection, please enter your selection as an integer from %d to %d, selecting 0 will take you to the previous entry", 1, sender->state->option_count);
	return;
    }

    sender->state = helpmod_make_selection(sender->state, hlp_target);
    helpmod_send_help(sender);

    return;
}
/* adds all of the abowe to the system */
void hcommands_add(void)
{
    hcommand_add("help", H_PEON, helpmod_cmd_help,"Offers the H1 type help");
    hcommand_add("status",H_OPER, helpmod_cmd_status,"Gives service status");
    hcommand_add("load", H_OPER, helpmod_cmd_load,"Loads a new help database");
    hcommand_add("aliases",H_STAFF, helpmod_cmd_aliases,"Lists all aliases currently in use");
    hcommand_add("showcommands", H_LAMER, helpmod_cmd_showcommands,"Lists all commands available to you");

    hcommand_add("improper", H_STAFF, helpmod_cmd_improper, "Sets the userlevel of the target to banned (lvl 0). Long term ban.");
    hcommand_add("peon", H_STAFF, helpmod_cmd_peon, "Sets the userlevel of the target to peon (lvl 1)");
    hcommand_add("friend", H_STAFF, helpmod_cmd_friend, "Sets the userlevel of the target to friend (lvl 2)");
    hcommand_add("trial", H_OPER, helpmod_cmd_trial, "Sets the userlevel of the target to trial staff (lvl 3)");
    hcommand_add("staff", H_OPER, helpmod_cmd_staff, "Sets the userlevel of the target to staff (lvl 4)");

    hcommand_add("oper", H_OPER, helpmod_cmd_oper, "Sets the userlevel of the target to oper (lvl 5)");
    hcommand_add("admin", H_ADMIN, helpmod_cmd_admin, "Sets the userlevel of the target to admin (lvl 6)");
    hcommand_add("deluser", H_OPER, helpmod_cmd_deluser, "Removes an account from " HELPMOD_NICK);
    hcommand_add("listuser", H_STAFF, helpmod_cmd_listuser, "Lists user accounts of " HELPMOD_NICK);

    hcommand_add("chanconf", H_STAFF, helpmod_cmd_chanconf, "Channel configuration");
    hcommand_add("acconf", H_TRIAL, helpmod_cmd_acconf, "Personalise " HELPMOD_NICK " behaviour");
    hcommand_add("welcome", H_STAFF, helpmod_cmd_welcome, "Views or changes the channel welcome message");
    hcommand_add("censor", H_STAFF, helpmod_cmd_censor, "Handles the censored patterns for a channel");

    hcommand_add("queue", H_TRIAL, helpmod_cmd_queue, "Handles the channel queue");
    hcommand_add("next", H_TRIAL, helpmod_cmd_next, "Same as queue next");
    hcommand_add("done", H_TRIAL, helpmod_cmd_done, "Same as queue done");
    hcommand_add("enqueue", H_TRIAL, helpmod_cmd_enqueue, "Same as queue on or chanconf +3");
    hcommand_add("dequeue", H_TRIAL, helpmod_cmd_dequeue, "Same as queue off or chanconf -3");
    hcommand_add("autoqueue", H_TRIAL, helpmod_cmd_autoqueue, "Same as queue maintain");

    hcommand_add("?", H_PEON, helpmod_cmd_term_find, "Same as term find with multiple targets");
    hcommand_add("?+", H_TRIAL, helpmod_cmd_term_find_plus, "Multitarget term find which advances the queue");
    hcommand_add("?-", H_TRIAL, helpmod_cmd_term_find_minus, "Multitarget term find which removes users from queue");
    hcommand_add("term", H_STAFF, helpmod_cmd_term, "Term handling");

    hcommand_add("klingon", H_OPER, helpmod_cmd_klingon, "Phrases in klingon, both targeted and general");
    hcommand_add("out", H_STAFF, helpmod_cmd_out, "Sets the userlevel of the target to banned (lvl 0) for a short while");
    hcommand_add("kick", H_TRIAL, helpmod_cmd_kick, "Kicks user(s) from a channel");
    hcommand_add("ban", H_STAFF, helpmod_cmd_ban, "Handles global bans");
    hcommand_add("chanban", H_TRIAL, helpmod_cmd_chanban, "Handles channel bans");
    hcommand_add("dnmo", H_STAFF, helpmod_cmd_dnmo, "DoNotMessageOpers informs the luser of his mistake and sets him to the bottom of the queue");
    hcommand_add("lamercontrol", H_OPER, helpmod_cmd_lamercontrol, "Views or changes the channel lamercontrol profile");
    hcommand_add("topic", H_STAFF, helpmod_cmd_topic, "Handles the topic of a channel");
    hcommand_add("idlekick", H_OPER, helpmod_cmd_idlekick, "Views or sets the idle kick time");
    hcommand_add("everyoneout", H_STAFF, helpmod_cmd_everyoneout, "Removes all normal users from a channel and enforces +i");

    hcommand_add("stats", H_STAFF, helpmod_cmd_stats, "Shows staff activity statistics");
    hcommand_add("chanstats", H_STAFF, helpmod_cmd_chanstats, "Shows channel activity statistics");
    hcommand_add("activestaff", H_ADMIN, helpmod_cmd_activestaff, "Shows active staff members for a channel");
    hcommand_add("top10", H_STAFF, helpmod_cmd_top10, "Shows the top 10 most active staff");
    hcommand_add("report", H_OPER, helpmod_cmd_report, "Sets the channel where to report this channels statistics every 5 minutes");

    hcommand_add("whoami", H_LAMER, helpmod_cmd_whoami, "Tells who you are to " HELPMOD_NICK);
    hcommand_add("whois", H_STAFF, helpmod_cmd_whois, "Tells you who someone is");
    hcommand_add("command", H_LAMER, helpmod_cmd_command, "Gives detailed information on a command");
    hcommand_add("addchan", H_ADMIN, helpmod_cmd_addchan, "Joins " HELPMOD_NICK " to a new channel");
    hcommand_add("delchan", H_ADMIN, helpmod_cmd_delchan, "Removes " HELPMOD_NICK " permanently from a channel");

    hcommand_add("seen", H_STAFF, helpmod_cmd_seen, "Tells when a specific user/account has had activity");
    hcommand_add("op", H_STAFF, helpmod_cmd_op, "Sets mode +o on channels");
    hcommand_add("deop", H_STAFF, helpmod_cmd_deop, "Sets mode -o on channels");
    hcommand_add("voice", H_TRIAL, helpmod_cmd_voice, "Sets mode +v on channels");
    hcommand_add("devoice", H_TRIAL, helpmod_cmd_devoice, "Sets mode -v on channels");

    hcommand_add("invite", H_PEON, helpmod_cmd_invite, "Invites you to a channel");
    hcommand_add("ticket", H_TRIAL, helpmod_cmd_ticket, "Gives a ticket to be used with invite");
    hcommand_add("resolve", H_STAFF, helpmod_cmd_resolve, "Resolves (deletes) a ticket");
    hcommand_add("tickets", H_STAFF, helpmod_cmd_tickets, "Lists all valid tickets for a channel");
    hcommand_add("showticket", H_STAFF, helpmod_cmd_showticket, "Shows the ticket for the user");

    hcommand_add("termstats", H_OPER, helpmod_cmd_termstats, "Lists usage statistics for terms");
    hcommand_add("checkchannel", H_STAFF, helpmod_cmd_checkchannel, "Shows channel information for any channel");
    hcommand_add("message", H_TRIAL, helpmod_cmd_message, "Sends a message to a channel");
    hcommand_add("version", H_PEON, helpmod_cmd_version, "G version information");
    hcommand_add("ticketmsg", H_STAFF, helpmod_cmd_ticketmsg, "Handle the ticket message for a channel");

    hcommand_add("lcedit", H_ADMIN, helpmod_cmd_lcedit, "Lamer control profile manager");
    hcommand_add("ged", H_STAFF, helpmod_cmd_ged, "Ged IRC text editor");
    hcommand_add("text", H_PEON, helpmod_cmd_text, "Lists or shows text files");
    hcommand_add("evilhack1", H_ADMIN, helpmod_cmd_evilhack1, "An evil hack, don't use");
    hcommand_add("evilhack2", H_ADMIN, helpmod_cmd_evilhack2, "Another evil hack, don't use");

    hcommand_add("statsdump", H_ADMIN, helpmod_cmd_statsdump, "Statistics dump command");
    hcommand_add("statsrepair", H_ADMIN, helpmod_cmd_statsrepair, "Statistics repair command");
    hcommand_add("statsreset", H_ADMIN, helpmod_cmd_statsreset, "Statistics reset command");
    hcommand_add("rating", H_TRIAL, helpmod_cmd_rating, "Simple rating for the current week");
    hcommand_add("writedb", H_OPER, helpmod_cmd_writedb, "Writes the " HELPMOD_NICK " database to disk");

    hcommand_add("channel", H_TRIAL, helpmod_cmd_channel, "Gives a list of all channel users");
    hcommand_add("weekstats", H_ADMIN, helpmod_cmd_weekstats, "Gives weekly stats for a channel");
    /*hcommand_add("megod", H_PEON, helpmod_cmd_megod, "Gives you userlevel 4, if you see this in the final version, please kill strutsi");*/
    /*hcommand_add("test", H_PEON, helpmod_cmd_test, "Gives you userlevel 4, if you see this in the final version, please kill strutsi");*/
}

void helpmod_command(huser *sender, channel* returntype, char *args)
{
    int argc = 0, useless_var;
    char args_copy[512];
    char *parsed_args[H_CMD_MAX_ARGS + 4], *ptr = args_copy;

    /* only accept commands from valid sources */
    if (huser_get_level(sender) > H_ADMIN)
        return;

    if (returntype && !strncmp(args, helpmodnick->nick, strlen(helpmodnick->nick)))
    {
        if (!args[1])
            return;
        else
            args+=strlen(helpmodnick->nick)+1;
    }

    if (*args == '-' || *args == '?')
        args++;

    strncpy(args_copy, args, (sizeof(args_copy) - 1));
    args_copy[sizeof(args_copy) - 1] = '\0';

    /* FIX stringituki */
    while (argc < (H_CMD_MAX_ARGS + 4))
    {
        while (isspace(*ptr) && *ptr)
            ptr++;

        if (*ptr == '\0')
            break;

        if (*ptr == '"' && strchr(ptr+1, '"'))
        { /* string support */
            parsed_args[argc++] = ptr+1;
            ptr = strchr(ptr+1, '"');

            *(ptr++) = '\0';

            while (!isspace(*ptr) && *ptr)
                ptr++;

            if (*ptr == '\0')
                break;
        }
        else
        {
            parsed_args[argc++] = ptr;

            while (!isspace(*ptr) && *ptr)
                ptr++;

            if (*ptr == '\0')
                break;

            *(ptr++) = '\0';
        }
    }

    if (!argc)
        return;

    /* old H compatibility */
    if (sscanf(parsed_args[0], "%d", &useless_var) && returntype == NULL)
    {
        helpmod_cmd_help(sender, NULL, parsed_args[0], 1, NULL);
	return;
    }

    {
        char *ostr = args, **argv = (char**)&parsed_args;
        hcommand *hcom = hcommand_get(parsed_args[0], huser_get_level(sender));

	if (hcom == NULL)
	{
            controlwall(NO_DEVELOPER, NL_ALL_COMMANDS, "(G) From: %s!%s@%s%s%s: %s",
                        sender->real_user->nick, sender->real_user->ident,
                        sender->real_user->host->name->content, IsAccount(sender->real_user)?"/":"",
                        IsAccount(sender->real_user)?sender->real_user->authname:"", args);            
	    if ((returntype == NULL) ||
		(sender->account != NULL && !(sender->account->flags & H_NO_CMD_ERROR)))
		helpmod_reply(sender, returntype, "Unknown command '%s', please see showcommands for a list of all commands available to you", parsed_args[0]);
	}
        else
        {
            controlwall(NO_DEVELOPER, NL_ALL_COMMANDS, "(G) From: %s!%s@%s%s%s: %s", sender->real_user->nick, sender->real_user->ident, sender->real_user->host->name->content, IsAccount(sender->real_user)?"/":"", IsAccount(sender->real_user)?sender->real_user->authname:"", args);
            SKIP_WORD;
            hcom->function(sender, returntype, ostr, argc, argv);
        }
    }
}

#undef SKIP_WORD
#undef DEFINE_HCHANNEL
#undef HCHANNEL_VERIFY_AUTHORITY
