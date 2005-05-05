#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "../nick/nick.h"

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

/* static functions, used internally */

static int hlc_violation_handle(hchannel *hchan, huser* husr, int violation)
{
        if (husr->lc[violation] >= hchan->lc_profile->tolerance_remove) /* get rid of the thing */
        {
            const char *banmask = hban_ban_string(husr->real_user, HBAN_REAL_HOST);

            switch (violation)
            {
            case HLC_CAPS:
                hban_add(banmask, "Excessive use of capital letters", time(NULL) + HLC_DEFAULT_BANTIME, 0);
                break;
            case HLC_REPEAT:
                hban_add(banmask, "Excessive repeating", time(NULL) + HLC_DEFAULT_BANTIME, 0);
                break;
            case HLC_CHARACTER_REPEAT:
                hban_add(banmask, "Excessive improper use of language", time(NULL) + HLC_DEFAULT_BANTIME, 0);
                break;
            case HLC_FLOOD:
                hban_add(banmask, "Excessive flooding", time(NULL) + HLC_DEFAULT_BANTIME, 0);
                break;
            case HLC_SPAM:
                hban_add(banmask, "Excessive spamming", time(NULL) + HLC_DEFAULT_BANTIME, 0);
                break;
            }

            if (IsAccount(husr->real_user))
                haccount_add(husr->real_user->authname, H_LAMER);

            return !0;
        }
        if (husr->lc[violation] >= hchan->lc_profile->tolerance_kick) /* get rid of the thing */
        {
            switch (violation)
            {
            case HLC_CAPS:
                helpmod_kick(hchan, husr, "channel rule violation: Excessive use of capital letters");
                break;
            case HLC_REPEAT:
                helpmod_kick(hchan, husr, "channel rule violation: Repeating");
                break;
            case HLC_CHARACTER_REPEAT:
                helpmod_kick(hchan, husr, "channel rule violation: Improper use of language");
                break;
            case HLC_FLOOD:
                helpmod_kick(hchan, husr, "channel rule violation: Flooding");
                break;
            case HLC_SPAM:
                helpmod_kick(hchan, husr, "channel rule violation: Spamming");
                break;
            }
            return !0;
        }
        if (husr->lc[violation] >= hchan->lc_profile->tolerance_warn) /* get rid of the thing */
        {
            switch (violation)
            {
            case HLC_CAPS:
                helpmod_reply(husr, hchan->real_channel, "You are violating the channel rule of %s : Excessive use of capital letters", hchannel_get_name(hchan));
                break;
            case HLC_REPEAT:
                helpmod_reply(husr, hchan->real_channel, "You are violating the channel rule of %s : Repeating", hchannel_get_name(hchan));
                break;
            case HLC_CHARACTER_REPEAT:
                helpmod_reply(husr, hchan->real_channel, "You are violating the channel rule of %s : Improper use of language", hchannel_get_name(hchan));
                break;
            case HLC_FLOOD:
                helpmod_reply(husr, hchan->real_channel, "You are violating the channel rule of %s : Flooding", hchannel_get_name(hchan));
                break;
            case HLC_SPAM:
                helpmod_reply(husr, hchan->real_channel, "You are violating the channel rule of %s : Spamming", hchannel_get_name(hchan));
                break;
            }
        }
    return 0;

}

static int hlc_check_caps(hlc_profile *hlc_prof, huser *husr, const char *line)
{
    int caps = 0;
    int noncaps = 0;
    int i;

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
    int repeats;

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

    if ((husr->flood_val - time(NULL)) >= (hlc_prof->tolerance_flood))
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
int hlc_check(void *hchan_ptr, void *husr_ptr, const char *line)
{
    hchannel *hchan = (hchannel*)hchan_ptr;
    huser *husr = (huser*)husr_ptr;

    if (hchan == NULL || hchan->lc_profile == NULL)
        return 0;

    if (hlc_check_caps(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_CAPS))
            return -1;
    if (hlc_check_repeat(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_REPEAT))
            return -1;
    if (hlc_check_character_repeats(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_CHARACTER_REPEAT))
            return -1;
    if (hlc_check_flood(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_FLOOD))
            return -1;
    if (hlc_check_spam(hchan->lc_profile, husr, line))
        if (hlc_violation_handle(hchan, husr, HLC_SPAM))
            return -1;

    return 0;
}

