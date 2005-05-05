/* the term database */
#ifndef HTERM_H
#define HTERM_H

#include "../lib/sstring.h"

typedef struct hterm_struct
{
    sstring *name;
    sstring *description;

    struct hterm_struct *next;
} hterm;

/* allow channel specifics ! */

extern hterm *hterms;

hterm *hterm_add(hterm**, const char*, const char*);
hterm *hterm_get(hterm*, const char*);
hterm *hterm_find(hterm*, const char*);
hterm *hterm_get_and_find(hterm*, const char*);
hterm *hterm_del(hterm**, hterm*);

int hterm_count(hterm*);

void hterm_del_all(hterm**);

#endif
