#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "../lib/sstring.h"

#include "helpmod.h"
#include "hcommand.h"
#include "hgen.h"

hcommand* hcommand_add(const char *str, hlevel lvl, hcommand_function func, const char *command_help)
{
    hcommand *tmp, **ptr = &hcommands;

    assert(hcommand_get(str, lvl) == NULL);

    for (;*ptr && (*ptr)->level <= lvl;ptr = &(*ptr)->next);

    tmp = *ptr;
    *ptr = (hcommand*)malloc(sizeof (hcommand));

    (*ptr)->name = getsstring(str, strlen(str));
    (*ptr)->help = getsstring(command_help, strlen(command_help));
    (*ptr)->level = lvl;
    (*ptr)->function = func;
    (*ptr)->next = tmp;

    return *ptr;
}

int hcommand_del(const char *str)
{
    hcommand **ptr = &hcommands;

    assert(hcommand_get(str, H_NONE) != NULL);

    for (;*ptr;ptr = &(*ptr)->next)
        if (!ci_strcmp(str, (*ptr)->name->content))
        {
            hcommand *tmp = (*ptr)->next;
            freesstring((*ptr)->name);
            free(*ptr);
            *ptr = tmp;

            return 0;
        }
    return 0;
}

int hcommand_del_all(void)
{
    while (hcommands)
        hcommand_del(hcommands->name->content);
    return 0;
}


hcommand* hcommand_get(const char *str, hlevel lvl)
{
    hcommand *ptr = hcommands;

    for (;ptr && ptr->level <= lvl;ptr = ptr->next)
        if (!ci_strcmp(str, ptr->name->content))
            return ptr;

    return NULL;
}

hcommand* hcommand_list(hlevel lvl)
{
    static hcommand *position = NULL;
    hcommand *tmp;

    if (position == NULL || position->level > lvl)
    {
        position = hcommands;
        return NULL;
    }

    tmp = position;
    position = position->next;

    return tmp;
}

int hcommand_is_command(const char *str)
{
    if (*str == '-' || *str == '?')
        return 1;
    if ((!strncmp(str, helpmodnick->nick, strlen(helpmodnick->nick)) && str[strlen(helpmodnick->nick)] == ' '))
        return 1;
    return 0;
}
