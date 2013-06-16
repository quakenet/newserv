#include "../nick/nick.h"
#include "../core/error.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"
#include "../dbapi2/dbapi2.h"
#include <stdlib.h>
#include "trusts.h"

DBAPIConn *trustsdb;
static int tgmaxid, thmaxid;
static int trustdb_loaded;
static int loaderror;
static void *flushschedule;

void trusts_flush(void (*)(trusthost_t *), void (*)(trustgroup_t *));
static void th_dbupdatecounts(trusthost_t *th);
static void tg_dbupdatecounts(trustgroup_t *tg);

static void trusts_dbtriggerdbloaded(void *arg);

static void loadgroups_fini(const DBAPIResult *result, void *tag);
static void loadhosts_fini(const DBAPIResult *result, void *tag);

static int trusts_connectdb(void);
static void loadcomplete(void);
void trusts_loadtrustgroups(const DBAPIResult *result, void *tag);
void trusts_loadtrusthosts(const DBAPIResult *result, void *tag);

int trusts_load_db(void) {
  if(!trusts_connectdb()) {
    Error("trusts", ERR_STOP, "Could not connect to database.");
    return 0;
  }

  loaderror = 0;

  if(trustdb_loaded)
    trusts_cleanup_db();

  trustdb_loaded = 1;

  trusts_create_tables(TABLES_REGULAR);

  /* need to adjust these to not load trusts with enddate = 0 */
  trustsdb->loadtable(trustsdb, NULL, trusts_loadtrustgroups, loadgroups_fini, NULL, "groups");
  trustsdb->loadtable(trustsdb, NULL, trusts_loadtrusthosts, loadhosts_fini, NULL, "hosts");

  return 1;
}

static int trusts_connectdb(void) {
  if(!trustsdb) {
    trustsdb = dbapi2open(NULL, "trusts");
    if(!trustsdb) {
      Error("trusts", ERR_WARNING, "Unable to connect to db -- not loaded.");
      return 0;
    }
  }

  trusts_create_tables(TABLES_REGULAR);

  return 1;
}

void trusts_create_tables(int mode) {
  char *groups, *hosts;

  if(mode == TABLES_REPLICATION) {
    groups = "replication_groups";
    hosts = "replication_hosts";
  } else {
    groups = "groups";
    hosts = "hosts";
  }

  trustsdb->createtable( trustsdb, NULL, NULL, 
    "CREATE TABLE ? ("
    "trustid      INT4 PRIMARY KEY,"
    "startdate    INT4,"
    "enddate      INT4,"
    "owneruserid  INT4,"
    "maxusage     INT4,"
    "enforceident INT2,"
    "maxclones    INT4,"
    "maxperident  INT4,"
    "maxperip     INT4,"
    "expires      INT4,"
    "lastused     INT4,"
    "modified     INT4,"
    "created      INT4"
    ")", "T", groups
    );

  trustsdb->createtable( trustsdb, NULL, NULL, 
    "CREATE TABLE ? ("
    "hostid     INT4 PRIMARY KEY,"
    "trustid    INT4,"
    "startdate  INT4,"
    "enddate    INT4,"
    "host       VARCHAR,"
    "maxusage   INT4,"
    "lastused   INT4,"
    "expires    INT4,"
    "modified   INT4,"
    "created    INT4"
    ")", "T", hosts
    );

  trustsdb->createtable( trustsdb, NULL, NULL, 
    "CREATE TABLE ? ("
    "logid          SERIAL NOT NULL PRIMARY KEY,"
    "trustid        INT4 NOT NULL,"
    "timestamp      INT4 NOT NULL,"
    "userid         INT4 NOT NULL,"
    "type           INT2,"
    "message        VARCHAR"
    ")", "T", "log"
  );
}

static void flushdatabase(void *arg) {
  trusts_flush(th_dbupdatecounts, tg_dbupdatecounts);
}

static void th_dbupdatecounts(trusthost_t *th) {
  trustsdb->squery(trustsdb, "UPDATE ? SET lastused = ?, maxusage = ? WHERE hostid = ?", "Ttuu", "hosts", th->lastused, th->maxusage, th->id);
}

static void tg_dbupdatecounts(trustgroup_t *tg) {
  trustsdb->squery(trustsdb, "UPDATE ? SET lastused = ?, maxusage = ? WHERE trustid = ?", "Ttuu", "groups", tg->lastused, tg->maxusage, tg->id);
}

void trusts_cleanup_db(void) {
  if(!trustsdb)
    return;

  if(flushschedule) {
    deleteschedule(flushschedule, flushdatabase, NULL);
    flushschedule = NULL;

    flushdatabase(NULL);
  }

  trustdb_loaded = 0;
  thmaxid = tgmaxid = 0;

  trustsdb->close(trustsdb);
  trustsdb = NULL;

  triggerhook(HOOK_TRUSTS_DB_CLOSED, NULL);
 
}

