/* events.h */

#ifndef __EVENTS_H
#define __EVENTS_H

#define STARTFDS    1000
#define GROWFDS     500

typedef void (*FDHandler)(int,short);

void inithandlers();
int registerhandler(int fd, short events, FDHandler handler);
int deregisterhandler(int fd, int doclose);
int handleevents(int timeout);

#endif
