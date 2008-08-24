/* nsmalloc: Simple pooled malloc() thing. */

#ifndef __NSMALLOC_H
#define __NSMALLOC_H

#ifdef __NSMALLOC_C
#define pool(x) #x
#define beginpools() char *nsmpoolnames[MAXPOOL] =
#define endpools();
#else
#define pool(x) POOL_ ## x
#define beginpools(x) typedef enum nsmallocpools
#define endpools() nsmallocpools; extern char *nsmpoolnames[MAXPOOL];

#include <stdlib.h>

void *nsmalloc(unsigned int poolid, size_t size);
void nsfree(unsigned int poolid, void *ptr);
void nsfreeall(unsigned int poolid);
void nsexit(void);
void *nsrealloc(unsigned int poolid, void *ptr, size_t size);
void nscheckfreeall(unsigned int poolid);

#define MAXPOOL		100

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

extern struct nsmpool nsmpools[MAXPOOL];

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
  pool(SQLITE),
  pool(PQSQL),
  pool(PATRICIA),
  pool(PATRICIANICK),
  pool(GLINE),
  pool(TRUSTS),
  pool(SPAMSCAN2),
} endpools()

#undef pool
#undef beginpools
#undef endpools

#endif
