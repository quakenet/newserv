#ifndef __DBAPI_H
#define __DBAPI_H

#include "../config.h"

#define DB_NULLIDENTIFIER 0
#define DB_CREATE 1

#ifdef DBAPI_OVERRIDE
#undef USE_DBAPI_PGSQL
#undef USE_DBAPI_SQLITE3
#endif

#ifndef BUILDING_DBAPI

#if defined(USE_DBAPI_PGSQL) || defined(DBAPI_OVERRIDE_PGSQL)

#include "../pqsql/pqsql.h"

typedef PQModuleIdentifier DBModuleIdentifier;
typedef PGconn DBConn;
typedef PQQueryHandler DBQueryHandler;
typedef PQResult DBResult;

#define dbconnected() pqconnected()
#define dbgetid() pqgetid()
#define dbfreeid(x) pqfreeid(x)

#define dbattach(schema) pqcreateschema(schema)
#define dbdetach(schema)
#define dbescapestring(buf, src, len)  PQescapeString(buf, src, len)
#define dbloadtable(tablename, init, data, fini) pqloadtable(tablename, init, data, fini);

#define dbasyncqueryf(id, handler, tag, flags, format, ...) pqasyncqueryf(id, handler, tag, flags, format , ##__VA_ARGS__)
#define dbquerysuccessful(x) pqquerysuccessful(x)
#define dbgetresult(conn) pqgetresult(conn)
#define dbnumfields(x) PQnfields(x->result)

#define dbfetchrow(result) pqfetchrow(result)
#define dbgetvalue(result, column) pqgetvalue(result, column)

#define dbclear(result) pqclear(result)

#endif /* DBAPI_PGSQL */

#if defined(USE_DBAPI_SQLITE3) || defined(DBAPI_OVERRIDE_SQLITE3)

#include "../sqlite/sqlite.h"

typedef SQLiteModuleIdentifier DBModuleIdentifier;
typedef SQLiteConn DBConn;
typedef SQLiteQueryHandler DBQueryHandler;
typedef SQLiteResult DBResult;

#define dbconnected() sqliteconnected()
#define dbgetid() sqlitegetid()
#define dbfreeid(x) sqlitefreeid(x)

#define dbattach(schema) sqliteattach(schema)
#define dbdetach(schema) sqlitedetach(schema)
#define dbescapestring(buf, src, len) sqliteescapestring(buf, (char *)(src), len)
#define dbloadtable(tablename, init, data, fini) sqliteloadtable(tablename, init, data, fini);

#define dbasyncqueryf(id, handler, tag, flags, format, ...) sqliteasyncqueryf(id, handler, tag, flags, format , ##__VA_ARGS__)
#define dbquerysuccessful(x) sqlitequerysuccessful(x)
#define dbgetresult(conn) sqlitegetresult(conn)
#define dbnumfields(x) sqlite3_column_count(x->r)

#define dbfetchrow(result) sqlitefetchrow(result)
#define dbgetvalue(result, column) sqlitegetvalue(result, column)

#define dbclear(result) sqliteclear(result)

#endif /* DBAPI_SQLITE3 */

#endif /* BUILDING_DBAPI */

#define dbasyncqueryi(identifier, handler, tag, format, ...) dbasyncqueryf(identifier, handler, tag, 0, format , ##__VA_ARGS__)
#define dbasyncquery(handler, tag, format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, handler, tag, 0, format , ##__VA_ARGS__)
#define dbcreatequery(format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, DB_CREATE, format , ##__VA_ARGS__)
#define dbquery(format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, 0, format , ##__VA_ARGS__)

#endif /* __DBAPI_H */
