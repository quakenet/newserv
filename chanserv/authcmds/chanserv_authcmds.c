#include "../../dbapi/dbapi.h"

DBModuleIdentifier q9adbid;

void authcmds_init(void) {
  q9adbid = dbgetid();
}

void authcmds_fini(void) {
  dbfreeid(q9adbid);
}
