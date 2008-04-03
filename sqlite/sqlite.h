#ifndef __SQLITE_DB_H
#define __SQLITE_DB_H

#include <sqlite3.h>

#ifdef SQLITE_THREADSAFE
#error cannot use thread safe sqlite, recompile with --enable-threadsafe=no
#endif

typedef struct SQLiteResult {
  sqlite3_stmt *r;
  char first, final;
} SQLiteResult;

typedef SQLiteResult SQLiteConn;
typedef int SQLiteModuleIdentifier;
typedef void (*SQLiteQueryHandler)(SQLiteConn *, void *);

void sqliteasyncqueryf(SQLiteModuleIdentifier identifier, SQLiteQueryHandler handler, void *tag, int flags, char *format, ...);

int sqliteconnected(void);

int sqlitegetid(void);
void sqlitefreeid(int);

void sqliteescapestring(char *, char *, unsigned int);

SQLiteResult *sqlitegetresult(SQLiteConn *);
int sqlitefetchrow(SQLiteResult *);
void sqliteclear(SQLiteResult *);

int sqlitequerysuccessful(SQLiteResult *);

#define sqlitegetvalue(result, column) (char *)sqlite3_column_text(result->r, column)

void sqliteattach(char *schema);
void sqlitedetach(char *schema);
void sqliteloadtable(char *tablename, SQLiteQueryHandler init, SQLiteQueryHandler data, SQLiteQueryHandler fini);

#endif
