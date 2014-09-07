/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "nsmalloc.h"
#define __NSMALLOC_C
#undef __NSMALLOC_H
#include "nsmalloc.h"

#include "../lib/valgrind.h"
#include "../lib/memcheck.h"
#include "../core/hooks.h"
#include "../core/error.h"

struct nsmpool nsmpools[MAXPOOL];

void *nsmalloc(unsigned int poolid, size_t size) {
  struct nsminfo *nsmp;
  
  if (poolid >= MAXPOOL)
    return NULL;
  
  /* Allocate enough for the structure and the required data */
  nsmp=(struct nsminfo *)malloc(sizeof(struct nsminfo)+size);

  if (!nsmp)
    return NULL;

  VALGRIND_CREATE_MEMPOOL(nsmp, 0, 0);

  nsmp->size=size;
  nsmpools[poolid].size+=size;
  nsmpools[poolid].count++;

  if (nsmpools[poolid].blocks) {
    nsmpools[poolid].blocks->prev = nsmp;
  }
  nsmp->next=nsmpools[poolid].blocks;
  nsmp->prev=NULL;
  nsmpools[poolid].blocks=nsmp;

  VALGRIND_MEMPOOL_ALLOC(nsmp, nsmp->data, nsmp->size);

  nsmp->redzone = REDZONE_MAGIC;
  VALGRIND_MAKE_MEM_NOACCESS(&nsmp->redzone, sizeof(nsmp->redzone));

  return (void *)nsmp->data;
}

void *nscalloc(unsigned int poolid, size_t nmemb, size_t size) {
  size_t total = nmemb * size;
  void *m;

  m = nsmalloc(poolid, total);
  if(!m)
    return NULL;

  memset(m, 0, total);

  return m;
}

void nsfree(unsigned int poolid, void *ptr) {
  struct nsminfo *nsmp;
  
  if (!ptr || poolid >= MAXPOOL)
    return;

  /* evil */
  nsmp=(struct nsminfo*)ptr - 1;

  VALGRIND_MAKE_MEM_DEFINED(&nsmp->redzone, sizeof(nsmp->redzone));
  assert(nsmp->redzone == REDZONE_MAGIC);

  if (nsmp->prev) {
    nsmp->prev->next = nsmp->next;
  } else
    nsmpools[poolid].blocks = NULL;

  if (nsmp->next) {
    nsmp->next->prev = nsmp->prev;
  }

  nsmpools[poolid].size-=nsmp->size;
  nsmpools[poolid].count--;

  VALGRIND_MEMPOOL_FREE(nsmp, nsmp->data);

  VALGRIND_DESTROY_MEMPOOL(nsmp);
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

  VALGRIND_MAKE_MEM_DEFINED(nsmp, sizeof(struct nsminfo));

  if (size == nsmp->size)
    return (void *)nsmp->data;

  nsmpn=(struct nsminfo *)realloc(nsmp, sizeof(struct nsminfo)+size);
  if (!nsmpn)
    return NULL;

  VALGRIND_MOVE_MEMPOOL(nsmp, nsmpn);

  nsmpools[poolid].size+=size-nsmpn->size;
  nsmpn->size=size;

  if (nsmpn->prev) {
    nsmpn->prev->next=nsmpn;
  } else
    nsmpools[poolid].blocks = nsmpn;

  if (nsmpn->next) {
    nsmpn->next->prev=nsmpn;
  }

  VALGRIND_MEMPOOL_CHANGE(nsmpn, nsmp->data, nsmpn->data, nsmpn->size);

  return (void *)nsmpn->data;
}

void nsfreeall(unsigned int poolid) {
  struct nsminfo *nsmp, *nnsmp;
 
  if (poolid >= MAXPOOL)
    return;
 
  for (nsmp=nsmpools[poolid].blocks;nsmp;nsmp=nnsmp) {
    nnsmp=nsmp->next;
    VALGRIND_MEMPOOL_FREE(nsmp, nsmp->data);

    VALGRIND_DESTROY_MEMPOOL(nsmp);
    free(nsmp);
  }
  
  nsmpools[poolid].blocks=NULL;
  nsmpools[poolid].size=0;
  nsmpools[poolid].count=0;
}

void nscheckfreeall(unsigned int poolid) {
  if (poolid >= MAXPOOL)
    return;
 
  if (nsmpools[poolid].blocks) {
    Error("core",ERR_INFO,"nsmalloc: Blocks still allocated in pool #%d (%s): %zub, %lu items",poolid,nsmpoolnames[poolid]?nsmpoolnames[poolid]:"??",nsmpools[poolid].size,nsmpools[poolid].count);
    nsfreeall(poolid);
  }
}

void nsinit(void) {
  memset(nsmpools, 0, sizeof(nsmpools));
}

void nsexit(void) {
  unsigned int i;
  
  for (i=0;i<MAXPOOL;i++)
    nsfreeall(i);
}

