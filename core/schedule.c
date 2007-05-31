/* schedule.c */

#include "schedule.h"
#include "error.h"
#include "hooks.h"
#include "../lib/array.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define INITSCHEDSIZE      1000
#define GROWSCHEDSIZE      500

#undef SCHEDDEBUG

schedule **events;
int heapsize;
int heapmax;

int schedadds;
int scheddels;
int scheddelfast;
int schedexes;

/* Local prototypes */
void schedulestats(int hooknum, void *arg);

void initschedule() {
  initschedulealloc();
  events=NULL;
  schedadds=scheddels=schedexes=scheddelfast=0;
  registerhook(HOOK_CORE_STATSREQUEST, &schedulestats);
  heapsize=0;
  heapmax=INITSCHEDSIZE;
  events=(schedule **)malloc(INITSCHEDSIZE*sizeof(schedule *));
}

void schedule_heapify(int index) {
  int firstindex=index;
  schedule *ep;
  
  /* If this node is a leaf, do nothing */  
  if ((index*2)+1 >= heapsize) {
    return;
  }

  /* Check left child */  
  if (events[index]->nextschedule > events[(index*2)+1]->nextschedule) {
    firstindex=(index*2)+1;
  }

  /* Check right (if exists) */
  if ((index*2)+2 < heapsize) {
    if (events[firstindex]->nextschedule > events[(index*2)+2]->nextschedule) {
      firstindex=(index*2)+2;
    }
  }
  
  /* If both children were scheduled after us, we're done */
  if (firstindex==index) {
    return;
  }

  /* Swap the two pointers around in the heap */  
  ep=events[firstindex];
  events[firstindex]=events[index];
  events[index]=ep;

  /* Fix up the "index" field in the structures */
  events[firstindex]->index=firstindex;
  events[index]->index=index;
  
  schedule_heapify(firstindex);
}

void insertschedule (schedule *sp) {
  int mypos,myparent;

  schedadds++;
  
  if (heapsize>=heapmax) {
    /* We need to grow the heap */
    heapmax+=GROWSCHEDSIZE;
    events=(schedule **)realloc((void *)events,heapmax*sizeof(schedule *));
  }
  
  mypos=heapsize++;
  
  /* Travel up the heap looking for a slot for this new element */
  /* mypos points at a (vacant) candidate space; we either put the element
   * in this space, or pull it's parent down and try again with it's parent's space */
  for (;;) {
    myparent=(mypos-1)/2;
    if (mypos==0 || (sp->nextschedule >= events[myparent]->nextschedule)) {
      /* We reached the top, or our parent is scheduled before us -- end */
      events[mypos]=sp;
      sp->index=mypos;
      break;
    } else {
      /* Pull the parent into this space and move up the heap */
      events[mypos]=events[myparent];
      events[mypos]->index=mypos;
      mypos=myparent;
    }
  }
}

void schedule_remove (int index) {
  int mypos,myparent;
  schedule *sp;

  assert(index<heapsize);

#ifdef SCHEDDEBUG
  Error("schedule",ERR_DEBUG,"schedule_remove: %d",index);
#endif

  if (index<0)
    return;

  scheddels++;  
  heapsize--;
  
  /* Move the last element into the position we just deleted, then heapify 
   * If we happen to be deleting the last element, do nothing */
  if (index!=heapsize) {
    events[index]->index=-1;
    events[index]=events[heapsize];
    events[index]->index=index;
    schedule_heapify(index);
    
    /* Now we may need to float the element up the heap, similar to the insert case */
    mypos=index;
    for (;;) {
      myparent=(mypos-1)/2;
      if (mypos==0 || (events[mypos]->nextschedule >= events[myparent]->nextschedule)) {
        break;
      } else {
        /* Swap the element up the tree */
        sp=events[myparent];
        events[myparent]=events[mypos];
        events[mypos]=sp;
        /* Fix up the index members */
        events[myparent]->index=myparent;
        events[mypos]->index=mypos;
        
        mypos=myparent;
      }
    }
  }
}
  
void *scheduleoneshot(time_t when, ScheduleCallback callback, void *arg) {
  schedule *sp;
  
  sp=getschedule();
  
  sp->nextschedule=when;
  sp->type=SCHEDULE_ONESHOT;
  sp->repeatinterval=0;
  sp->repeatcount=1;
  sp->callback=callback;
  sp->callbackparam=arg;

  insertschedule(sp);
  
#ifdef SCHEDDEBUG
  Error("schedule",ERR_DEBUG,"scheduleoneshot: (%ld, %x, %x) = %x",when, callback, arg, sp);
#endif

  return (void *)sp;
}

