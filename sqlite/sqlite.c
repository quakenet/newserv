/* 
 * SQLite module
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define __USE_POSIX199309
#include <time.h>

#include "../core/config.h"
#include "../core/error.h"
#include "../core/events.h"
#include "../core/hooks.h"
#include "../lib/version.h"
#include "../lib/strlfunc.h"
#include "../core/nsmalloc.h"
#include "../core/schedule.h"

#define BUILDING_DBAPI
#include "../dbapi/dbapi.h"

#include "sqlite.h"

MODULE_VERSION("");

static int dbconnected = 0;
static struct sqlite3 *conn;
static SQLiteModuleIdentifier modid;

struct sqlitequeue {
  sqlite3_stmt *statement;
  SQLiteQueryHandler handler;
  void *tag;
  int identifier;
  struct sqlitequeue *next;
};

static struct sqlitequeue *head, *tail;
static int queuesize;
static void *processsched;
static int inited;

#define SYNC_MODE "OFF"

static void sqlitequeueprocessor(void *arg);
static void dbstatus(int hooknum, void *arg);

void _init(void) {
  sstring *dbfile;
  int rc;

  dbfile = getcopyconfigitem("sqlite", "file", "newserv.db", 100);

  if(!dbfile) {
    Error("sqlite", ERR_ERROR, "Unable to get config settings.");
    return;
  }
  
  processsched = schedulerecurring(time(NULL), 0, 1, &sqlitequeueprocessor, NULL);
  if(!processsched) {
    Error("sqlite", ERR_ERROR, "Unable to schedule query processor.");
    freesstring(dbfile);
    return;
  }


  if(sqlite3_initialize() != SQLITE_OK) {
    Error("sqlite", ERR_ERROR, "Unable to initialise sqlite");
    return;
  }
  sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);

  inited = 1;

  rc = sqlite3_open(dbfile->content, &conn);
  freesstring(dbfile);

  if(rc) {
    Error("sqlite", ERR_ERROR, "Unable to connect to database: %s", sqlite3_errmsg(conn));
    deleteschedule(processsched, &sqlitequeueprocessor, NULL);
    return;
  }

  dbconnected = 1;

  sqliteasyncqueryf(0, NULL, NULL, 0, "PRAGMA synchronous=" SYNC_MODE ";");
  registerhook(HOOK_CORE_STATSREQUEST, dbstatus);
}

void _fini(void) {
  struct sqlitequeue *q, *nq;

  if(sqliteconnected()) {
    deregisterhook(HOOK_CORE_STATSREQUEST, dbstatus);
    deleteschedule(processsched, &sqlitequeueprocessor, NULL);

    /* we assume every module that's being unloaded
     * has us as a dependency and will have cleaned up
     * their queries by using freeid..
     */
    for(q=head;q;q=nq) {
      nq = q->next;
      sqlite3_finalize(q->statement);
      nsfree(POOL_SQLITE, q);
    }

    sqlite3_close(conn);

    dbconnected = 0;
  }

  if(inited) {
    sqlite3_shutdown();
    inited = 0;
  }

  nscheckfreeall(POOL_SQLITE);
}

/* busy processing is done externally as that varies depending on what you are... */
static void processstatement(int rc, sqlite3_stmt *s, SQLiteQueryHandler handler, void *tag, char *querybuf) {
  if(handler) { /* the handler deals with the cleanup */
    SQLiteResult *r;

    if((rc != SQLITE_ROW) && (rc != SQLITE_DONE)) {
      Error("sqlite", ERR_WARNING, "SQL error %d: %s (query: %s)", rc, sqlite3_errmsg(conn), querybuf);
      handler(NULL, tag);
      return;
    }

    r = (SQLiteResult *)nsmalloc(POOL_SQLITE, sizeof(SQLiteResult));
    r->r = s;
    r->first = 1;
    if(rc == SQLITE_ROW) {
      r->final = 0;
    } else if(rc == SQLITE_DONE) {
      r->final = 1;
    }
    handler(r, tag);
  } else {
    if(rc == SQLITE_ROW) {
      Error("sqlite", ERR_WARNING, "Unhandled data from query: %s", querybuf);
    } else if(rc != SQLITE_DONE) {
      Error("sqlite", ERR_WARNING, "SQL error %d: %s (query: %s)", rc, sqlite3_errmsg(conn), querybuf);
    }
    sqlite3_finalize(s);
  }
}

static void pushqueue(sqlite3_stmt *s, int identifier, SQLiteQueryHandler handler, void *tag) {
  struct sqlitequeue *q = (struct sqlitequeue *)nsmalloc(POOL_SQLITE, sizeof(struct sqlitequeue));

  q->identifier = identifier;
  q->handler = handler;
  q->tag = tag;
  q->next = NULL;
  q->statement = s;

  if(!tail) {
    head = q;
  } else {
    tail->next = q;
  }
  tail = q;
  queuesize++;
}

static struct sqlitequeue *peekqueue(void) {
  return head;
}

/* a weird pop that doesn't return the value */
static void popqueue(void) {
  struct sqlitequeue *q;
  if(!head)
    return;

  q = head;
  if(head == tail) {
    head = NULL;
    tail = NULL;
  } else {
    head = head->next;
  }

  nsfree(POOL_SQLITE, q);
  queuesize--;
  return;
}

void sqliteasyncqueryf(int identifier, SQLiteQueryHandler handler, void *tag, int flags, char *format, ...) {
  char querybuf[8192];
  int len;
  int rc;
  sqlite3_stmt *s;
  va_list va;

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

  if(head) { /* stuff already queued */
    pushqueue(s, identifier, handler, tag);
    return;
  }

  rc = sqlite3_step(s);
  if(rc == SQLITE_BUSY) {
    pushqueue(s, identifier, handler, tag);
    return;
  }

  processstatement(rc, s, handler, tag, querybuf);
}

