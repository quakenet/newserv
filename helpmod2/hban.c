#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "../channel/channel.h"
#include "../nick/nick.h"
#include "../lib/sstring.h"
#include "../localuser/localuserchannel.h"

#include "hban.h"
#include "hchannel.h"
#include "huser.h"
#include "helpmod.h"
#include "hgen.h"

hban *hban_add(const char* pat, const char* rsn, time_t exp, int now)
{
    hban *ptr;
    chanban *tmp;

    if (hban_get(pat) || exp <= time(NULL))
        return NULL;

    if ((tmp = makeban(pat)) == NULL) /* bad ban */
        return NULL;

    ptr = (hban*)malloc(sizeof(hban));
    ptr->real_ban = tmp;
    ptr->expiration = exp;

    if (rsn == NULL)
        ptr->reason = NULL;
    else
        ptr->reason = getsstring(rsn, strlen(rsn));

    ptr->next = hbans;
    hbans = ptr;

    { /* additional logic here */
        huser* tmpu;
	for (tmpu = husers;tmpu;tmpu = tmpu->next)
	    if (nickmatchban(tmpu->real_user, tmp) && !IsOper(tmpu->real_user))
            {
                hchannel *assert_hchan = NULL;
                while (tmpu->hchannels)
                {
                    if (tmpu->hchannels->hchan == assert_hchan)
                    {
                        Error("helpmod", ERR_ERROR, "hban.c hban_add() current channel is the previous channel. Preventing lockup.");
                        break;
                    }
                    assert_hchan = tmpu->hchannels->hchan;
                    helpmod_setban(tmpu->hchannels->hchan, bantostring(ptr->real_ban), HELPMOD_BAN_DURATION, MCB_ADD, now);
                    helpmod_kick(tmpu->hchannels->hchan, tmpu, "%s", hban_get_reason(ptr));
                }
	    }
    }

    return ptr;
}

hban *hban_huser(huser *husr, const char* rsn, time_t exp, int now)
{
    const char *banmask = hban_ban_string(husr->real_user, HBAN_HOST);

    return hban_add(banmask, rsn, exp, now);
}

hban *hban_del(hban* hb, int now)
{
    hban **ptr = &hbans;
    for (;*ptr;ptr = &(*ptr)->next)
        if (*ptr == hb)
        {
            hban *tmp = (*ptr)->next;

            { /* additional logic here */
                hchannel *hchan;
                for (hchan = hchannels;hchan;hchan = hchan->next)
                    helpmod_setban(hchan, bantostring((*ptr)->real_ban), 0, MCB_DEL, now);
            }

            freechanban((*ptr)->real_ban); /* evil ? */
            freesstring((*ptr)->reason);
            free(*ptr);

            *ptr = tmp;
            return NULL;
        }
    return hb;
}

hban *hban_get(const char* pat)
{
    hban *tmp = hbans;
    for (;tmp;tmp = tmp->next)
        if (!ci_strcmp(bantostring(tmp->real_ban),pat))
            return tmp;

    return NULL;
}

int hban_count(void)
{
    int count = 0;
    hban *hb = hbans;
    for (;hb;hb = hb->next)
        count++;
    return count;
}

hban *hban_check(nick *nck)
{
    hban *ptr = hbans;
    for (;ptr;ptr = ptr->next)
        if (nickmatchban(nck, ptr->real_ban))
            return ptr;
    return NULL;
}

void hban_remove_expired(void)
{
    hban **ptr = &hbans;
    while (*ptr)
        if ((*ptr)->expiration <= time(NULL)) /* expired */
            hban_del(*ptr, 0);
        else
            ptr = &(*ptr)->next;
    hcommit_modes();
}

void hban_del_all(void)
{
    while (hbans)
        hban_del(hbans, 1);
}

const char *hban_get_reason(hban* hb)
{
    if (hb == NULL)
        return "error (please contact strutsi)";
    else if (hb->reason == NULL)
        return "banned";
    else
        return hb->reason->content;
}

const char *hban_ban_string(nick *nck, int banflags)
{
    static char buffer[256];
    buffer[0] = '\0';

    if (banflags & HBAN_NICK)
        strcat(buffer, nck->nick);
    else
        strcat(buffer, "*");

    strcat(buffer, "!");

    if (banflags & HBAN_IDENT)
        strcat(buffer, nck->ident);
    else
        strcat(buffer, "*");

    strcat(buffer, "@");

    if ((banflags & HBAN_HOST) && IsAccount(nck) && IsHideHost(nck))
    {
        strcat(buffer, nck->authname);
        strcat(buffer, ".users.quakenet.org");
    }
    else if ((banflags & HBAN_REAL_HOST) || ((banflags & HBAN_HOST)))
    {
      if (IsSetHost(nck))
        strcat(buffer, nck->sethost->content);
      else
        strcat(buffer, nck->host->name->content);
    }
    else
        strcat(buffer, "*");

    assert(strcmp(buffer, "*!*@*"));

    return buffer;
}
