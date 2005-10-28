
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../lib/sstring.h"
#include "../lib/irc_string.h"

#include "haccount.h"
#include "helpmod.h"
#include "huser.h"
#include "hgen.h"

haccount *haccount_get_by_name(const char *name)
{
    haccount *tmp;

    if (name == NULL)
        return NULL;

    if (*name == '#')
        name++;

    tmp = haccounts;
    for (;tmp;tmp = tmp->next)
        if (!ircd_strcmp(tmp->name->content, name))
            return tmp;

    return NULL;
}

haccount *haccount_add(const char *name, hlevel lvl)
{
    haccount *tmp = (haccount*)malloc(sizeof (haccount));

    if (haccount_get_by_name(name))
    {
	Error("helpmod", ERR_ERROR, "Attempt to add a duplicate account: %s", name);
	return haccount_get_by_name(name);
    }

    if (*name == '#')
        name++;

    tmp->name = getsstring(name, strlen(name));
    tmp->level = lvl;
    tmp->flags = H_ACCFLAGS_DEFAULT;
    tmp->last_activity = time(NULL);
    tmp->stats = NULL;

    tmp->next = haccounts;
    haccounts = tmp;

    return tmp;
}

int haccount_del(haccount *hack)
{
    huser *h_ptr = husers;
    haccount **ptr = &haccounts;

    for (;h_ptr;h_ptr = h_ptr->next)
        if (h_ptr->account == hack)
            h_ptr->account = NULL;

    for (;*ptr;ptr = &(*ptr)->next)
        if (*ptr == hack)
        {
            hstat_account *tmp_stat;
            haccount *tmp = (*ptr)->next;

            while ((*ptr)->stats)
            {
                tmp_stat = (*ptr)->stats->next;
                free((*ptr)->stats);
                (*ptr)->stats = tmp_stat;
            }

            freesstring((*ptr)->name);
            free(*ptr);

            *ptr = tmp;
            return 0;
        }

    return 0;
}

void haccount_clear_inactives(void)
{
    haccount **ptr = &haccounts;
    while (*ptr)
	if ((time(NULL) - (*ptr)->last_activity > HELPMOD_ACCOUNT_EXPIRATION[(*ptr)->level]) && !((*ptr)->flags & H_NO_EXPIRE))
	    haccount_del(*ptr);
	else
            ptr = &(*ptr)->next;
}

void haccount_set_level(haccount *hack, hlevel lvl)
{
    if (hack == NULL)
        return;
    hack->level = lvl;
}

const char *haccount_get_state(haccount* hacc, int mask)
{
    if (hacc->flags & mask)
        return "Yes";
    else
        return "No";
}

int haccount_count(hlevel lvl)
{
    haccount *hacc = haccounts;
    int count = 0;
    for (;hacc;hacc = hacc->next)
        if (lvl == H_ANY)
            count++;
        else if (hacc->level == lvl)
            count++;
    return count;
}

const char *haccount_get_sname(int index)
{
    switch (index)
    {
    case 0:
        return "Send replies as private messages";
    case 1:
        return "Send replies as notices";
    case 2:
        return "Do not send verbose messages";
    case 3:
        return "Auto-op on join";
    case 4:
        return "Auto-voice on join";
    case 5:
        return "Suppress unknown command error";
    default:
        return "Error. Please contact strutsi";
    }
}