int sqliteconnected(void) {
  return dbconnected;
}

size_t sqliteescapestring(char *buf, char *src, unsigned int len) {
  unsigned int i;
  char *p;

  for(p=src,i=0;i<len;i++,p++) {
    if(*p == '\'')
      *buf++ = *p;
    *buf++ = *p;
  }
  *buf = '\0';

  return p - src;
}

SQLiteResult *sqlitegetresult(SQLiteConn *r) {
  return r;
}

int sqlitefetchrow(SQLiteResult *r) {
  int rc, v;
  struct timespec t;

  if(!r || !r->r || r->final)
    return 0;

  if(r->first) { /* we've extracted the first row already */
    r->first = 0;
    return 1;
  }

  t.tv_sec = 0;
  for(;;) {
    rc = sqlite3_step(r->r);
    if(rc != SQLITE_BUSY)
      break;

    v = rand() % 50 + 50;
    t.tv_nsec = v * 1000000;
    Error("sqlite", ERR_WARNING, "SQLite is busy, retrying in %fs...", (double)v / 1000);
    nanosleep(&t, NULL);
  }

  if(rc == SQLITE_DONE) {
    r->final = 1;
    return 0;
  }

  if(rc != SQLITE_ROW) {
    Error("sqlite", ERR_WARNING, "SQL error %d: %s", rc, sqlite3_errmsg(conn));
    r->final = 1;
    return 0;
  }

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
  void *tag;
  char tablename[];
};

static void loadtablerows(SQLiteConn *c, void *tag) {
  struct sqlitetableloader *t = (struct sqlitetableloader *)tag;

  if(!c) { /* pqsql doesnt call the handlers so we don't either */
    nsfree(POOL_SQLITE, t);
    return;
  }

  /* the handlers do all the checking and cleanup */
  if(t->init)
    (t->init)(c, t->tag);

  (t->data)(c, t->tag);

  if(t->fini)
    (t->fini)(c, t->tag);

  nsfree(POOL_SQLITE, t);
}

static void loadtablecount(SQLiteConn *c, void *tag) {
  struct sqlitetableloader *t = (struct sqlitetableloader *)tag;
  SQLiteResult *r = NULL;

  if(!c) { /* unloaded */
    nsfree(POOL_SQLITE, t);
    return;
  } 

  if(!(r = sqlitegetresult(c)) || !sqlitefetchrow(r)) {
    Error("sqlite", ERR_ERROR, "Error getting row count for %s.", t->tablename);
    nsfree(POOL_SQLITE, t);

    if(r)
      sqliteclear(r);
    return;
  }

  Error("sqlite", ERR_INFO, "Found %s entries in table %s, loading...", (char *)sqlite3_column_text(r->r, 0), t->tablename);
  sqliteclear(r);
  
  sqliteasyncqueryf(0, loadtablerows, t, 0, "SELECT * FROM %s", t->tablename);
}

void sqliteloadtable(char *tablename, SQLiteQueryHandler init, SQLiteQueryHandler data, SQLiteQueryHandler fini, void *tag) {
  struct sqlitetableloader *t;
  int len;

  if(!sqliteconnected())
    return;

  len = strlen(tablename);

  t = (struct sqlitetableloader *)nsmalloc(POOL_SQLITE, sizeof(struct sqlitetableloader) + len + 1);
  memcpy(t->tablename, tablename, len + 1);
  t->init = init;
  t->data = data;
  t->fini = fini;
  t->tag = tag;

  sqliteasyncqueryf(0, loadtablecount, t, 0, "SELECT COUNT(*) FROM %s", tablename);
}

void sqliteattach(char *schema) {
  sqliteasyncqueryf(0, NULL, NULL, 0, "ATTACH DATABASE '%s.db' AS %s", schema, schema);
  sqliteasyncqueryf(0, NULL, NULL, 0, "PRAGMA %s.synchronous=" SYNC_MODE, schema);
}

void sqlitedetach(char *schema) {
  sqliteasyncqueryf(0, NULL, NULL, 0, "DETACH DATABASE %s", schema);
}

int sqlitegetid(void) {
  modid++; 
  if(modid == 0)
    modid = 1;

  return modid;
}

void sqlitefreeid(int id) {
  struct sqlitequeue *q, *pq;
  if(id == 0)
    return;

  for(pq=NULL,q=head;q;) {
    if(q->identifier == id) {
      if(pq) {
        pq->next = q->next;
        if(q == tail)
          tail = pq;
      } else { /* head */
        head = q->next;
        if(q == tail)
          tail = NULL;
      }
      sqlite3_finalize(q->statement);

      q->handler(NULL, q->tag);
      nsfree(POOL_SQLITE, q);

      queuesize--;
      if(pq) {
        q = pq->next;
      } else {
        q = head;
      }
    } else {
      pq = q;
      q = q->next;
    }
  }
}

static void sqlitequeueprocessor(void *arg) {
  struct sqlitequeue *q = peekqueue();

  while(q) {
    int rc = sqlite3_step(q->statement);
    if(rc == SQLITE_BUSY)
      return;

    processstatement(rc, q->statement, q->handler, q->tag, "??");
    popqueue();

    q = peekqueue();
  }
}

static void dbstatus(int hooknum, void *arg) {
  if((long)arg > 10) {
    char message[100];

    snprintf(message, sizeof(message), "SQLite  : %6d queries queued.", queuesize);
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }
}

