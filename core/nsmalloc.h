/* nsmalloc: Simple pooled malloc() thing. */

#include <stdlib.h>

void *nsmalloc(unsigned int poolid, size_t size);
void nsfree(unsigned int poolid, void *ptr);
void nsfreeall(unsigned int poolid);

#define MAXPOOL		100

/* Pools here in the order they were created */
#define POOL_AUTHEXT	0
#define POOL_CHANINDEX	1
