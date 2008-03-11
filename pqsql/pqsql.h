#ifndef __PQSQL_DB_H
#define __PQSQL_DB_H

#include <libpq-fe.h>

#define QH_CREATE 0x01
#define PQ_ERRORMSG_LENGTH 1024

#define QH_NULLIDENTIFIER 0
#define QH_ALREADYFIRED 1

typedef int PQModuleIdentifier;
typedef void (*PQQueryHandler)(PGconn *, void *);

void pqloadtable(char *tablename, PQQueryHandler init, PQQueryHandler data, PQQueryHandler fini);

void pqasyncqueryf(PQModuleIdentifier identifier, PQQueryHandler handler, void *tag, int flags, char *format, ...);
#define pqasyncqueryi(identifier, handler, tag, format, ...) pqasyncqueryf(identifier, handler, tag, 0, format , ##__VA_ARGS__)
#define pqasyncquery(handler, tag, format, ...) pqasyncqueryf(QH_NULLIDENTIFIER, handler, tag, 0, format , ##__VA_ARGS__)
#define pqcreatequery(format, ...) pqasyncqueryf(QH_NULLIDENTIFIER, NULL, NULL, QH_CREATE, format , ##__VA_ARGS__)
#define pqquery(format, ...) pqasyncqueryf(QH_NULLIDENTIFIER, NULL, NULL, 0, format , ##__VA_ARGS__)

int pqconnected(void);
char* pqlasterror(PGconn *pgconn);

PQModuleIdentifier pqgetid(void);
void pqfreeid(PQModuleIdentifier identifier);

#endif
