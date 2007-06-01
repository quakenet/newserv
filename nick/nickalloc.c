/* nick/host/realname/authname allocator */

#include "nick.h"
#include <assert.h>
#include <stdlib.h>

#define ALLOCUNIT   100

/* Hosts and realname structures are the same size */
/* This assumption is checked in initnickalloc(); */

nick *freenicks;
host *freehosts;

void initnickalloc() {
  freenicks=NULL;
  freehosts=NULL;
  
  assert(sizeof(host)==sizeof(realname));
}

realname *newrealname() {
  return (realname *)newhost();
}

void freerealname(realname *rn) {
  freehost((host *)rn);
}

nick *newnick() {
  nick *np;
  int i;
  
  if (freenicks==NULL) {
    freenicks=(nick *)malloc(ALLOCUNIT*sizeof(nick));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freenicks[i].next=&(freenicks[i+1]);
    }
    freenicks[ALLOCUNIT-1].next=NULL;
  }
  
  np=freenicks;
  freenicks=np->next;
  
  return np;
} 

void freenick (nick *np) {
  np->next=freenicks;
  freenicks=np;
}

host *newhost() {
  host *nh;
  int i;
  
  if (freehosts==NULL) {
    freehosts=(host *)malloc(ALLOCUNIT*sizeof(host));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freehosts[i].next=&(freehosts[i+1]);
    }
    freehosts[ALLOCUNIT-1].next=NULL;
  }
  
  nh=freehosts;
  freehosts=nh->next;
  
  return nh;
}

void freehost (host *hp) {
  hp->next=freehosts;
  freehosts=hp;
}

