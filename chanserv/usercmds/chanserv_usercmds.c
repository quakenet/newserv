#include "../../dbapi/dbapi.h"

DBModuleIdentifier q9udbid;

void usercmds_init(void) {
  q9udbid = dbgetid();
}

void usercmds_fini(void) {
  dbfreeid(q9udbid);
}

