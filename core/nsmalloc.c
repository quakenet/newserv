/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>

#include "nsmalloc.h"
#define __NSMALLOC_C
#undef __NSMALLOC_H
#include "nsmalloc.h"

#include "../core/error.h"

void *nsmalloc(unsigned int poolid, size_t size);
void nsfree(unsigned int poolid, void *ptr);
void nsfreeall(unsigned int poolid);

struct nsminfo {
  struct nsminfo *next;
  struct nsminfo *prev;

  size_t size;
  char data[];
};

struct nsmpool {
  struct nsminfo first;

  unsigned long count;
  size_t size;
};

struct nsmpool pools[MAXPOOL];

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
  pools[poolid].count++;

  nsmp->next=pools[poolid].first.next;
  nsmp->prev=&pools[poolid].first;
  if (pools[poolid].first.next)
    pools[poolid].first.next->prev=nsmp;
  pools[poolid].first.next=nsmp;

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
  nsmp->prev->next=nsmp->next;
  if (nsmp->next)
    nsmp->next->prev=nsmp->prev;

  pools[poolid].size-=nsmp->size;
  pools[poolid].count--;

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
  nsmpn->prev->next=nsmpn;

  if (nsmpn->next)
    nsmpn->next->prev=nsmpn;

  return (void *)nsmpn->data;
}

void nsfreeall(unsigned int poolid) {
  struct nsminfo *nsmp, *nnsmp;
 
  if (poolid >= MAXPOOL)
    return;
 
  for (nsmp=pools[poolid].first.next;nsmp;nsmp=nnsmp) {
    nnsmp=nsmp->next;
    free(nsmp);
  }
  
  pools[poolid].first.next=NULL;
  pools[poolid].size=0;
  pools[poolid].count=0;
}

void nsexit() {
  unsigned int i;
  
  for (i=0;i<MAXPOOL;i++) {
    if (pools[i].first.next) {
      Error("core",ERR_INFO,"nsmalloc: Blocks still allocated in pool #%d (%lub, %lu items)\n",i,pools[i].size, pools[i].count);
      nsfreeall(i);
    }
  }
}

int nspoolstats(unsigned int poolid, size_t *size, unsigned long *count, char **poolname, size_t *realsize) {
  if (poolid >= MAXPOOL)
    return 0;

  *size = pools[poolid].size;
  *realsize = pools[poolid].size + pools[poolid].count * sizeof(struct nsminfo) + sizeof(struct nsmpool);
  *count = pools[poolid].count;
  *poolname = poolnames[poolid];

  return 1;
}
