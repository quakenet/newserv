#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helpmod.h"
#include "htopic.h"

htopic *htopic_add(htopic** htop, const char* entry, int pos)
{
    htopic *tmp;

    for (;*htop && pos;htop = &(*htop)->next, pos--);

    tmp = *htop;

    *htop = malloc(sizeof(htopic));
    (*htop)->entry = getsstring(entry, strlen(entry));
    (*htop)->next = tmp;

    return *htop;
}

htopic *htopic_del(htopic** htop, htopic* target)
{
    htopic *tmp;
    for (;*htop;htop = &(*htop)->next)
        if (*htop == target)
            break;
    if (!*htop)
        return target;
    tmp = (*htop)->next;
    freesstring((*htop)->entry);
    free(*htop);
    *htop = tmp;


    return NULL;
}

void htopic_del_all(htopic** htop)
{
    while (*htop)
        htopic_del(htop, *htop);
}

htopic *htopic_get(htopic *htop, int pos)
{
    for (;htop && pos;htop = htop->next,pos--);
    return htop;
}

const char *htopic_construct(htopic *htop)
{
    static char buffer[512] = "";

    *buffer = '\0';

    for (;htop;htop = htop->next)
    {
        strcat(buffer, htop->entry->content);
        if (htop->next)
            strcat(buffer, " | ");
    }
    return buffer;
}

int htopic_count(htopic *htop)
{
    int count = 0;
    for (;htop;htop = htop->next)
        count++;
    return count;
}

int htopic_len(htopic *htop)
{
    int len = 0;

    if (htopic_count(htop))
        len+=(htopic_count(htop) - 1) * 3;
    for (;htop;htop = htop->next)
        len+=strlen(htop->entry->content);

    return len;
}

