/* schedulealloc.c */

#include "schedule.h"
#include "nsmalloc.h"

schedule *getschedule() {
  return nsmalloc(POOL_SCHEDULE, sizeof(schedule));
}

void freeschedule(schedule *sp) {
  return nsfree(POOL_SCHEDULE, sp);
}

