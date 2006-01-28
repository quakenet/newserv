#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hconf.h"
#include "helpmod.h"
#include "hterm.h"

static int hconf_version;

static void helpmod_line_fix(char **ptr)
{
    while (isspace(**ptr))
        (*ptr)++;

    if (strchr(*ptr, '\n') != NULL)
        *strchr(*ptr, '\n') = '\0';

    if (strchr(*ptr, '\r') != NULL)
        *strchr(*ptr, '\r') = '\0';
}


int helpmod_config_read(const char *fname)
{
    FILE *in;
    char buf[512], *ptr;

    /* Assume G 2.0 configuration */
    hconf_version = HELPMOD_VERSION_2_0;

    if ((in = fopen(fname,"rt")) == NULL)
        return -1;

    while (!feof(in))
    {
        ptr = (char*)buf;

        fgets(buf, 512, in);
        if (feof(in))
            break;

        helpmod_line_fix(&ptr);

        /* support comments */
        if (*ptr == '%')
            continue;
        if (!*ptr)
            continue;

        /* check what kind of a line it was */
	if (!strcmp(ptr, "version"))
	{ /* If no version is present, then the assumed G 2.0 version is used */
	    if (helpmod_config_read_version(in))
		Error("helpmod", ERR_WARNING, "Reading of database entry 'version' failed");

	    if (hconf_version > HELPMOD_VERSION_INTERNAL)
		Error("helpmod", ERR_WARNING, "Database version is higher than the current version");
	}
	else if (!strcmp(ptr, "channel"))
        {
            if (helpmod_config_read_channel(in))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'channel' failed");
        }
        else if (!strcmp(ptr, "account"))
        {
            if (helpmod_config_read_account(in))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'account' failed");
        }
        else if (!strcmp(ptr, "ban"))
        {
            if (helpmod_config_read_ban(in))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'ban' failed");
        }
        else if (!strcmp(ptr, "lamercontrol profile"))
        {
            if (helpmod_config_read_hlc_profile(in))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'lamercontrol_profile' failed");
        }
        else if (!strcmp(ptr, "term"))
        {
            if (helpmod_config_read_term(in, NULL))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'term' failed");
        }
        else if (!strcmp(ptr, "globals"))
        {
            if (helpmod_config_read_globals(in))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'globals' failed");
        }
        else if (!strcmp(ptr, "report"))
        {
            if (helpmod_config_read_report(in))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'report' failed");
        }
        else if (!strcmp(ptr, "ticket"))
        {
            if (helpmod_config_read_ticket(in))
                Error("helpmod", ERR_WARNING, "Reading of database entry 'ticket' failed");
        }
	else
            Error("helpmod", ERR_WARNING, "Unknown database entry '%s'", ptr);
    }

    hchannels_match_accounts();

    fclose(in);
    return 0;
}

