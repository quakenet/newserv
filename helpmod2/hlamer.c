#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "huser.h"
#include "hban.h"
#include "hchannel.h"
#include "haccount.h"
#include "helpmod.h"

#include "hgen.h"

hlc_profile* hlc_add(const char *str)
{
    hlc_profile *tmp;

    if (hlc_get(str) != NULL)
        return NULL;

    tmp = (hlc_profile*)malloc(sizeof(hlc_profile));
    tmp->name = getsstring(str, strlen(str));
    tmp->next = hlc_profiles;
    hlc_profiles = tmp;
    return tmp;
}

hlc_profile *hlc_del(hlc_profile *hlc_prof)
{
    hchannel *hchan = hchannels;
    hlc_profile **ptr = &hlc_profiles;

    for (;hchan;hchan = hchan->next)
        if (hchan->lc_profile == hlc_prof)
            hchan->lc_profile = NULL;

    for (;*ptr;ptr = &(*ptr)->next)
        if (*ptr == hlc_prof)
        {
            hlc_profile *tmp = (*ptr)->next;
            freesstring ((*ptr)->name);
            free(*ptr);
            *ptr = tmp;
            return NULL;
        }
    return hlc_prof;
}

void hlc_del_all(void)
{
    while (hlc_profiles)
        hlc_del(hlc_profiles);
}

hlc_profile *hlc_get(const char *str)
{
    hlc_profile *hlc_prof = hlc_profiles;
    for (;hlc_prof;hlc_prof = hlc_prof->next)
        if (!ci_strcmp(hlc_prof->name->content, str))
            return hlc_prof;
    return NULL;
}

const char *hlc_get_cname(hlc_component comp)
{
    switch (comp)
    {
    default:
        return "Error, contact strutsi";
    }
}

/* static functions, used internally */

static int hlc_violation_handle(hchannel *hchan, huser* husr, int violation)
{
    if (husr->lc[violation] >= hchan->lc_profile->tolerance_remove) /* get rid of the thing */
    {
	const char *banmask = hban_ban_string(husr->real_user, HBAN_HOST);
	char reason_buffer[128];
        sprintf(reason_buffer, "Excessive violations: %s", hlc_get_violation_name(violation));
	hban_add(banmask, reason_buffer, time(NULL) + HLC_DEFAULT_BANTIME, 0);

	if (IsAccount(husr->real_user))
	    haccount_add(huser_get_auth(husr), H_LAMER);

	return !0;
    }
    if (husr->lc[violation] >= hchan->lc_profile->tolerance_kick) /* get rid of the thing */
    {
	helpmod_kick(hchan, husr, "Channel rule violation: %s", hlc_get_violation_name(violation));
	return !0;
    }
    if (husr->lc[violation] >= hchan->lc_profile->tolerance_warn) /* get rid of the thing */
    {
	helpmod_reply(husr, NULL, "You are violating the channel rule of %s : %s. Continuous violation of this rule will result in you being removed from %s", hchannel_get_name(hchan), hlc_get_violation_name(violation), hchannel_get_name(hchan));
    }
    return 0;

}

static int hlc_check_caps(hlc_profile *hlc_prof, huser *husr, const char *line)
{
    int caps = 0;
    int noncaps = 0;
    int i;
    int firstword;

    if (strchr(line, ' '))
	firstword = strchr(line, ' ') - line;
    else
        firstword = 0;

    /* Handle the thing sent with /me */
    if (!strncmp(line, "\1ACTION", 6 + 1))
	line+=(6 + 1);
    else if (firstword && firstword < NICKLEN + 3)
    {
	char buffer[NICKLEN + 3];
	strncpy(buffer, line, firstword);
	buffer[firstword] = '\0';
	if (buffer[firstword - 1] == ':')
	    buffer[firstword - 1] = '\0';
	if (getnickbynick(buffer))
            line+=firstword + 1;
    }

    for (i = 0;line[i];i++)
    {
        if (isalpha(line[i]))
        {
            if (isupper(line[i]))
                caps++;
            else
                noncaps++;
        }
    }

    if ((noncaps + caps) < hlc_prof->caps_min_count)
        return 0;

    if (((100 * caps) / (caps + noncaps)) >= hlc_prof->caps_max_percentage) /* violation */
    {
        return ++husr->lc[HLC_CAPS];
    }
    else
        return 0;
}

