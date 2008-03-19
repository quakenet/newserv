#ifndef __DBAPI_H
#define __DBAPI_H

#include "../config.h"

#define DB_NULLIDENTIFIER 0
#define DB_CREATE 1

#if defined(HAVE_PGSQL) && !defined(USE_SQLITE3)

#include "../pqsql/pqsql.h"

typedef PQModuleIdentifier DBModuleIdentifier;
typedef PGconn DBConn;
typedef PQQueryHandler DBQueryHandler;
typedef PQResult DBResult;

#define dbconnected() pqconnected()
#define dbgetid() pqconnected()
#define dbfreeid(x) pqfreeid(x)

#define dbcreateschema(schema) pqcreateschema(schema)
#define dbescapestring(buf, src, len)  PQescapeString(buf, src, len)
#define dbloadtable(tablename, init, data, fini) pqloadtable(tablename, init, data, fini);

#define dbasyncqueryf(id, handler, tag, flags, format, ...) pqasyncqueryf(id, handler, tag, flags, format , ##__VA_ARGS__)
#define dbquerysuccessful(x) pqquerysuccessful(x)
#define dbgetresult(conn) pqgetresult(conn)
#define dbnumfields(x) PQnfields(x->result)

#define dbfetchrow(result) pqfetchrow(result)
#define dbgetvalue(result, column) pqgetvalue(result, column)

#define dbclear(result) pqclear(result)

#else
#if defined(HAVE_SQLITE3)

#include "../sqlite/sqlite.h"

typedef SQLiteModuleIdentifier DBModuleIdentifier;
typedef SQLiteConn DBConn;
typedef SQLiteQueryHandler DBQueryHandler;
typedef SQLiteResult DBResult;

#define dbconnected() sqliteconnected()
#define dbgetid() sqliteconnected()
#define dbfreeid(x) sqlitefreeid()

#define dbcreateschema(schema) sqlitecreateschema(schema)
#define dbescapestring(buf, src, len) sqliteescapestring(buf, src, len)
#define dbloadtable(tablename, init, data, fini) sqliteloadtable(tablename, init, data, fini);

#define dbasyncqueryf(id, handler, tag, flags, format, ...) sqliteasyncqueryf(id, handler, tag, flags, format , ##__VA_ARGS__)
#define dbquerysuccessful(x) sqlitequerysuccessful(x)
#define dbgetresult(conn) sqlitegetresult(conn)
#define dbnumfields(x) sqlite3_column_count(x->r)

#define dbfetchrow(result) sqlitefetchrow(result)
#define dbgetvalue(result, column) sqlitegetvalue(result, column)

#define dbclear(result) sqliteclear(result)

#else
#error No DB API available.
#endif
#endif

#define dbasyncqueryi(identifier, handler, tag, format, ...) dbasyncqueryf(identifier, handler, tag, 0, format , ##__VA_ARGS__)
#define dbasyncquery(handler, tag, format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, handler, tag, 0, format , ##__VA_ARGS__)
#define dbcreatequery(format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, DB_CREATE, format , ##__VA_ARGS__)
#define dbquery(format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, 0, format , ##__VA_ARGS__)

#endif
