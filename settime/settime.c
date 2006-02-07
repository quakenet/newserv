#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../irc/irc.h"
#include "../core/schedule.h"

schedule *settime_schedule;

void settime_sendsettime()
{
time_t newtime;

    newtime = time(NULL);
    irc_send("%s SE %ld", mynumeric->content, newtime);
    setnettime(newtime);
}

void _init()
{
    settime_schedule = schedulerecurring(time(NULL) + 300, 0, 1800, &settime_sendsettime, NULL);
}

void _fini()
{
    deleteschedule(settime_schedule, &settime_sendsettime, NULL);
}
