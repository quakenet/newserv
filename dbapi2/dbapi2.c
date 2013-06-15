#define MAX_PROVIDERS 10
#define PROVIDER_NAME_LEN 100
#define QUERYBUFLEN 8192*2

#define VSNPF_MAXARGS   20
#define VSNPF_MAXARGLEN 2048

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <stdint.h>

#include "../core/error.h"
#include "../lib/strlfunc.h"
#include "../lib/stringbuf.h"
#include "../lib/version.h"
#include "dbapi2.h"

MODULE_VERSION("");

struct DBAPIProviderData {
  char name[PROVIDER_NAME_LEN+1];
};

static DBAPIProvider *providerobjs[MAX_PROVIDERS];
static struct DBAPIProviderData providerdata[MAX_PROVIDERS];

static void dbvsnprintf(const DBAPIConn *db, char *buf, size_t size, const char *format, const char *types, va_list ap);

void _init(void) {
  memset(providerobjs, 0, sizeof(providerobjs));
}

void _fini(void) {
  /* everything should be unregistered already */
}

int registerdbprovider(const char *name, DBAPIProvider *provider) {
  int i;

  for(i=0;i<MAX_PROVIDERS;i++) {
    if(providerobjs[i])
      continue;

    providerobjs[i] = provider;
    providerobjs[i]->__providerdata = &providerdata[i];

    strlcpy(providerobjs[i]->__providerdata->name, name, PROVIDER_NAME_LEN);

    Error("dbapi2", ERR_INFO, "Database API registered: %s", name);

    return i;
  }

  Error("dbapi2", ERR_WARNING, "No remaining space for new database provider: %s", name);
  return -1;
}

void deregisterdbprovider(int handle) {
  if(handle < 0)
    return;

  providerobjs[handle] = NULL;
}

static void dbclose(DBAPIConn *db) {
  db->__close(db);
  free((DBAPIConn *)db);
}

static void dbunsafequery(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];
  size_t ret;

  va_start(ap, format);
  ret = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if(ret >= sizeof(buf))
    Error("dbapi2", ERR_STOP, "Query truncated in dbunsafequery, format: '%s', database: %s", format, db->name);

  db->__query(db, cb, data, buf);
}

static void dbunsafecreatetable(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];
  size_t ret;

  va_start(ap, format);
  ret = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if(ret >= sizeof(buf))
    Error("dbapi2", ERR_STOP, "Query truncated in dbunsafecreatetable, format: '%s', database: %s", format, db->name);

  db->__createtable(db, cb, data, buf);
}

static void dbunsafesimplequery(const DBAPIConn *db, const char *format, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];
  size_t ret;

  va_start(ap, format);
  ret = vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  if(ret >= sizeof(buf))
    Error("dbapi2", ERR_STOP, "Query truncated in dbunsafequery, format: '%s', database: %s", format, db->name);

  db->__query(db, NULL, NULL, buf);
}

static void dbsafequery(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, const char *types, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];

  va_start(ap, types);
  dbvsnprintf(db, buf, sizeof(buf), format, types, ap);
  va_end(ap);

  db->__query(db, cb, data, buf);
}

static void dbsafecreatetable(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *format, const char *types, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];

  va_start(ap, types);
  dbvsnprintf(db, buf, sizeof(buf), format, types, ap);
  va_end(ap);

  db->__createtable(db, cb, data, buf);
}

static void dbsafesimplequery(const DBAPIConn *db, const char *format, const char *types, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];

  va_start(ap, types);
  dbvsnprintf(db, buf, sizeof(buf), format, types, ap);
  va_end(ap);

  db->__query(db, NULL, NULL, buf);
}

static void dbloadtable(const DBAPIConn *db, DBAPIQueryCallback init, DBAPIQueryCallback data, DBAPIQueryCallback fini, DBAPIUserData tag, const char *tablename) {
  db->__loadtable(db, init, data, fini, tag, db->tablename(db, tablename));
}

static void dbcall(const DBAPIConn *db, DBAPIQueryCallback cb, DBAPIUserData data, const char *function, const char *format, const char *types, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];

  va_start(ap, types);
  dbvsnprintf(db, buf, sizeof(buf), format, types, ap);
  va_end(ap);

  db->__call(db, cb, data, function, buf);
}

static void dbsimplecall(const DBAPIConn *db, const char *function, const char *format, const char *types, ...) {
  va_list ap;
  char buf[QUERYBUFLEN];

  va_start(ap, types);
  dbvsnprintf(db, buf, sizeof(buf), format, types, ap);
  va_end(ap);

  db->__call(db, NULL, NULL, function, buf);
}

