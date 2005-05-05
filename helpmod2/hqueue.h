/* H channel queue functions */
#ifndef HQUEUE_H
#define HQUEUE_H

#include "hchannel.h"
#include "huser.h"
#include "helpmod.h"

enum
{
    HQ_DONE,
    HQ_NEXT,
    HQ_ON,
    HQ_OFF,
    HQ_MAINTAIN,
    HQ_LIST,
    HQ_SUMMARY,
    HQ_RESET,
    HQ_NONE
};

void helpmod_queue_handler (huser *, channel *, hchannel *, int, char*, int, char**);

/* called from hooks */
void hqueue_handle_queue(hchannel *, huser*);

int hqueue_get_position(hchannel*, huser*);

int hqueue_average_time(hchannel *);

void hqueue_advance(hchannel *, huser *, int);

#endif
