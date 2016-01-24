#include "../../lib/version.h"
#include "../../newsearch/newsearch.h"
#include "../../core/hooks.h"
#include "../rbl.h"
#include "rbl_newsearch.h"

MODULE_VERSION("");

void _init(void) {
  registersearchterm(reg_nicksearch, "rbl", rbl_parse, 0, "");
}

void _fini(void) {
  deregistersearchterm(reg_nicksearch, "rbl", rbl_parse);
}
