#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "../nick/nick.h"

#include "huser.h"
#include "hchannel.h"
#include "haccount.h"
#include "helpmod.h"
#include "hban.h"
#include "hgen.h"

huser *huser_add(nick *nck)
{
    huser *tmp;
    int i;
    assert(huser_get(nck) == NULL);

    tmp = malloc(sizeof(huser));
    tmp->real_user = nck;
    tmp->last_activity = time(NULL);
    tmp->flags = H_USRFLAGS_DEFAULT;
    tmp->state = helpmod_base;
    tmp->hchannels = NULL;

    if (IsAccount(nck))
        tmp->account = haccount_get_by_name(nck->authname);
    else
        tmp->account = NULL;

    tmp->hchannels = NULL;

    for (i=0;i<5;i++)
        tmp->lc[i] = 0;

    tmp->last_line_repeats = 0;
    tmp->last_line[0] = '\0';

    tmp->flood_val = 0;
    tmp->spam_val = 0.0;

    tmp->next = husers;
    husers = tmp;

    return tmp;
}

void huser_del(huser *husr)
{
    huser **ptr = &husers;
    if (!husr)
        return;

    for (;*ptr;ptr = &(*ptr)->next)
        if (*ptr  == husr)
        {
            huser *tmp = (*ptr)->next;
            free(*ptr);
            *ptr = tmp;

            return;
        }
}

void huser_del_all(void)
{
    huser **ptr = &husers;

    while (*ptr)
        huser_del (*ptr);
}

huser *huser_get(nick *nck)
{
    huser *tmp = husers;

    if (nck == NULL)
        return NULL;

    for (;tmp;tmp = tmp->next)
        if (tmp->real_user == nck)
            return tmp;
    return NULL;
}

void huser_activity(huser *husr, hchannel *hchan)
{
    huser_channel *huserchan;

    husr->last_activity = time(NULL);
    if (husr->account)
        husr->account->last_activity = time(NULL);
    if (hchan != NULL)
        if ((huserchan = huser_on_channel(husr, hchan)) != NULL)
            huserchan->last_activity = time(NULL);
}

huser_channel *huser_add_channel(huser *husr, hchannel *hchan)
{
    huser_channel *tmp;
    assert(huser_on_channel(husr, hchan) == NULL);

    tmp = (huser_channel*)malloc(sizeof(huser_channel));

    tmp->next = husr->hchannels;
    tmp->hchan = hchan;
    tmp->responsible_oper = NULL;
    tmp->last_activity = time(NULL);
    tmp->flags = 0;

    husr->hchannels = tmp;

    assert(huser_on_channel(husr, hchan) != NULL);

    return tmp;
}

void huser_del_channel(huser *husr, hchannel *hchan)
{
    huser_channel **tmp = &husr->hchannels;
    assert(huser_on_channel(husr, hchan) != NULL);

    for (;*tmp;tmp = &(*tmp)->next)
        if ((*tmp)->hchan == hchan)
        {
            huser_channel *ptr = (*tmp)->next;

            free(*tmp);
            *tmp = ptr;
            break;
        }

    assert(huser_on_channel(husr, hchan) == NULL);
}

huser_channel *huser_on_channel(huser *husr, hchannel *hchan)
{
    huser_channel *tmp = husr->hchannels;

    for (;tmp;tmp = tmp->next)
        if (tmp->hchan == hchan)
            return tmp;
    return NULL;
}

void huser_clear_inactives(void)
{
    huser **ptr = &husers;
    while (*ptr)
        if (!(*ptr)->hchannels && (time(NULL) - (*ptr)->last_activity) > HELPMOD_USER_TIMEOUT)
            huser_del (*ptr);
        else
            ptr = &(*ptr)->next;
}

int huser_count(void)
{
    int count = 0;
    huser *tmp = husers;

    for (;tmp;tmp = tmp->next)
        count++;

    return count;
}

