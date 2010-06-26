#include "../../dbapi/dbapi.h"
#include "../chanserv.h"
#include "../../lib/version.h"

MODULE_VERSION(QVERSION)

DBModuleIdentifier q9cdbid;

void chancmds_init(void) {
  q9cdbid = dbgetid();
}

void chancmds_fini(void) {
  dbfreeid(q9cdbid);
}

