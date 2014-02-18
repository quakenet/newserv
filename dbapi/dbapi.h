#ifndef __DBAPI_H
#define __DBAPI_H

#ifndef DBAPI2_ADAPTER
#include "../config.h"
#endif

#define DB_NULLIDENTIFIER 0
#define DB_CREATE 1
#define DB_CALL 2

#ifdef DBAPI_OVERRIDE
#undef USE_DBAPI_PGSQL
#undef USE_DBAPI_SQLITE
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
#define dbloadtable(tablename, init, data, fini) pqloadtable(tablename, init, data, fini, NULL);
#define dbloadtable_tag(tablename, init, data, fini, tag) pqloadtable(tablename, init, data, fini, tag);

#define dbasyncqueryf(id, handler, tag, flags, format, ...) pqasyncqueryf(id, handler, tag, flags, format , ##__VA_ARGS__)
#define dbquerysuccessful(x) pqquerysuccessful(x)
#define dbgetresult(conn) pqgetresult(conn)
#define dbnumfields(x) PQnfields(x->result)
#define dbnumaffected(c, x) strtoul(PGcmdTuples(x->result), NULL, 10)

#define dbfetchrow(result) pqfetchrow(result)
#define dbgetvalue(result, column) pqgetvalue(result, column)

#define dbclear(result) pqclear(result)
#define dbcall(id, handler, tag, function, ...) pqasyncqueryf(id, handler, tag, (handler) == NULL ? DB_CALL : 0, "SELECT %s(%s)", function , ##__VA_ARGS__)

#endif /* DBAPI_PGSQL */

#if defined(USE_DBAPI_SQLITE) || defined(DBAPI_OVERRIDE_SQLITE)

#include "../sqlite/sqlite.h"

typedef SQLiteModuleIdentifier DBModuleIdentifier;
typedef SQLiteConn DBConn;
typedef SQLiteQueryHandler DBQueryHandler;
typedef SQLiteResult DBResult;

#define dbconnected() sqliteconnected()
#define dbgetid() sqlitegetid()
#define dbfreeid(x) sqlitefreeid(x)

#define dbattach(schema) sqliteattach((schema))
#define dbdetach(schema) sqlitedetach((schema))
#define dbescapestring(buf, src, len) sqliteescapestring(buf, (char *)(src), len)
#define dbloadtable(tablename, init, data, fini) sqliteloadtable(tablename, init, data, fini, NULL);
#define dbloadtable_tag(tablename, init, data, fini, tag) sqliteloadtable(tablename, init, data, fini, tag);

#define dbasyncqueryf(id, handler, tag, flags, format, ...) sqliteasyncqueryf(id, handler, tag, flags, format , ##__VA_ARGS__)
#define dbquerysuccessful(x) sqlitequerysuccessful(x)
#define dbgetresult(conn) sqlitegetresult(conn)
#define dbnumfields(x) sqlite3_column_count(x->r)
#define dbnumaffected(c, x) sqlite3_changes(sqlitegetconn())

#define dbfetchrow(result) sqlitefetchrow(result)
#define dbgetvalue(result, column) sqlitegetvalue(result, column)

#define dbclear(result) sqliteclear(result)

#define dbcall(...) abort() /* HA */

#endif /* DBAPI_SQLITE */

#endif /* BUILDING_DBAPI */

#define dbasyncqueryi(identifier, handler, tag, format, ...) dbasyncqueryf(identifier, handler, tag, 0, format , ##__VA_ARGS__)
#define dbasyncquery(handler, tag, format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, handler, tag, 0, format , ##__VA_ARGS__)
#define dbcreatequery(format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, DB_CREATE, format , ##__VA_ARGS__)
#define dbquery(format, ...) dbasyncqueryf(DB_NULLIDENTIFIER, NULL, NULL, 0, format , ##__VA_ARGS__)

#endif /* __DBAPI_H */
