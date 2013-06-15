#include <stdio.h>
#include <time.h>
#include "../lib/version.h"
#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../server/server.h"
#include "../control/control.h"

MODULE_VERSION("")

static DBAPIConn *db;

static void hook_server(int hook, void *arg);
static void create_entry(unsigned long servernum, int start_transaction);
static int list_numerics(void *source, int cargc, char **cargv);
static void rescan_network(void);
static void rescan_network_s(void *arg);

void _init(void) {
  db = dbapi2open(NULL, "numerictracker");
  if(!db) {
    Error("numerictracker", ERR_WARNING, "Unable to connect to db -- not loaded.");
    return;
  }

  db->createtable(db, NULL, NULL, "CREATE TABLE ? (numeric VARCHAR(2) NOT NULL, server_name VARCHAR(?), last_seen INT, PRIMARY KEY (numeric, server_name));", "Td", "numerics", HOSTLEN);

  registerhook(HOOK_SERVER_NEWSERVER, &hook_server);
  registerhook(HOOK_SERVER_LOSTSERVER, &hook_server);
  rescan_network();
  schedulerecurring(time(NULL)+60*5, 0, 60*5, rescan_network_s, NULL);

  registercontrolhelpcmd("listnumerics", NO_OPER, 0, list_numerics, "Usage: listnumerics\nLists all numeric-servername pairs ever seen by the service.");
}

void _fini(void) {
  if(!db)
    return;

  deregisterhook(HOOK_SERVER_NEWSERVER, &hook_server);
  deregisterhook(HOOK_SERVER_LOSTSERVER, &hook_server);
  deregistercontrolcmd("listnumerics", list_numerics);
  deleteallschedules(rescan_network_s);
}

static void rescan_network() {
  int i;

  db->squery(db, "BEGIN TRANSACTION;", "");

  for(i=0;i<MAXSERVERS;i++)
    if(serverlist[i].linkstate != LS_INVALID)
      create_entry(i, 0);

  db->squery(db, "COMMIT;", "");
}

static void rescan_network_s(void *arg) {
  rescan_network();
}

static void hook_server(int hook, void *arg) {
  unsigned long servernum = (unsigned long)arg;

  create_entry(servernum, 1);
}

static void create_entry(unsigned long servernum, int start_transaction) {
  char *server_name = serverlist[servernum].name->content;
  char *numeric = longtonumeric(servernum, 2);

  if(start_transaction)
    db->squery(db, "BEGIN TRANSACTION;", "");

  db->squery(db, "DELETE FROM ? WHERE numeric = ? AND server_name = ?;", "Tss", "numerics", numeric, server_name);
  db->squery(db, "INSERT INTO ? (numeric, server_name, last_seen) VALUES (?, ?, ?);", "Tsst", "numerics", numeric, server_name, time(NULL));

  if(start_transaction)
    db->squery(db, "COMMIT;", "");
}

static void list_numerics_callback(const DBAPIResult *result, void *tag) {
  nick *sender = getnickbynumeric((unsigned long)tag);

  if(!result) {
    if(sender)
      controlreply(sender, "DB query error 1.");
    return;
  }

  if(!sender || !result->success || (result->fields != 3)) {
    if(sender)
      controlreply(sender, "DB query error 2.");

    result->clear(result);
    return;
  }

  controlreply(sender, "%s %-50s %s", "NN", "Server name", "Last seen");
  while(result->next(result)) {
    char *numeric;
    char *server_name;
    time_t last_seen;
    char timebuf[50];

    numeric = result->get(result, 0);
    server_name = result->get(result, 1);
    last_seen = strtoul(result->get(result, 2), NULL, 10);

    strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M GMT", gmtime(&last_seen));
    controlreply(sender, " %s %-50s %s", numeric, server_name, timebuf);
  }
  result->clear(result);

  controlreply(sender, "End of listing.");
}

static int list_numerics(void *source, int cargc, char **cargv) {
  nick *sender = (nick *)source;

  rescan_network();
  db->query(db, list_numerics_callback, (void *)sender->numeric, "SELECT numeric, server_name, last_seen FROM ? ORDER BY last_seen ASC, numeric, server_name", "T", "numerics");

  return CMD_OK;
}