void huser_reset_states(void)
{
    huser *tmp = husers;

    for (;tmp;tmp = tmp->next)
        tmp->state = helpmod_base;
}

hlevel huser_get_level(huser *husr)
{
    if ((strlen(huser_get_nick(husr))) == 1 && isalpha(huser_get_nick(husr)[0]))
	return H_SERVICE; /* network services, keeps them out of harms way */

    if (husr->account != NULL)
    {
        if (IsOper(husr->real_user) && (husr->account->level < H_STAFF))
            return H_STAFF;
        else if (!IsOper(husr->real_user) && (husr->account->level >= H_OPER))
            return H_STAFF;
        else
            return husr->account->level;
    }
    else
    {
        if (IsOper(husr->real_user) || IsXOper(husr->real_user))
            return H_STAFF;
        else
            return H_PEON;
    }
}

int on_queue(huser *husr, huser_channel *huserchan)
{
    if (huserchan == NULL || huser_get_level(husr) > H_PEON)
        return 0;
    if (!(huserchan->hchan->flags & H_QUEUE))
        return 0;
    if (huserchan->flags & (HCUMODE_OP | HCUMODE_VOICE))
        return 0;
    if (huserchan->flags & HQUEUE_DONE)
        return 0;
    return 1;
}

int on_desk(huser* husr, huser_channel *huserchan)
{
    if (huserchan == NULL || huser_get_level(husr) > H_PEON)
        return 0;
    if (!(huserchan->hchan->flags & H_QUEUE))
        return 0;
    if (!(huserchan->flags & (HCUMODE_OP | HCUMODE_VOICE)))
        return 0;
    if (!(huserchan->flags & HQUEUE_DONE))
        return 0;
    return 1;
}

int huser_get_account_flags(huser *husr)
{
    if (husr->account != NULL)
        return husr->account->flags;
    else
        return H_ACCFLAGS_DEFAULT;
}

const char *huser_get_nick(huser *husr)
{
    if (husr == NULL)
	return "Error";
    else
        return husr->real_user->nick;
}

const char *huser_get_ident(huser *husr)
{
    if (husr == NULL)
	return "Error";
    else
        return husr->real_user->ident;
}

const char *huser_get_host(huser *husr)
{
    if (husr == NULL)
	return "Error";
    else
        return husr->real_user->host->name->content;
}

const char *huser_get_auth(huser *husr)
{
    if (husr == NULL || !IsAccount(husr->real_user))
	return "Error";
    else
        return husr->real_user->authname;
}

const char *huser_get_realname(huser *husr)
{
    if (husr == NULL)
	return "Error";
    else
        return husr->real_user->realname->name->content;

}

int huser_valid(huser* husr)
{
    huser *ptr = husers;
    for (;ptr;ptr = ptr->next)
	if (ptr == husr)
            return !0;
    return 0;
}

int huser_is_trojan(huser *husr)
{
    /* case 1 */
    char buffer[32];

    if (IsOper(husr->real_user))
        return 0;

    if (!IsAccount(husr->real_user))
        if (strisupper(huser_get_nick(husr)) && (strnumcount( huser_get_nick(husr)) >= 2))
        {
            sprintf(buffer, "*%s*",  huser_get_nick(husr));
            if (hword_count(huser_get_realname(husr)) == 1)
                if (strregexp(huser_get_realname(husr), buffer))
                    return 1;
        }

    /* case 2 */
    if (!IsAccount(husr->real_user) && (huser_get_ident(husr)[0] == '~'))
        if (strislower(huser_get_nick(husr)) && strisalpha( huser_get_nick(husr)))
            if (strislower(huser_get_ident(husr)) && strisalpha(huser_get_ident(husr)))
                if (strislower(huser_get_realname(husr)) && strisalpha(huser_get_realname(husr)))
                    if (hword_count(huser_get_realname(husr)) == 1)
			return 1;

    return 0;
}
