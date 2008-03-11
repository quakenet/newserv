#include "../../pqsql/pqsql.h"

PQModuleIdentifier q9apqid;

void authcmds_init(void) {
  q9apqid = pqgetid();
}

void authcmds_fini(void) {
  pqfreeid(q9apqid);
}
