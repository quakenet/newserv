#include <stdlib.h>
#include <stdio.h> /* for debug */
#include <string.h>

#include "hcensor.h"

#include "hgen.h"

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
        hcens = hcens->next;

    return hcens;
}

hcensor *hcensor_check(hcensor *hcens, const char *str)
{
    for (;hcens;hcens = hcens->next)
        if (strregexp(str, hcens->pattern->content))
            return hcens;
    return NULL;
}

hcensor *hcensor_add(hcensor **hcens, const char *pat, const char *rsn)
{
    hcensor *tmp;

    if (hcensor_get_by_pattern(*hcens, pat))
        return NULL;

    tmp = malloc(sizeof(hcensor));
    tmp->next = *hcens;

    tmp->pattern = getsstring(pat, strlen(pat));
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
