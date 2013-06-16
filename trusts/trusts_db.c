#include <stdio.h>
#include <stdarg.h>
#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../control/control.h"
#include "trusts.h"

DBAPIConn *trustsdb;
static int tgmaxid, thmaxid;
static int loaderror;
static void *flushschedule;

void createtrusttables(int migration);
void trusts_flush(void (*)(trusthost *), void (*)(trustgroup *));
void trusts_freeall(void);
static void th_dbupdatecounts(trusthost *th);
static void tg_dbupdatecounts(trustgroup *tg);

void createtrusttables(int mode) {
  char *groups, *hosts;

  if(mode == TABLES_MIGRATION) {
    groups = "migration_groups";
    hosts = "migration_hosts";
  } else if(mode == TABLES_REPLICATION) {
    groups = "replication_groups";
    hosts = "replication_hosts";
  } else {
    groups = "groups";
    hosts = "hosts";
  }

  trustsdb->createtable(trustsdb, NULL, NULL,
    "CREATE TABLE ? (id INT PRIMARY KEY, name VARCHAR(?), trustedfor INT, mode INT, maxperident INT, maxusage INT, expires INT, lastseen INT, lastmaxusereset INT, createdby VARCHAR(?), contact VARCHAR(?), comment VARCHAR(?))",
    "Tdddd", groups, TRUSTNAMELEN, CREATEDBYLEN, CONTACTLEN, COMMENTLEN
  );

  /* I'd like multiple keys here but that's not gonna happen on a cross-database platform :( */
  trustsdb->createtable(trustsdb, NULL, NULL, "CREATE TABLE ? (id INT PRIMARY KEY, groupid INT, host VARCHAR(?), maxusage INT, created INT, lastseen INT)", "Td", hosts, TRUSTHOSTLEN);

  trustsdb->createtable(trustsdb, NULL, NULL,
    "CREATE TABLE ? (id INT PRIMARY KEY, groupid INT, groupname VARCHAR(?), ts INT, username VARCHAR(?), message VARCHAR(?))",
    "Tddd", "log", TRUSTNAMELEN, CREATEDBYLEN, TRUSTLOGLEN
  );
}

static void flushdatabase(void *arg) {
  trusts_flush(th_dbupdatecounts, tg_dbupdatecounts);
}

static void triggerdbloaded(void *arg) {
  triggerhook(HOOK_TRUSTS_DB_LOADED, NULL);
}

static void loadcomplete(void) {
  /* error has already been shown */
  if(loaderror)
    return;

  trustsdbloaded = 1;
  flushschedule = schedulerecurring(time(NULL) + 300, 0, 300, flushdatabase, NULL);

  scheduleoneshot(time(NULL), triggerdbloaded, NULL);
}

