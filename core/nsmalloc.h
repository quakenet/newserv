/* nsmalloc: Simple pooled malloc() thing. */

#ifndef __NSMALLOC_H
#define __NSMALLOC_H

#ifdef __NSMALLOC_C
#define pool(x) #x
#define beginpools() char *poolnames[MAXPOOL] =
#define endpools();
#else
#define pool(x) POOL_ ## x
#define beginpools(x) typedef enum nsmallocpools
#define endpools() nsmallocpools;

#include <stdlib.h>

void *nsmalloc(unsigned int poolid, size_t size);
void nsfree(unsigned int poolid, void *ptr);
void nsfreeall(unsigned int poolid);
void nsexit(void);
void *nsrealloc(unsigned int poolid, void *ptr, size_t size);
void nscheckfreeall(unsigned int poolid);
void initnsmalloc(void);

#define MAXPOOL		100

#endif

/* Pools here in the order they were created */

beginpools() {
  pool(AUTHEXT),
  pool(CHANINDEX),
  pool(BANS),
  pool(CHANNEL),
  pool(NICK),
  pool(CHANSERVDB),
  pool(SSTRING),
  pool(AUTHTRACKER),
  pool(PROXYSCAN),
  pool(LUA),
  pool(TROJANSCAN),
  pool(NTERFACER),
} endpools()

#undef pool
#undef beginpools
#undef endpools

#endif
