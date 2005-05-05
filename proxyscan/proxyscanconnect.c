/* proxyscanconnect: handle connections etc. */

#include "proxyscan.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "../core/error.h"
#include <sys/ioctl.h>
#include <string.h>

int createconnectsocket(long ip, int socknum) {
  int fd;
  struct sockaddr_in sin;
  int res=1;
  unsigned int opt=1;
  
  memset(&sin,0,sizeof(sin));
  
  sin.sin_family=AF_INET;
  sin.sin_port=htons(socknum);
  sin.sin_addr.s_addr=htonl(ip);
  
  if ((fd=socket(AF_INET,SOCK_STREAM,0))<0) {
    Error("proxyscan",ERR_ERROR,"Unable to create socket (%d)",errno);
    return -1;
  }
  if (ioctl(fd,FIONBIO,&res)!=0) {
    Error("proxyscan",ERR_ERROR,"Unable to make socket nonblocking");
    return -1;
  }
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt))!=0) {
    Error("proxyscan",ERR_ERROR,"Unable to set SO_REUSEADDR on scan socket.");
    return -1;
  }
#ifdef __FreeBSD__
  opt=IP_PORTRANGE_HIGH;
  if (setsockopt(fd, IPPROTO_IP, IP_PORTRANGE, (char *) &opt, sizeof(opt))!=0) {
    Error("proxyscan",ERR_WARNING,"Error selecting high port range.");
  }
#endif
  if (connect(fd,(const struct sockaddr *) &sin, sizeof(sin))) {
    if (errno != EINPROGRESS) {
      Error("proxyscan",ERR_ERROR,"Unable to connect socket (%d)",errno);
      return -1;
    }
  }  
  return fd;
}

