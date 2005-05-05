/* new H command system */

#ifndef HCOMMAND_H
#define HCOMMAND_H

#include "../lib/sstring.h"
#include "../nick/nick.h"

#include "hdef.h"
#include "huser.h"
#include "../channel/channel.h"

typedef void (*hcommand_function)(huser *, channel*, char*, int, char **);

typedef struct hcommand_struct
{
    sstring* name;
    sstring* help;
    hlevel level;
    hcommand_function function;
    struct hcommand_struct *next;
} hcommand;

extern hcommand* hcommands;

hcommand* hcommand_add(const char *, hlevel, hcommand_function, const char *);

int hcommand_del(const char *);

int hcommand_del_all(void);

hcommand* hcommand_get(const char *, hlevel);

hcommand* hcommand_list(hlevel);

int hcommand_is_command(const char*);


#endif
