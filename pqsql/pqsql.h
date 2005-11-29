#ifndef __PQSQL_DB_H
#define __PQSQL_DB_H

#include <libpq-fe.h>

typedef void (*PQQueryHandler)(PGconn *, void *);

void pqasyncquery(PQQueryHandler handler, void *tag, char *format, ...);
#define pqquery(format, ...) pqasyncquery(NULL, NULL, format, __VA_ARGS__)
void pqsyncquery(char *format, ...);
int pqconnected(void);

#endif
