#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "../core/hooks.h"
#include "trusts.h"

DBAPIConn *trustsdb;
static int tgmaxid;
static int loaderror;
int trustsdbloaded;

void trusts_reloaddb(void);
void createtrusttables(int migration);

int trusts_loaddb(void) {
  trustsdb = dbapi2open(NULL, "trusts");
  if(!trustsdb) {
    Error("trusts", ERR_WARNING, "Unable to connect to db -- not loaded.");
    return 0;
  }

  createtrusttables(0);

  trusts_reloaddb();
  return 1;
}

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
    "CREATE TABLE ? (id INT PRIMARY KEY, name VARCHAR(?), trustedfor INT, mode INT, maxperident INT, maxseen INT, expires INT, lastseen INT, lastmaxuserreset INT, createdby VARCHAR(?), contact VARCHAR(?), comment VARCHAR(?))",
    "Tdddd", groups, TRUSTNAMELEN, NICKLEN, CONTACTLEN, COMMENTLEN
  );
  trustsdb->createtable(trustsdb, NULL, NULL, "CREATE TABLE ? (groupid INT, host VARCHAR(?), max INT, lastseen INT, PRIMARY KEY (groupid, host))", "Td", hosts, TRUSTHOSTLEN);
}

static void trusts_freedb(void) {
  trustgroup *tg, *ntg;
  trusthost *th, *nth;

  for(tg=tglist;tg;tg=ntg) {
    ntg = tg->next;
    for(th=tg->hosts;th;th=nth) {
      nth = th->next;

      freesstring(th->host);
      free(th);
    }

    freesstring(tg->name);
    freesstring(tg->createdby);
    freesstring(tg->contact);
    freesstring(tg->comment);
    free(tg);
  }

  trustsdbloaded = 0;
  tglist = NULL;
  tgmaxid = 0;
}

trustgroup *tg_getbyid(unsigned int id) {
  trustgroup *tg;

  for(tg=tglist;tg;tg=tg->next)
    if(tg->id == id)
      return tg;

  return NULL;
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

  if(result->fields != 4) {
    Error("trusts", ERR_ERROR, "Wrong number of fields in hosts table.");
    loaderror = 1;

    result->clear(result);
    return;
  }

  while(result->next(result)) {
    unsigned int groupid;
    trusthost *th;
    trustgroup *tg;

    groupid = strtoul(result->get(result, 0), NULL, 10);

    tg = tg_getbyid(groupid);
    if(!tg) {
      Error("trusts", ERR_WARNING, "Orphaned trust group host: %d", groupid);
      continue;
    }

    th = malloc(sizeof(trusthost));
    if(!th)
      continue;

    th->host = getsstring(result->get(result, 1), TRUSTHOSTLEN);
    th->maxseen = strtoul(result->get(result, 2), NULL, 10);
    th->lastseen = (time_t)strtoul(result->get(result, 3), NULL, 10);

    th->next = tg->hosts;
    tg->hosts = th;
  }

  result->clear(result);

  if(!loaderror) {
    trustsdbloaded = 1;
    triggerhook(HOOK_TRUSTS_DB_LOADED, NULL);
  }
}

static void loadhosts_fini(const DBAPIResult *result, void *tag) {
  Error("trusts", ERR_INFO, "Finished loading hosts.");
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
    trustgroup *tg;

    id = strtoul(result->get(result, 0), NULL, 10);
    if(id > tgmaxid)
      tgmaxid = id;

    tg = malloc(sizeof(trustgroup));
    if(!tg)
      continue;

    tg->id = id;
    tg->name = getsstring(result->get(result, 1), TRUSTNAMELEN);
    tg->trustedfor = strtoul(result->get(result, 2), NULL, 10);
    tg->mode = atoi(result->get(result, 3));
    tg->maxperident = strtoul(result->get(result, 4), NULL, 10);
    tg->maxseen = strtoul(result->get(result, 5), NULL, 10);
    tg->expires = (time_t)strtoul(result->get(result, 6), NULL, 10);
    tg->lastseen = (time_t)strtoul(result->get(result, 7), NULL, 10);
    tg->lastmaxuserreset = (time_t)strtoul(result->get(result, 8), NULL, 10);
    tg->createdby = getsstring(result->get(result, 9), NICKLEN);
    tg->contact = getsstring(result->get(result, 10), CONTACTLEN);
    tg->comment = getsstring(result->get(result, 11), COMMENTLEN);

    tg->hosts = NULL;

    tg->next = tglist;
    tglist = tg;
  }

  result->clear(result);  
}

static void loadgroups_fini(const DBAPIResult *result, void *tag) {
  Error("trusts", ERR_INFO, "Finished loading groups, maximum id: %d.", tgmaxid);
}

void trusts_reloaddb(void) {
  trusts_freedb();

  loaderror = 0;

  trustsdb->loadtable(trustsdb, NULL, loadgroups_data, loadgroups_fini, NULL, "groups");
  trustsdb->loadtable(trustsdb, NULL, loadhosts_data, loadhosts_fini, NULL, "hosts");
}

void trusts_closedb(void) {
  if(!trustsdb)
    return;

  trustsdb->close(trustsdb);
  trustsdb = NULL;
}
