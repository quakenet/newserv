#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "trusts.h"

DBAPIConn *trustsdb;
static int tgmaxid, thmaxid;
static int loaderror;
static void *flushschedule;

int trustsdbloaded;

void createtrusttables(int migration);
void trusts_flush(void);
void trusts_freeall(void);

void createtrusttables(int migration) {
  char *groups, *hosts;

  if(migration) {
    groups = "migration_groups";
    hosts = "migration_hosts";
  } else {
    groups = "groups";
    hosts = "hosts";
  }

  trustsdb->createtable(trustsdb, NULL, NULL,
    "CREATE TABLE ? (id INT PRIMARY KEY, name VARCHAR(?), trustedfor INT, mode INT, maxperident INT, maxusage INT, expires INT, lastseen INT, lastmaxuserreset INT, createdby VARCHAR(?), contact VARCHAR(?), comment VARCHAR(?))",
    "Tdddd", groups, TRUSTNAMELEN, NICKLEN, CONTACTLEN, COMMENTLEN
  );

  /* I'd like multiple keys here but that's not gonna happen on a cross-database platform :( */
  trustsdb->createtable(trustsdb, NULL, NULL, "CREATE TABLE ? (id INT PRIMARY KEY, groupid INT, host VARCHAR(?), maxusage INT, lastseen INT)", "Td", hosts, TRUSTHOSTLEN);
}

static void flushdatabase(void *arg) {
  trusts_flush();
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

  if(result->fields != 5) {
    Error("trusts", ERR_ERROR, "Wrong number of fields in hosts table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

  while(result->next(result)) {
    unsigned int groupid, id;
    char *host;
    unsigned int maxusage, lastseen;
    trustgroup *tg;

    id = strtoul(result->get(result, 0), NULL, 10);
    if(id > thmaxid)
      thmaxid = id;

    groupid = strtoul(result->get(result, 1), NULL, 10);

    tg = tg_getbyid(groupid);
    if(!tg) {
      Error("trusts", ERR_WARNING, "Orphaned trust group host: %d", groupid);
      continue;
    }

    /* NOTE: 2 is at the bottom */
    maxusage = strtoul(result->get(result, 3), NULL, 10);
    lastseen = (time_t)strtoul(result->get(result, 4), NULL, 10);
    host = result->get(result, 2);

    if(!th_add(tg, id, host, maxusage, lastseen))
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
    unsigned int id;
    sstring *name, *createdby, *contact, *comment;
    unsigned int trustedfor, mode, maxperident, maxusage;
    time_t expires, lastseen, lastmaxuserreset;

    id = strtoul(result->get(result, 0), NULL, 10);
    if(id > tgmaxid)
      tgmaxid = id;

    name = getsstring(result->get(result, 1), TRUSTNAMELEN);
    trustedfor = strtoul(result->get(result, 2), NULL, 10);
    mode = atoi(result->get(result, 3));
    maxperident = strtoul(result->get(result, 4), NULL, 10);
    maxusage = strtoul(result->get(result, 5), NULL, 10);
    expires = (time_t)strtoul(result->get(result, 6), NULL, 10);
    lastseen = (time_t)strtoul(result->get(result, 7), NULL, 10);
    lastmaxuserreset = (time_t)strtoul(result->get(result, 8), NULL, 10);
    createdby = getsstring(result->get(result, 9), NICKLEN);
    contact = getsstring(result->get(result, 10), CONTACTLEN);
    comment = getsstring(result->get(result, 11), COMMENTLEN);

    if(name && createdby && contact && comment) {
      if(!tg_add(id, name->content, trustedfor, mode, maxperident, maxusage, expires, lastseen, lastmaxuserreset, createdby->content, contact->content, comment->content))
        Error("trusts", ERR_WARNING, "Error adding trustgroup %d: %s", id, name->content);
    } else {
      Error("trusts", ERR_ERROR, "Error allocating sstring in group loader, id: %d", id);
    }

    freesstring(name);
    freesstring(createdby);
    freesstring(contact);
    freesstring(comment);
  }

  result->clear(result);  
}

static void loadgroups_fini(const DBAPIResult *result, void *tag) {
  Error("trusts", ERR_INFO, "Finished loading groups, maximum id: %d.", tgmaxid);
}

int trusts_loaddb(void) {
  if(!trustsdb) {
    trustsdb = dbapi2open(NULL, "trusts");
    if(!trustsdb) {
      Error("trusts", ERR_WARNING, "Unable to connect to db -- not loaded.");
      return 0;
    }
  }

  createtrusttables(0);

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

void th_dbupdatecounts(trusthost *th) {
  trustsdb->squery(trustsdb, "UPDATE ? SET lastseen = ?, maxusage = ? WHERE id = ?", "Ttuu", "hosts", th->lastseen, th->maxusage, th->id);
}

void tg_dbupdatecounts(trustgroup *tg) {
  trustsdb->squery(trustsdb, "UPDATE ? SET lastseen = ?, maxusage = ? WHERE id = ?", "Ttuu", "groups", tg->lastseen, tg->maxusage, tg->id);
}

trusthost *th_new(trustgroup *tg, char *host) {
  trusthost *th, *superset, *subset;
  u_int32_t ip, mask;

  /* ugh */
  if(!trusts_str2cidr(host, &ip, &mask))
    return NULL;

  th_getsuperandsubsets(ip, mask, &superset, &subset);

  th = th_add(tg, thmaxid + 1, host, 0, 0);
  if(!th)
    return NULL;

  thmaxid++;

  trustsdb->squery(trustsdb,
    "INSERT INTO ? (id, groupid, host, maxusage, lastseen) VALUES (?, ?, ?, ?, ?)",
    "Tuusut", "hosts", th->id, tg->id, trusts_cidr2str(th->ip, th->mask), th->maxusage, th->lastseen
  );

  th_adjusthosts(th, subset, superset);

  return th;
}

trustgroup *tg_new(char *name, unsigned int trustedfor, int mode, unsigned int maxperident, time_t expires, char *createdby, char *contact, char *comment) {
  trustgroup *tg = tg_add(tgmaxid + 1, name, trustedfor, mode, maxperident, 0, expires, 0, 0, createdby, contact, comment);
  if(!tg)
    return NULL;

  tgmaxid++;

  trustsdb->squery(trustsdb,
    "INSERT INTO ? (id, name, trustedfor, mode, maxperident, maxusage, expires, lastseen, lastmaxuserreset, createdby, contact, comment) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
    "Tusuuuutttsss", "groups", tg->id, tg->name->content, tg->trustedfor, tg->mode, tg->maxperident, tg->maxusage, tg->expires, tg->lastseen, tg->lastmaxuserreset, tg->createdby->content, tg->contact->content, tg->comment->content
  );

  return tg;
}
