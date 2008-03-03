/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "nsmalloc.h"
#define __NSMALLOC_C
#undef __NSMALLOC_H
#include "nsmalloc.h"

#include "../core/hooks.h"
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

void nsmstats(int hookhum, void *arg);

void initnsmalloc(void) {
  registerhook(HOOK_CORE_STATSREQUEST, &nsmstats);
}

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

void nscheckfreeall(unsigned int poolid) {
  if (poolid >= MAXPOOL)
    return;
 
  if (pools[poolid].first.next) {
    Error("core",ERR_INFO,"nsmalloc: Blocks still allocated in pool #%d (%s): %lub, %lu items",poolid,poolnames[poolid]?poolnames[poolid]:"??",pools[poolid].size,pools[poolid].count);
    nsfreeall(poolid);
  }
}

void nsexit(void) {
  unsigned int i;
  
  for (i=0;i<MAXPOOL;i++)
    nscheckfreeall(i);
}

static char *formatmbuf(unsigned long count, size_t size, size_t realsize) {
  static char buf[1024];

  snprintf(buf, sizeof(buf), "%lu items, %luKb allocated for %luKb, %luKb (%.2f%%) overhead", count, (unsigned long)size / 1024, (unsigned long)realsize / 1024, (unsigned long)(realsize - size) / 1024, (double)(realsize - size) / (double)size * 100);
  return buf;
}

void nsmstats(int hookhum, void *arg) {
  int i;
  char buf[1024], extra[1024];
  unsigned long totalcount = 0;
  size_t totalsize = 0, totalrealsize = 0;
  long level = (long)arg;

  for (i=0;i<MAXPOOL;i++) {
    struct nsmpool *pool=&pools[i];
    size_t realsize;

    if (!pool->count)
      continue;

    realsize=pool->size + pool->count * sizeof(struct nsminfo) + sizeof(struct nsmpool);

    totalsize+=pool->size;
    totalrealsize+=realsize;
    totalcount+=pool->count;

    if(level > 10) {
      extra[0] = '\0';
      if(level > 100) {
        struct nsminfo *np = pool->first.next;
        double mean = (double)pool->size / pool->count, variance;
        unsigned long long int sumsq = 0;

        for (np=pool->first.next;np;np=np->next)
          sumsq+=np->size * np->size;

        variance=(double)sumsq / pool->count - mean * mean;

        snprintf(extra, sizeof(extra), ", mean: %.2fKb stddev: %.2fKb", mean / 1024, sqrtf(variance) / 1024);
      }

      snprintf(buf, sizeof(buf), "NSMalloc: pool %2d (%10s): %s%s", i, poolnames[i]?poolnames[i]:"??", formatmbuf(pool->count, pool->size, realsize), extra);
      triggerhook(HOOK_CORE_STATSREPLY, buf);
    }
  }

  snprintf(buf, sizeof(buf), "NSMalloc: pool totals: %s", formatmbuf(totalcount, totalsize, totalrealsize));
  triggerhook(HOOK_CORE_STATSREPLY, buf);
}