void *schedulerecurring(time_t first, int count, time_t interval, ScheduleCallback callback, void *arg) {
  schedule *sp;

  if (count==1) {
    return scheduleoneshot(first, callback, arg);
  }

  sp=getschedule();

  sp->nextschedule=first;
  sp->type=SCHEDULE_REPEATING;
  sp->repeatinterval=interval;
  sp->repeatcount=(count-1);
  sp->callback=callback;
  sp->callbackparam=arg;
  
  insertschedule(sp);
  
  return (void *)sp;
}

void deleteschedule(void *sch, ScheduleCallback callback, void *arg) {
  schedule *sp;
  int i;
  
  /* New (optional) faster path: Clients can track the schedule pointer if they wish and 
   * pass it in here for an O(1) *cough* O(lg n) delete */

#ifdef SCHEDDEBUG
  Error("schedule",ERR_DEBUG,"deleteschedule(%x,%x,%x)",sch,callback, arg);
#endif

  if (sch) {
    sp=(schedule *)sch;
    /* Double check the params are correct: 
     *  because we recycle and never free schedule structs it's
     *  actually OK to try and delete a schedule that has been executed... */
     
    if (sp->callback==callback && sp->callbackparam==arg) {
      scheddelfast++;
      schedule_remove(sp->index);
      freeschedule(sp);
    }
    return;
  }
  
  /* Argh, have to find it by brute force */

  for(i=0;i<heapsize;i++) {
    if ((events[i]->callback==callback) && (events[i]->callbackparam==arg)) {
      sp=events[i];
      schedule_remove(sp->index);
      freeschedule(sp);
      return;
    }
  }
}

void deleteallschedules(ScheduleCallback callback) {
  schedule *sp;
  int i;
  
trydel:
  /* OK, this gets to be REALLY cheesy and stupidly slow as well */
  
  for(i=0;i<heapsize;i++) {
    if (events[i]->callback==callback) {
      sp=events[i];
      schedule_remove(sp->index);
      freeschedule(sp);
      goto trydel;
    }
  }
}

void doscheduledevents(time_t when) {
  void *arg;
  ScheduleCallback sc;
  schedule *sp;

  while (heapsize && events[0] && events[0]->nextschedule <= when) {
    /* Pick out the first element first */
    sp=events[0];
    sp->index=-1; /* Invalidate index so that an explicit delete doesn't screw us up */

    /* Remove from the top of the heap */
    heapsize--;
    events[0]=events[heapsize];
    events[0]->index=0;
    schedule_heapify(0);
    
    if (sp->callback==NULL) {
      Error("core",ERR_ERROR,"Tried to call NULL function in doscheduledevents(): (%x, %x, %x)",sp,sp->callback,sp->callbackparam);
      continue;
    }

    /* Store the callback */
    arg=(sp->callbackparam);
    sc=(sp->callback);
    
    /* Update the structures _before_ doing the callback.. */
    switch(sp->type) {
    case SCHEDULE_ONESHOT:
      freeschedule(sp);
      break;
        
    case SCHEDULE_REPEATING:
      sp->nextschedule+=sp->repeatinterval;
      /* Repeat count:
       *  0 for repeat forever
       *  1 for repeat set number of times..
       *
       * When we schedule it for the last time, change it to a ONESHOT event
       */
      if (sp->repeatcount>0) {
	sp->repeatcount--;
	if (sp->repeatcount==0) {
	  sp->type=SCHEDULE_ONESHOT;       
	}
      }
      insertschedule(sp);
      break;
    }
#ifdef SCHEDDEBUG
    Error("schedule",ERR_DEBUG,"exec schedule:(%x, %x, %x)", sp, sc, arg);
#endif
    (sc)(arg);
#ifdef SCHEDDEBUG
    Error("schedule",ERR_DEBUG,"schedule run OK");
#endif
    schedexes++;
  }
}

void schedulestats(int hooknum, void *arg) {
  long level=(long)arg;
  char buf[512];

  if (level>5) {
    sprintf(buf,"Schedule:%7d events scheduled, %7d events executed",schedadds,schedexes);
    triggerhook(HOOK_CORE_STATSREPLY,(void *)buf);
    sprintf(buf,"Schedule:%7d events deleted,   %7d fast deletes (%.2f%%)",scheddels,scheddelfast,(float)(scheddelfast*100)/scheddels);
    triggerhook(HOOK_CORE_STATSREPLY,(void *)buf);
    sprintf(buf,"Schedule:%7d events currently in queue",heapsize);
    triggerhook(HOOK_CORE_STATSREPLY,(void *)buf);
  }
}
