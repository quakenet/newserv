/* schedulealloc.c */

#include "schedule.h"
#include <stdlib.h>

#define ALLOCUNIT 100

schedule *freescheds;

void initschedulealloc() {
  freescheds=NULL;
}

schedule *getschedule() {
  int i;
  schedule *sp;

  if (freescheds==NULL) {
    freescheds=(schedule *)malloc(ALLOCUNIT*sizeof(schedule));
    for (i=0;i<ALLOCUNIT-1;i++) {
      freescheds[i].callbackparam=(void *)&(freescheds[i+1]);
    }
    freescheds[ALLOCUNIT-1].callbackparam=NULL;
  }

  sp=freescheds;
  freescheds=(schedule *)sp->callbackparam;

  return sp;
}

void freeschedule(schedule *sp) {
  sp->callbackparam=(void *)freescheds;
  sp->callback=NULL;
  freescheds=sp;
}

