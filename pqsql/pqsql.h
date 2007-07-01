#ifndef __PQSQL_DB_H
#define __PQSQL_DB_H

#include <libpq-fe.h>

#define QH_CREATE 0x01
#define PQ_ERRORMSG_LENGTH 1024

typedef void (*PQQueryHandler)(PGconn *, void *);

void pqasyncqueryf(PQQueryHandler handler, void *tag, int flags, char *format, ...);
void pqloadtable(char *tablename, PQQueryHandler init, PQQueryHandler data, PQQueryHandler fini);

#define pqasyncquery(handler, tag, format, ...) pqasyncqueryf(handler, tag, 0, format , ##__VA_ARGS__)
#define pqcreatequery(format, ...) pqasyncqueryf(NULL, NULL, QH_CREATE, format , ##__VA_ARGS__)
#define pqquery(format, ...) pqasyncqueryf(NULL, NULL, 0, format , ##__VA_ARGS__)
#define min(a,b) ((a > b) ? b : a)

int pqconnected(void);
char* pqlasterror(PGconn * pgconn);

#endif
