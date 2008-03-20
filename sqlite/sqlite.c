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
#include "../core/nsmalloc.h"
#include "sqlite.h"

#define BUILDING_DBAPI
#include "../dbapi/dbapi.h"

#include <stdlib.h>
#include <sys/poll.h>
#include <stdarg.h>
#include <string.h>
#define __USE_POSIX199309
#include <time.h>

MODULE_VERSION("");

static int dbconnected = 0;
static struct sqlite3 *conn;
static SQLiteModuleIdentifier modid;

#define SYNC_MODE "OFF"

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

  sqliteasyncqueryf(0, NULL, NULL, 0, "PRAGMA synchronous=" SYNC_MODE ";");
}

void _fini(void) {
  if(!sqliteconnected())
    return;

  sqlite3_close(conn);

  dbconnected = 0;
  nscheckfreeall(POOL_SQLITE);
}

int sqlitestep(sqlite3_stmt *s) {
  int rc;
  struct timespec t;

  t.tv_sec = 0;

  for(;;) {
    rc = sqlite3_step(s);
    if((rc == SQLITE_ROW) || (rc == SQLITE_OK) || (rc == SQLITE_DONE))
      return rc;
    if(rc == SQLITE_BUSY) {
      t.tv_nsec = rand() % 50 + 50;
      Error("sqlite", ERR_WARNING, "SQLite is busy, retrying in %dns...", t.tv_nsec);
      nanosleep(&t, NULL);
    } else {
      Error("sqlite", ERR_WARNING, "SQL error %d: %s", rc, sqlite3_errmsg(conn));
      return 0;
    }
  }
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
    rc = sqlitestep(s);
    sqlite3_finalize(s);
    if(!rc)
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

  r2 = (SQLiteResult *)nsmalloc(POOL_SQLITE, sizeof(SQLiteResult));
  r2->r = r;
  return r2;
}

int sqlitefetchrow(SQLiteResult *r) {
  if(sqlitestep(r->r) != SQLITE_ROW)
    return 0;

  return 1;
}

void sqliteclear(SQLiteResult *r) {
  if(!r)
    return;

  if(r->r)
    sqlite3_finalize(r->r);

  nsfree(POOL_SQLITE, r);
}

int sqlitequerysuccessful(SQLiteResult *r) {
  if(r && r->r)
    return 1;

  return 0;
}

struct sqlitetableloader {
  SQLiteQueryHandler init, data, fini;
  char tablename[];
};

static void loadtablerows(SQLiteConn *c, void *tag) {
  struct sqlitetableloader *t = (struct sqlitetableloader *)tag;

  if(!c) { /* pqsql doesnt call the handlers so we don't either */
    free(t);
    return;
  }

  /* the handlers do all the checking and cleanup */
  if(t->init)
    (t->init)(c, NULL);

  (t->data)(c, NULL);

  if(t->fini)
    (t->fini)(c, NULL);

  free(t);
}

static void loadtablecount(SQLiteConn *c, void *tag) {
  struct sqlitetableloader *t = (struct sqlitetableloader *)tag;
  SQLiteResult *r = NULL;

  if(!c) { /* unloaded */
    free(t);
    return;
  } 

  if(!(r = sqlitegetresult(c)) || !sqlitefetchrow(r)) {
    Error("sqlite", ERR_ERROR, "Error getting row count for %s.", t->tablename);
    free(t);

    if(r)
      sqliteclear(r);
    return;
  }

  Error("sqlite", ERR_INFO, "Found %s entries in table %s, loading...", (char *)sqlite3_column_text(r->r, 0), t->tablename);
  sqliteclear(r);
  
  sqliteasyncqueryf(0, loadtablerows, t, 0, "SELECT * FROM %s", t->tablename);
}

void sqliteloadtable(char *tablename, SQLiteQueryHandler init, SQLiteQueryHandler data, SQLiteQueryHandler fini) {
  struct sqlitetableloader *t;
  int len;

  if(!sqliteconnected())
    return;

  len = strlen(tablename);

  t = (struct sqlitetableloader *)malloc(sizeof(struct sqlitetableloader) + len + 1);
  memcpy(t->tablename, tablename, len + 1);
  t->init = init;
  t->data = data;
  t->fini = fini;

  sqliteasyncqueryf(0, loadtablecount, t, 0, "SELECT COUNT(*) FROM %s", tablename);
}

void sqlitecreateschema(char *schema) {
  sqliteasyncqueryf(0, NULL, NULL, 0, "ATTACH DATABASE '%s.db' AS %s", schema, schema);
  sqliteasyncqueryf(0, NULL, NULL, 0, "PRAGMA %s.synchronous=" SYNC_MODE ";", schema);
}

int sqlitegetid(void) {
  modid++; 
  if(modid == 0)
    modid = 1;

  return modid;
}

void sqlitefreeid(int id) {

}