int helpmod_config_write(const char *fname)
{
    FILE *out;
    time_t timer = time(NULL);

    if ((out = fopen(fname,"wt")) == NULL)
        return -1;

    fprintf(out, "%% G2 version %s database\n", HELPMOD_VERSION);
    fprintf(out, "%% %s\n\n", asctime(localtime(&timer)));


    { /* version */
	fprintf(out, "%% G internal version\n");
        fprintf(out, "version\n");
        helpmod_config_write_version(out);
    }

    { /* globals */

        fprintf(out,"\n%% global variables\n");
        fprintf(out,"%%  hstat_cycle\n");
        fprintf(out,"globals\n");
        helpmod_config_write_globals(out);
    }

    { /* lamercontrol profiles */
        hlc_profile *ptr = hlc_profiles;

        fprintf(out,"%% lamercontrol profile structure:\n");
        fprintf(out,"%%  X (string):\n");
        fprintf(out,"%%  Y (string):\n");
        fprintf(out,"%%  Z (int):\n");

        for(;ptr;ptr=ptr->next)
        {
            fprintf(out, "lamercontrol profile\n");
            helpmod_config_write_hlc_profile(out, ptr);
        }
    }

    { /* channels */
        hchannel *ptr = hchannels;

        fprintf(out,"\n%% channel structure:\n");
        fprintf(out,"%%  name (string):\n");
        fprintf(out,"%%  flags (integer):\n");
        fprintf(out,"%%  welcome message (string):\n");
        fprintf(out,"%%  lamercontrol profile (string):\n");

        for(;ptr;ptr=ptr->next)
        {
            fprintf(out, "channel\n");
            helpmod_config_write_channel(out, ptr);
        }
    }

    fprintf(out,"\n");

    { /* accounts */
        haccount *ptr = haccounts;

        fprintf(out,"\n%% account structure:\n");
        fprintf(out,"%%  name (string):\n");
        fprintf(out,"%%  level (integer) flags (integer) last_activity (integer):\n");

        for(;ptr;ptr=ptr->next)
        {
            fprintf(out, "account\n");
            helpmod_config_write_account(out, ptr);
        }
    }

    fprintf(out,"\n");

    { /* bans */
        hban *ptr = hbans;

        fprintf(out,"\n%% ban structure:\n");
        fprintf(out,"%%  banmask (string):\n");
        fprintf(out,"%%  reason (string):\n");
        fprintf(out,"%%  expiration (int):\n");

        for(;ptr;ptr=ptr->next)
        {
            fprintf(out, "ban\n");
            helpmod_config_write_ban(out, ptr);
        }
    }

    { /* global terms */
        hterm *ptr = hterms;

        fprintf(out,"\n%% term structure:\n");
        fprintf(out,"%%  name (string):\n");
        fprintf(out,"%%  description (string):\n");

        for(;ptr;ptr=ptr->next)
        {
            fprintf(out, "term\n");
            helpmod_config_write_term(out, ptr);
        }
    }

    { /* tickets */
        hchannel *hchan;
        hticket *htick;

        fprintf(out, "\n%% ticket structure:\n");
        fprintf(out, "%%  channel (string)\n");
        fprintf(out, "%%  authname (string)\n");
        fprintf(out, "%%  expiration time (int)\n");

        for (hchan = hchannels;hchan;hchan = hchan->next)
            for (htick = hchan->htickets;htick;htick = htick->next)
            {
                fprintf(out, "ticket\n");
                helpmod_config_write_ticket(out, htick, hchan);
            }
    }

    { /* reports */
        hchannel *hchan;

        fprintf(out, "\n%% report structure:\n");
        fprintf(out, "%%  channel reported\n");
        fprintf(out, "%%  channel reported to\n");

        for (hchan = hchannels;hchan;hchan = hchan->next)
        {
            if ((hchan->flags & H_REPORT) && hchannel_is_valid(hchan->report_to))
            {
                fprintf(out, "report\n");
                helpmod_config_write_report(out, hchan);
            }
        }
    }

    fclose(out);

    return 0;
}