static int hlc_check_repeat(hlc_profile *hlc_prof, huser *husr, const char *line)
{
    if (!strncmp(husr->last_line, line, strlen(husr->last_line)) && (strlen(husr->last_line) >= hlc_prof->repeats_min_length))
        husr->last_line_repeats++;
    else if (!strcmp(husr->last_line, line))
        husr->last_line_repeats++;
    else
        husr->last_line_repeats = 0;

    strcpy(husr->last_line, line);

    if (husr->last_line_repeats >= hlc_prof->repeats_max_count && strlen(line) >= hlc_prof->repeats_min_length) /* violation */
        return ++husr->lc[HLC_REPEAT];
    else
        return 0;
}

static int hlc_check_character_repeats(hlc_profile *hlc_prof, huser *husr, const char *line)
{
    char chr = '\0';
    int i;
    int repeats = 0;

    for (i = 0;line[i];i++)
        if (line[i] == chr)
        {
            repeats++;
            if (repeats >= hlc_prof->character_repeat_max_count && isalnum(chr)) /* violation */
                return ++husr->lc[HLC_CHARACTER_REPEAT];
            else if (repeats >= hlc_prof->symbol_repeat_max_count && ispunct(chr)) /* violation */
                return ++husr->lc[HLC_CHARACTER_REPEAT];
        }
        else
        {
            repeats = 0;
            chr = line[i];
        }
    for (i=0, repeats = 0; line[i]; i++)
    {
        if (ispunct(line[i]))
            repeats++;
        else
            repeats = 0;

        if (repeats >= hlc_prof->symbol_max_count)
            return ++husr->lc[HLC_CHARACTER_REPEAT];
    }
    return 0;
}

static int hlc_check_flood(hlc_profile *hlc_prof, huser *husr, const char *line)
{
    if (husr->flood_val < time(NULL))
        husr->flood_val = time(NULL);
    else
        husr->flood_val++;

    if ((husr->flood_val - time(NULL)) > (hlc_prof->tolerance_flood))
    {
        husr->flood_val = time(NULL);
        return ++husr->lc[HLC_FLOOD];
    }

    return 0;
}

static int hlc_check_spam(hlc_profile *hlc_prof, huser *husr, const char *line)
{
    if (husr->spam_val < (float)time(NULL))
        husr->spam_val = (float)time(NULL);

    husr->spam_val += (hlc_prof->constant_spam * (double)strlen(line));

    if (((int)husr->spam_val - time(NULL)) >= (hlc_prof->tolerance_spam))
    {
        husr->spam_val = time(NULL);
        return ++husr->lc[HLC_SPAM];
    }
    return 0;
}

/* checks a string for lameness, returns non-zero if lameness is present */
int hlc_check(hchannel *hchan, huser* husr, const char *line)
{
    if (hchan == NULL || hchan->lc_profile == NULL)
        return 0;

    if (hlc_check_flood(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_FLOOD))
            return -1;
    if (hlc_check_spam(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_SPAM))
	    return -1;
    if (hlc_check_caps(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_CAPS))
            return -1;
    if (hlc_check_repeat(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_REPEAT))
            return -1;
    if (hlc_check_character_repeats(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_CHARACTER_REPEAT))
            return -1;

    return 0;
}

const char *hlc_get_violation_name(hlc_violation violation)
{
    switch (violation)
    {
    case HLC_CAPS:
	return "Excessive use of capital letters";
    case HLC_REPEAT:
	return "Repeating";
    case HLC_CHARACTER_REPEAT:
	return "Improper use of language";
    case HLC_FLOOD:
	return "Flooding";
    case HLC_SPAM:
	return "Spamming";
    default:
        return "Error, please contact strutsi";
    }
}
