#ifndef HCENSOR_H
#define HCENSOR_H

#include "../lib/sstring.h"

typedef struct hcensor_struct
{
    sstring *pattern;
    sstring *reason; /* optional */
    struct hcensor_struct *next;
} hcensor;

hcensor *hcensor_get_by_pattern(hcensor *, const char *);
hcensor *hcensor_get_by_index(hcensor *, int);
hcensor *hcensor_check(hcensor *, const char *); /* first matching pattern is returned, NULL if ok */
hcensor *hcensor_add(hcensor **, const char*, const char*);
hcensor *hcensor_del(hcensor **, hcensor *);
int hcensor_count(hcensor *);

void hcensor_del_all(hcensor **);

#endif
