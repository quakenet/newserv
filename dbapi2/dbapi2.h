#ifndef __DBAPI2_H
#define __DBAPI2_H

#define DBNAME_LEN 100

#if defined(USE_DBAPI_PGSQL)
#define DBAPI2_DEFAULT "pgsql"
#elseif defined(USE_DBAPI_SQLITE)
#define DBAPI2_DEFAULT "sqlite"
#else
#define DBAPI2_DEFAULT NULL
#endif

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

typedef DBAPI2_HANDLE *(*DBAPINew)(struct DBAPIConn *);
typedef void (*DBAPIClose)(struct DBAPIConn *);

typedef void (*DBAPIQueryCallback)(struct DBAPIResult *, void *);

typedef void (*DBAPIQuery)(struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, ...);
typedef void (*DBAPISimpleQuery)(struct DBAPIConn *, const char *, ...);
typedef void (*DBAPIQueryV)(struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, va_list);
typedef void (*DBAPICreateTable)(struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, ...);
typedef void (*DBAPILoadTable)(struct DBAPIConn *, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIQueryCallback, DBAPIUserData, const char *);

typedef void (*DBAPISafeQuery)(struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, const char *, ...);
typedef void (*DBAPISafeQueryV)(struct DBAPIConn *, DBAPIQueryCallback, DBAPIUserData, const char *, const char *, va_list);
typedef void (*DBAPISafeSimpleQuery)(struct DBAPIConn *, const char *, const char *, ...);
typedef void (*DBAPISafeCreateTable)(struct DBAPIConn *, const char *, ...);
typedef void (*DBAPISafeLoadTable)(struct DBAPIConn *, const char *, ...);

typedef void (*DBAPIEscapeString)(struct DBAPIConn *, char *, const char *, size_t);
typedef int (*DBAPIQuoteString)(struct DBAPIConn *, char *, size_t, const char *, size_t);

struct DBAPIProviderData;

typedef struct DBAPIProvider {
  DBAPINew new;
  DBAPIClose close;

  DBAPIQueryV query;
  DBAPICreateTable createtable;
  DBAPILoadTable loadtable;
/*
  DBAPISafeQuery safequery;
  DBAPISafeSimpleQuery safesimplequery;
  DBAPISafeCreateTable safecreatetable;
  DBAPISafeLoadTable safeloadtable;
*/

  DBAPIEscapeString escapestring;
  DBAPIQuoteString quotestring;

/* private members */
  struct DBAPIProviderData *__providerdata;
} DBAPIProvider;

typedef struct DBAPIConn {
  DBAPIClose close;

  DBAPIQuery query;
  DBAPISimpleQuery squery;
  DBAPICreateTable createtable;
  DBAPILoadTable loadtable;
  DBAPIEscapeString escapestring; /* deprecated */

  char name[DBNAME_LEN+1];

  void *handle;

/* private members */
  DBAPIClose __close;
  DBAPIQuoteString __quotestring;
  DBAPIQueryV __query;
} DBAPIConn;

typedef char *(*DBAPIResultGet)(struct DBAPIResult *, int);
typedef int (*DBAPIResultNext)(struct DBAPIResult *);
typedef void (*DBAPIResultClear)(struct DBAPIResult *);

typedef struct DBAPIResult {
  DBAPI2_RESULT_HANDLE *handle;

  unsigned short success, fields;

  DBAPIResultGet get;
  DBAPIResultNext next;
  DBAPIResultClear clear;
} DBAPIResult;

int registerdbprovider(const char *, DBAPIProvider *);
void deregisterdbprovider(int);
DBAPIConn *dbapi2open(const char *, int, const char *);

void dbsnprintf(DBAPIConn *, char *, size_t, const char *, const char *, ...);

#endif
