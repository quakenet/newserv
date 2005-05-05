#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "hchannel.h"
#include "hticket.h"
#include "hgen.h"

hticket *hticket_get(char *authname, struct hchannel_struct *hchan)
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
            *ptr = htick->next;
            free(htick);
            return NULL;
        }

    return htick;
}

hticket *hticket_add(char *authname, time_t expiration, struct hchannel_struct *hchan)
{
    hticket *tmp = hticket_get(authname, hchan);

    if (hchan == NULL)
        return NULL;

    if (tmp != NULL)
        return tmp;

    tmp = (hticket*)malloc(sizeof(struct hticket_struct));

    strcpy(tmp->authname, authname);
    tmp->time_expiration = expiration;
    tmp->next = hchan->htickets;

    hchan->htickets = tmp;

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
    hticket *htick, *next;
    for (hchan = hchannels;hchan;hchan = hchan->next)
        for (htick = hchan->htickets; htick; htick = next)
        {
            next = htick->next;
            if (htick->time_expiration < time(NULL))
                hticket_del(htick, hchan);
        }
}

