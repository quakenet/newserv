#include <stdlib.h>
#include <assert.h>
#include "../core/nsmalloc.h"
#include "trusts.h"

#define ALLOCUNIT  100

trustgroup_t *trustgroup_freelist;
trustgroupidentcount_t *trustgroupidentcount_freelist;
trusthost_t *trusthost_freelist;

trustgroup_t *newtrustgroup() {
  trustgroup_t *trustgroup;
  int i;

  if( trustgroup_freelist ==NULL ) {
    trustgroup_freelist=(trustgroup_t *)nsmalloc(POOL_TRUSTS,ALLOCUNIT*sizeof(trustgroup_t));

    for (i=0;i<(ALLOCUNIT-1);i++) {
      trustgroup_freelist[i].nextbyid=(trustgroup_t *)&(trustgroup_freelist[i+1]);
    }
    trustgroup_freelist[ALLOCUNIT-1].nextbyid=NULL;
  }

  trustgroup=trustgroup_freelist;
  trustgroup_freelist=(trustgroup_t *)trustgroup->nextbyid;

  return trustgroup;
}

void freetrustgroup (trustgroup_t *trustgroup) {
  trustgroup->nextbyid=(trustgroup_t *)trustgroup_freelist;
  trustgroup_freelist=trustgroup;
}

trusthost_t *newtrusthost() {
  trusthost_t *trusthost;
  int i;

  if( trusthost_freelist ==NULL ) {
    trusthost_freelist=(trusthost_t *)nsmalloc(POOL_TRUSTS,ALLOCUNIT*sizeof(trusthost_t));

    for (i=0;i<(ALLOCUNIT-1);i++) {
      trusthost_freelist[i].nextbyid=(trusthost_t *)&(trusthost_freelist[i+1]);
    }
    trusthost_freelist[ALLOCUNIT-1].nextbyid=NULL;
  }

  trusthost=trusthost_freelist;
  trusthost_freelist=(trusthost_t *)trusthost->nextbyid;

  return trusthost;
}

void freetrusthost (trusthost_t *trusthost) {
  trusthost->nextbyid=(trusthost_t *)trusthost_freelist;
  trusthost_freelist=trusthost;
}


trustgroupidentcount_t *newtrustgroupidentcount() {
  trustgroupidentcount_t *trustgroupidentcount;
  int i;

  if( trustgroupidentcount_freelist ==NULL ) {
    trustgroupidentcount_freelist=(trustgroupidentcount_t *)nsmalloc(POOL_TRUSTS,ALLOCUNIT*sizeof(trustgroupidentcount_t));

    for (i=0;i<(ALLOCUNIT-1);i++) {
      trustgroupidentcount_freelist[i].next=(trustgroupidentcount_t *)&(trustgroupidentcount_freelist[i+1]);
    }
    trustgroupidentcount_freelist[ALLOCUNIT-1].next=NULL;
  }

  trustgroupidentcount=trustgroupidentcount_freelist;
  trustgroupidentcount_freelist=(trustgroupidentcount_t *)trustgroupidentcount->next;

  return trustgroupidentcount;
}

void freetrustgroupidentcount (trustgroupidentcount_t *trustgroupidentcount) {
  trustgroupidentcount->next=(trustgroupidentcount_t *)trustgroupidentcount_freelist;
  trustgroupidentcount_freelist=trustgroupidentcount;
}

