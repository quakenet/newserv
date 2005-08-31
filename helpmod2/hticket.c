#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "hchannel.h"
#include "hticket.h"
#include "hgen.h"

hticket *hticket_get(const char *authname, struct hchannel_struct *hchan)
{
    hticket *tmp;
    if (hchan == NULL)
        return NULL;
    for (tmp = hchan->htickets;tmp;tmp = tmp->next)
        if (!ci_strcmp(authname, tmp->authname))
            return tmp;
    return NULL;
}

hticket *hticket_del(hticket *htick, struct hchannel_struct *hchan)
{
    hticket **ptr;
    for (ptr = &hchan->htickets;*ptr;ptr = &(*ptr)->next)
        if (*ptr == htick)
        {
            hticket *tmp = (*ptr)->next;
            free(*ptr);
            *ptr = tmp;
            return NULL;
        }

    return htick;
}

hticket *hticket_add(const char *authname, time_t expiration, struct hchannel_struct *hchan)
{
    hticket *tmp = hticket_get(authname, hchan), **ptr;

    if (hchan == NULL)
        return NULL;

    if (tmp != NULL)
        return tmp;

    tmp = (hticket*)malloc(sizeof(struct hticket_struct));

    strcpy(tmp->authname, authname);
    tmp->time_expiration = expiration;

    /* find the correct position */
    for (ptr = &hchan->htickets;*ptr && (*ptr)->time_expiration >= expiration;ptr = &(*ptr)->next);

    tmp->next = *ptr;
    *ptr = tmp;

    return tmp;
}

int hticket_count(void)
{
    int count = 0;
    hticket *htick;
    hchannel *hchan;

    for (hchan = hchannels;hchan;hchan = hchan->next)
        for (htick = hchan->htickets;htick;htick = htick->next)
            count++;

    return count;
}

void hticket_remove_expired(void)
{
    hchannel *hchan;
    hticket **tmp;
    for (hchan = hchannels;hchan;hchan = hchan->next)
    {
        tmp = &hchan->htickets;
        while (*tmp)
            if ((*tmp)->time_expiration < time(NULL))
                hticket_del(*tmp, hchan);
            else
                tmp = &(*tmp)->next;
    }
}
