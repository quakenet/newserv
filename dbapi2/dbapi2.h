#ifndef __DBAPI2_H
#define __DBAPI2_H

#define DBNAME_LEN 100

#include "../config.h"

#define DBAPI2_DEFAULT NULL

#include <stdlib.h>
#include <stdarg.h>

struct DBAPIConn;

#ifndef DBAPI2_HANDLE
#define DBAPI2_HANDLE void
#endif

#ifndef DBAPI2_RESULT_HANDLE
#define DBAPI2_RESULT_HANDLE void
#endif

typedef void *DBAPIUserData;

struct DBAPIResult;

typedef DBAPI2_HANDLE *(*DBAPINew)(const struct DBAPIConn *);
typedef void (*DBAPIClose)(struct DBAPIConn *);

typedef void (*DBAPIQueryCallback)(const struct DBAPIResult *, void *);

typedef void (*DBAPIQuery)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, ...) __attribute__ ((format (printf, 4, 5)));
typedef void (*DBAPISimpleQuery)(const struct DBAPIConn *, const char *, ...) __attribute__ ((format (printf, 2, 3)));
typedef void (*DBAPIQueryV)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *);
typedef void (*DBAPICallV)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, const char *);
typedef void (*DBAPICreateTable)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, ...) __attribute__ ((format (printf, 4, 5)));
typedef void (*DBAPICreateTableV)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *);
typedef void (*DBAPILoadTable)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIUserData, const char *);

typedef void (*DBAPISafeQuery)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, const char *, ...);
typedef void (*DBAPISafeSimpleQuery)(const struct DBAPIConn *, const char *, const char *, ...);
typedef void (*DBAPISafeCreateTable)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, const char *, ...);

typedef void (*DBAPIEscapeString)(const struct DBAPIConn *, char *, const char *, size_t);
typedef int (*DBAPIQuoteString)(const struct DBAPIConn *, char *, size_t, const char *, size_t);
typedef void (*DBAPICall)(const struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, const char *, const char *, ...);
typedef void (*DBAPISimpleCall)(const struct DBAPIConn *, const char *, const char *, const char *, ...);

typedef char *(*DBAPITableName)(const struct DBAPIConn *, const char *);

struct DBAPIProviderData;

typedef struct DBAPIProvider {
  DBAPINew new;
  DBAPIClose close;

  DBAPIQueryV query;
  DBAPICreateTableV createtable;
  DBAPILoadTable loadtable;

  DBAPITableName tablename;

  DBAPIEscapeString escapestring;
  DBAPIQuoteString quotestring;
  DBAPICallV call;

/* private members */
  struct DBAPIProviderData *__providerdata;
} DBAPIProvider;

typedef struct DBAPIConn {
  DBAPIClose close;

  DBAPIQuery unsafequery;
  DBAPISimpleQuery unsafesquery;
  DBAPICreateTable unsafecreatetable;
  DBAPILoadTable loadtable;
  DBAPIEscapeString escapestring; /* deprecated */
  DBAPITableName tablename;

  DBAPISafeQuery query;
  DBAPISafeSimpleQuery squery;
  DBAPISafeCreateTable createtable;

  DBAPICall call;
  DBAPISimpleCall scall;

  char name[DBNAME_LEN+1];

  void *handle;

/* private members */
  DBAPIClose __close;
  DBAPIQuoteString __quotestring;
  DBAPIQueryV __query;
  DBAPICreateTableV __createtable;
  DBAPILoadTable __loadtable;
  DBAPICallV __call;
} DBAPIConn;

typedef char *(*DBAPIResultGet)(const struct DBAPIResult *, unsigned int);
typedef int (*DBAPIResultNext)(const struct DBAPIResult *);
typedef void (*DBAPIResultClear)(const struct DBAPIResult *);

typedef struct DBAPIResult {
  DBAPI2_RESULT_HANDLE *handle;

  unsigned short success, fields, affected;

  DBAPIResultGet get;
  DBAPIResultNext next;
  DBAPIResultClear clear;
} DBAPIResult;

int registerdbprovider(const char *, DBAPIProvider *);
void deregisterdbprovider(int);
DBAPIConn *dbapi2open(const char *, const char *);

#endif
