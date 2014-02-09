#include "../server/server.h"
#include "../core/hooks.h"
#include "../core/events.h"
#include "../nick/nick.h"
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
#include <string.h>
#include "../lib/version.h"

MODULE_VERSION("")

unsigned int nicks;
unsigned int quits;
int nickrate_listenfd;

void nr_nick(int hooknum, void *arg);
void nr_handlelistensocket(int fd, short events);
int nr_openlistensocket(int portnum);

void _init() {
  nicks=0;
  quits=0;
  registerhook(HOOK_NICK_NEWNICK, &nr_nick);
  registerhook(HOOK_NICK_LOSTNICK, &nr_nick);

  nickrate_listenfd=nr_openlistensocket(6002);
  if (nickrate_listenfd>0) {
    registerhandler(nickrate_listenfd,POLLIN,&nr_handlelistensocket);
  }
          
}

void _fini() {
  deregisterhook(HOOK_NICK_NEWNICK, &nr_nick);
  deregisterhook(HOOK_NICK_LOSTNICK, &nr_nick);
  deregisterhandler(nickrate_listenfd,1);    
}

void nr_nick(int hooknum, void *arg) {
  nick *np=(nick *)arg;
  
  if (serverlist[homeserver(np->numeric)].linkstate == LS_LINKED) {
    if (hooknum==HOOK_NICK_NEWNICK) {
      nicks++;
    } else {
      quits++;
    }
  }
}

int nr_openlistensocket(int portnum) {
  struct sockaddr_in sin;
  int                fd;
  unsigned int       opt=1;
  
  if ((fd=socket(AF_INET,SOCK_STREAM,0))==-1) {
    Error("proxyscan",ERR_ERROR,"Unable to open listening socket (%d).",errno);
    return -1;
  }
  
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt))!=0) {
    close(fd);
    Error("proxyscan",ERR_ERROR,"Unable to set SO_REUSEADDR on listen socket.");
    return -1;
  }
  
  /* Initialiase the addresses */
  memset(&sin,0,sizeof(sin));
  sin.sin_family=AF_INET;
  sin.sin_port=htons(portnum);
  
  if (bind(fd, (struct sockaddr *) &sin, sizeof(sin))) {
    close(fd);
    Error("proxyscan",ERR_ERROR,"Unable to bind listen socket (%d).",errno);
    return -1;
  }
  
  listen(fd,5);
  
  if (ioctl(fd, FIONBIO, &opt)!=0) {
    close(fd);
    Error("proxyscan",ERR_ERROR,"Unable to set listen socket non-blocking.");
    return -1;
  }
  
  return fd;
}

/* Here's what we do when poll throws the listen socket at us: 
 * accept() the connection, spam a short message and immediately
 * close it again */
 
void nr_handlelistensocket(int fd, short events) {
  struct sockaddr_in sin;
  socklen_t addrsize=sizeof(sin);
  char buf[20];
  int newfd;
  if ((newfd=accept(fd, (struct sockaddr *)&sin, &addrsize))>0) {
    /* Got new connection */
    sprintf(buf,"%u\n",nicks);
    write(newfd,buf,strlen(buf));
    sprintf(buf,"%u\n",quits);
    write(newfd,buf,strlen(buf));
    close(newfd);
  }
}
