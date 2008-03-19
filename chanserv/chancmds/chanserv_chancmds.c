#include "../../dbapi/dbapi.h"

DBModuleIdentifier q9cdbid;

void chancmds_init(void) {
  q9cdbid = dbgetid();
}

void chancmds_fini(void) {
  dbfreeid(q9cdbid);
}

