#include <stdlib.h>
#include <string.h>

#include "../localuser/localuser.h"
#include "../lib/irc_string.h"

#include "hchanban.h"
#include "helpmod.h"

hchanban *hchanban_add(hchannel* hchan, const char* banmask, time_t expiration)
{
    hchanban *tmp = (hchanban*)malloc(sizeof(hchanban));

    tmp->hchan = hchan;
    tmp->banmask = getsstring(banmask, strlen(banmask));
    tmp->expiration = expiration;

    tmp->next = hchanbans;
    hchanbans = tmp;

    return hchanbans;
}

hchanban *hchanban_del(hchanban *hcban)
{
    hchanban **ptr = &hchanbans;
    for (;*ptr;ptr = &(*ptr)->next)
        if (*ptr == hcban)
        {
            hchanban *tmp = (*ptr)->next;
            freesstring((*ptr)->banmask);
            free(*ptr);
            *ptr = tmp;

            return NULL;
        }
    return hcban;
}

hchanban *hchanban_del_all(void)
{
    while (hchanbans)
        hchanban_del(hchanbans);

    return NULL;
}

hchanban *hchanban_get(hchannel* hchan, const char* banmask)
{
    hchanban *ptr = hchanbans;
    for (;ptr;ptr = ptr->next)
        if (ptr->hchan == hchan && !ircd_strcmp(ptr->banmask->content, banmask))
            return ptr;
    return NULL;
}

void hchanban_schedule_entry(hchanban* item)
{
    if (hchannel_is_valid(item->hchan))
        helpmod_setban(item->hchan, item->banmask->content, 0, MCB_DEL, HNOW);

    hchanban_del(item);
}
