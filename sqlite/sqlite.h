#ifndef __SQLITE_DB_H
#define __SQLITE_DB_H

#include <sqlite3.h>

typedef sqlite3_stmt SQLiteConn;
typedef int SQLiteModuleIdentifier;
typedef void (*SQLiteQueryHandler)(SQLiteConn *, void *);

typedef struct SQLiteResult {
  SQLiteConn *r;
} SQLiteResult;

void sqliteasyncqueryf(SQLiteModuleIdentifier identifier, SQLiteQueryHandler handler, void *tag, int flags, char *format, ...);

int sqliteconnected(void);

#define sqlitelitegetid(void) 0
#define sqlitefreeid(x)

void sqliteescapestring(char *, char *, unsigned int);

SQLiteResult *sqlitegetresult(SQLiteConn *);
int sqlitefetchrow(SQLiteResult *);
void sqliteclear(SQLiteResult *);

int sqlitequerysuccessful(SQLiteResult *);

#define sqlitegetvalue(result, column) (char *)sqlite3_column_text(result->r, column)

void sqlitecreateschema(char *schema);
void sqliteloadtable(char *tablename, SQLiteQueryHandler init, SQLiteQueryHandler data, SQLiteQueryHandler fini);

#endif
