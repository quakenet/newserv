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

int createconnectsocket(struct irc_in_addr *ip, int socknum) {
  union {
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } u;

  int proto;
  int s;
  int res=1;
  unsigned int opt=1;
  int fd;

  if(irc_in_addr_is_ipv4(ip)) {
    s = sizeof(u.sin);
    proto=u.sin.sin_family=AF_INET;
    u.sin.sin_port=htons(socknum);
    u.sin.sin_addr.s_addr=htonl(irc_in_addr_v4_to_int(ip));
  } else {
    s = sizeof(u.sin6);
    proto=u.sin6.sin6_family=AF_INET6;
    u.sin6.sin6_port=htons(socknum);
    memcpy(&u.sin6.sin6_addr.s6_addr, ip->in6_16, sizeof(ip->in6_16));
  }

  if ((fd=socket(proto,SOCK_STREAM,0))<0) {
    Error("proxyscan",ERR_ERROR,"Unable to create socket (%d)",errno);
    return -1;
  }
  if (ioctl(fd,FIONBIO,&res)!=0) {
    close(fd);
    Error("proxyscan",ERR_ERROR,"Unable to make socket nonblocking");
    return -1;
  }
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt))!=0) {
    close(fd);
    Error("proxyscan",ERR_ERROR,"Unable to set SO_REUSEADDR on scan socket.");
    return -1;
  }
#ifdef __FreeBSD__
  opt=IP_PORTRANGE_HIGH;
  if (setsockopt(fd, IPPROTO_IP, IP_PORTRANGE, (char *) &opt, sizeof(opt))!=0) {
    close(fd);
    Error("proxyscan",ERR_WARNING,"Error selecting high port range.");
  }
#endif

  if (connect(fd,(const struct sockaddr *) &u, s)) {
    if (errno != EINPROGRESS) {
      close(fd);
      Error("proxyscan",ERR_ERROR,"Unable to connect socket (%d)",errno);
      return -1;
    }
  }

  return fd;
}
