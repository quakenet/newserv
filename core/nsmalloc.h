/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>

void *nsmalloc(unsigned int poolid, size_t size);
void nsfree(unsigned int poolid, void *ptr);
void nsfreeall(unsigned int poolid);
void nsexit(void);
void *nsrealloc(unsigned int poolid, void *ptr, size_t size);
int nspoolstats(unsigned int poolid, size_t *size, unsigned long *count);

#define MAXPOOL		100

/* Pools here in the order they were created */
#define POOL_AUTHEXT		0
#define POOL_CHANINDEX		1
#define POOL_BANS		2
#define POOL_CHANNEL		3
#define POOL_NICK		4
#define POOL_CHANSERVDB		5
#define POOL_SSTRING		6
#define POOL_AUTHTRACKER	7
#define POOL_PROXYSCAN		8
#define POOL_LUA		9
#define POOL_TROJANSCAN		10
