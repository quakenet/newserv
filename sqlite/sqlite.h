#ifndef __SQLITE_DB_H
#define __SQLITE_DB_H

#include "../sqlite/libsqlite3/sqlite3.h"

typedef struct SQLiteResult {
  sqlite3_stmt *r;
  char first, final;
} SQLiteResult;

typedef SQLiteResult SQLiteConn;
typedef int SQLiteModuleIdentifier;
typedef void (*SQLiteQueryHandler)(SQLiteConn *, void *);

void sqliteasyncqueryf(SQLiteModuleIdentifier identifier, SQLiteQueryHandler handler, void *tag, int flags, char *format, ...) __attribute__ ((format (printf, 5, 6)));

int sqliteconnected(void);

int sqlitegetid(void);
void sqlitefreeid(int);

void sqliteescapestring(char *, char *, unsigned int);

SQLiteResult *sqlitegetresult(SQLiteConn *);
int sqlitefetchrow(SQLiteResult *);
void sqliteclear(SQLiteResult *);

int sqlitequerysuccessful(SQLiteResult *);

#define sqlitegetvalue(result, column) ((char *)sqlite3_column_text(result->r, column))

void sqliteattach(char *schema);
void sqlitedetach(char *schema);
void sqliteloadtable(char *tablename, SQLiteQueryHandler init, SQLiteQueryHandler data, SQLiteQueryHandler fini);

#endif