static void loadhosts_data(const DBAPIResult *result, void *tag) {
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

  if(result->fields != 6) {
    Error("trusts", ERR_ERROR, "Wrong number of fields in hosts table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

  while(result->next(result)) {
    unsigned int groupid;
    trusthost th;
    char *host;

    th.id = strtoul(result->get(result, 0), NULL, 10);
    if(th.id > thmaxid)
      thmaxid = th.id;

    groupid = strtoul(result->get(result, 1), NULL, 10);

    th.group = tg_getbyid(groupid);
    if(!th.group) {
      Error("trusts", ERR_WARNING, "Orphaned trust group host: %d", groupid);
      continue;
    }

    host = result->get(result, 2);
    if(!trusts_str2cidr(host, &th.ip, &th.mask)) {
      Error("trusts", ERR_WARNING, "Error parsing cidr for host: %s", host);
      continue;
    }

    th.maxusage = strtoul(result->get(result, 3), NULL, 10);
    th.created = (time_t)strtoul(result->get(result, 4), NULL, 10);
    th.lastseen = (time_t)strtoul(result->get(result, 5), NULL, 10);

    if(!th_add(&th))
      Error("trusts", ERR_WARNING, "Error adding host to trust %d: %s", groupid, host);
  }

  result->clear(result);

  loadcomplete();
}

static void loadhosts_fini(const DBAPIResult *result, void *tag) {
  Error("trusts", ERR_INFO, "Finished loading hosts, maximum id: %d", thmaxid);
}

static void loadgroups_data(const DBAPIResult *result, void *tag) {
  if(!result) {
    loaderror = 1;
    return;
  }

  if(!result->success) {
    Error("trusts", ERR_ERROR, "Error loading group table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

  if(result->fields != 12) {
    Error("trusts", ERR_ERROR, "Wrong number of fields in groups table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

  while(result->next(result)) {
    trustgroup tg;

    tg.id = strtoul(result->get(result, 0), NULL, 10);
    if(tg.id > tgmaxid)
      tgmaxid = tg.id;

    tg.name = getsstring(rtrim(result->get(result, 1)), TRUSTNAMELEN);
    tg.trustedfor = strtoul(result->get(result, 2), NULL, 10);
    tg.mode = atoi(result->get(result, 3));
    tg.maxperident = strtoul(result->get(result, 4), NULL, 10);
    tg.maxusage = strtoul(result->get(result, 5), NULL, 10);
    tg.expires = (time_t)strtoul(result->get(result, 6), NULL, 10);
    tg.lastseen = (time_t)strtoul(result->get(result, 7), NULL, 10);
    tg.lastmaxusereset = (time_t)strtoul(result->get(result, 8), NULL, 10);
    tg.createdby = getsstring(rtrim(result->get(result, 9)), CREATEDBYLEN);
    tg.contact = getsstring(rtrim(result->get(result, 10)), CONTACTLEN);
    tg.comment = getsstring(rtrim(result->get(result, 11)), COMMENTLEN);

    if(tg.name && tg.createdby && tg.contact && tg.comment) {
      if(!tg_add(&tg))
        Error("trusts", ERR_WARNING, "Error adding trustgroup %d: %s", tg.id, tg.name->content);
    } else {
      Error("trusts", ERR_ERROR, "Error allocating sstring in group loader, id: %d", tg.id);
    }

    freesstring(tg.name);
    freesstring(tg.createdby);
    freesstring(tg.contact);
    freesstring(tg.comment);
  }

  result->clear(result);  
}

static void loadgroups_fini(const DBAPIResult *result, void *tag) {
  Error("trusts", ERR_INFO, "Finished loading groups, maximum id: %d.", tgmaxid);
}

static int trusts_connectdb(void) {
  if(!trustsdb) {
    trustsdb = dbapi2open(NULL, "trusts");
    if(!trustsdb) {
      Error("trusts", ERR_WARNING, "Unable to connect to db -- not loaded.");
      return 0;
    }
  }

  createtrusttables(TABLES_REGULAR);

  return 1;
}

int trusts_loaddb(void) {
  if(!trusts_connectdb())
    return 0;

  loaderror = 0;

  trustsdb->loadtable(trustsdb, NULL, loadgroups_data, loadgroups_fini, NULL, "groups");
  trustsdb->loadtable(trustsdb, NULL, loadhosts_data, loadhosts_fini, NULL, "hosts");

  return 1;
}

void trusts_closedb(int closeconnection) {
  if(!trustsdb)
    return;

  if(flushschedule) {
    deleteschedule(flushschedule, flushdatabase, NULL);
    flushschedule = NULL;

    flushdatabase(NULL);
  }

  trusts_freeall();

  trustsdbloaded = 0;
  thmaxid = tgmaxid = 0;

  if(closeconnection) {
    trustsdb->close(trustsdb);
    trustsdb = NULL;
  }

  triggerhook(HOOK_TRUSTS_DB_CLOSED, NULL);
}

static void th_dbupdatecounts(trusthost *th) {
  trustsdb->squery(trustsdb, "UPDATE ? SET lastseen = ?, maxusage = ? WHERE id = ?", "Ttuu", "hosts", th->lastseen, th->maxusage, th->id);
}

static void tg_dbupdatecounts(trustgroup *tg) {
  trustsdb->squery(trustsdb, "UPDATE ? SET lastseen = ?, maxusage = ? WHERE id = ?", "Ttuu", "groups", tg->lastseen, tg->maxusage, tg->id);
}

trusthost *th_copy(trusthost *ith) {
  trusthost *th, *superset, *subset;

  th = th_add(ith);
  if(!th)
    return NULL;

  trustsdb_insertth("hosts", th, th->group->id);

  th_getsuperandsubsets(ith->ip, ith->mask, &superset, &subset);
  th_adjusthosts(th, subset, superset);

  return th;
}

trusthost *th_new(trustgroup *tg, char *host) {
  trusthost *th, nth;

  if(!trusts_str2cidr(host, &nth.ip, &nth.mask))
    return NULL;

  nth.group = tg;
  nth.id = thmaxid + 1;
  nth.created = time(NULL);
  nth.lastseen = 0;
  nth.maxusage = 0;

  th = th_copy(&nth);
  if(!th)
    return NULL;

  thmaxid++;

  return th;
}

trustgroup *tg_copy(trustgroup *itg) {
  trustgroup *tg = tg_add(itg);
  if(!tg)
    return NULL;

  trustsdb_inserttg("groups", tg);
  return tg;
}

trustgroup *tg_new(trustgroup *itg) {
  trustgroup *tg;

  itg->id = tgmaxid + 1;
  itg->maxusage = 0;
  itg->lastseen = 0;
  itg->lastmaxusereset = 0;

  tg = tg_copy(itg);
  if(!tg)
    return NULL;

  tgmaxid++;

  return tg;
}

void trustsdb_insertth(char *table, trusthost *th, unsigned int groupid) {
  trustsdb->squery(trustsdb,
    "INSERT INTO ? (id, groupid, host, maxusage, created, lastseen) VALUES (?, ?, ?, ?, ?, ?)",
    "Tuusut", table, th->id, groupid, trusts_cidr2str(th->ip, th->mask), th->maxusage, th->created, th->lastseen
  );
}

void trustsdb_inserttg(char *table, trustgroup *tg) {
  trustsdb->squery(trustsdb,
    "INSERT INTO ? (id, name, trustedfor, mode, maxperident, maxusage, expires, lastseen, lastmaxusereset, createdby, contact, comment) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
    "Tusuuuutttsss", table, tg->id, tg->name->content, tg->trustedfor, tg->mode, tg->maxperident, tg->maxusage, tg->expires, tg->lastseen, tg->lastmaxusereset, tg->createdby->content, tg->contact->content, tg->comment->content
  );
}

void tg_update(trustgroup *tg) {
  trustsdb->squery(trustsdb,
    "UPDATE ? SET name = ?, trustedfor = ?, maxperident = ?, maxusage = ?, expires = ?, lastseen = ?, lastmaxusereset = ?, createdby = ?, contact = ?, comment = ? WHERE id = ?",
    "Tsuuuutttsssu", "groups", tg->name->content, tg->trustedfor, tg->mode, tg->maxperident, tg->maxusage, tg->expires, tg->lastseen, tg->lastmaxusereset, tg->createdby->content, tg->contact->content, tg->comment->content, tg->id
  );
}

void trustsdb_deletetg(char *table, trustgroup *tg)  {
  trustsdb->squery(trustsdb,
    "DELETE FROM ? WHERE id = ?",
    "Tu", "groups", tg->id);
}

void tg_delete(trustgroup *tg) {
  trustgroup **pnext;

  for(pnext=&tglist;*pnext;pnext=&((*pnext)->next)) {
    if(*pnext == tg) {
      *pnext = tg->next;
      break;
    }
  }

  trustsdb_deletetg("groups", tg);
  tg_free(tg, 1);
}

void trustsdb_deleteth(char *table, trusthost *th) {
  trustsdb->squery(trustsdb,
    "DELETE FROM ? WHERE id = ?",
    "Tu", "hosts", th->id); 
}

void th_delete(trusthost *th) {
  trusthost **pnext;
  nick *np;

  for(pnext=&(th->group->hosts);*pnext;pnext=&((*pnext)->next)) {
    if(*pnext == th) {
      *pnext = th->next;
      break;
    }
  }

  for(np=th->users;np;np=nextbytrust(np))
    settrusthost(np, NULL);

  th->group->count -= th->count;

  trustsdb_deleteth("hosts", th);
  th_free(th);
}

void trustlog(trustgroup *tg, const char *user, const char *format, ...) {
  char buf[TRUSTLOGLEN+1];
  va_list va;
  unsigned int now;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  now = time(NULL);

  trustsdb->squery(trustsdb,
    "INSERT INTO ? (ts, groupid, groupname, username, message) VALUES (?, ?, ?, ?, ?)",
    "Tuusss", "log", now, tg->id, tg->name->content, user, buf);
}

static void trustlogquery_callback(const struct DBAPIResult *result, void *arg) {
  nick *np = (nick *)arg;
  int rows = 0;

  if(!result || !result->success) {
    controlreply(np, "Error querying the log.");

    if(result)
      result->clear(result);

    return;
  }

  while(result->next(result)) {
    unsigned int ts, groupid;
    char *groupname, *user, *message;

    ts = strtoul(result->get(result, 0), NULL, 10);
    groupid = strtoul(result->get(result, 1), NULL, 10);
    groupname = result->get(result, 2);
    user = result->get(result, 3);
    message = result->get(result, 4);

    controlreply(np, "[%s] #%d/%s (%s) %s", trusts_timetostr(ts), groupid, groupname, user, message);
    rows++;
  }

  controlreply(np, "--- Done. Found %d entries.", rows);
}

void trustlogspewid(nick *np, unsigned int groupid, unsigned int limit) {
  trustsdb->query(trustsdb, trustlogquery_callback, (void *)np,
    "SELECT ts, groupid, groupname, username, message FROM ? WHERE groupid = ? ORDER BY ts DESC LIMIT ?",
    "Tuu", "log", groupid, limit);
}

void trustlogspewname(nick *np, const char *groupname, unsigned int limit) {
  trustsdb->query(trustsdb, trustlogquery_callback, (void *)np,
    "SELECT ts, groupid, groupname, username, message FROM ? WHERE groupname = ? ORDER BY ts DESC LIMIT ?",
    "Tsu", "log", groupname, limit);
}

void trustloggrep(nick *np, const char *pattern, unsigned int limit) {
  char buf[512];
  snprintf(buf, sizeof(buf), "%%%s%%", pattern);

  trustsdb->query(trustsdb, trustlogquery_callback, (void *)np,
    "SELECT ts, groupid, username, message FROM ? WHERE message LIKE ? ORDER BY ts DESC LIMIT ?",
    "Tsu", "log", buf, limit);
}

void _init(void) {
  trusts_connectdb();
}

void _fini(void) {
  trusts_closedb(1);
}
