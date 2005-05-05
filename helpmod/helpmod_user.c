#include "helpmod_user.h"
#include "helpmod_entries.h"

helpmod_user helpmod_get_user(long numeric)
{
    
    helpmod_user *tmp,tmp2;
    for(tmp=&helpmod_users;*tmp;tmp=&(*tmp)->next)
	if ((*tmp)->numeric == numeric)
	{
	    (*tmp)->last_active = time(NULL);
	    return *tmp;
	}
    tmp2 = *tmp;
    *tmp = (helpmod_user)malloc(sizeof(struct helpmod_user_struct));
    (*tmp)->next = tmp2;
    (*tmp)->last_active = time(NULL);
    (*tmp)->numeric = numeric;
    (*tmp)->state = helpmod_base;
    return *tmp;
}

void helpmod_clear_users(void)
{
    helpmod_user tmp;
    while (helpmod_users)
    {
	tmp = helpmod_users->next;
	free(helpmod_users);
	helpmod_users = tmp;
    }
    helpmod_users = NULL;
}

void helpmod_init_users(void)
{
    helpmod_users = NULL;
}

void helpmod_clear_inactives(void)
{
    helpmod_user *tmp, tmp2;
    for (tmp = &helpmod_users;*tmp;tmp = &(*tmp)->next)
	while (time(NULL) - (*tmp)->last_active > HELPMOD_USER_TIMEOUT)
	{
	    tmp2 = (*tmp)->next;
	    free(*tmp);
	    *tmp = tmp2;
	    if (!*tmp)
                return;
	}
}

long helpmod_user_count(void)
{
    helpmod_user *tmp;
    long counter;
    for (counter=0, tmp = &helpmod_users;*tmp;counter++, tmp = &(*tmp)->next);
    return counter;
}
