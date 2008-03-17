#include "../../pqsql/pqsql.h"

PQModuleIdentifier q9cpqid;

void chancmds_init(void) {
  q9cpqid = pqgetid();
}

void chancmds_fini(void) {
  pqfreeid(q9cpqid);
}

