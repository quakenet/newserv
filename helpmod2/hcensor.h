#ifndef HCENSOR_H
#define HCENSOR_H

#include "../lib/sstring.h"

/* forward declarations */
struct hchannel_struct;
struct huser_struct;

typedef enum
{
    HCENSOR_WARN,
    HCENSOR_KICK,
    HCENSOR_CHANBAN,
    HCENSOR_BAN
} hcensor_type;

typedef struct hcensor_struct
{
    sstring *pattern;
    sstring *reason; /* optional */
    hcensor_type type;

    struct hcensor_struct *next;
} hcensor;

hcensor *hcensor_get_by_pattern(hcensor *, const char *);
hcensor *hcensor_get_by_index(hcensor *, int);
hcensor *hcensor_check(hcensor *, const char *); /* first matching pattern is returned, NULL if ok */
hcensor *hcensor_add(hcensor **, const char*, const char*, hcensor_type);
hcensor *hcensor_del(hcensor **, hcensor *);
/* Handle a censor match, if returnvalue is non-zero then the user was removed from channel */
int hcensor_match(struct hchannel_struct*, struct huser_struct*, hcensor*);
const char *hcensor_get_typename(hcensor_type);

int hcensor_count(hcensor *);

void hcensor_del_all(hcensor **);

#endif
