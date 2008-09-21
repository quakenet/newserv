#ifndef __PQSQL_DB_H
#define __PQSQL_DB_H

#include <libpq-fe.h>

#define PQ_ERRORMSG_LENGTH 1024

#define QH_ALREADYFIRED 1

typedef struct PQResult {
  PGresult *result;
  int row;
  int rows;
} PQResult;

typedef int PQModuleIdentifier;
typedef void (*PQQueryHandler)(PGconn *, void *);

void pqloadtable(char *tablename, PQQueryHandler init, PQQueryHandler data, PQQueryHandler fini, void *tag);

void pqasyncqueryf(PQModuleIdentifier identifier, PQQueryHandler handler, void *tag, int flags, char *format, ...) __attribute__ ((format (printf, 5, 6)));
#define pqasyncqueryi(identifier, handler, tag, format, ...) pqasyncqueryf(identifier, handler, tag, 0, format , ##__VA_ARGS__)
#define pqasyncquery(handler, tag, format, ...) pqasyncqueryf(DB_NULLIDENTIFIER, handler, tag, 0, format , ##__VA_ARGS__)
#define pqcreatequery(format, ...) pqasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, DB_CREATE, format , ##__VA_ARGS__)
#define pqquery(format, ...) pqasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, 0, format , ##__VA_ARGS__)

int pqconnected(void);

PQModuleIdentifier pqgetid(void);
void pqfreeid(PQModuleIdentifier identifier);

#define pqquerysuccessful(x) (x && (PQresultStatus(x->result) == PGRES_TUPLES_OK))

PQResult *pqgetresult(PGconn *c);
int pqfetchrow(PQResult *res);
char *pqgetvalue(PQResult *res, int column);
void pqclear(PQResult *res);

#define pqcreateschema(schema) pqcreatequery("CREATE SCHEMA %s;", schema)

#endif
