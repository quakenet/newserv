/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>

#include "nsmalloc.h"
#define __NSMALLOC_C
#undef __NSMALLOC_H
#include "nsmalloc.h"

#include "../core/hooks.h"
#include "../core/error.h"

struct nsmpool nsmpools[MAXPOOL];

#ifndef USE_NSMALLOC_VALGRIND

void *nsmalloc(unsigned int poolid, size_t size) {
  struct nsminfo *nsmp;
  
  if (poolid >= MAXPOOL)
    return NULL;
  
  /* Allocate enough for the structure and the required data */
  nsmp=(struct nsminfo *)malloc(sizeof(struct nsminfo)+size);

  if (!nsmp)
    return NULL;
  
  nsmp->size=size;
  nsmpools[poolid].size+=size;
  nsmpools[poolid].count++;

  nsmp->next=nsmpools[poolid].first.next;
  nsmp->prev=&nsmpools[poolid].first;
  if (nsmpools[poolid].first.next)
    nsmpools[poolid].first.next->prev=nsmp;
  nsmpools[poolid].first.next=nsmp;

  return (void *)nsmp->data;
}

/* we dump core on ptr == NULL */
void nsfree(unsigned int poolid, void *ptr) {
  struct nsminfo *nsmp;
  
  if (!ptr || poolid >= MAXPOOL)
    return;

  /* evil */
  nsmp=(struct nsminfo*)ptr - 1;

  /* always set as we have a sentinel */
  nsmp->prev->next=nsmp->next;
  if (nsmp->next)
    nsmp->next->prev=nsmp->prev;

  nsmpools[poolid].size-=nsmp->size;
  nsmpools[poolid].count--;

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

  nsmpools[poolid].size+=size-nsmpn->size;
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
 
  for (nsmp=nsmpools[poolid].first.next;nsmp;nsmp=nnsmp) {
    nnsmp=nsmp->next;
    free(nsmp);
  }
  
  nsmpools[poolid].first.next=NULL;
  nsmpools[poolid].size=0;
  nsmpools[poolid].count=0;
}

void nscheckfreeall(unsigned int poolid) {
  if (poolid >= MAXPOOL)
    return;
 
  if (nsmpools[poolid].first.next) {
    Error("core",ERR_INFO,"nsmalloc: Blocks still allocated in pool #%d (%s): %lub, %lu items",poolid,nsmpoolnames[poolid]?nsmpoolnames[poolid]:"??",nsmpools[poolid].size,nsmpools[poolid].count);
    nsfreeall(poolid);
  }
}

void nsexit(void) {
  unsigned int i;
  
  for (i=0;i<MAXPOOL;i++)
    nsfreeall(i);
}

#else

void *nsmalloc(unsigned int poolid, size_t size) {
  return malloc(size);
}

void *nsrealloc(unsigned int poolid, void *ptr, size_t size) {
  return realloc(ptr, size);
}

void nsfree(unsigned int poolid, void *ptr) {
  if(ptr)
    free(ptr);
}

void nsfreeall(unsigned int poolid) {
}

void nsexit(void) {
}

void nscheckfreeall(unsigned int poolid) {
}

#endif

