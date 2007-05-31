/*
 * events.c: the event handling core, poll() version
 */

#include <stdio.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "../lib/array.h"
#include "events.h"
#include "error.h"
#include "hooks.h"

typedef struct {
  FDHandler handler;
  int fdarraypos;
} reghandler;

/*
 * OK, new data structure format for here to make everything
 * faster.  Two fixed static arrays, one of struct pollfds
 * (for feeding to poll) in no particular order.  Second array
 * has an array of reghandler structs; this is indexed by FD for 
 * quick reference...
 */

struct pollfd *eventfds;
reghandler *eventhandlers;

unsigned int maxfds;

int eventadds;
int eventdels;
int eventexes;

/* How many fds are currently registered */
int regfds;

void eventstats(int hooknum, void *arg);

void inithandlers() {
  regfds=0;
  eventadds=eventdels=eventexes=0;
  maxfds=STARTFDS;
  eventfds=(struct pollfd *)malloc(maxfds*sizeof(struct pollfd));
  memset(eventfds,0,maxfds*sizeof(struct pollfd));
  eventhandlers=(reghandler *)malloc(maxfds*sizeof(reghandler));
  memset(eventhandlers,0,maxfds*sizeof(reghandler));
  registerhook(HOOK_CORE_STATSREQUEST, &eventstats);
}

/*
 * checkindex():
 *  Given the number of a new file descriptor, makes sure that the arrays
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

  eventfds=(struct pollfd *)realloc((void *)eventfds,maxfds*sizeof(struct pollfd));
  memset(&eventfds[oldmax],0,maxfds-oldmax);
  eventhandlers=(reghandler *)realloc((void *)eventhandlers,maxfds*sizeof(reghandler));
  memset(&eventhandlers[oldmax],0,maxfds-oldmax);
}

/* 
 * registerhandler():
 *  Add an fd to the poll() array.
 *
 * Now O(1)
 */

int registerhandler(int fd, short events, FDHandler handler) {
  checkindex(fd);

  /* Check that it's not already registered */
  if (eventhandlers[fd].handler!=NULL) {
    return 1;
  }  
   
  eventhandlers[fd].handler=handler;
  eventhandlers[fd].fdarraypos=regfds;

  eventfds[regfds].fd=fd;
  eventfds[regfds].events=events;
  eventfds[regfds].revents=0;

  eventadds++;
  regfds++;
  return 0;
}

/*
 * deregisterhandler():
 *  Remove an fd from the poll() array.
 *
 * The poll() version can't save any time if doclose is set, so
 * we just do the same work and then close the socket if it's set.
 * 
 * Now O(1)
 */


int deregisterhandler(int fd, int doclose) {
  int oldfdpos;
  int lastreggedfd;
  
  /* Check that the handler exists */
  if (eventhandlers[fd].handler==NULL)
    return 1;
    
  /* We need to rearrange the fds array slightly to handle the delete */
  eventdels++;
  regfds--;
  if (regfds>0) {
    oldfdpos=eventhandlers[fd].fdarraypos;
    lastreggedfd=eventfds[regfds].fd;
    if (lastreggedfd!=fd) {
      /* We need to move the "lastreggedfd" into "oldfdpos" */
      memcpy(&eventfds[oldfdpos],&eventfds[regfds],sizeof(struct pollfd));
      eventhandlers[lastreggedfd].fdarraypos=oldfdpos;
    }
  }
  
  eventhandlers[fd].handler=NULL;
  eventhandlers[fd].fdarraypos=-1;

  if (doclose) {
    close(fd);
  }

  return 0;
}

/*
 * handleevents():
 *  Call poll() and handle and call appropiate handlers
 *  for any sockets that come up.
 *
 * Unavoidably O(n)
 */

int handleevents(int timeout) {
  int i,res;

  for (i=0;i<regfds;i++) {
    eventfds[i].revents=0;
  }
  
  res=poll(eventfds,regfds,timeout);
  if (res<0) {
    return 1;
  }
  
  for (i=0;i<regfds;i++) {
    if(eventfds[i].revents>0) {
      (eventhandlers[eventfds[i].fd].handler)(eventfds[i].fd, eventfds[i].revents);
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

