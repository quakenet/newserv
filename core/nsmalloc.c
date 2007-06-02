/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>

#include "nsmalloc.h"
#include "../core/error.h"

void *nsmalloc(unsigned int poolid, size_t size);
void nsfree(unsigned int poolid, void *ptr);
void nsfreeall(unsigned int poolid);

struct nsminfo {
  struct nsminfo *next;
  char data[];
};

struct nsminfo *pools[MAXPOOL];

void *nsmalloc(unsigned int poolid, size_t size) {
  struct nsminfo *nsmp;
  
  if (poolid >= MAXPOOL)
    return NULL;
  
  /* Allocate enough for the structure and the required data */
  nsmp=(struct nsminfo *)malloc(sizeof(struct nsminfo)+size);

  if (!nsmp)
    return NULL;
  
  nsmp->next=pools[poolid];
  pools[poolid]=nsmp;
  
  return (void *)nsmp->data;
}

void nsfree(unsigned int poolid, void *ptr) {
  struct nsminfo *nsmp, **nsmh;
  
  if (poolid >= MAXPOOL)
    return;
    
  for (nsmh=&(pools[poolid]);*nsmh;nsmh=&((*nsmh)->next)) {
    if ((void *)&((*nsmh)->data) == ptr) {
      nsmp=*nsmh;
      *nsmh = nsmp->next;
      free(nsmp);
      return;
    }
  }
  
  Error("core",ERR_WARNING,"Attempt to free unknown pointer %p in pool %d\n",ptr,poolid);
}

void nsfreeall(unsigned int poolid) {
  struct nsminfo *nsmp, *nnsmp;
 
  if (poolid >= MAXPOOL)
    return;
 
  for (nsmp=pools[poolid];nsmp;nsmp=nnsmp) {
    nnsmp=nsmp->next;
    free(nsmp);
  }
  
  pools[poolid]=NULL;
}

void nsexit() {
  unsigned int i;
  
  for (i=0;i<MAXPOOL;i++) {
    if (pools[i]) {
      Error("core",ERR_INFO,"nsmalloc: Blocks still allocated in pool #%d\n",i);
      nsfreeall(i);
    }
  }
}

