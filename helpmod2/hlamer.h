/* H lamer control */
#ifndef HLAMER_H
#define HLAMER_H

#include "../lib/sstring.h"

#define HLC_DEFAULT_BANTIME (60 * 60 * 24)

typedef enum
{
    HLC_CAPS,
    HLC_REPEAT,
    HLC_CHARACTER_REPEAT,
    HLC_FLOOD,
    HLC_SPAM
} hlc_violation;

typedef struct hlamercontrol_profile_struct
{
    sstring *name;

    int caps_max_percentage;
    int caps_min_count;

    int repeats_max_count;
    int repeats_min_length;

    int symbol_repeat_max_count;
    int character_repeat_max_count;
    int symbol_max_count;

    int tolerance_flood;

    int tolerance_spam;
    float constant_spam;

    int tolerance_warn;
    int tolerance_kick;
    int tolerance_remove;

    struct hlamercontrol_profile_struct *next;
} hlc_profile;

extern hlc_profile *hlc_profiles;
/* just adds a profile, does NOT set any values */
hlc_profile* hlc_add(const char *);
hlc_profile *hlc_del(hlc_profile *);
void hlc_del_all(void);
hlc_profile *hlc_get(const char *);

/* checks a string for lameness, returns non-zero if lameness is present and user is kicked */
/* the first parameter is hchannel* and second is huser*, but since hchannel.h includes hlamer.h, this is needed */
int hlc_check(void *, void*, const char *);

#endif