int helpmod_config_read_channel(FILE *in)
{
    hchannel *hchan;

    char buf[256],*ptr=(char*)buf;
    int flags, entries, idlekick, i;
    /* name */
    fgets(buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    hchan = hchannel_add(ptr);
    /* flags */
    fgets((ptr = buf), 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%x", (unsigned int*)&flags) != 1)
        return -1;

    hchan->flags = flags;
    /* welcome message */
    fgets((ptr = buf), 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    strcpy(hchan->welcome, ptr);

    /* lamercontrol profile */
    fgets((ptr = buf), 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    hchan->lc_profile = hlc_get(ptr);

    if (hconf_version >= HELPMOD_VERSION_2_11)
    {
	fgets((ptr = buf), 256, in);
	if (feof(in))
	    return -1;
	helpmod_line_fix(&ptr);

	if (sscanf(ptr, "%d", &idlekick) != 1)
	    return -1;

        hchan->max_idle = idlekick;

	fgets((ptr = buf), 256, in);
	if (feof(in))
	    return -1;
	helpmod_line_fix(&ptr);

        hchan->ticket_message = getsstring(ptr,strlen(ptr));
    }

    /* censor entries for channel, a bit complex */
    fgets((ptr = buf), 256, in);
    if (feof(in))
        return -1;

    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d", &entries) != 1)
        return -1;
    for (i = 0;i<entries;i++)
    {
	char buf2[512], *ptr2;
	int type;

        fgets((ptr = buf), 256, in);
	if (feof(in))
            return -1;
        helpmod_line_fix(&ptr);

	if (hconf_version >= HELPMOD_VERSION_2_10)
	{
	    if (!sscanf(ptr, "%d", &type))
		return -1;

	    fgets((ptr = buf), 256, in);
	    if (feof(in))
		return -1;
	    helpmod_line_fix(&ptr);
	}
	else
            type = HCENSOR_KICK;

        fgets((ptr2 = buf2), 256, in);
        if (feof(in))
            return -1;
        helpmod_line_fix(&ptr2);

	if (ptr2[0] == '\0')
            ptr2 = NULL;

        hcensor_add(&hchan->censor, ptr, ptr2, type);
    }
    /* channel specific hterms */
    fgets((ptr = buf), 256, in);
    if (feof(in))
        return -1;

    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d", &entries) != 1)
        return -1;
    for (i=0;i<entries;i++)
        helpmod_config_read_term(in, &hchan->channel_hterms);

    helpmod_config_read_chanstats(in, hchan->stats);

    /* needs to be done here */
    hchannel_mode_check(hchan);

    return 0;
}

int helpmod_config_write_channel(FILE *out, hchannel *target)
{
    fprintf(out, "\t%s\n", hchannel_get_name(target));
    fprintf(out, "\t%x\n", target->flags & 0x0FFFFFF);
    fprintf(out, "\t%s\n", target->welcome);
    if (target->lc_profile == NULL)
        fprintf(out, "\t(null)\n");
    else
	fprintf(out, "\t%s\n", target->lc_profile->name->content);

    fprintf(out, "\t%d\n", target->max_idle);

    if (target->ticket_message == NULL)
	fprintf(out, "\t(null)\n");
    else
	fprintf(out, "\t%s\n", target->ticket_message->content);

    fprintf(out, "\t%d %% censor\n", hcensor_count(target->censor));
    {
        hcensor *hcens = target->censor;
        for (;hcens;hcens = hcens->next)
	{
            fprintf(out, "\t\t%d\n", hcens->type);
            fprintf(out, "\t\t%s\n", hcens->pattern->content);
	    fprintf(out, "\t\t%s\n", hcens->reason?hcens->reason->content:"");
        }
    }

    fprintf(out, "\t%d %% terms\n", hterm_count(target->channel_hterms));
    {
        hterm *tmp = target->channel_hterms;
        for (;tmp;tmp = tmp->next)
        {
            helpmod_config_write_term(out, tmp);
        }
    }

    helpmod_config_write_chanstats(out, target->stats);

    return 0;
}


int helpmod_config_read_account(FILE *in)
{
    haccount *hack;
    int nstats;

    char buf[256],*ptr=(char*)buf;
    int flags, level, last_activity = time(NULL);

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    hack = haccount_add(ptr, H_PEON);

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);
    if (hconf_version < HELPMOD_VERSION_2_11)
    {
	if (sscanf(ptr, "%x %x", (unsigned int*)&level, (unsigned int*)&flags) != 2)
	    return -1;
    }
    else
	if (sscanf(ptr, "%x %x %x", (unsigned int*)&level, (unsigned int*)&flags, (unsigned int *)&last_activity) != 3)
	    return -1;

    if (hconf_version < HELPMOD_VERSION_2_16)
    { /* For the new friend userlevel */
	if (level >= H_FRIEND)
	    level++;
    }

    hack->level = level;
    hack->flags = flags;
    hack->last_activity = last_activity;

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d", &nstats) != 1)
        return -1;

    while (nstats)
    {
        hstat_account *tmp = hack->stats;
        hack->stats = (hstat_account*)malloc(sizeof(hstat_account));
        hack->stats->next = tmp;
        if (helpmod_config_read_stats(in, hack->stats) == -1)
            return -1;
        nstats--;
    }

    return 0;
}

int helpmod_config_write_account(FILE *out, haccount *target)
{
    hstat_account *tmp;
    fprintf(out, "\t%s\n", target->name->content);
    fprintf(out, "\t%x\t%x\t%x\n", target->level, target->flags, (unsigned int)target->last_activity);

    fprintf(out, "\t%d %% statistics for this channel\n", hstat_account_count(target->stats));
    for (tmp = target->stats;tmp;tmp = tmp->next)
        helpmod_config_write_stats(out, tmp);

    return 0;
}

int helpmod_config_read_ban(FILE *in)
{
    char buf1[512], buf2[512], buf3[512], *ptr1, *ptr2, *ptr3;
    int tmp;

    fgets(ptr1 = buf1, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr1);

    fgets(ptr2 = buf2, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr2);

    fgets(ptr3 = buf3, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr3);

    if (sscanf(ptr3, "%d", &tmp) != 1)
        return -1;

    hban_add(ptr1, ptr2, tmp, 0);

    return 0;
}

int helpmod_config_write_ban(FILE *out, hban *hb)
{
    fprintf(out, "\t%s\n", bantostring(hb->real_ban));
    fprintf(out, "\t%s\n", hb->reason?hb->reason->content:NULL);
    fprintf(out, "\t%u\n", (unsigned int)hb->expiration);
    return 0;
}

int helpmod_config_read_hlc_profile(FILE *in)
{
    hlc_profile *hlc_prof;
    int tmp[3];
    float tmp_float;
    char buf[256],*ptr=(char*)buf;

    /* name */
    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if ((hlc_prof = hlc_add(ptr)) == NULL)
	return -1;

    /*  caps */
    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d %d", &tmp[0], &tmp[1]) != 2)
        return -1;

    hlc_prof->caps_max_percentage = tmp[0];
    hlc_prof->caps_min_count = tmp[1];

    /* repeating */
    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d %d", &tmp[0], &tmp[1]) != 2)
        return -1;

    hlc_prof->repeats_max_count = tmp[0];
    hlc_prof->repeats_min_length = tmp[1];

    /* character / symbol repeat */
    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d %d %d", &tmp[0], &tmp[1], &tmp[2]) != 3)
        return -1;

    hlc_prof->symbol_repeat_max_count = tmp[0];
    hlc_prof->character_repeat_max_count = tmp[1];
    hlc_prof->symbol_max_count = tmp[1];

    /* flood, spam const, spam */
    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d %f %d", &tmp[0], &tmp_float, &tmp[1]) != 3)
        return -1;

    hlc_prof->tolerance_flood = tmp[0];
    hlc_prof->constant_spam = tmp_float;
    hlc_prof->tolerance_spam = tmp[1];

    /* tolerances */
    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d %d %d", &tmp[0], &tmp[1], &tmp[2]) != 3)
        return -1;

    hlc_prof->tolerance_warn = tmp[0];
    hlc_prof->tolerance_kick = tmp[1];
    hlc_prof->tolerance_remove = tmp[2];

    return 0;
}

/* we use extended buffers here */
int helpmod_config_read_term(FILE *in, hterm** target)
{
    char buf1[2048], buf2[2048], *ptr1 = buf1, *ptr2 = buf2;

    /* name */
    fgets(buf1, 2048, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr1);

    /* description */
    fgets(buf2, 2048, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr2);

    /* add to public domain */
    hterm_add(target, ptr1, ptr2);

    return 0;
}

int helpmod_config_write_term(FILE *out, hterm *htrm)
{
    fprintf(out, "\t%s\n", htrm->name->content);
    fprintf(out, "\t%s\n", htrm->description->content);
    return 0;
}

int helpmod_config_write_hlc_profile(FILE *out, hlc_profile *hlc_prof)
{
    fprintf(out, "\t%s\n", hlc_prof->name->content);
    fprintf(out, "\t%d %d\n", hlc_prof->caps_max_percentage, hlc_prof->caps_min_count);
    fprintf(out, "\t%d %d\n", hlc_prof->repeats_max_count, hlc_prof->repeats_min_length);
    fprintf(out, "\t%d %d %d\n", hlc_prof->symbol_repeat_max_count, hlc_prof->character_repeat_max_count, hlc_prof->symbol_max_count);
    fprintf(out, "\t%d %.3f %d\n", hlc_prof->tolerance_flood, hlc_prof->constant_spam, hlc_prof->tolerance_spam);
    fprintf(out, "\t%d %d %d\n", hlc_prof->tolerance_warn, hlc_prof->tolerance_kick, hlc_prof->tolerance_remove);
    return 0;
}

int helpmod_config_read_chanstats(FILE *in, hstat_channel *hs_chan)
{
    char buf[256],*ptr=(char*)buf;
    int i;
    hstat_channel_entry *entry;

    for (i=0;i<17;i++)
    {
        fgets(ptr = buf, 256, in);
        if (feof(in))
            return -1;
        helpmod_line_fix(&ptr);

        if (i < 7) /* days */
            entry = &hs_chan->week[(hstat_day() + i) % 7];
        else /* weeks */
            entry = &hs_chan->longterm[(hstat_week() + (i-7)) % 10];

	if (hconf_version < HELPMOD_VERSION_2_10)
	{
	    if (sscanf(buf, "%d %d %d %d %d %d", &entry->time_spent, &entry->prime_time_spent, &entry->joins, &entry->queue_use, &entry->lines, &entry->words) != 6)
            return -1;
	}
	else
	{
	    if (sscanf(buf, "%d %d %d %d %d %d %d %d", &entry->active_time, &entry->staff_active_time, &entry->time_spent, &entry->prime_time_spent, &entry->joins, &entry->queue_use, &entry->lines, &entry->words) != 8)
		return -1;
	}
    }
    return 0;
}

int helpmod_config_write_chanstats(FILE *out, hstat_channel *hs_chan)
{
    int i;
    hstat_channel_entry *entry;

    for (i=0;i<17;i++)
    {
        if (i < 7) /* days */
            entry = &hs_chan->week[(hstat_day() + i) % 7];
        else /* weeks */
            entry = &hs_chan->longterm[(hstat_week() + (i-7)) % 10];

        fprintf(out, "\t%d %d %d %d %d %d %d %d\n", entry->active_time, entry->staff_active_time, entry->time_spent, entry->prime_time_spent, entry->joins, entry->queue_use, entry->lines, entry->words);
    }
    return 0;
}

int helpmod_config_read_stats(FILE *in, hstat_account *hs_acc)
{
    char buf[256],*ptr=(char*)buf;
    int i;
    hstat_account_entry *entry;

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    hs_acc->hchan = hchannel_get_by_name(ptr);

    if (hs_acc->hchan == NULL)
        return -1;

    for (i=0;i<17;i++)
    {
        fgets(ptr = buf, 256, in);
        if (feof(in))
            return -1;
        helpmod_line_fix(&ptr);

        if (i < 7) /* days */
            entry = &hs_acc->week[(hstat_day() + i) % 7];
        else /* weeks */
            entry = &hs_acc->longterm[(hstat_week() + (i-7)) % 10];

        if (sscanf(buf, "%d %d %d %d", &entry->time_spent, &entry->prime_time_spent, &entry->lines, &entry->words) != 4)
            return -1;
    }
    return 0;
}

int helpmod_config_write_stats(FILE *out, hstat_account *hs_acc)
{
    int i;
    hstat_account_entry *entry;

    fprintf(out, "\t%s\n", hchannel_get_name(hs_acc->hchan));

    for (i=0;i<17;i++)
    {
        if (i < 7) /* days */
            entry = &hs_acc->week[(hstat_day() + i) % 7];
        else /* weeks */
            entry = &hs_acc->longterm[(hstat_week() + (i-7)) % 10];

        fprintf(out, "\t%d %d %d %d\n", entry->time_spent, entry->prime_time_spent, entry->lines, entry->words);
    }
    return 0;
}

int helpmod_config_read_globals(FILE *in)
{
    char buf[256],*ptr=(char*)buf;

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d", &hstat_cycle) != 1)
        return -1;

    return 0;
}

int helpmod_config_write_globals(FILE *out)
{
    fprintf(out, "\t%d\n", hstat_cycle);
    return 0;
}

int helpmod_config_read_report(FILE *in)
{
    char buf[256], *ptr=(char*)buf;
    hchannel *tmp1, *tmp2;

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    tmp1 = hchannel_get_by_name(ptr);

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    tmp2 = hchannel_get_by_name(ptr);

    if (hchannel_is_valid(tmp1) && hchannel_is_valid(tmp2))
        tmp1->report_to = tmp2;

    return 0;
}

int helpmod_config_write_report(FILE *out, hchannel *hchan)
{
    fprintf(out, "\t%s\n", hchannel_get_name(hchan));
    fprintf(out, "\t%s\n", hchannel_get_name(hchan->report_to));
    return 0;
}

int helpmod_config_read_ticket(FILE *in)
{
    char buf[256], buf2[256], buf3[64], *ptr=(char*)buf;
    unsigned int tmp;

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    strcpy(buf2, ptr);

    fgets(ptr = buf, 64, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    strcpy(buf3, ptr);

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (!sscanf(ptr, "%u", &tmp))
        return -1;

    if (tmp > time(NULL))
    {
	if (hconf_version < HELPMOD_VERSION_2_17)
	    hticket_add(buf3, tmp, hchannel_get_by_name(buf2), NULL);
	else
	{
	    fgets(ptr = buf, 256, in);
	    if (feof(in))
		return -1;
	    helpmod_line_fix(&ptr);

	    if (*ptr == '\0')
		hticket_add(buf3, tmp, hchannel_get_by_name(buf2), NULL);
	    else
                hticket_add(buf3, tmp, hchannel_get_by_name(buf2), ptr);
	}
    }

    return 0;
}

int helpmod_config_write_ticket(FILE *out, hticket *htick, hchannel *hchan)
{
    fprintf(out, "\t%s\n", hchannel_get_name(hchan));
    fprintf(out, "\t%s\n", htick->authname);
    fprintf(out, "\t%u\n", (unsigned int)htick->time_expiration);
    if (htick->message)
	fprintf(out, "\t%s\n", htick->message->content);
    else
        fprintf(out, "\n");

    return 0;
}

int helpmod_config_read_version(FILE *in)
{
    char buf[256], *ptr=(char*)buf;

    fgets(ptr = buf, 256, in);
    if (feof(in))
        return -1;
    helpmod_line_fix(&ptr);

    if (sscanf(ptr, "%d", &hconf_version) != 1)
        return -1;

    return 0;
}

int helpmod_config_write_version(FILE *out)
{
    fprintf(out, "\t%d\n", HELPMOD_VERSION_INTERNAL);

    return 0;
}

void helpmod_config_scheduled_events(void)
{
    rename(HELPMOD_DEFAULT_DB, HELPMOD_DEFAULT_DB".old");
    helpmod_config_write(HELPMOD_DEFAULT_DB);
}