void trusts_loadtrustgroups(const DBAPIResult *result, void *tag) {
  trustgroup_t *t;

  if(!result) {
    Error("trusts", ERR_ERROR, "Error loading group table.");
    loaderror=1;
    return;
  }

  if(!result->success) {
    Error("trusts", ERR_ERROR, "Error loading group table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

  if(result->fields != 13) {
    Error("trusts", ERR_ERROR, "Wrong number of fields in groups table. (Got %d)", result->fields);
    loaderror = 1;

    result->clear(result);
    return;
  }

  trusts_lasttrustgroupid = 1; 

  while(result->next(result)) {
    t = createtrustgroupfromdb( 
                     /*id*/          strtoul(result->get(result, 0),NULL,10),
                     /*maxusage*/    strtoul(result->get(result, 1),NULL,10),
                     /*maxclones*/   strtoul(result->get(result, 2),NULL,10),
                     /*maxperident*/ strtoul(result->get(result, 3),NULL,10),
                     /*maxperip*/    strtoul(result->get(result, 4),NULL,10),
        /*TODOTYPE*/ /*enforceident*/strtoul(result->get(result, 5),NULL,10),
                     /*startdate*/   strtoul(result->get(result, 6),NULL,10),
                     /*lastused*/    strtoul(result->get(result, 7),NULL,10),
                     /*expire*/      strtoul(result->get(result, 8),NULL,10),
                     /*ownerid*/     strtoul(result->get(result, 9),NULL,10),
                     /*created*/     strtoul(result->get(result, 10),NULL,10),
                     /*modified*/    strtoul(result->get(result, 11),NULL,10)
                     );
    if (!t) {
      Error("trusts", ERR_ERROR, "Error loading trust group.");
      return;
    }

    if(t->id > trusts_lasttrustgroupid)
      tgmaxid = t->id;
  }

  result->clear(result); 
}

static void loadgroups_fini(const DBAPIResult *result, void *tag) {
  Error("trusts", ERR_INFO, "Finished loading groups, maximum id: %d.", tgmaxid);
}

void trusts_loadtrusthosts(const DBAPIResult *result, void *tag) {
  trusthost_t *t;
  trustgroup_t *tg;
  patricia_node_t *node;
  struct irc_in_addr sin;
  unsigned char bits;

  if(!result) {
    loaderror = 1;
    return;
  }

  if(!result->success) {
    Error("trusts", ERR_ERROR, "Error loading hosts table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

 if(result->fields != 10) {
    Error("trusts", ERR_ERROR, "Wrong number of fields in hosts table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

  while(result->next(result)) {
    char *host;

    host = result->get(result, 4);
    /*node*/
    if( ipmask_parse(host, &sin, &bits) == 0) {
      Error("trusts", ERR_ERROR, "Failed to parse trusthost: %s", host);
      continue;
    }

    node  = refnode(iptree, &sin, bits);

    /* thid */
    int thid = strtoul(result->get(result, 0),NULL,10);
    if(thid > thmaxid)
      thmaxid = thid;

    /*tg*/
    int tgid = strtoul(result->get(result, 1),NULL,10);
    tg=findtrustgroupbyid(tgid);
    if (!tg) { 
      Error("trusts", ERR_ERROR, "Error loading trusthosts - invalid group: %d.", tgid);
      continue;
    }

    t = createtrusthostfromdb( 
                    /*id*/        thid,
                    /*node*/      node,
                    /*startdate*/ strtoul(result->get(result, 2),NULL,10),
                    /*lastused*/  (time_t)strtoul(result->get(result, 6),NULL,10),
                    /*expire*/    strtoul(result->get(result, 7),NULL,10),
                    /*maxusage*/  strtoul(result->get(result, 5),NULL,10),
                    /*trustgroup*/ tg,
                    /*created*/   strtoul(result->get(result, 9),NULL,10),
                    /*modified*/ strtoul(result->get(result, 8),NULL,10)
                  );
    if (!t) {
      Error("trusts", ERR_ERROR, "Error adding trusthost %s to group %d.", host, tg);
      return;
    }
    node = 0;

    trusthost_addcounters(t);
  }

  result->clear(result);

  loadcomplete();
}

static void loadhosts_fini(const DBAPIResult *result, void *tag) {
  Error("trusts", ERR_INFO, "Finished loading hosts, maximum id: %d", thmaxid);
}

static void loadcomplete(void) {
  /* error has already been shown */
  if(loaderror)
    return;

  trusts_loaded = 1;
  flushschedule = schedulerecurring(time(NULL) + 300, 0, 300, flushdatabase, NULL);
  scheduleoneshot(time(NULL), trusts_dbtriggerdbloaded, NULL);
}

static void trusts_dbtriggerdbloaded(void *arg) {
  triggerhook(HOOK_TRUSTS_DBLOADED, NULL);
}

/* trust group */
void trustsdb_addtrustgroup(trustgroup_t *t) {
  trustsdb->squery(trustsdb,"INSERT INTO ? (trustid,startdate,enddate,owneruserid,maxusage,enforceident,maxclones,maxperident,maxperip,expires,lastused,modified,created ) VALUES (?,?,0,?,?,?,?,?,?,?,?,?,? )", "Tuuuuuuuuuuuuu", "groups", t->id,t->startdate,t->ownerid,t->maxusage,t->enforceident,t->maxclones,t->maxperident,t->maxperip, t->expire, t->lastused, t->modified, t->created ); 
}

void trustsdb_updatetrustgroup(trustgroup_t *t) {
  trustsdb->squery(trustsdb,"UPDATE ? SET startdate=?,owneruserid=?,maxusage=?,enforceident=?,maxclones=?, maxperident=?,maxperip=?,expires=?,lastused=?,modified=?,created=? WHERE trustid = ?", "Tuuuuuuuuuuu", "groups", t->startdate, t->ownerid,t->maxusage,t->enforceident,t->maxclones,t->maxperident,t->maxperip, t->expire, t->lastused, t->modified, t->created, t->id);
}

void trustsdb_deletetrustgroup(trustgroup_t *t) {
  trustsdb->squery(trustsdb,"UPDATE ? SET enddate=? WHERE trustid = ?", "Ttu", "groups", (intmax_t)getnettime(), t->id);
}

/* trust host */
void trustsdb_addtrusthost(trusthost_t *th) {
  trustsdb->squery(trustsdb,"INSERT INTO ? (hostid,trustid,startdate,enddate,host,maxusage,lastused,expires,modified,created) VALUES (?,?,?,0,?,?,?,?,?,?)", "Tuuusuuuuuu", "hosts", th->id, th->trustgroup->id, th->startdate, node_to_str(th->node), th->maxusage, th->lastused, th->expire, th->modified, th->created);
}

void trustsdb_updatetrusthost(trusthost_t *th) {
  trustsdb->squery(trustsdb,"UPDATE ? SET hostid=?,trustid=?,startdate=?,host=?,maxusage=?,lastused=?,expires=?,modified=?,created=?", "Tuuuuuuuuuu", "hosts", th->id, th->trustgroup->id, th->startdate, node_to_str(th->node), th->maxusage, th->lastused, th->expire, th->modified, th->created);
}

void trustsdb_deletetrusthost(trusthost_t *th) {
  trustsdb->squery(trustsdb,"UPDATE ? SET enddate=? WHERE hostid=?", "Ttu", "hosts", (intmax_t)getnettime(), th->id);
}

/* trust log */
/* logid, trustid, timestamp, userid, type, message */
/* @@@ TODO */

void trustsdb_logmessage(trustgroup_t *tg, unsigned long userid, int type, char *message) {
  trustsdb->squery(trustsdb,"INSERT INTO ? (trustid, timestamp, userid, type, message) VALUES (?,?,?,?,?)", "Tuuuus", "log", tg->id, getnettime(), userid, type, message);
}

void trust_dotrustlog_real(const struct DBAPIResult *result, void *arg) {
  nick *np=(nick *)arg;
  unsigned long logid, trustid, userid, type;
  time_t timestamp;
  char *message;
  char timebuf[30];
  int header=0;

  if(!result || !result->success) {
    controlreply(np, "Error querying the log.");

    if(result)
      result->clear(result);
    return;
  }

  if(result->fields != 6) {
    Error("trusts", ERR_ERROR, "trusts log data format error.");
    result->clear(result);
    return;
  }

  if(!np)
    return;
 
  while(result->next(result)) {
    logid=strtoul(result->get(result, 0), NULL, 10);
    trustid=strtoul(result->get(result, 1), NULL, 10);
    timestamp=strtoul(result->get(result, 2), NULL, 10);
    userid=strtoul(result->get(result, 3), NULL, 10);
    type=strtoul(result->get(result, 4), NULL, 10);
    message=result->get(result, 5);

    if (!header) {
      header=1;
      controlreply(np, "Display trustlog for trust %lu", trustid); 
      controlreply(np, "ID  Time           OperID  Type Message"); 
    }
    strftime(timebuf, 30, "%d/%m/%y %H:%M", localtime(&timestamp));
    controlreply(np, "%-3lu %s %-7lu %-2lu   %s", logid, timebuf, userid, type, message);  
  }

  if (!header) {
    controlreply(np, "No trust log entries found."); 
  } else {
    controlreply(np, "End Of List.");
  }
  result->clear(result);
}


void trustsdb_retrievetrustlog(nick *np, unsigned int trustid, time_t starttime) {
  trustsdb->query(trustsdb, trust_dotrustlog_real, (void *) np, "SELECT * FROM ? WHERE trustid=? AND timestamp>? order by timestamp desc limit 1000", "Tuu", "log", trustid, starttime); 
}
