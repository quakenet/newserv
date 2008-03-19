/* 
 * SQLite module
 */

#include "../core/config.h"
#include "../core/error.h"
#include "../irc/irc_config.h"
#include "../core/events.h"
#include "../core/hooks.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../lib/strlfunc.h"
#include "sqlite.h"
#include "../dbapi/dbapi.h"

#include <stdlib.h>
#include <sys/poll.h>
#include <stdarg.h>
#include <string.h>

MODULE_VERSION("");

static int dbconnected = 0;
static struct sqlite3 *conn;

void _init(void) {
  sstring *dbfile;
  int rc;

  dbfile = getcopyconfigitem("sqlite", "file", "newserv.db", 100);

  if(!dbfile)
    return;
  
  rc = sqlite3_open(dbfile->content, &conn);
  freesstring(dbfile);

  if(rc) {
    Error("sqlite", ERR_ERROR, "Unable to connect to database: %s", sqlite3_errmsg(conn));
    return;
  }

  dbconnected = 1;

  sqliteasyncqueryf(0, NULL, NULL, 0, "PRAGMA synchronous=OFF;");

}

void _fini(void) {
  if(!sqliteconnected())
    return;

  sqlite3_close(conn);

  dbconnected = 0;
}

void sqliteasyncqueryf(int identifier, SQLiteQueryHandler handler, void *tag, int flags, char *format, ...) {
  char querybuf[8192];
  va_list va;
  int len;
  int rc;
  sqlite3_stmt *s;

  if(!sqliteconnected())
    return;

  va_start(va, format);
  len = vsnprintf(querybuf, sizeof(querybuf), format, va);
  va_end(va);

  rc = sqlite3_prepare(conn, querybuf, -1, &s, NULL);
  if(rc != SQLITE_OK) {
    if(flags != DB_CREATE)
      Error("sqlite", ERR_WARNING, "SQL error %d: %s (query: %s)", rc, sqlite3_errmsg(conn), querybuf);
    if(handler)
      handler(NULL, tag);
    return;
  }

  if(handler) {
    handler(s, tag);
  } else {
    rc = sqlite3_step(s);
    if(rc != SQLITE_DONE)
      Error("sqlite", ERR_WARNING, "SQL error %d: %s (query: %s)", rc, sqlite3_errmsg(conn), querybuf);
  }
}

int sqliteconnected(void) {
  return dbconnected;
}

void sqliteescapestring(char *buf, char *src, unsigned int len) {
  unsigned int i;
  char *p;

  for(p=src,i=0;i<len;i++,p++) {
    if(*p == '\'')
      *buf++ = *p;
    *buf++ = *p;
  }
  *buf = '\0';
}

SQLiteResult *sqlitegetresult(SQLiteConn *r) {
  SQLiteResult *r2;
  if(!r)
    return NULL;

  r2 = (SQLiteResult *)malloc(sizeof(SQLiteResult));
  r2->r = r;
  return r2;
}

int sqlitefetchrow(SQLiteResult *r) {
  int rc = sqlite3_step(r->r);
  if(rc != SQLITE_ROW)
    return 0;

  return 1;
}

void sqliteclear(SQLiteResult *r) {
  if(!r)
    return;

  if(r->r)
    sqlite3_finalize(r->r);

  free(r);
}

int sqlitequerysuccessful(SQLiteResult *r) {
  if(r && r->r)
    return 1;

  return 0;
}

void sqliteloadtable(char *tablename, SQLiteQueryHandler init, SQLiteQueryHandler data, SQLiteQueryHandler fini) {
  int rc;
  sqlite3_stmt *s;
  char buf[1024];
  
  if(!sqliteconnected())
    return;

  snprintf(buf, sizeof(buf), "SELECT COUNT(*) FROM %s", tablename);
  rc = sqlite3_prepare(conn, buf, -1, &s, NULL);
  if(rc != SQLITE_OK) {
    Error("sqlite", ERR_ERROR, "Error getting row count for %s.", tablename);
    return;
  }

  rc = sqlite3_step(s);
  if(rc != SQLITE_ROW) {
    Error("sqlite", ERR_ERROR, "Error getting row count for %s.", tablename);
    sqlite3_finalize(s);
    return;
  }

  Error("sqlite", ERR_INFO, "Found %s entries in table %s, loading...", (char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);

  snprintf(buf, sizeof(buf), "SELECT * FROM %s", tablename);
  rc = sqlite3_prepare(conn, buf, -1, &s, NULL);

  if(rc != SQLITE_OK) {
    Error("sqlite", ERR_WARNING, "SQL error %d: %s", rc, sqlite3_errmsg(conn));
    return;
  }

  if(init)
    init(s, NULL);
  data(s, NULL);
  if(fini)
    fini(s, NULL);
}
