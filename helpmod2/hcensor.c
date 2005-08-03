#include <stdlib.h>
#include <stdio.h> /* for debug */
#include <string.h>

#include "hcensor.h"
#include "hcommands.h"
#include "helpmod.h"
#include "hgen.h"
#include "hban.h"

hcensor *hcensor_get_by_pattern(hcensor *hcens, const char *pat)
{
    for (;hcens;hcens = hcens->next)
        if (!ci_strcmp(pat, hcens->pattern->content))
            return hcens;
    return NULL;
}

hcensor *hcensor_get_by_index(hcensor *hcens, int index)
{
    if (index > 512) /* some sanity */
        return NULL;

    while (index && hcens)
    {
	hcens = hcens->next;
	index--;
    }

    return hcens;
}

hcensor *hcensor_check(hcensor *hcens, const char *str)
{
    for (;hcens;hcens = hcens->next)
        if (strregexp(str, hcens->pattern->content))
            return hcens;
    return NULL;
}

hcensor *hcensor_add(hcensor **hcens, const char *pat, const char *rsn, hcensor_type type)
{
    hcensor *tmp;

    if (hcensor_get_by_pattern(*hcens, pat))
        return NULL;

    tmp = malloc(sizeof(hcensor));
    tmp->next = *hcens;

    tmp->pattern = getsstring(pat, strlen(pat));
    tmp->type = type;

    if (rsn)
        tmp->reason = getsstring(rsn, strlen(rsn));
    else
        tmp->reason = NULL;

    *hcens = tmp;

    return tmp;
}

hcensor *hcensor_del(hcensor **hcens, hcensor *ptr)
{
    for (;*hcens;hcens = &(*hcens)->next)
        if (*hcens == ptr)
        {
            hcensor *tmp = (*hcens)->next;
            freesstring((*hcens)->pattern);
            freesstring((*hcens)->reason);
            free (*hcens);
            *hcens = tmp;
            return NULL;
        }
    return ptr;
}

void hcensor_del_all(hcensor **hcens)
{
    while (*hcens)
        hcensor_del(hcens, *hcens);
}

int hcensor_count(hcensor *hcens)
{
    int count = 0;
    for (;hcens;hcens = hcens->next, count++);
    return count;
}

int hcensor_match(hchannel *hchan, huser *husr, hcensor *hcens)
{
    switch (hcens->type)
    {
    case HCENSOR_WARN:
        if (hcens->reason)
	    helpmod_reply(husr, NULL, hcens->reason->content);
	return 0;
    case HCENSOR_KICK:
	helpmod_kick(hchan, husr, hcens->reason?hcens->reason->content:"Improper user");
        return !0;
    case HCENSOR_BAN:
	hban_add(hban_ban_string(husr->real_user, HBAN_HOST), hcens->reason?hcens->reason->content:"Censor violation", HCMD_OUT_DEFAULT + time(NULL), 1);
	return !0;
    default:
	Error("helpmod", ERR_ERROR, "Unknown censor type %d", hcens->type);
        return !0;
    }
}

const char *hcensor_get_typename(hcensor_type type)
{
    switch (type)
    {
    case HCENSOR_WARN:
	return "warn";
    case HCENSOR_KICK:
	return "kick";
    case HCENSOR_BAN:
        return "ban";
    default:
        return "error";
    }
}
