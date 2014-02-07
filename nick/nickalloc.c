/* nick/host/realname/authname allocator */

#include "nick.h"
#include "../core/nsmalloc.h"

#include <assert.h>
#include <stdlib.h>

/* Hosts and realname structures are the same size */
/* This assumption is checked in initnickalloc(); */

realname *newrealname() {
  return (realname *)newhost();
}

void freerealname(realname *rn) {
  freehost((host *)rn);
}

nick *newnick() {
  return nsmalloc(POOL_NICK, sizeof(nick));
} 

void freenick(nick *np) {
  nsfree(POOL_NICK, np);
}

host *newhost() {
  return nsmalloc(POOL_NICK, sizeof(host));
}

void freehost(host *hp) {
  nsfree(POOL_NICK, hp);
}

