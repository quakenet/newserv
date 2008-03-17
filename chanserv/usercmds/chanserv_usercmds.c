#include "../../pqsql/pqsql.h"

PQModuleIdentifier q9upqid;

void usercmds_init(void) {
  q9upqid = pqgetid();
}

void usercmds_fini(void) {
  pqfreeid(q9upqid);
}

