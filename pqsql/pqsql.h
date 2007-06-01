#ifndef __PQSQL_DB_H
#define __PQSQL_DB_H

#include <libpq-fe.h>

#define QH_CREATE 0x01

typedef void (*PQQueryHandler)(PGconn *, void *);

void pqasyncqueryf(PQQueryHandler handler, void *tag, int flags, char *format, ...);

#define pqasyncquery(handler, tag, format, ...) pqasyncqueryf(handler, tag, 0, format , ##__VA_ARGS__)
#define pqcreatequery(format, ...) pqasyncqueryf(NULL, NULL, QH_CREATE, format , ##__VA_ARGS__)
#define pqquery(format, ...) pqasyncquery(NULL, NULL, 0, format , ##__VA_ARGS__)

int pqconnected(void);

#endif
