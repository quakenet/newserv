/* schedule.h */

#ifndef __SCHEDULE_H
#define __SCHEDULE_H

#include <time.h>

#define SCHEDULE_ONESHOT    0
#define SCHEDULE_REPEATING  1

typedef void (*ScheduleCallback)(void *);

typedef struct schedule {
  time_t            nextschedule;
  int               type;
  int               repeatinterval;
  int               repeatcount;
  ScheduleCallback  callback;
  void             *callbackparam;
  int               index; /* Where in the array this event is currently situated */
} schedule;


/* schedulealloc.c */

void initschedulealloc();
schedule *getschedule();
void freeschedule(schedule *sp);

/* schedule.c */
void initschedule();
void sortschedule();
void *scheduleoneshot(time_t when, ScheduleCallback callback, void *arg);
void *schedulerecurring(time_t first, int count, time_t interval, ScheduleCallback callback, void *arg);
void deleteschedule(void *sch, ScheduleCallback callback, void *arg);
void deleteallschedules(ScheduleCallback callback);
void doscheduledevents(time_t when);
void finischedule();
  
#endif
