#include <stdarg.h>

#include "sqlite.h"

#define DBAPI2_RESULT_HANDLE SQLiteConn
#include "../dbapi2/dbapi2.h"

#include "../dbapi/dbapi.h"
#include "../lib/stringbuf.h"

static DBAPI2_HANDLE *dbapi2_sqlite_new(DBAPIConn *);
static void dbapi2_sqlite_close(DBAPIConn *);

static void dbapi2_sqlite_query(DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, va_list);
static void dbapi2_sqlite_createtable(DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, ...);
static void dbapi2_sqlite_loadtable(DBAPIConn *, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIUserData data, const char *);

static void dbapi2_sqlite_escapestring(DBAPIConn *, char *, const char *, size_t);
static int dbapi2_sqlite_quotestring(DBAPIConn *, char *, size_t, const char *, size_t);

static DBAPIProvider sqliteprovider = {
  .new = dbapi2_sqlite_new,
  .close = dbapi2_sqlite_close,

  .query = dbapi2_sqlite_query,
  .createtable = dbapi2_sqlite_createtable,
  .loadtable = dbapi2_sqlite_loadtable,

  .escapestring = dbapi2_sqlite_escapestring,
  .quotestring = dbapi2_sqlite_quotestring

};

struct DBAPI2SQLiteQueryCallback {
  DBAPIConn *db;
  DBAPIUserData data;
  DBAPIQueryCallback callback;
};

struct DBAPI2SQLiteLoadTableCallback {
  DBAPIConn *db;
  DBAPIUserData data;
  DBAPIQueryCallback init, callback, fini;
};

static int sqlitehandle;

void registersqliteprovider(void) {
  sqlitehandle = registerdbprovider("sqlite", &sqliteprovider);
}

void deregistersqliteprovider(void) {
  deregisterdbprovider(sqlitehandle);
}

static DBAPI2_HANDLE *dbapi2_sqlite_new(DBAPIConn *db) {
  long id = sqlitegetid();

  sqliteattach(db->name);

  return (void *)id;
}

static void dbapi2_sqlite_close(DBAPIConn *db) {
  sqlitefreeid((int)(long)db->handle);
  sqlitedetach(db->name);
}

static char *dbapi2_sqlite_result_get(DBAPIResult *r, int column) {
  return sqlitegetvalue(r->handle, column);
}

static int dbapi2_sqlite_result_next(DBAPIResult *r) {
  return sqlitefetchrow(r->handle);
}

static void dbapi2_sqlite_result_clear(DBAPIResult *r) {
  if(!r)
    return;

  sqliteclear(r->handle);
/*  free(r);*/
}

static DBAPIResult *wrapresult(DBAPIResult *r, SQLiteConn *c) {
  if(!c)
    return NULL;

  if(!r)
    r = calloc(1, sizeof(DBAPIResult));

  r->clear = dbapi2_sqlite_result_clear;

  if(!sqlitequerysuccessful(c))
    return r;

  r->success = 1;
  r->fields = sqlite3_column_count(c->r);

  r->handle = c;
  r->get = dbapi2_sqlite_result_get;
  r->next = dbapi2_sqlite_result_next;

  return r;
}

static void dbapi2_sqlite_querywrapper(SQLiteConn *c, void *data) {
  struct DBAPI2SQLiteQueryCallback *a = data;
  DBAPIResult r_, *r = wrapresult(&r_, c);

  a->callback(r, a->data);

  free(a);
}

static void sqquery(DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, va_list ap, int flags) {
  struct DBAPI2SQLiteQueryCallback *a;

  if(cb) {
    a = malloc(sizeof(struct DBAPI2SQLiteQueryCallback));

    a->db = db;
    a->data = data;
    a->callback = cb;
  } else {
    a = NULL;
  }

  sqliteasyncqueryfv((int)(long)db->handle, cb?dbapi2_sqlite_querywrapper:NULL, a, flags, (char *)format, ap);
}

static void dbapi2_sqlite_query(DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, va_list ap) {
  sqquery(db, cb, data, format, ap, 0);
}

static void dbapi2_sqlite_createtable(DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, ...) {
  va_list ap;

  va_start(ap, format);
  sqquery(db, cb, data, format, ap, DB_CREATE);
  va_end(ap);
}

static void dbapi2_sqlite_loadtablewrapper_init(SQLiteConn *c, void *data) {
  struct DBAPI2SQLiteLoadTableCallback *a = data;

  if(a->init)
    a->init(NULL, a->data);
}

static void dbapi2_sqlite_loadtablewrapper_data(SQLiteConn *c, void *data) {
  struct DBAPI2SQLiteLoadTableCallback *a = data;

  a->callback(NULL, a->data);
}

static void dbapi2_sqlite_loadtablewrapper_fini(SQLiteConn *c, void *data) {
  struct DBAPI2SQLiteLoadTableCallback *a = data;

  if(a->fini)
    a->fini(NULL, a->data);

  free(a);
}

static void dbapi2_sqlite_loadtable(DBAPIConn *db, DBAPIQueryCallback init, DBAPIQueryCallback cb, DBAPIQueryCallback final, DBAPIUserData data, const char *table) {
  struct DBAPI2SQLiteLoadTableCallback *a = malloc(sizeof(struct DBAPI2SQLiteLoadTableCallback));

  a->db = db;
  a->data = data;

  a->init = init;
  a->callback = cb;
  a->fini = final;

  sqliteloadtable((char *)table, init?dbapi2_sqlite_loadtablewrapper_init:NULL, cb?dbapi2_sqlite_loadtablewrapper_data:NULL, dbapi2_sqlite_loadtablewrapper_fini, a);
}

static void dbapi2_sqlite_escapestring(DBAPIConn *db, char *buf, const char *data, size_t len) {
  sqliteescapestring(buf, (char *)data, len);
}

static int dbapi2_sqlite_quotestring(DBAPIConn *db, char *buf, size_t buflen, const char *data, size_t len) {
  StringBuf b;
  sbinit(&b, buf, buflen);
  char xbuf[len * 2 + 5];
  size_t esclen;

  esclen = sqliteescapestring(xbuf, (char *)data, len);

  sbaddchar(&b, '\'');
  sbaddstrlen(&b, xbuf, esclen);
  sbaddchar(&b, '\'');

  if(sbterminate(&b))
    return 0;

  return 1;
}
