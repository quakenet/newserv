/* proxyscanconnect: handle connections etc. */

#define _GNU_SOURCE
#include "proxyscan.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../core/error.h"
#include <sys/ioctl.h>
#include <string.h>

int createconnectsocket(struct irc_in_addr *ip, int socknum) {
  union {
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
  } u = {};

  int proto;
  int s;
  int fd;

  if(socknum == 0) {
    if (!exthost || !extport)
      return -1;

    s = sizeof(u.sin);
    proto=u.sin.sin_family=AF_INET;
    u.sin.sin_port=htons(extport);

    if (inet_aton(exthost->content, &u.sin.sin_addr) == 0) {
      Error("proxyscan",ERR_ERROR,"Invalid external address: %s", exthost->content);
      return -1;
    }
  } else {
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
  }

  if ((fd=socket(proto,SOCK_STREAM|SOCK_NONBLOCK,0))<0) {
    Error("proxyscan",ERR_ERROR,"Unable to create socket (%d)",errno);
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
