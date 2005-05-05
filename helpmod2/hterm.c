#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "../lib/sstring.h"

#include "hterm.h"

#include "hgen.h"

hterm *hterm_add(hterm** ptr, const char *name, const char *desc)
{
    hterm *htrm;

    if (ptr == NULL)
        ptr = &hterms;

    if (name == NULL || desc == NULL || hterm_get(*ptr, name))
        return NULL;

    htrm = (hterm*)malloc(sizeof(htrm));
    htrm->name = getsstring(name, strlen(name));
    htrm->description = getsstring(desc, strlen(desc));

    htrm->next = *ptr;
    *ptr = htrm;

    return htrm;
}

hterm *hterm_get(hterm *source, const char *str)
{
    hterm *ptr;
    if (source == NULL)
        ptr = hterms;
    else
        ptr = source;
    for (;ptr;ptr = ptr->next)
        if (!ci_strcmp(ptr->name->content, str))
            return ptr;
    if (source != NULL)
        return hterm_get(NULL, str);
    else
        return NULL;
}

hterm *hterm_find(hterm *source, const char *str)
{
    hterm *ptr;
    char buffer[512];

    if (source == NULL)
        source = hterms;

    sprintf(buffer, "*%s*", str);

    for (ptr = source;ptr;ptr = ptr->next)
        if (strregexp(ptr->name->content, buffer))
            return ptr;
    for (ptr = source;ptr;ptr = ptr->next)
        if (strregexp(ptr->description->content, buffer))
            return ptr;
    if (source == NULL)
        return hterm_find(NULL, str);
    else
        return NULL;
}

hterm *hterm_get_and_find(hterm *source, const char *str)
{
    /*hterm *ptr = hterm_get(source, str);
    if (ptr == NULL)
        ptr = hterm_find(source, str);
        return ptr;*/
    /* search order: get source, get NULL, find source, find NULL */
    hterm *ptr;
    ptr = hterm_get(source, str);
    if (ptr != NULL)
        return ptr;
    ptr = hterm_get(NULL, str);
    if (ptr != NULL)
        return ptr;
    ptr = hterm_find(source, str);
    if (ptr != NULL)
        return ptr;
    ptr = hterm_find(NULL, str);
    return ptr;
}

hterm *hterm_del(hterm** start, hterm *htrm)
{
    hterm** ptr = start;

    if (start == NULL)
        ptr = &hterms;

    for (;*ptr;ptr = &(*ptr)->next)
        if (*ptr == htrm)
        {
            hterm *tmp = (*ptr)->next;
            freesstring((*ptr)->name);
            freesstring((*ptr)->description);
            free(*ptr);
            *ptr = tmp;

            return NULL;
        }

    if (start != NULL)
        return hterm_del(NULL, htrm);
    else
        return htrm;
}

void hterm_del_all(hterm **source)
{
    if (source == NULL)
        source = &hterms;

    while (*source)
        hterm_del(source, *source);
}

int hterm_count(hterm* ptr)
{
    int i = 0;
    for (;ptr;ptr = ptr->next)
        i++;
    return i;
}
