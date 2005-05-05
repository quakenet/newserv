#ifndef HELPMOD_USER_H
#define HELPMOD_USER_H

#include <time.h>
#include "helpmod_entries.h"
#define HELPMOD_USER_TIMEOUT 300
typedef struct helpmod_user_struct
{
    long numeric;
    time_t last_active;
    helpmod_entry state;
    struct helpmod_user_struct* next;
} *helpmod_user;

extern helpmod_user helpmod_users;
extern helpmod_entry helpmod_base;

helpmod_user helpmod_get_user(long numeric);
void helpmod_clear_users(void);
void helpmod_clear_inactives(void);
void helpmod_init_users(void);
long helpmod_user_count(void);

#endif
