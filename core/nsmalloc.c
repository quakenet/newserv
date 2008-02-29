/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>

#include "nsmalloc.h"
#include "../core/error.h"

void *nsmalloc(unsigned int poolid, size_t size);
void nsfree(unsigned int poolid, void *ptr);
void nsfreeall(unsigned int poolid);

struct nsminfo {
  struct nsminfo *next;
  union {
    struct nsminfo *prev;
    unsigned long count;
  } p;

  size_t size;
  char data[];
};

struct nsminfo pools[MAXPOOL];

void *nsmalloc(unsigned int poolid, size_t size) {
  struct nsminfo *nsmp;
  
  if (poolid >= MAXPOOL)
    return NULL;
  
  /* Allocate enough for the structure and the required data */
  nsmp=(struct nsminfo *)malloc(sizeof(struct nsminfo)+size);

  if (!nsmp)
    return NULL;
  
  nsmp->size=size;
  pools[poolid].size+=size;
  pools[poolid].p.count++;

  nsmp->next=pools[poolid].next;
  nsmp->p.prev=&pools[poolid];
  if (pools[poolid].next)
    pools[poolid].next->p.prev=nsmp;
  pools[poolid].next=nsmp;

  return (void *)nsmp->data;
}

/* we dump core on ptr == NULL */
void nsfree(unsigned int poolid, void *ptr) {
  struct nsminfo *nsmp;
  
  if (poolid >= MAXPOOL)
    return;

  /* evil */
  nsmp=(struct nsminfo*)ptr - 1;

  /* always set as we have a sentinel */
  nsmp->p.prev->next=nsmp->next;
  if (nsmp->next)
    nsmp->next->p.prev=nsmp->p.prev;

  pools[poolid].size-=nsmp->size;
  pools[poolid].p.count--;

  free(nsmp);
  return;
}

void *nsrealloc(unsigned int poolid, void *ptr, size_t size) {
  struct nsminfo *nsmp, *nsmpn;

  if (ptr == NULL)
    return nsmalloc(poolid, size);

  if (size == 0) {
    nsfree(poolid, ptr);
    return NULL;
  }

  if (poolid >= MAXPOOL)
    return NULL;

  /* evil */
  nsmp=(struct nsminfo *)ptr - 1;

  if (size == nsmp->size)
    return (void *)nsmp->data;

  nsmpn=(struct nsminfo *)realloc(nsmp, sizeof(struct nsminfo)+size);
  if (!nsmpn)
    return NULL;

  pools[poolid].size+=size-nsmpn->size;
  nsmpn->size=size;

  /* always set as we have a sentinel */
  nsmpn->p.prev->next=nsmpn;

  if (nsmpn->next)
    nsmpn->next->p.prev=nsmpn;

  return (void *)nsmpn->data;
}

void nsfreeall(unsigned int poolid) {
  struct nsminfo *nsmp, *nnsmp;
 
  if (poolid >= MAXPOOL)
    return;
 
  for (nsmp=pools[poolid].next;nsmp;nsmp=nnsmp) {
    nnsmp=nsmp->next;
    free(nsmp);
  }
  
  pools[poolid].next=NULL;
  pools[poolid].size=0;
  pools[poolid].p.count=0;
}

void nsexit() {
  unsigned int i;
  
  for (i=0;i<MAXPOOL;i++) {
    if (pools[i].next) {
      Error("core",ERR_INFO,"nsmalloc: Blocks still allocated in pool #%d (%lub, %lu items)\n",i,pools[i].size, pools[i].p.count);
      nsfreeall(i);
    }
  }
}

int nspoolstats(unsigned int poolid, size_t *size, unsigned long *count) {
  if (poolid >= MAXPOOL)
    return 0;

  *size = pools[poolid].size;
  *count = pools[poolid].p.count;

  return 1;
}
