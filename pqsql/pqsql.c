/* 
 * PQSQL module
 *
 * 99% of the handling is stolen from Q9.
 */

#include "../core/config.h"
#include "../core/error.h"
#include "../irc/irc_config.h"
#include "../core/events.h"
#include "../core/hooks.h"
#include "../lib/version.h"
#include "pqsql.h"

#include <stdlib.h>
#include <sys/poll.h>
#include <stdarg.h>

MODULE_VERSION("");

/* It's possible that we might want to do a very long query, longer than the
 * IRC-oriented SSTRING_MAX value.  One option would be to increase
 * SSTRING_MAX, but the whole purpose of sstring's is to efficiently deal
 * with situations where the malloc() padding overhead is large compared to
 * string length and strings are frequently recycled.  Since neither of
 * these are necessarily true for longer strings it makes more sense to use
 * malloc() for them. 
 *
 * So, query always points at the query string.  If it fitted in a sstring,
 * query_ss will point at the sstring for freeing purposes.  If query_ss is
 * NULL then it was malloc'd so should be free()'d directly.
 */
typedef struct pqasyncquery_s {
  sstring *query_ss;
  char *query;
  void *tag;
  PQQueryHandler handler;
  int flags;
  struct pqasyncquery_s *next;
} pqasyncquery_s;

pqasyncquery_s *queryhead = NULL, *querytail = NULL;

int dbconnected = 0;
PGconn *dbconn;

void dbhandler(int fd, short revents);
void dbstatus(int hooknum, void *arg);
void disconnectdb(void);
void connectdb(void);

void _init(void) {
  connectdb();
}

void _fini(void) {
  disconnectdb();
}

void connectdb(void) {
  sstring *dbhost, *dbusername, *dbpassword, *dbdatabase, *dbport;
  char connectstr[1024];

  if(pqconnected())
    return;

  /* stolen from chanserv as I'm lazy */
  dbhost = getcopyconfigitem("pqsql", "host", "localhost", HOSTLEN);
  dbusername = getcopyconfigitem("pqsql", "username", "newserv", 20);
  dbpassword = getcopyconfigitem("pqsql", "password", "moo", 20);
  dbdatabase = getcopyconfigitem("pqsql", "database", "newserv", 20);
  dbport = getcopyconfigitem("pqsql", "port", "431", 8);

  if(!dbhost || !dbusername || !dbpassword || !dbdatabase || !dbport) {
    /* freesstring allows NULL */
    freesstring(dbhost);
    freesstring(dbusername);
    freesstring(dbpassword);
    freesstring(dbdatabase);
    freesstring(dbport);
    return;
  }

  snprintf(connectstr, sizeof(connectstr), "dbname=%s user=%s password=%s", dbdatabase->content, dbusername->content, dbpassword->content);
  
  freesstring(dbhost);
  freesstring(dbusername);
  freesstring(dbpassword);
  freesstring(dbdatabase);
  freesstring(dbport);

  Error("pqsql", ERR_INFO, "Attempting database connection: %s", connectstr);

  /* Blocking connect for now.. */
  dbconn = PQconnectdb(connectstr);
  
  if (!dbconn || (PQstatus(dbconn) != CONNECTION_OK)) {
    Error("pqsql", ERR_ERROR, "Unable to connect to db.");
    return;
  }
  Error("pqsql", ERR_INFO, "Connected!");

  dbconnected = 1;

  PQsetnonblocking(dbconn, 1);

  /* this kicks ass, thanks splidge! */
  registerhandler(PQsocket(dbconn), POLLIN, dbhandler);
  registerhook(HOOK_CORE_STATSREQUEST, dbstatus);
}

