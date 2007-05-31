/*
 * events.c: the event handling core, kqueue() version
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <unistd.h>
#include "../lib/array.h"
#include "events.h"
#include "error.h"
#include "hooks.h"

/*
 * OK, for the kqueue() version we just keep an array (indexed by fd)
 * of what we put in the kqueue() so we can remove it later.  This is only
 * required because the deregisterhandler() call doesn't include the 
 * "events" field, thus the kqueue filter used is unknown.
 *
 * We have a seperate fixed array addqueue() for stuff we're adding to
 * the queue; this gets flushed if it's full or held over until we
 * next call handleevents().
 */

#define UPDATEQUEUESIZE    100

struct kevent  addqueue[UPDATEQUEUESIZE];
struct kevent *eventfds;

unsigned int maxfds;
unsigned int updates;

int kq;

int eventadds;
int eventdels;
int eventexes;

/* How many fds are currently registered */
int regfds;

void eventstats(int hooknum, void *arg);

void inithandlers() {
  regfds=0;
  updates=0;
  eventadds=eventdels=eventexes=0;
  maxfds=0;
  eventfds=NULL;
  kq=kqueue();
  registerhook(HOOK_CORE_STATSREQUEST, &eventstats);
}

/*
 * checkindex():
 *  Given the number of a new file descriptor, makes sure that the array
 * will be big enough to deal with it.
 */ 

void checkindex(unsigned index) {
  int oldmax=maxfds;
  
  if (index<maxfds) {
    return;
  }

  while (maxfds<=index) {
    maxfds+=GROWFDS;
  }

  eventfds=(struct kevent *)realloc((void *)eventfds,maxfds*sizeof(struct kevent));
  memset(&eventfds[oldmax],0,(maxfds-oldmax)*sizeof(struct kevent));
}

/* 
 * registerhandler():
 *  Create a kevent structure and put it on the list to be added next 
 *  time kevent() is called.  If that list is full, we call kevent() 
 *  to flush it first.
 *
 * We pass the handler in as the udata field to the kernel, this way
 * we don't have to hunt for it when the kernel throws the kevent back
 * at us.
 */

int registerhandler(int fd, short events, FDHandler handler) {
  checkindex(fd);
   
  /* Check that it's not already registered */
  if (eventfds[fd].filter!=0) {
    return 1;
  }
  
  eventfds[fd].ident=fd;
  if (events & POLLIN) {
    eventfds[fd].filter=EVFILT_READ;
  } else {
    eventfds[fd].filter=EVFILT_WRITE;
  }
  eventfds[fd].flags=EV_ADD;
  eventfds[fd].fflags=0;
  eventfds[fd].data=0;
  eventfds[fd].udata=(void *)handler;

/*  Error("core",ERR_DEBUG,"Adding fd %d filter %d",fd,eventfds[fd].filter); */
  
  if (updates>=UPDATEQUEUESIZE) {
    kevent(kq, addqueue, updates, NULL, 0, NULL);
    updates=0;
  }
  
  addqueue[updates++]=eventfds[fd];
  
  eventadds++;
  regfds++;
  return 0;
}

/*
 * deregisterhandler():
 *  Removes the fd's kevent from the kqueue.  Note that if we're 
 *  going to be closing the fd, it will automatically be removed
 *  from the queue so we don't have to do anything except the close().
 */

int deregisterhandler(int fd, int doclose) {

  if (!doclose) {
    if (updates>=UPDATEQUEUESIZE) {
      kevent(kq, addqueue, updates, NULL, 0, NULL);
      updates=0;
    }
  
    eventfds[fd].flags=EV_DELETE;
    addqueue[updates++]=eventfds[fd];

/*    Error("core",ERR_DEBUG,"Deleting fd %d filter %d",fd,eventfds[fd].filter); */
  } else {
    close(fd);
  }

  regfds--;
  eventdels++;
  eventfds[fd].filter=0;

  return 0;
}

/*
 * handleevents():
 *  Call kevent() and handle and call appropiate handlers
 *  for any sockets that come up.
 *
 * This is O(n) in the number of fds returned, rather than the number 
 * of fds polled as in the poll() case.
 */

int handleevents(int timeout) {
  int i,res;
  struct timespec ts;
  struct kevent theevents[100];
  short revents;
  
  ts.tv_sec=(timeout/1000);
  ts.tv_nsec=(timeout%1000)*1000000;

  res=kevent(kq, addqueue, updates, theevents, 100, &ts);
  updates=0;
  
  if (res<0) {
    return 1;
  }
  
  for (i=0;i<res;i++) {
    revents=0;
    
    /* If EV_ERROR is set it's a failed addition of a kevent to the queue.
     * This shouldn't happen so we flag a warning. */
    if(theevents[i].flags & EV_ERROR) {
      Error("core",ERR_WARNING,"Got EV_ERROR return from kqueue: fd %d filter %d error %d",theevents[i].ident,theevents[i].filter,theevents[i].data); 
    } else {    
      /* Otherwise, translate the result into poll() format.. */
      if(theevents[i].filter==EVFILT_READ) {
        if (theevents[i].data>0) {
          revents|=POLLIN;
        }
      } else {
        if (theevents[i].data>0) {
          revents|=POLLOUT;
        }
      }
      
      if (theevents[i].flags & EV_EOF || theevents[i].data<0) {
        revents|=POLLERR;
      }
      
      /* Call the handler */
      ((FDHandler)(theevents[i].udata))(theevents[i].ident, revents);
      eventexes++;
    }
  }  
  return 0;
}   

void eventstats(int hooknum, void *arg) {
  char buf[512];
  long level=(long) arg;
  
  if (level>5) {
    sprintf(buf,"Events  :%7d fds registered,   %7d fds deregistered",eventadds,eventdels);
    triggerhook(HOOK_CORE_STATSREPLY,(void *)buf);
    sprintf(buf,"Events  :%7d events triggered,  %6d fds active",eventexes,regfds);
    triggerhook(HOOK_CORE_STATSREPLY,(void *)buf);
  }
}

