#include <stdarg.h>

#include "../dbapi2/dbapi2.h"
#include "sqlite.h"
#include "../dbapi/dbapi.h"
#include "../lib/stringbuf.h"

DBAPI2_HANDLE *dbapi2_sqlite_new(DBAPIConn *);
void dbapi2_sqlite_close(DBAPIConn *);

void dbapi2_sqlite_query(DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, ...);
void dbapi2_sqlite_createtable(DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, ...);
void dbapi2_sqlite_loadtable(DBAPIConn *, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIUserData data, const char *);

void dbapi2_sqlite_escapestring(DBAPIConn *, char *, const char *, size_t);
int dbapi2_sqlite_quotestring(DBAPIConn *, char *, size_t, const char *, size_t);

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

DBAPI2_HANDLE *dbapi2_sqlite_new(DBAPIConn *db) {
  long id = sqlitegetid();

  sqliteattach(db->name);

  return (void *)id;
}

void dbapi2_sqlite_close(DBAPIConn *db) {
  sqlitefreeid((int)(long)db->handle);
  sqlitedetach(db->name);
}

static void dbapi2_sqlite_querywrapper(SQLiteConn *c, void *data) {
  struct DBAPI2SQLiteQueryCallback *a = data;

  a->callback(NULL, a->data);

  free(a);
}

void dbapi2_sqlite_query(DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, ...) {
  va_list ap;
  struct DBAPI2SQLiteQueryCallback *a = malloc(sizeof(struct DBAPI2SQLiteQueryCallback));

  a->db = db;
  a->data = data;
  a->callback = cb;

  va_start(ap, format);
  sqliteasyncqueryfv((int)(long)db->handle, dbapi2_sqlite_querywrapper, a, 0, (char *)format, ap);
  va_end(ap);
}

void dbapi2_sqlite_createtable(DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, ...) {
  va_list ap;
  struct DBAPI2SQLiteQueryCallback *a = malloc(sizeof(struct DBAPI2SQLiteQueryCallback));

  a->db = db;
  a->data = data;
  a->callback = cb;

  va_start(ap, format);
  sqliteasyncqueryfv((int)(long)db->handle, dbapi2_sqlite_querywrapper, a, DB_CREATE, (char *)format, ap);
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

void dbapi2_sqlite_loadtable(DBAPIConn *db, DBAPIQueryCallback init, DBAPIQueryCallback cb, DBAPIQueryCallback final, DBAPIUserData data, const char *table) {
  struct DBAPI2SQLiteLoadTableCallback *a = malloc(sizeof(struct DBAPI2SQLiteLoadTableCallback));

  a->db = db;
  a->data = data;

  a->init = init;
  a->callback = cb;
  a->fini = final;

  sqliteloadtable((char *)table, init?dbapi2_sqlite_loadtablewrapper_init:NULL, cb?dbapi2_sqlite_loadtablewrapper_data:NULL, dbapi2_sqlite_loadtablewrapper_fini, a);
}

void dbapi2_sqlite_escapestring(DBAPIConn *db, char *buf, const char *data, size_t len) {
  sqliteescapestring(buf, (char *)data, len);
}

int dbapi2_sqlite_quotestring(DBAPIConn *db, char *buf, size_t buflen, const char *data, size_t len) {
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