void dbhandler(int fd, short revents) {
  PGresult *res;
  pqasyncquery_s *qqp;

  if(revents & POLLIN) {
    PQconsumeInput(dbconn);
    
    if(!PQisBusy(dbconn)) { /* query is complete */
      if(queryhead->handler)
        (queryhead->handler)(dbconn, queryhead->tag);

      while((res = PQgetResult(dbconn))) {
        switch(PQresultStatus(res)) {
          case PGRES_TUPLES_OK:
            Error("pqsql", ERR_WARNING, "Unhandled tuples output (query: %s)", queryhead->query->content);
            break;

          case PGRES_NONFATAL_ERROR:
          case PGRES_FATAL_ERROR:
            /* if a create query returns an error assume it went ok, paul will winge about this */
            if(!(queryhead->flags & QH_CREATE))
              Error("pqsql", ERR_WARNING, "Unhandled error response (query: %s)", queryhead->query->content);
            break;
	  
          default:
            break;
        }
	
        PQclear(res);
      }

      /* Free the query and advance */
      qqp = queryhead;
      if(queryhead == querytail)
        querytail = NULL;

      queryhead = queryhead->next;

      if (qqp->query_ss) {
        freesstring(qqp->query_ss);
        qqp->query_ss=NULL;
        qqp->query=NULL;
      } else if (qqp->query) {
        free(qqp->query);
        qqp->query=NULL;
      }
      free(qqp);

      if(queryhead) { /* Submit the next query */	      
        PQsendQuery(dbconn, queryhead->query->content);
        PQflush(dbconn);
      }
    }
  }
}

/* sorry Q9 */
void pqasyncqueryf(PQQueryHandler handler, void *tag, int flags, char *format, ...) {
  char querybuf[8192];
  va_list va;
  int len;
  pqasyncquery_s *qp;

  if(!pqconnected())
    return;

  va_start(va, format);
  len = vsnprintf(querybuf, sizeof(querybuf), format, va);
  va_end(va);

  /* PPA: no check here... */
  qp = (pqasyncquery_s *)malloc(sizeof(pqasyncquery_s));

  if(!qp)
    Error("pqsql",ERR_STOP,"malloc() failed in pqsql.c");

  /* Use sstring or allocate (see above rant) */
  if (len > SSTRING_MAX) {
    qp->query = (char *)malloc(strlen(len)+1);
    strcpy(qp->query,querybuf);
    qp->query_ss=NULL;
  } else {
    qp->query_ss = getsstring(querybuf, len);
    qp->query = qp->query_ss->content;
  }
  qp->tag = tag;
  qp->handler = handler;
  qp->next = NULL; /* shove them at the end */
  qp->flags = flags;

  if(querytail) {
    querytail->next = qp;
    querytail = qp;
  } else {
    querytail = queryhead = qp;
    PQsendQuery(dbconn, qp->query->content);
    PQflush(dbconn);
  }
}

void disconnectdb(void) {
  pqasyncquery_s *qqp = queryhead, *nqqp;

  if(!pqconnected())
    return;

  /* do this first else we may get conflicts */
  deregisterhandler(PQsocket(dbconn), 0);

  /* Throw all the queued queries away, beware of data malloc()ed inside the query item.. */
  while(qqp) {
    nqqp = qqp->next;
    if (qqp->query_ss) {
      freesstring(qqp->query);
      qqp->query_ss=NULL;
      qqp->query=NULL;
    } else if (qqp->query) {
      free(qqp->query);
      qqp->query=NULL;
    }
    free(qqp);
    qqp = nqqp;
  }

  deregisterhook(HOOK_CORE_STATSREQUEST, dbstatus);
  PQfinish(dbconn);
  dbconn = NULL; /* hmm? */

  dbconnected = 0;
}

/* more stolen code from Q9 */
void dbstatus(int hooknum, void *arg) {
  if ((long)arg > 10) {
    int i = 0;
    pqasyncquery_s *qqp;
    char message[100];

    if(queryhead)
      for(qqp=queryhead;qqp;qqp=qqp->next)
        i++;
     
    snprintf(message, sizeof(message), "PQSQL   : %6d queries queued.",i);
    
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }  
}

int pqconnected(void) {
  return dbconnected;
}
