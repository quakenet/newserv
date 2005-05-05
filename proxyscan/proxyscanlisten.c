/*
 * proxyscanlisten.c: handle the listening socket part of proxyscan
 */

#include "proxyscan.h"

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "../core/error.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <netinet/in.h>

/* Open our listen socket.  We just use this as a connection target when we
 * try and exploit proxies.  The init routine should call this, then register the 
 * resulting fd with the handler below. */

int openlistensocket(int portnum) {
  struct sockaddr_in sin;
  int                fd;
  unsigned int       opt=1;
  
  if ((fd=socket(AF_INET,SOCK_STREAM,0))==-1) {
    Error("proxyscan",ERR_ERROR,"Unable to open listening socket (%d).",errno);
    return -1;
  }
  
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt))!=0) {
    Error("proxyscan",ERR_ERROR,"Unable to set SO_REUSEADDR on listen socket.");
    return -1;
  }
  
  /* Initialiase the addresses */
  memset(&sin,0,sizeof(sin));
  sin.sin_family=AF_INET;
  sin.sin_port=htons(portnum);
  
  if (bind(fd, (struct sockaddr *) &sin, sizeof(sin))) {
    Error("proxyscan",ERR_ERROR,"Unable to bind listen socket (%d).",errno);
    return -1;
  }
  
  listen(fd,5);
  
  if (ioctl(fd, FIONBIO, &opt)!=0) {
    Error("proxyscan",ERR_ERROR,"Unable to set listen socket non-blocking.");
    return -1;
  }
  
  return fd;
}

/* Here's what we do when poll throws the listen socket at us: 
 * accept() the connection, spam a short message and immediately 
 * close it again */
 
void handlelistensocket(int fd, short events) {
  struct sockaddr_in sin;
  unsigned int addrsize=sizeof(sin);
  int newfd;
  if ((newfd=accept(fd, (struct sockaddr *)&sin, &addrsize))>0) {
    /* Got new connection */
    write(newfd,"OPEN PROXY!\r\n",13);
    close(newfd);
  }
}
