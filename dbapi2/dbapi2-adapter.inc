#include <stdarg.h>
#include <stdio.h>

#define DBAPI2_ADAPTER
#include "../dbapi/dbapi.h"

#define DBAPI2_RESULT_HANDLE DBResult

#include "../dbapi2/dbapi2.h"
#include "../lib/stringbuf.h"
#include "../lib/version.h"

static DBAPI2_HANDLE *dbapi2_adapter_new(const DBAPIConn *);
static void dbapi2_adapter_close(DBAPIConn *);

static void dbapi2_adapter_query(const DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *);
static void dbapi2_adapter_createtable(const DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *);
static void dbapi2_adapter_loadtable(const DBAPIConn *, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIUserData data, const char *);

static void dbapi2_adapter_escapestring(const DBAPIConn *, char *, const char *, size_t);
static int dbapi2_adapter_quotestring(const DBAPIConn *, char *, size_t, const char *, size_t);

static char *dbapi2_adapter_tablename(const DBAPIConn *, const char *);

static void dbapi2_adapter_call(const DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, const char *);

static DBAPIProvider adapterprovider = {
  .new = dbapi2_adapter_new,
  .close = dbapi2_adapter_close,

  .query = dbapi2_adapter_query,
  .createtable = dbapi2_adapter_createtable,
  .loadtable = dbapi2_adapter_loadtable,

  .escapestring = dbapi2_adapter_escapestring,
  .quotestring = dbapi2_adapter_quotestring,

  .tablename = dbapi2_adapter_tablename,

  .call = dbapi2_adapter_call,
};

struct DBAPI2AdapterQueryCallback {
  const DBAPIConn *db;
  DBAPIUserData data;
  DBAPIQueryCallback callback;
};

struct DBAPI2AdapterLoadTableCallback {
  const DBAPIConn *db;
  DBAPIUserData data;
  DBAPIQueryCallback init, callback, fini;
};

static int adapterhandle;

static void registeradapterprovider(void) {
  adapterhandle = registerdbprovider(DBAPI2_ADAPTER_NAME, &adapterprovider);
}

static void deregisteradapterprovider(void) {
  deregisterdbprovider(adapterhandle);
}

static DBAPI2_HANDLE *dbapi2_adapter_new(const DBAPIConn *db) {
  long id = dbgetid();

  dbattach(((DBAPIConn *)db)->name);

  return (void *)id;
}

static void dbapi2_adapter_close(DBAPIConn *db) {
  dbfreeid((int)(long)db->handle);
  dbdetach(db->name);
}

static char *dbapi2_adapter_result_get(const DBAPIResult *r, unsigned int column) {
  return dbgetvalue(r->handle, column);
}

static int dbapi2_adapter_result_next(const DBAPIResult *r) {
  return dbfetchrow(r->handle);
}

static void dbapi2_adapter_result_clear(const DBAPIResult *r) {
  if(!r)
    return;

  dbclear(r->handle);
}

static DBAPIResult *wrapresult(DBAPIResult *r, DBConn *c) {
  if(!c)
    return NULL;

  if(!r)
    r = calloc(1, sizeof(DBAPIResult));

  r->clear = dbapi2_adapter_result_clear;
  r->handle = dbgetresult(c);

  if(!dbquerysuccessful(r->handle))
    return r;

  r->success = 1;
  r->fields = dbnumfields(r->handle);

  r->get = dbapi2_adapter_result_get;
  r->next = dbapi2_adapter_result_next;

  return r;
}

static void dbapi2_adapter_querywrapper(DBConn *c, void *data) {
  struct DBAPI2AdapterQueryCallback *a = data;
  DBAPIResult r;

  a->callback(wrapresult(&r, c), a->data);

  free(a);
}

static void sqquery(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, int flags, const char *query) {
  struct DBAPI2AdapterQueryCallback *a;

  if(cb) {
    a = malloc(sizeof(struct DBAPI2AdapterQueryCallback));

    a->db = db;
    a->data = data;
    a->callback = cb;
  } else {
    a = NULL;
  }

  dbasyncqueryf((int)(long)db->handle, cb?dbapi2_adapter_querywrapper:NULL, a, flags, "%s", query);
}

static void dbapi2_adapter_query(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *query) {
  sqquery(db, cb, data, 0, query);
}

static void dbapi2_adapter_createtable(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *query) {
  sqquery(db, cb, data, DB_CREATE, query);
}

static void dbapi2_adapter_loadtablewrapper_init(DBConn *c, void *data) {
  struct DBAPI2AdapterLoadTableCallback *a = data;

  a->init(NULL, a->data);
}

static void dbapi2_adapter_loadtablewrapper_data(DBConn *c, void *data) {
  struct DBAPI2AdapterLoadTableCallback *a = data;
  DBAPIResult r;

  a->callback(wrapresult(&r, c), a->data);
}

static void dbapi2_adapter_loadtablewrapper_fini(DBConn *c, void *data) {
  struct DBAPI2AdapterLoadTableCallback *a = data;

  if(a->fini)
    a->fini(NULL, a->data);

  free(a);
}

static void dbapi2_adapter_loadtable(const DBAPIConn *db, DBAPIQueryCallback init, DBAPIQueryCallback cb, DBAPIQueryCallback final, DBAPIUserData data, const char *table) {
  struct DBAPI2AdapterLoadTableCallback *a = malloc(sizeof(struct DBAPI2AdapterLoadTableCallback));

  a->db = db;
  a->data = data;

  a->init = init;
  a->callback = cb;
  a->fini = final;

  dbloadtable_tag((char *)table, init?dbapi2_adapter_loadtablewrapper_init:NULL, cb?dbapi2_adapter_loadtablewrapper_data:NULL, dbapi2_adapter_loadtablewrapper_fini, a);
}

static void dbapi2_adapter_escapestring(const DBAPIConn *db, char *buf, const char *data, size_t len) {
  dbescapestring(buf, (char *)data, len);
}

#ifndef DBAPI2_CUSTOM_QUOTESTRING
static int dbapi2_adapter_quotestring(const DBAPIConn *db, char *buf, size_t buflen, const char *data, size_t len) {
  StringBuf b;
  char xbuf[len * 2 + 5];
  size_t esclen;

  sbinit(&b, buf, buflen);
  esclen = dbescapestring(xbuf, (char *)data, len);

  sbaddchar(&b, '\'');
  sbaddstrlen(&b, xbuf, esclen);
  sbaddchar(&b, '\'');

  if(!sbterminate(&b))
    return 0;

  return 1;
}
#endif

#ifndef DBAPI2_CUSTOM_TABLENAME
static char *dbapi2_adapter_tablename(const DBAPIConn *db, const char *tablename) {
  static char buf[1024];

  snprintf(buf, sizeof(buf), "%s.%s", db->name, tablename);

  return buf;
}
#endif

static void dbapi2_adapter_call(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *function, const char *query) {
  struct DBAPI2AdapterQueryCallback *a;
  char buf[512];

  if(cb) {
    a = malloc(sizeof(struct DBAPI2AdapterQueryCallback));

    a->db = db;
    a->data = data;
    a->callback = cb;
  } else {
    a = NULL;
  }

  snprintf(buf, sizeof(buf), "%s", db->tablename(db, function));
  dbcall((int)(long)db->handle, cb?dbapi2_adapter_querywrapper:NULL, a, buf, query);
}