DBAPIConn *dbapi2open(const char *provider, const char *database) {
  int i, found = -1;
  DBAPIConn *db;
  DBAPIProvider *p;

  if(provider) {
    for(i=0;i<MAX_PROVIDERS;i++) {
      if(providerobjs[i] && !strcmp(provider, providerobjs[i]->__providerdata->name)) {
        found = i;
        break;
      }
    }
    if(found == -1) {
      Error("dbapi2", ERR_WARNING, "Couldn't find forced database provider %s", provider);
      return NULL;
    }
  } else {
    for(i=0;i<MAX_PROVIDERS;i++) {
      if(providerobjs[i]) {
        found = i;
        break;
      }
    }

    if(found == -1) {
      Error("dbapi2", ERR_WARNING, "No database providers found.");
      return NULL;
    }
  }

  p = providerobjs[found];

  db = calloc(1, sizeof(DBAPIConn));
  if(!db)
    return NULL;

  db->close = dbclose;
  db->query = dbsafequery;
  db->createtable = dbsafecreatetable;
  db->squery = dbsafesimplequery;
  db->loadtable = dbloadtable;
  db->escapestring = p->escapestring;
  db->tablename = p->tablename;
  db->unsafequery = dbunsafequery;
  db->unsafesquery = dbunsafesimplequery;
  db->unsafecreatetable = dbunsafecreatetable;
  db->call = dbcall;
  db->scall = dbsimplecall;

  db->__query = p->query;
  db->__close = p->close;
  db->__quotestring = p->quotestring;
  db->__createtable = p->createtable;
  db->__loadtable = p->loadtable;
  db->__call = p->call;

  strlcpy(db->name, database, DBNAME_LEN);

  db->handle = p->new(db);
  if(!db->handle) {
    free(db);
    Error("dbapi2", ERR_WARNING, "Unable to initialise database %s, provider: %s", database, p->__providerdata->name);
    return NULL;
  }

  Error("dbapi2", ERR_INFO, "Database %s opened with provider %s.", database, p->__providerdata->name);

  return db;
}

static void dbvsnprintf(const DBAPIConn *db, char *buf, size_t size, const char *format, const char *types, va_list ap) {
  StringBuf b;
  const char *p;
  static char convbuf[VSNPF_MAXARGS][VSNPF_MAXARGLEN+10];
  int arg, argcount;

  if(size == 0)
    return;

  {
    int i;

    int d;
    char *s;
    double g;
    unsigned int u;
    size_t l;
    int fallthrough;
    time_t t;
    unsigned long ul;
    long _l;

    for(i=0;i<VSNPF_MAXARGS;i++)
      convbuf[i][0] = '\0';

    argcount=0;
    for(;*types;types++) {
      char *cb = convbuf[argcount];

      if(argcount++ >= VSNPF_MAXARGS) {
        /* calls exit(0) */
        Error("dbapi2", ERR_STOP, "Maximum arguments reached in dbvsnprintf, format: '%s', database: %s", format, db->name);
      }

      fallthrough = 0;
      switch(*types) {
        case 's':
          s = va_arg(ap, char *);
          if(s)
            l = strlen(s);
          fallthrough = 1;

        /* falling through */
        case 'S':
          if(!fallthrough) {
            s = va_arg(ap, char *);
            l = va_arg(ap, size_t);
          }

          if(!s) {
            strlcpy(cb, "NULL", sizeof(convbuf[0]));
          } else if((l > (VSNPF_MAXARGLEN / 2)) || !db->__quotestring(db, cb, sizeof(convbuf[0]), s, l)) {
            /* now... this is a guess, but we should catch it most of the time */
            Error("dbapi2", ERR_STOP, "Long string truncated, format: '%s', database: %s", format, db->name);
            l = VSNPF_MAXARGLEN;
          }

          break;
        case 'R':
          s = va_arg(ap, char *);

          strlcpy(cb, s, sizeof(convbuf[0]));
          break;
        case 'T':
          s = va_arg(ap, char *);

          strlcpy(cb, db->tablename(db, s), sizeof(convbuf[0]));
          break;
        case 'd':
          d = va_arg(ap, int);
          snprintf(cb, VSNPF_MAXARGLEN, "%d", d);
          break;
        case 'u':
          u = va_arg(ap, unsigned int);
          snprintf(cb, VSNPF_MAXARGLEN, "%u", u);
          break;
        case 't':
          t = va_arg(ap, time_t);
          snprintf(cb, VSNPF_MAXARGLEN, "%jd", (intmax_t)t);
          break;
        case 'D':
          _l = va_arg(ap, long);
          snprintf(cb, VSNPF_MAXARGLEN, "%ld", _l);
          break;
        case 'U':
          ul = va_arg(ap, unsigned long);
          snprintf(cb, VSNPF_MAXARGLEN, "%lu", ul);
          break;
        case 'g':
          g = va_arg(ap, double);
          snprintf(cb, VSNPF_MAXARGLEN, "%.1f", g);
          break;
        default:
          /* calls exit(0) */
          Error("dbapi2", ERR_STOP, "Bad format specifier '%c' supplied in dbvsnprintf, format: '%s', database: %s", *types, format, db->name);
      }
    }
  }

  sbinit(&b, buf, size);

  for(arg=0,p=format;*p;p++) {
    if(*p != '?') {
      if(!sbaddchar(&b, *p))
        break;
      continue;
    }

    if(arg >= argcount)
      Error("dbapi2", ERR_STOP, "Gone over number of arguments in dbvsnprintf, format: '%s', database: %s", format, db->name);

    if(!sbaddstr(&b, convbuf[arg]))
      Error("dbapi2", ERR_STOP, "Possible truncation in dbvsnprintf, format: '%s', database: %s", format, db->name);

    arg++;
  }

  sbterminate(&b);
}
