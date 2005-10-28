/* 
 * NOperserv v0.01
 *
 * A replacement for Germania's ageing Operservice2
 * PSQL functions
 *
 * Copyright (C) 2005 Chris Porter.
 * 99% of the PGSQL handling is stolen from Q9, copyright (C) David Mansell.
 */

#include "../core/config.h"
#include "../core/error.h"
#include "../irc/irc_config.h"
#include "../core/events.h"
#include "../core/hooks.h"
#include "noperserv_psql.h"

#include <stdlib.h>
#include <sys/poll.h>
#include <stdarg.h>

typedef struct no_asyncquery {
  sstring *query;
  void *tag;
  NoQueryHandler handler;
  struct no_asyncquery *next;
} no_asyncquery;

no_asyncquery *queryhead = NULL, *querytail = NULL;

int db_connected = 0;
PGconn *no_dbconn;

void noperserv_db_handler(int fd, short revents);
void noperserv_db_status(int hooknum, void *arg);

int noperserv_connect_db(NoCreateHandler createtables) {
  sstring *dbhost, *dbusername, *dbpassword, *dbdatabase, *dbport;
  char connectstr[1024];

  if(db_connected)
    return 1;

  /* stolen from chanserv as I'm lazy */
  dbhost = getcopyconfigitem("noperserv", "dbhost", "localhost", HOSTLEN);
  dbusername = getcopyconfigitem("noperserv", "dbusername", "noperserv", 20);
  dbpassword = getcopyconfigitem("noperserv", "dbpassword", "moo", 20);
  dbdatabase = getcopyconfigitem("noperserv", "dbdatabase", "noperserv", 20);
  dbport = getcopyconfigitem("noperserv", "dbport", "3306", 8);

  if(!dbhost || !dbusername || !dbpassword || !dbdatabase || !dbport) {
    /* freesstring allows NULL */
    freesstring(dbhost);
    freesstring(dbusername);
    freesstring(dbpassword);
    freesstring(dbdatabase);
    freesstring(dbport);
    return 2;
  }

  snprintf(connectstr, sizeof(connectstr), "dbname=%s user=%s password=%s", dbdatabase->content, dbusername->content, dbpassword->content);
  
  freesstring(dbhost);
  freesstring(dbusername);
  freesstring(dbpassword);
  freesstring(dbdatabase);
  freesstring(dbport);

  Error("noperserv", ERR_INFO, "Attempting database connection: %s", connectstr);

  /* Blocking connect for now.. */
  no_dbconn = PQconnectdb(connectstr);
  
  if (!no_dbconn || (PQstatus(no_dbconn) != CONNECTION_OK))
    return 1;

  db_connected = 1;

  if(createtables)
    createtables();

  PQsetnonblocking(no_dbconn, 1);

  /* this kicks ass, thanks splidge! */
  registerhandler(PQsocket(no_dbconn), POLLIN, noperserv_db_handler);
  registerhook(HOOK_CORE_STATSREQUEST, noperserv_db_status);

  return 0;
}

void noperserv_db_handler(int fd, short revents) {
  PGresult *res;
  no_asyncquery *qqp;

  if(revents & POLLIN) {
    PQconsumeInput(no_dbconn);
    
    if(!PQisBusy(no_dbconn)) { /* Query is complete */
      if(queryhead->handler)
        (queryhead->handler)(no_dbconn, queryhead->tag);

      while((res = PQgetResult(no_dbconn))) {
        switch(PQresultStatus(res)) {
          case PGRES_TUPLES_OK:
            Error("noperserv", ERR_WARNING, "Unhandled tuples output (query: %s)", queryhead->query->content);
            break;

          case PGRES_NONFATAL_ERROR:
          case PGRES_FATAL_ERROR:
            Error("noperserv", ERR_WARNING, "Unhandled error response (query: %s)", queryhead->query->content);
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

      freesstring(qqp->query);
      free(qqp);

      if(queryhead) { /* Submit the next query */	      
        PQsendQuery(no_dbconn, queryhead->query->content);
        PQflush(no_dbconn);
      }
    }
  }
}

/* sorry Q9 */
void noperserv_async_query(NoQueryHandler handler, void *tag, char *format, ...) {
  char querybuf[8192];
  va_list va;
  int len;
  no_asyncquery *qp;

  if(!db_connected)
    return;

  va_start(va, format);
  len = vsnprintf(querybuf, sizeof(querybuf), format, va);
  va_end(va);

  qp = (no_asyncquery *)malloc(sizeof(no_asyncquery));
  qp->query = getsstring(querybuf, len);
  qp->tag = tag;
  qp->handler = handler;
  qp->next = NULL; /* shove them at the end */

  if(querytail) {
    querytail->next = qp;
    querytail = qp;
  } else {
    querytail = queryhead = qp;
    PQsendQuery(no_dbconn, qp->query->content);
    PQflush(no_dbconn);
  }
}

void noperserv_sync_query(char *format, ...){
  char querybuf[8192];
  va_list va;
  int len;

  if(!db_connected)
    return;

  va_start(va, format);
  len = vsnprintf(querybuf, sizeof(querybuf), format, va);
  va_end(va);

  PQclear(PQexec(no_dbconn, querybuf));
}

void noperserv_disconnect_db(void) {
  no_asyncquery *qqp = queryhead, *nqqp;

  if(!db_connected)
    return;

  /* do this first else we may get conflicts */
  deregisterhandler(PQsocket(no_dbconn), 0);

  /* Throw all the queued queries away, beware of data malloc()ed inside the query item.. */
  while(qqp) {
    nqqp = qqp->next;
    freesstring(qqp->query);
    free(qqp);
    qqp = nqqp;
  }

  deregisterhook(HOOK_CORE_STATSREQUEST, noperserv_db_status);
  PQfinish(no_dbconn);
  no_dbconn = NULL; /* hmm? */

  db_connected = 0;
}

/* more stolen code from Q9 */
void noperserv_db_status(int hooknum, void *arg) {
  if ((int)arg > 10) {
    int i = 0;
    no_asyncquery *qqp;
    char message[100];

    if(queryhead)
      for(qqp=queryhead;qqp;qqp=qqp->next)
        i++;
     
    snprintf(message, sizeof(message), "NOServ  : %6d database queries queued.",i);
    
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }  
}
