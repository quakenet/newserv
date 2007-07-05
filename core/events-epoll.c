/*
 * events.c: the event handling core, epoll() version
 */

#include <stdio.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "../lib/array.h"
#include "events.h"
#include "error.h"
#include "hooks.h"

/* We need to track the handler for each fd - epoll() can 
 * return an fd or a pointer but not both :(.  We'll keep the
 * fd in the epoll_data structure for simplicity. */

typedef struct {
  FDHandler          handler;
} reghandler;

reghandler *eventhandlers;

unsigned int maxfds;

int eventadds;
int eventdels;
int eventexes;

/* How many fds are currently registered */
int regfds;
int epollfd;

void eventstats(int hooknum, void *arg);

void inithandlers() {
  regfds=0;
  eventadds=eventdels=eventexes=0;
  maxfds=STARTFDS;
  eventhandlers=(reghandler *)malloc(maxfds*sizeof(reghandler));
  memset(eventhandlers,0,maxfds*sizeof(reghandler));

  /* Get an epoll FD */
  if ((epollfd=epoll_create(STARTFDS))<0) {
    Error("events",ERR_STOP,"Unable to initialise epoll.");
  }

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

  eventhandlers=(reghandler *)realloc((void *)eventhandlers,maxfds*sizeof(reghandler));
  memset(&eventhandlers[oldmax],0,(maxfds-oldmax)*sizeof(reghandler));
}

/*
 * polltoepoll():
 *  Converts a poll-style event variable to an epoll one.
 */
unsigned int polltoepoll(short events) {
  unsigned int epe=EPOLLERR|EPOLLHUP; /* Default event mask */
  
  if (events & POLLIN)  epe |= EPOLLIN;
  if (events & POLLOUT) epe |= EPOLLOUT;
  if (events & POLLPRI) epe |= EPOLLPRI;

  return epe;
}

short epolltopoll(unsigned int events) {
  short e=0;
  
  if (events & EPOLLIN)  e |= POLLIN;
  if (events & EPOLLOUT) e |= POLLOUT;
  if (events & EPOLLERR) e |= POLLERR;
  if (events & EPOLLPRI) e |= POLLPRI;
  if (events & EPOLLHUP) e |= POLLHUP;

  return e;
}  

/* 
 * registerhandler():
 *  Add an fd to the epoll array.
 */

int registerhandler(int fd, short events, FDHandler handler) {
  struct epoll_event epe;

  checkindex(fd);

  /* Check that it's not already registered */
  if (eventhandlers[fd].handler!=NULL) {
    Error("events",ERR_WARNING,"Attempting to register already-registered fd %d.",fd);
    return 1;
  }  
   
  eventhandlers[fd].handler=handler;

  epe.data.fd=fd;
  epe.events=polltoepoll(events);

  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &epe)) {
    Error("events",ERR_WARNING,"Error %d adding fd %d to epoll (events=%d)",errno,fd,epe.events);
    return 1;
  }

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
  struct epoll_event epe;
  
  checkindex(fd);
  
  /* Check that the handler exists */
  if (eventhandlers[fd].handler==NULL) {
    Error("events",ERR_WARNING,"Attempt to deregister unregistered fd: %d",fd);
    return 1;
  }
      
  eventhandlers[fd].handler=NULL;

  if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &epe)) {
    Error("events",ERR_WARNING,"Error deleting FD %d from epoll: %d",fd,errno);
  }

  if (doclose) {
    close(fd);
  }

  eventdels++;
  regfds--;
  
  return 0;
}

/*
 * handleevents():
 *  Call epoll_wait() and handle and call appropiate handlers
 *  for any sockets that come up.
 */

int handleevents(int timeout) {
  int i,res;
  struct epoll_event epes[100];

  res=epoll_wait(epollfd, epes, 100, timeout);

  if (res<0) {
    Error("events",ERR_WARNING,"Error in epoll_wait(): %d",errno);
    return 1;
  }
  
  for (i=0;i<res;i++) {
    (eventhandlers[epes[i].data.fd].handler)(epes[i].data.fd, epolltopoll(epes[i].events));
    eventexes++;
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
