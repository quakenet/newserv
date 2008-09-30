#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "trusts.h"

DBAPIConn *trustsdb;
extern trustgroup *tglist;

void trusts_reloaddb(void);

int trusts_loaddb(void) {
  trustsdb = dbapi2open(NULL, "trusts");
  if(!trustsdb) {
    Error("trusts", ERR_WARNING, "Unable to connect to db -- not loaded.");
    return 0;
  }

  trustsdb->createtable(trustsdb, NULL, NULL,
    "CREATE TABLE ? (id INT PRIMARY KEY, name VARCHAR(100), trustedfor INT, mode INT, maxperident INT, maxseen INT, expires INT, lastseen INT, lastmaxuserreset INT, createdby VARCHAR(?), contact VARCHAR(?), comment VARCHAR(?))",
    "Tddd", "groups", NICKLEN, CONTACTLEN, COMMENTLEN
  );
  trustsdb->createtable(trustsdb, NULL, NULL, "CREATE TABLE ? (groupid INT, host VARCHAR(100), max INT, lastseen INT, PRIMARY KEY (groupid, host))", "T", "hosts");

  trusts_reloaddb();
  return 1;
}

static void trusts_freedb(void) {
/* do stuff */

  tglist = NULL;
}

void trusts_reloaddb(void) {
  trusts_freedb();


}

void trusts_closedb(void) {
  if(!trustsdb)
    return;

  trustsdb->close(trustsdb);
  trustsdb = NULL;
}
