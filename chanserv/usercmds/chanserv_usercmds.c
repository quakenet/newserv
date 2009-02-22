#include "../../dbapi/dbapi.h"
#include "../chanserv.h"
#include "../../lib/version.h"

MODULE_VERSION(QVERSION);

DBModuleIdentifier q9udbid;

void usercmds_init(void) {
  q9udbid = dbgetid();
}

void usercmds_fini(void) {
  dbfreeid(q9udbid);
}

